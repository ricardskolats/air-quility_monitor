#ifndef SENSOR_HPP
#define SENSOR_HPP

#include "types.hpp"

// Sensor driver - reads temperature and hummidity
// Sensor data stubbed 
class cSensor {
public:
    int  init(void);
    bool read(int16_t *temp_x100, uint16_t *humidity_x100);
    
    uint32_t get_error_count(void) const { return error_count_; }

private:
    bool initialized_ = false;
    int  test_index_ = 0;
    uint32_t error_count_ = 0;
};

extern cSensor g_sensor;

#endif // SENSOR_HPP
