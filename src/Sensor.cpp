#include "Sensor.hpp"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(sensor, CONFIG_AQM_LOG_LEVEL);

cSensor g_sensor;

// Test data array - goes thru some valid readings and simulated errors
// I2C or SPI initialization and received data handler should be implemented
static const struct {
    int16_t  temp;
    uint16_t humidity;
    bool     error;
} test_data[] = {
    { 2200, 5500, false },  // 22.00°C, 55%
    { 2250, 5400, false },
    { 2180, 5600, false },
    { -1,   0,    true  },  // Simulated sensor failure
    { 2300, 5200, false },
    { 2150, 5800, false },
};

int cSensor::init(void)
{
    LOG_INF("Sensor init");
    // Initialize I2C or SPI in here. Check sensor connection etc. The state of initialization would be returned. 
    initialized_ = true;
    test_index_ = 0;
    error_count_ = 0;
    return 0;
}

bool cSensor::read(int16_t *temp_x100, uint16_t *humidity_x100)
{
    if (!initialized_) {
        return false;
    }
    
    // Get next test value
    int idx = test_index_ % ARRAY_SIZE(test_data);
    test_index_++;
    
    if (test_data[idx].error) {
        error_count_++;
        LOG_WRN("Sensor read failed (simulated)");
        return false;
    }
    
    *temp_x100 = test_data[idx].temp;
    *humidity_x100 = test_data[idx].humidity;
    
    // Reset error count on successful read
    error_count_ = 0;
    return true;
}
