# Air Quality Monitor 

## Platform

- **MCU:** Nordic nRF9160 (Cortex-M33 with LTE-M modem)
- **SDK:** nRF Connect SDK v2.6.x (based on Zephyr RTOS)
- **Language:** C++17

## Build Instructions

Note: building the project is not tested.  

### Prerequisites

1. nRF Connect SDK (version 2.6.0 or later)
2. VS Code with nRF Connect extension

### Building

```bash
# For nRF9160-DK development board
west build -b nrf9160dk_nrf9160_ns

```

### Flashing

```bash
west flash
```

### Configuration

In `prj.conf` MQTT broker can be set:
```
CONFIG_AQM_MQTT_BROKER_HOSTNAME="your-broker.example.com"
CONFIG_AQM_MQTT_BROKER_PORT=8883
```

## Architecture

```
main.cpp
   │
   ├── cSensor      - Reads temp/humidity (Sensor.cpp)
   │
   ├── cBuffer      - Stores samples (Buffer.cpp)
   │       │
   │       └── 80% full? ──► cUploader.trigger()
   │
   └── cUploader    - Periodic upload (Uploader.cpp)
           │
           ├── cNetwork  - LTE-M modem (Network.cpp)
           │
           └── cMqtt     - MQTT+TLS (Mqtt.cpp)
```
