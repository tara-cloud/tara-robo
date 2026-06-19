#include "TaraOTA.h"
#include "TaraCore.h"
#include <ArduinoJson.h>
#include <HTTPUpdate.h>
#include <WiFiClient.h>
#include <PubSubClient.h>

// ─── Dedicated OTA MQTT client ────────────────────────────────────────────────

static WiFiClient   _otaWifi;
static PubSubClient _otaMqtt(_otaWifi);

static void _otaCallback(const char* topic, byte* payload, unsigned int length) {
    String msg = String((char*)payload, length);
    Serial.printf("[OTA MQTT] %s: %s\n", topic, msg.c_str());
    handleOTA(msg);
}

void otaMqttConnect() {
    if (mqttHost.length() == 0) {
        Serial.println("[OTA MQTT] no mqttHost — skipping");
        return;
    }

    // Topic: projectId.deviceName.ota  (e.g. "tara01.Tara.ota")
    String topic = projectId + "." + String(DEVICE_NAME) + ".ota";

    _otaMqtt.setServer(mqttHost.c_str(), mqttPort);
    _otaMqtt.setCallback(_otaCallback);

    String clientId = robotId + "-ota";
    Serial.printf("[OTA MQTT] connecting to %s:%d  topic: %s\n",
                  mqttHost.c_str(), mqttPort, topic.c_str());

    int attempts = 0;
    while (!_otaMqtt.connected() && attempts++ < 5) {
        if (_otaMqtt.connect(clientId.c_str())) {
            _otaMqtt.subscribe(topic.c_str(), 1);
            Serial.printf("[OTA MQTT] connected, subscribed to %s\n", topic.c_str());
            tlog("OTA MQTT: OK");
        } else {
            Serial.printf("[OTA MQTT] failed (%d), retrying...\n", _otaMqtt.state());
            delay(2000);
        }
    }
}

void otaMqttLoop() {
    if (!_otaMqtt.connected()) {
        Serial.println("[OTA MQTT] reconnecting...");
        otaMqttConnect();
    }
    _otaMqtt.loop();
}

// ─── Status publisher ─────────────────────────────────────────────────────────

void publishOTAStatus(const String& state, const String& version,
                      int progress, const String& error) {
    JsonDocument doc;
    doc["state"]   = state;
    doc["version"] = version;
    if (progress >= 0)      doc["progress"] = progress;
    if (error.length() > 0) doc["error"]    = error;

    String msg;
    serializeJson(doc, msg);
    _otaMqtt.publish(robotTopic("ota_status").c_str(), msg.c_str(), false);
    Serial.printf("[OTA] status → %s\n", msg.c_str());
}

// ─── Inbound handler ─────────────────────────────────────────────────────────

void handleOTA(const String& json) {
    JsonDocument doc;
    if (deserializeJson(doc, json) != DeserializationError::Ok) return;

    String version = doc["version"] | String("");
    String url     = doc["url"]     | String("");

    if (url.length() == 0) {
        Serial.println("[OTA] No URL in payload");
        publishOTAStatus("failed", version, -1, "no url");
        return;
    }

    Serial.printf("[OTA] Starting update v%s from %s\n", version.c_str(), url.c_str());
    tlog("OTA: v" + version);
    tlog("Downloading...");
    publishOTAStatus("downloading", version);

    WiFiClient client;

    httpUpdate.onProgress([&version](int recv, int total) {
        if (total <= 0) return;
        int pct = (recv * 100) / total;
        static int lastPct = -1;
        if (pct != lastPct && pct % 10 == 0) {
            lastPct = pct;
            Serial.printf("[OTA] %d%%\n", pct);
            publishOTAStatus("progress", version, pct);
        }
    });

    t_httpUpdate_return ret = httpUpdate.update(client, url);

    switch (ret) {
        case HTTP_UPDATE_FAILED: {
            String err = httpUpdate.getLastErrorString();
            Serial.printf("[OTA] Failed: %s\n", err.c_str());
            tlog("OTA Failed!");
            publishOTAStatus("failed", version, -1, err);
            setState(STATE_ERROR);
            break;
        }
        case HTTP_UPDATE_NO_UPDATES:
            Serial.println("[OTA] No update needed");
            tlog("OTA: up to date");
            publishOTAStatus("ok", version, 100);
            setState(STATE_IDLE);
            break;

        case HTTP_UPDATE_OK:
            Serial.println("[OTA] Success — rebooting");
            tlog("OTA OK! Rebooting");
            publishOTAStatus("ok", version, 100);
            // device reboots automatically
            break;
    }
}
