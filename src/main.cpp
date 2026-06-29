#include <Arduino.h>
#include <WiFi.h>
#include "TaraCore.h"
#include <wifi4h.h>
#include <reg4h.h>
#include <socket4h.h>
#include <ota4h.h>

// ─── Globals ─────────────────────────────────────────────────────────────────
String     robotId;
String     serverUrl;
String     projectId;
String     socketHost;
uint16_t   socketPort    = 3001;
RobotState currentState  = STATE_BOOTING;

// ─── Timing ──────────────────────────────────────────────────────────────────
static unsigned long _lastHeartbeat = 0;
static unsigned long _lastHealth    = 0;
static unsigned long _lastFrame     = 0;

// ─── Setup ───────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(100);

    pinMode(16, OUTPUT);
    digitalWrite(16, HIGH);

    log4c_init();
    log4c_set("device", DEVICE_NAME);
    log4c_set("firmwareVersion", FW_VERSION);

    setupDeviceHardware();
    tlog(String(DEVICE_NAME) + " v" + FW_VERSION);

    // Derive robotId from MAC
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    char macStr[13];
    snprintf(macStr, sizeof(macStr), "%02X%02X%02X%02X%02X%02X",
        mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    robotId = String(macStr);
    tlog("Robot ID: " + robotId);

    // ─── WiFi provisioning ───────────────────────────────────────────────────
    wifi4h_set_device_id(robotId);
    wifi4h_set_namespace("tara-wifi");
    wifi4h_add_field("ssid",      "string",   true);
    wifi4h_add_field("password",  "password", false);
    wifi4h_add_field("projectId", "string",   true);
    wifi4h_add_field("host",      "string",   true);
    wifi4h_add_field("port",      "number",   false);
    wifi4h_on_event([](const String& ev, const String& detail) {
        tlog(ev + (detail.length() ? ": " + detail : ""));
    });

    wifi4h_load();

    projectId  = wifi4h_get("projectId");
    socketHost = wifi4h_get("host");
    uint16_t p = (uint16_t)wifi4h_get("port").toInt();
    if (p > 0) socketPort = p;
    if (socketHost.length() > 0)
        serverUrl = "http://" + socketHost + ":30400";

    setState(STATE_CONNECTING);
    wifi4h_connect();

    // ─── OTA state callback ───────────────────────────────────────────────────
    ota4h_on_state([](const String& state, int pct) {
        if (state == "progress") tlog("OTA: " + String(pct) + "%");
        else                     tlog("OTA: " + state);
        if (state == "failed")   setState(STATE_ERROR);
    });

    // ─── Register socket handlers BEFORE connecting ───────────────────────────
    // Must be set up before registerRobot() — server pushes config immediately
    // after receiving the register message.
    socket4h_on_message("config", [](const JsonDocument& doc) {
        String pid = doc["projectId"] | String("");
        if (pid.length() > 0) projectId = pid;
        applySocketConfig(doc);
        setState(STATE_IDLE);
        tlog("Config: applied");
    });
    socket4h_on_message("display", [](const JsonDocument& doc) {
        String json; serializeJson(doc, json);
        handleDisplay(json);
    });
    socket4h_on_message("emotion", [](const JsonDocument& doc) {
        String json; serializeJson(doc, json);
        handleEmotion(json);
    });
    socket4h_on_message("speech", [](const JsonDocument& doc) {
        String json; serializeJson(doc, json);
        handleSpeech(json);
    });
    socket4h_on_message("ota", [](const JsonDocument& doc) {
        String url     = doc["url"]     | String("");
        String version = doc["version"] | String("?");
        ota4h_handle(url, version);
    });

    // ─── TCP socket ──────────────────────────────────────────────────────────
    tlog("Connecting socket...");
    for (int i = 0; i < 5 && !socket4h_connected(); i++) {
        if (socket4h_connect(socketHost, socketPort)) break;
        delay(2000);
    }

    // Register — server will immediately push config back on same socket
    registerRobot();

    // Process any incoming data (config response) right after registration
    unsigned long wait = millis();
    while (millis() - wait < 3000) {
        socket4h_loop();
        if (currentState == STATE_IDLE) break;
        delay(10);
    }

    // Forward log4c entries to server via socket
    log4c_set_mqtt(String(DEVICE_NAME) + "/logs",
        [](const char* /*topic*/, const char* payload) -> bool {
            JsonDocument doc;
            if (deserializeJson(doc, payload) != DeserializationError::Ok) return false;
            doc["type"]            = "log";
            doc["deviceId"]        = robotId;
            doc["firmwareVersion"] = FW_VERSION;
            socket4h_send("log", doc);
            return true;
        });

    LINFO("Tara ready. v%s id=%s", FW_VERSION, robotId.c_str());
    if (currentState < STATE_IDLE) setState(STATE_WAITING_CONFIG);
}

// ─── Loop ────────────────────────────────────────────────────────────────────
void loop() {
    wifi4h_reconnect();
    socket4h_loop();
    updateTouch();

    unsigned long now = millis();

    if (now - _lastHeartbeat >= 30000) {
        _lastHeartbeat = now;
        JsonDocument doc;
        doc["deviceId"] = robotId;
        doc["ip"]       = WiFi.localIP().toString();
        socket4h_send("heartbeat", doc);
    }

    if (now - _lastHealth >= 60000) {
        _lastHealth = now;
        JsonDocument doc;
        doc["deviceId"]        = robotId;
        doc["status"]          = "online";
        doc["firmwareVersion"] = FW_VERSION;
        socket4h_send("health", doc);
    }

    // Render eye at ~30fps max
    if (currentState >= STATE_IDLE && now - _lastFrame >= 33) {
        _lastFrame = now;
        renderEye();
    }
}
