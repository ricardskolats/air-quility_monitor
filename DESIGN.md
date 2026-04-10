# Design Document - Air Quality Monitor

**Hardware:** nRF9160
**Language:** C++17  
**SDK:** nRF Connect SDK (built on Zephyr RTOS)  
**Target boards:** nrf9160dk_nrf9160_ns

**APIs used:**
- Zephyr
- nRF SDK

---

## 1. Protocol Choice: MQTT over TLS

**Why MQTT:**
- Easier to be implemented/supported by cloud platforms. Much more common then CoAP. 
- Works well with PSM sleep (device can wake, publish, sleep)
- In this case also more familiar with mqtt, then CoAP.

**Main weakness:**
- TCP-based, so each connection has TLS handshake overhead (~5-10 KB)
- Mitigation: Not implemented. 

---

## 2. Power Strategy

```
Order of actions:
├─ Initilise everything (Sensor(i2c/spi + power, if controllable from mcu), lte, mqtt etc.)
├─ Get readings from sensor.
├─ Deinit sensor (i2c/spi + power)
├─ MCU Sleep (Zephyr idle thread)
├─ Wakes up from sleep after 5 min (1Hz rtc) 
├─ ... 
├─ ...
├─ (After 30 min) 
├─ Init lte modem
├─ Send payload batch via mqtt
├─ Send lte modem to sleep (PSM)
├─ MCU sleep
```

**Estimated battery life:** With minimal sleep current (usually around few uA), and sending lte transmission only every 30 min, should work for a few months with no problems.

---

## 3. Security Decisions (CRA Requirements)

| Requirement | Implementation |
|-------------|----------------|
| **Secure transport** | TLS 1.2 with server certificate validation. MQTT port 8883. Modem's built-in TLS stack handles crypto. |
| **Secrets handling** | TLS credentials stored in modem's secure storage (not in application flash). Device ID from IMEI - no shared secrets in firmware binary. |
| **OTA updates** | nRF Connect SDK includes FOTA library. Firmware signed, download via HTTPS, bootloader verifies signature before applying. Would use AWS S3 + metadata file or nRF Cloud FOTA. |

**What I would NOT do:**
- Hardcode credentials in source
- Disable certificate validation "for testing"
- Use unencrypted MQTT (port 1883)

**Secure transport**
    Using TLS with server certificate validation.
**Secrets handling**
    TLS credentials stored in modem's secure storage is recommended. 
**OTA updates**
    nRF Connect SDK includes FOTA library. Firmware signed, download via HTTPS, bootloader verifies signature before applying. More detailed steps of FOTA update must be studied. Time limitation did not allowed it.
---

## 4. Troubleshooting Design

### Log Format
```
[timestamp][severity][module] message
Example: [00:05:23][ERR][aqm] Sensor read failed
```

Severity levels: DBG, INF, WRN, ERR

### Current implementation: 
  - RAM-only (logs lost on reset)  
### Recommended implemention:
  - Would use NVS (flash key-value store) to persist last N log entries

### Remote Access
Two approaches considered:
1. **Upload with telemetry** - Include last 10 logs in each MQTT publish (simple)
2. **On-demand request** - Cloud sends command, device uploads logs (more control)

---

## 5. What I Would Do Next (Priority Order)

1. **Persistent buffer (NVS)** - Currently RAM-only, data lost on reset. Would use Zephyr's NVS or FCB (Flash Circular Buffer) for reliability.

2. **Real sensor driver** - Replace stub with actual I2C driver for real temperature and humidity sensor.

3. **CBOR encoding** - Currently using JSON. Would use zcbor library to encode samples as compact binary (saves bandwidth).

4. **Watchdog timer** - Add hardware watchdog to reset device if it hangs. Insures that device does not freeze.

5. **OTA update support** - Integrate nRF FOTA library. Allow remote firmware updates.

---

## 6. Assumptions Made

- Sensor already connected via I2C (driver implementation out of scope)
- MQTT broker exists and accepts connections
- SIM card provisioned for LTE-M
- Server certificates pre-loaded in modem secure storage

---
