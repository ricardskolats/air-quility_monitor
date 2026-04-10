#ifndef MQTT_HPP
#define MQTT_HPP

#include "types.hpp"
#include <cstddef>

// MQTT client for uploading sensor data
// TODO: Zephyrs MQTT library with TLS support, subcribing to topics, FOTA update using SDK libraries.
class cMqtt {
public:
    int  init(void);
    int  connect(void);
    void disconnect(void);
    int  publish(const SensorSample *samples, size_t count);
    
    bool is_connected(void) const { return connected_; }

private:
    bool connected_ = false;
};

extern cMqtt g_mqtt;

#endif // MQTT_HPP
