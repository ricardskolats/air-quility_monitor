#include "Uploader.hpp"
#include "Buffer.hpp"
#include "Network.hpp"
#include "Mqtt.hpp"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(uploader, CONFIG_AQM_LOG_LEVEL);

// Global instance
cUploader g_uploader;

// Zephyr timer and work item (must be static/global)
K_TIMER_DEFINE(upload_timer, cUploader::timer_handler, NULL);
K_WORK_DEFINE(upload_work, cUploader::work_handler);

// Max samples per MQTT message
#define BATCH_SIZE 12

int cUploader::init(void)
{
    LOG_INF("Uploader init (interval: %d sec)", CONFIG_AQM_UPLOAD_INTERVAL_SEC);
    failure_count_ = 0;
    return 0;
}

void cUploader::start(void)
{
    if (running_) return;
    
    running_ = true;
    k_timer_start(&upload_timer,
                  K_SECONDS(CONFIG_AQM_UPLOAD_INTERVAL_SEC),
                  K_SECONDS(CONFIG_AQM_UPLOAD_INTERVAL_SEC));
    LOG_INF("Upload task started");
}

void cUploader::stop(void)
{
    if (!running_) return;
    
    running_ = false;
    k_timer_stop(&upload_timer);
    LOG_INF("Upload task stopped");
}

void cUploader::trigger(void)
{
    // Schedule immediate upload
    k_work_submit(&upload_work);
}

void cUploader::timer_handler(struct k_timer *timer)
{
    ARG_UNUSED(timer);
    // Timer runs in ISR context - just schedule work
    k_work_submit(&upload_work);
}

void cUploader::work_handler(struct k_work *work)
{
    ARG_UNUSED(work);
    
    int ret = g_uploader.do_upload();
    
    if (ret != 0) {
        g_uploader.failure_count_++;
        LOG_WRN("Upload failed (%d consecutive)", g_uploader.failure_count_);
        
        // Retry with backoff
        if (g_uploader.failure_count_ < CONFIG_AQM_UPLOAD_RETRY_COUNT) {
            int delay = CONFIG_AQM_UPLOAD_RETRY_BACKOFF_SEC * g_uploader.failure_count_;
            LOG_INF("Retry in %d seconds", delay);
            k_timer_start(&upload_timer, K_SECONDS(delay), 
                         K_SECONDS(CONFIG_AQM_UPLOAD_INTERVAL_SEC));
        } else {
            LOG_ERR("Upload retries exhausted, will try next interval");
            g_uploader.failure_count_ = 0;
        }
    } else {
        g_uploader.failure_count_ = 0;
    }
}

int cUploader::do_upload(void)
{
    if (g_buffer.is_empty()) {
        LOG_DBG("Nothing to upload");
        return 0;
    }
    
    LOG_INF("Starting upload (%d samples)", g_buffer.count());
    
    // Connect to LTE-M network
    if (g_network.connect() != 0) {
        LOG_ERR("Network unavailable");
        return -1;
    }
    
    // Connect to MQTT broker
    if (g_mqtt.connect() != 0) {
        g_network.disconnect();
        return -1;
    }
    
    // Upload samples in batches
    int uploaded = 0;
    while (!g_buffer.is_empty()) {
        SensorSample batch[BATCH_SIZE];
        size_t n = g_buffer.peek(batch, BATCH_SIZE);
        
        if (g_mqtt.publish(batch, n) == 0) {
            g_buffer.pop(n);  // Only pop if the publish was successful
            uploaded += n;
        } else {
            LOG_ERR("Publish failed, keeping %d samples", g_buffer.count());
            break;
        }
    }
    
    // Cleanup
    g_mqtt.disconnect();
    g_network.disconnect();
    
    LOG_INF("Upload complete (%d samples sent)", uploaded);
    return (uploaded > 0) ? 0 : -1;
}
