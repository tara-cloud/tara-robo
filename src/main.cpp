#include <Arduino.h>
#include <WiFi.h>
#include "TaraCore.h"
#include <wifi4h.h>
#include <ota4h.h>
#include <config4h.h>

// ─── Globals ─────────────────────────────────────────────────────────────────
String     robotId;
String     serverUrl;
String     projectId;
String     mqttHost;
uint16_t   mqttPort     = 1883;
RobotState currentState = STATE_BOOTING;
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

// ─── Setup ───────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);

    log4c_init();
    log4c_set("device", DEVICE_NAME);

    tlog(String(DEVICE_NAME) + " v" + FW_VERSION);

    // Derive robotId from MAC
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    char macStr[13];
    snprintf(macStr, sizeof(macStr),
        "%02X%02X%02X%02X%02X%02X",
        mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    robotId = String(macStr);
    tlog("Robot ID: " + robotId);

    setupDeviceHardware();
    tlog("Boot v" FW_VERSION);

    // ─── WiFi provisioning ───────────────────────────────────────────────────
    wifi4h_set_device_id(robotId);
    wifi4h_set_namespace("tara-wifi");
    wifi4h_add_field("ssid",        "string",   true);
    wifi4h_add_field("password",    "password", false);
    wifi4h_add_field("projectId",   "string",   true);
    wifi4h_add_field("host",        "string",   true);
    wifi4h_add_field("servicePort", "number",   false);
    wifi4h_add_field("mqttPort",    "number",   false);
    wifi4h_on_event([](const String& ev, const String& detail) {
        tlog(ev + (detail.length() ? ": " + detail : ""));
    });

    wifi4h_load();

    projectId        = wifi4h_get("projectId");
    String host      = wifi4h_get("host");
    uint16_t svcPort = (uint16_t)wifi4h_get("servicePort").toInt();
    mqttPort         = (uint16_t)wifi4h_get("mqttPort").toInt();
    if (svcPort  == 0) svcPort  = 30400;
    if (mqttPort == 0) mqttPort = 1883;
    if (host.length() > 0) {
        serverUrl = "http://" + host + ":" + String(svcPort);
        mqttHost  = host;
    }

    setState(STATE_CONNECTING);
    wifi4h_connect();

    // ─── OTA ─────────────────────────────────────────────────────────────────
    ota4h_init(mqttHost, mqttPort, projectId, String(DEVICE_NAME));
    ota4h_on_state([](const String& state, int pct) {
        tlog("OTA: " + state + (pct >= 0 ? " " + String(pct) + "%" : ""));
        if (state == "failed") setState(STATE_ERROR);
        if (state == "ok")     setState(STATE_IDLE);
    });

    // ─── Config ──────────────────────────────────────────────────────────────
    config4h_on_change([]() {
        applyRobotConfig();
        setState(STATE_IDLE);
        tlog("Config: applied");
    });
    config4h_init(mqttHost, mqttPort, projectId, String(DEVICE_NAME));

    registerRobot();

    taraLogInit(nullptr, projectId, String(DEVICE_NAME));

    LINFO("Tara ready. v%s id=%s", FW_VERSION, robotId.c_str());
    setState(STATE_WAITING_CONFIG);
}

// ─── Loop ────────────────────────────────────────────────────────────────────
void loop() {
    wifi4h_reconnect();
    ota4h_loop();
    config4h_loop();

    if (currentState == STATE_WAITING_CONFIG) {
        renderConfusedFace();
    }

    if (currentState == STATE_IDLE) {
        renderIdleFace();
    }

    delay(10);
}
