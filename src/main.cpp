// Thread-based Air Quality Monitor
// Thread queue version instead of timer+work queue

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <date_time.h>

#include "types.hpp"
#include "Sensor.hpp"
#include "Buffer.hpp"
#include "Network.hpp"
#include "Mqtt.hpp"

LOG_MODULE_REGISTER(main, CONFIG_AQM_LOG_LEVEL);

// Each thread needs its own stack
K_THREAD_STACK_DEFINE(sensor_stack, 2048);
K_THREAD_STACK_DEFINE(upload_stack, 4096);

static struct k_thread sensor_thread;
static struct k_thread upload_thread;

// Semaphore to trigger early upload when buffer hits high water mark
static K_SEM_DEFINE(upload_trigger, 0, 1);

#define BATCH_SIZE 12

// ============================================================================
// SENSOR THREAD - reads sensor every 5 min
// ============================================================================

static void sensor_thread_fn(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    while (1) {
        SensorSample sample = {0};

        // Get timestamp
        int64_t now_ms;
        if (date_time_now(&now_ms) == 0) {
            sample.timestamp = now_ms / 1000;
        } else {
            sample.timestamp = k_uptime_get() / 1000;
        }

        // Read sensor
        int16_t temp;
        uint16_t humidity;
        if (g_sensor.read(&temp, &humidity)) {
            sample.temp_x100 = temp;
            sample.humidity_x100 = humidity;
            sample.flags = 0;
            LOG_INF("Sensor: %d.%02d C, %u.%02u %%",
                    temp / 100, abs(temp % 100),
                    humidity / 100, humidity % 100);
        } else {
            sample.flags = 1;
            LOG_WRN("Sensor error (count: %d)", g_sensor.get_error_count());
        }

        // Store in buffer
        g_buffer.push(&sample);
        LOG_DBG("Buffer: %d/%d samples", g_buffer.count(), g_buffer.capacity());

        // Trigger early upload if buffer getting full (80%)
        if (g_buffer.fill_percent() >= CONFIG_AQM_BUFFER_HIGH_WATER_PERCENT) {
            LOG_WRN("Buffer at %d%%, triggering early upload", g_buffer.fill_percent());
            k_sem_give(&upload_trigger);
        }

        // Sleep 5 minutes
        k_sleep(K_SECONDS(CONFIG_AQM_SENSOR_INTERVAL_SEC));
    }
}

// ============================================================================
// UPLOAD THREAD - uploads every 30 min or buffera triggers
// ============================================================================

static void upload_thread_fn(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    int failure_count = 0;

    while (1) {
        // Wait for 30 min timeout OR early trigger 
        k_sem_take(&upload_trigger, K_SECONDS(CONFIG_AQM_UPLOAD_INTERVAL_SEC));

        if (g_buffer.is_empty()) {
            continue;
        }

        LOG_INF("Starting upload (%d samples)", g_buffer.count());

        // Connect to LTE-M network
        if (g_network.connect() != 0) {
            LOG_ERR("Network unavailable");
            failure_count++;
            goto retry;
        }

        // Connect to MQTT broker
        if (g_mqtt.connect() != 0) {
            LOG_ERR("MQTT connect failed");
            g_network.disconnect();
            failure_count++;
            goto retry;
        }

        // Upload samples in batches
        {
            int uploaded = 0;
            while (!g_buffer.is_empty()) {
                SensorSample batch[BATCH_SIZE];
                size_t n = g_buffer.peek(batch, BATCH_SIZE);

                if (g_mqtt.publish(batch, n) == 0) {
                    g_buffer.pop(n);
                    uploaded += n;
                } else {
                    LOG_ERR("Publish failed, keeping %d samples", g_buffer.count());
                    break;
                }
            }

            g_mqtt.disconnect();
            g_network.disconnect();

            LOG_INF("Upload complete (%d samples sent)", uploaded);
            failure_count = 0;
            continue;
        }

retry:
        // Retry with backoff
        if (failure_count < CONFIG_AQM_UPLOAD_RETRY_COUNT) {
            int delay = CONFIG_AQM_UPLOAD_RETRY_BACKOFF_SEC * failure_count;
            LOG_WRN("Upload failed (%d), retry in %d sec", failure_count, delay);
            k_sleep(K_SECONDS(delay));
            k_sem_give(&upload_trigger);  // Re-trigger immediately after backoff
        } else {
            LOG_ERR("Upload retries exhausted, will try next interval");
            failure_count = 0;
        }
    }
}

// ============================================================================
// MAIN
// ============================================================================

int main(void)
{
    LOG_INF("========================================");
    LOG_INF("Air Quality Monitor v1.0 (thread version)");
    LOG_INF("Sensor interval: %d sec", CONFIG_AQM_SENSOR_INTERVAL_SEC);
    LOG_INF("Upload interval: %d sec", CONFIG_AQM_UPLOAD_INTERVAL_SEC);
    LOG_INF("Buffer size: %d samples", CONFIG_AQM_BUFFER_SIZE);
    LOG_INF("========================================");

    // Initialize
    g_sensor.init();
    g_buffer.init();
    g_network.init();
    g_mqtt.init();

    // Create sensor thread (priority 5)
    k_thread_create(&sensor_thread, sensor_stack,
                    K_THREAD_STACK_SIZEOF(sensor_stack),
                    sensor_thread_fn,
                    NULL, NULL, NULL,
                    5, 0, K_NO_WAIT);
    k_thread_name_set(&sensor_thread, "sensor");

    // Create upload thread (priority 7 - lower priority than sensor)
    k_thread_create(&upload_thread, upload_stack,
                    K_THREAD_STACK_SIZEOF(upload_stack),
                    upload_thread_fn,
                    NULL, NULL, NULL,
                    7, 0, K_NO_WAIT);
    k_thread_name_set(&upload_thread, "upload");

    LOG_INF("Threads started");

    // Main thread
    while (1) {
        if (g_sensor.get_error_count() >= 10) {
            LOG_ERR("Excessive sensor errors: %d", g_sensor.get_error_count());
        }

        k_sleep(K_SECONDS(60));
    }

    return 0;
}
