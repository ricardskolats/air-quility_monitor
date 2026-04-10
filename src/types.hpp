#ifndef TYPES_HPP
#define TYPES_HPP

#include <cstdint>

// Sensor sample - 16 bytes total
struct SensorSample {
    int64_t  timestamp;      // Unix time in seconds
    int16_t  temp_x100;      // Temperature * 100 (e.g., 2250 = 22.50°C)
    uint16_t humidity_x100;  // Humidity * 100 (e.g., 5500 = 55.00%)
    uint8_t  flags;          // 0=OK, 1=sensor error
    uint8_t  reserved[3];    // Padding for alignment
};

#endif // TYPES_HPP
