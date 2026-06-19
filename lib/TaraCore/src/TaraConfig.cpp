#include "TaraConfig.h"
#include "TaraCore.h"
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <WiFiClient.h>

// ─── Dedicated config MQTT client ────────────────────────────────────────────

static WiFiClient   _cfgWifi;
static PubSubClient _cfgMqtt(_cfgWifi);

static void _configCallback(const char* topic, byte* payload, unsigned int length) {
    String msg = String((char*)payload, length);
    TLOG(LOG_DEBUG, "Config MQTT [%s]: %s", topic, msg.c_str());
    handleConfig(msg);
}

void configMqttConnect() {
    if (mqttHost.length() == 0) {
        TLOG(LOG_WARN, "Config MQTT: no mqttHost — skipping");
        return;
    }

    String topic    = projectId + "." + String(DEVICE_NAME) + ".config";
    String clientId = robotId + "-cfg";

    _cfgMqtt.setServer(mqttHost.c_str(), mqttPort);
    _cfgMqtt.setCallback(_configCallback);

    TLOG(LOG_INFO, "Config MQTT connecting to %s:%d topic=%s", mqttHost.c_str(), mqttPort, topic.c_str());

    int attempts = 0;
    while (!_cfgMqtt.connected() && attempts++ < 5) {
        if (_cfgMqtt.connect(clientId.c_str())) {
            _cfgMqtt.subscribe(topic.c_str(), 1);
            TLOG(LOG_INFO, "Config MQTT connected, subscribed to %s", topic.c_str());
            tlog("Cfg MQTT: OK");
        } else {
            TLOG(LOG_WARN, "Config MQTT failed (%d), retrying", _cfgMqtt.state());
            delay(2000);
        }
    }
}

void configMqttLoop() {
    if (!_cfgMqtt.connected()) {
        TLOG(LOG_WARN, "Config MQTT reconnecting");
        configMqttConnect();
    }
    _cfgMqtt.loop();
}

// ─── Inbound handler ─────────────────────────────────────────────────────────

void handleConfig(const String& json) {
    JsonDocument doc;
    if (deserializeJson(doc, json) != DeserializationError::Ok) {
        TLOG(LOG_ERROR, "Config: invalid JSON");
        return;
    }

    String out;
    serializeJson(doc, out);
    TLOG(LOG_INFO, "Config received: %s", out.c_str());

    setState(STATE_IDLE);
}
