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
    Serial.printf("[Config MQTT] %s: %s\n", topic, msg.c_str());
    handleConfig(msg);
}

void configMqttConnect() {
    if (mqttHost.length() == 0) {
        Serial.println("[Config MQTT] no mqttHost — skipping");
        return;
    }

    String topic    = projectId + "." + String(DEVICE_NAME) + ".config";
    String clientId = robotId + "-cfg";

    _cfgMqtt.setServer(mqttHost.c_str(), mqttPort);
    _cfgMqtt.setCallback(_configCallback);

    Serial.printf("[Config MQTT] connecting to %s:%d  topic: %s\n",
                  mqttHost.c_str(), mqttPort, topic.c_str());

    int attempts = 0;
    while (!_cfgMqtt.connected() && attempts++ < 5) {
        if (_cfgMqtt.connect(clientId.c_str())) {
            _cfgMqtt.subscribe(topic.c_str(), 1);
            Serial.printf("[Config MQTT] connected, subscribed to %s\n", topic.c_str());
            tlog("Cfg MQTT: OK");
        } else {
            Serial.printf("[Config MQTT] failed (%d), retrying...\n", _cfgMqtt.state());
            delay(2000);
        }
    }
}

void configMqttLoop() {
    if (!_cfgMqtt.connected()) {
        Serial.println("[Config MQTT] reconnecting...");
        configMqttConnect();
    }
    _cfgMqtt.loop();
}

// ─── Inbound handler ─────────────────────────────────────────────────────────

void handleConfig(const String& json) {
    JsonDocument doc;
    if (deserializeJson(doc, json) != DeserializationError::Ok) {
        Serial.println("[Config] Invalid JSON");
        return;
    }

    Serial.println("[Config] Received:");
    serializeJsonPretty(doc, Serial);
    Serial.println();

    setState(STATE_IDLE);
}
