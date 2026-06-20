#include <Arduino.h>
#include <WiFi.h>
#include "TaraCore.h"
#include <ota4h.h>
#include "TaraConfig.h"

// ─── Globals ─────────────────────────────────────────────────────────────────
String     robotId;
String     wifiSSID;
String     wifiPassword;
String     serverUrl;
String     projectId;
String     mqttHost;
uint16_t   mqttPort     = 1883;
String     configTopic  = "taraConfig";
RobotState currentState = STATE_BOOTING;
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

// ─── Setup ───────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    
    // Init log4c — Serial appender active immediately, MQTT wired after connect
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

    loadWiFiConfig();
    connectToWiFi();

    registerRobot();
    ota4h_init(mqttHost, mqttPort, projectId, String(DEVICE_NAME));
    ota4h_on_state([](const String& state, int pct) {
        tlog("OTA: " + state + (pct >= 0 ? " " + String(pct) + "%" : ""));
        if (state == "failed") setState(STATE_ERROR);
        if (state == "ok")     setState(STATE_IDLE);
    });
    configMqttConnect();

    // Wire log4c MQTT appender using the OTA MQTT client's broker connection
    taraLogInit(nullptr, projectId, String(DEVICE_NAME));

    LINFO("Tara ready. v%s id=%s", FW_VERSION, robotId.c_str());

    setState(STATE_WAITING_CONFIG);
}

// ─── Loop ────────────────────────────────────────────────────────────────────
void loop() {
    if (WiFi.status() != WL_CONNECTED) {
        connectToWiFi();
    }

    ota4h_loop();
    configMqttLoop();

    if (currentState == STATE_WAITING_CONFIG) {
        renderConfusedFace();
    }

    if (currentState == STATE_IDLE) {
        renderIdleFace();
    }

    delay(10);
}
