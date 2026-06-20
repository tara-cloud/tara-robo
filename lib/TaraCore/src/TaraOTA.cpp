#include "TaraOTA.h"
#include "TaraCore.h"
#include "TaraLog.h"
#include <ArduinoJson.h>
#include <HTTPUpdate.h>
#include <HTTPClient.h>
#include <WiFiClient.h>
#include <PubSubClient.h>

// ─── Dedicated OTA MQTT client ────────────────────────────────────────────────

static WiFiClient   _otaWifi;
static PubSubClient _otaMqtt(_otaWifi);

static void _otaCallback(const char* topic, byte* payload, unsigned int length) {
    String msg = String((char*)payload, length);
    Serial.printf("[OTA MQTT] topic=%s msg=%s\n", topic, msg.c_str());
    handleOTA(msg);
}

void otaMqttConnect() {
    if (mqttHost.length() == 0) {
        TLOG(LOG_WARN, "OTA MQTT: no mqttHost — skipping");
        return;
    }

    String topic    = projectId + "." + String(DEVICE_NAME) + ".ota";
    String clientId = robotId + "-ota";

    _otaMqtt.setServer(mqttHost.c_str(), mqttPort);
    _otaMqtt.setCallback(_otaCallback);

    TLOG(LOG_INFO, "OTA MQTT connecting to %s:%d topic=%s", mqttHost.c_str(), mqttPort, topic.c_str());

    int attempts = 0;
    while (!_otaMqtt.connected() && attempts++ < 5) {
        if (_otaMqtt.connect(clientId.c_str())) {
            _otaMqtt.subscribe(topic.c_str(), 1);
            Serial.printf("[OTA MQTT] connected — subscribed to: %s\n", topic.c_str());
            tlog("OTA MQTT: OK");

            // Wire log4c MQTT appender now that broker is reachable
            String logTopic = projectId + "/" + String(DEVICE_NAME) + "/logs";
            log4c_set_mqtt(logTopic, [](const char* t, const char* p) -> bool {
                return _otaMqtt.publish(t, p, false);
            });
        } else {
            TLOG(LOG_WARN, "OTA MQTT failed (%d), retrying", _otaMqtt.state());
            delay(2000);
        }
    }
}

void otaMqttLoop() {
    if (!_otaMqtt.connected()) {
        TLOG(LOG_WARN, "OTA MQTT reconnecting");
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
    TLOG(LOG_INFO, "OTA status: %s", msg.c_str());
}

// ─── Inbound handler ─────────────────────────────────────────────────────────

void handleOTA(const String& json) {
    JsonDocument doc;
    if (deserializeJson(doc, json) != DeserializationError::Ok) return;

    String version = doc["version"] | String("");
    String url     = doc["url"]     | String("");
    String apiKey  = doc["apiKey"]  | String("");

    if (url.length() == 0) {
        TLOG(LOG_ERROR, "OTA: no URL in payload");
        publishOTAStatus("failed", version, -1, "no url");
        return;
    }

    TLOG(LOG_INFO, "OTA starting v%s from %s", version.c_str(), url.c_str());
    tlog("OTA: v" + version);
    tlog("Downloading...");
    publishOTAStatus("downloading", version);

    WiFiClient wifiCli;
    HTTPClient http;
    http.begin(wifiCli, url);
    if (apiKey.length() > 0) {
        http.addHeader("x-pocket-token", apiKey);
    }

    httpUpdate.onProgress([&version](int recv, int total) {
        if (total <= 0) return;
        int pct = (recv * 100) / total;
        static int lastPct = -1;
        if (pct != lastPct && pct % 10 == 0) {
            lastPct = pct;
            TLOG(LOG_DEBUG, "OTA progress: %d%%", pct);
            publishOTAStatus("progress", version, pct);
        }
    });

    t_httpUpdate_return ret = httpUpdate.update(http);

    switch (ret) {
        case HTTP_UPDATE_FAILED: {
            String err = httpUpdate.getLastErrorString();
            TLOG(LOG_ERROR, "OTA failed: %s", err.c_str());
            tlog("OTA Failed!");
            publishOTAStatus("failed", version, -1, err);
            setState(STATE_ERROR);
            break;
        }
        case HTTP_UPDATE_NO_UPDATES:
            TLOG(LOG_INFO, "OTA: no update needed");
            tlog("OTA: up to date");
            publishOTAStatus("ok", version, 100);
            setState(STATE_IDLE);
            break;

        case HTTP_UPDATE_OK:
            TLOG(LOG_INFO, "OTA success — rebooting");
            tlog("OTA OK! Rebooting");
            publishOTAStatus("ok", version, 100);
            break;
    }
}
