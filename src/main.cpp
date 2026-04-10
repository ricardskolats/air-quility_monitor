#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <date_time.h>

#include "types.hpp"
#include "Sensor.hpp"
#include "Buffer.hpp"
#include "Network.hpp"
#include "Mqtt.hpp"
#include "Uploader.hpp"

LOG_MODULE_REGISTER(main, CONFIG_AQM_LOG_LEVEL);

static void sensor_work_fn(struct k_work *work);
static void sensor_timer_fn(struct k_timer *timer);

K_WORK_DEFINE(sensor_work, sensor_work_fn);
K_TIMER_DEFINE(sensor_timer, sensor_timer_fn, NULL);

static void sensor_timer_fn(struct k_timer *timer)
{
    ARG_UNUSED(timer);
    k_work_submit(&sensor_work);
}

static void sensor_work_fn(struct k_work *work)
{
    ARG_UNUSED(work);
    
    SensorSample sample = {0};
    
    // Getting timestamp
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
        sample.flags = 1;  // Error flag
        LOG_WRN("Sensor error (count: %d)", g_sensor.get_error_count());
    }
    
    // Store in buffer
    g_buffer.push(&sample);
    LOG_DBG("Buffer: %d/%d samples", g_buffer.count(), g_buffer.capacity());
    
    // Trigger early upload if buffer getting full (80%)
    if (g_buffer.fill_percent() >= CONFIG_AQM_BUFFER_HIGH_WATER_PERCENT) {
        LOG_WRN("Buffer at %d%%, triggering early upload", g_buffer.fill_percent());
        g_uploader.trigger();
    }
}

int main(void)
{ 
    // Initialize all modules
    g_sensor.init();
    g_buffer.init();
    g_network.init();
    g_mqtt.init();
    g_uploader.init();
    
    // Start timer to read sensor periodically
    k_timer_start(&sensor_timer,
                  K_SECONDS(CONFIG_AQM_SENSOR_INTERVAL_SEC),
                  K_SECONDS(CONFIG_AQM_SENSOR_INTERVAL_SEC));
    LOG_INF("Sensor task started");
    
    // Start upload task
    g_uploader.start();
    
    while (1) {
        if (g_sensor.get_error_count() >= 10) {
            LOG_ERR("Excessive sensor errors: %d", g_sensor.get_error_count());
        }
        
        k_sleep(K_SECONDS(60));
    }
    
    return 0;
}
