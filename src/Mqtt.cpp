#include "Mqtt.hpp"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <net/mqtt_helper.h>

LOG_MODULE_REGISTER(mqtt, CONFIG_AQM_LOG_LEVEL);

cMqtt g_mqtt;

// Semaphore to wait for CONNACK
static K_SEM_DEFINE(mqtt_connected_sem, 0, 1);

// MQTT event callbacks
static void on_mqtt_connack(enum mqtt_conn_return_code return_code)
{
    if (return_code == MQTT_CONNECTION_ACCEPTED) {
        LOG_INF("MQTT connected");
        k_sem_give(&mqtt_connected_sem);
    } else {
        LOG_ERR("MQTT connection rejected: %d", return_code);
    }
}

static void on_mqtt_disconnect(int result)
{
    LOG_INF("MQTT disconnected: %d", result);
}

static void on_mqtt_publish(struct mqtt_helper_buf topic, struct mqtt_helper_buf payload)
{
    // We don't subscribe, so this won't be called
    LOG_DBG("Received publish on topic: %.*s", topic.size, topic.ptr);
}

static void on_mqtt_suback(uint16_t message_id, int result)
{
    // We don't subscribe
}

int cMqtt::init(void)
{
    int err;

    LOG_INF("MQTT init");

    struct mqtt_helper_cfg cfg = {
        .cb = {
            .on_connack = on_mqtt_connack,
            .on_disconnect = on_mqtt_disconnect,
            .on_publish = on_mqtt_publish,
            .on_suback = on_mqtt_suback,
        },
    };

    err = mqtt_helper_init(&cfg);
    if (err) {
        LOG_ERR("mqtt_helper_init failed: %d", err);
        return err;
    }

    connected_ = false;
    return 0;
}

int cMqtt::connect(void)
{
    int err;

    if (connected_) {
        return 0;
    }

    LOG_INF("Connecting to MQTT broker %s:%d...",
            CONFIG_AQM_MQTT_BROKER_HOSTNAME,
            CONFIG_AQM_MQTT_BROKER_PORT);

    // Reset semaphore
    k_sem_reset(&mqtt_connected_sem);

    // Connection parameters
    struct mqtt_helper_conn_params params = {
        .hostname = {
            .ptr = CONFIG_AQM_MQTT_BROKER_HOSTNAME,
            .size = strlen(CONFIG_AQM_MQTT_BROKER_HOSTNAME),
        },
        .device_id = {
            .ptr = CONFIG_AQM_MQTT_CLIENT_ID_PREFIX,
            .size = strlen(CONFIG_AQM_MQTT_CLIENT_ID_PREFIX),
        },
    };

    err = mqtt_helper_connect(&params);
    if (err) {
        LOG_ERR("mqtt_helper_connect failed: %d", err);
        return err;
    }

    // Wait for CONNACK (timeout 30 seconds)
    err = k_sem_take(&mqtt_connected_sem, K_SECONDS(30));
    if (err) {
        LOG_ERR("MQTT connection timeout");
        mqtt_helper_disconnect();
        return -ETIMEDOUT;
    }

    connected_ = true;
    return 0;
}

void cMqtt::disconnect(void)
{
    if (!connected_) {
        return;
    }

    LOG_INF("MQTT disconnect");
    mqtt_helper_disconnect();
    connected_ = false;
}

int cMqtt::publish(const SensorSample *samples, size_t count)
{
    int err;

    if (!connected_) {
        return -ENOTCONN;
    }

    // topic: devices/{client_id}/telemetry
    static char topic[64];
    snprintf(topic, sizeof(topic), "devices/%s/telemetry",
             CONFIG_AQM_MQTT_CLIENT_ID_PREFIX);

    // Build simple JSON payload
    // Format: {"samples":[{"ts":123,"t":2200,"h":5500},...]}
    static char payload[512];
    int pos = 0;

    pos += snprintf(payload + pos, sizeof(payload) - pos, "{\"samples\":[");

    for (size_t i = 0; i < count && pos < sizeof(payload) - 50; i++) {
        if (i > 0) {
            pos += snprintf(payload + pos, sizeof(payload) - pos, ",");
        }
        pos += snprintf(payload + pos, sizeof(payload) - pos,
                        "{\"ts\":%lld,\"t\":%d,\"h\":%u,\"f\":%u}",
                        samples[i].timestamp,
                        samples[i].temp_x100,
                        samples[i].humidity_x100,
                        samples[i].flags);
    }

    pos += snprintf(payload + pos, sizeof(payload) - pos, "]}");

    LOG_DBG("Publishing to %s: %s", topic, payload);

    struct mqtt_publish_param param = {
        .message = {
            .topic = {
                .topic = {
                    .utf8 = topic,
                    .size = strlen(topic),
                },
                .qos = MQTT_QOS_1_AT_LEAST_ONCE,
            },
            .payload = {
                .data = payload,
                .len = strlen(payload),
            },
        },
        .message_id = mqtt_helper_msg_id_get(),
    };

    err = mqtt_helper_publish(&param);
    if (err) {
        LOG_ERR("mqtt_helper_publish failed: %d", err);
        return err;
    }

    LOG_INF("Published %d samples", count);
    return 0;
}
