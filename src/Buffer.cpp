#include "Buffer.hpp"
#include <zephyr/kernel.h>
#include <zephyr/sys/ring_buffer.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(buffer, CONFIG_AQM_LOG_LEVEL);

cBuffer g_buffer;

// Zephyr ring buffer - stores raw bytes
#define MAX_SAMPLES CONFIG_AQM_BUFFER_SIZE
RING_BUF_DECLARE(sample_ring, MAX_SAMPLES * sizeof(SensorSample));

int cBuffer::init(void)
{
    ring_buf_reset(&sample_ring);
    count_ = 0;
    LOG_INF("Buffer init (capacity: %d samples)", MAX_SAMPLES);
    return 0;
}

int cBuffer::push(const SensorSample *sample)
{
    // If buffer full, drop oldest
    // Alternative would be rejecting new samples, but dropping oldest is better
    if (count_ >= MAX_SAMPLES) {
        SensorSample discard;
        ring_buf_get(&sample_ring, (uint8_t*)&discard, sizeof(discard));
        count_--;
        LOG_WRN("Buffer full, dropped oldest sample");
    }
    
    uint32_t written = ring_buf_put(&sample_ring, 
                                     (const uint8_t*)sample, 
                                     sizeof(SensorSample));
    if (written != sizeof(SensorSample)) {
        LOG_ERR("Buffer write failed");
        return -1;
    }
    
    count_++;
    return 0;
}

size_t cBuffer::peek(SensorSample *samples, size_t max_count)
{
    if (count_ == 0 || max_count == 0) {
        return 0;
    }
    
    size_t n = MIN(count_, max_count);
    uint32_t bytes = ring_buf_peek(&sample_ring, (uint8_t*)samples, 
                                    n * sizeof(SensorSample));
    return bytes / sizeof(SensorSample);
}

void cBuffer::pop(size_t count)
{
    SensorSample discard;
    for (size_t i = 0; i < count && count_ > 0; i++) {
        ring_buf_get(&sample_ring, (uint8_t*)&discard, sizeof(discard));
        count_--;
    }
}

void cBuffer::clear(void)
{
    ring_buf_reset(&sample_ring);
    count_ = 0;
    LOG_INF("Buffer cleared");
}

size_t cBuffer::capacity(void) const
{
    return MAX_SAMPLES;
}

int cBuffer::fill_percent(void) const
{
    return (count_ * 100) / MAX_SAMPLES;
}
