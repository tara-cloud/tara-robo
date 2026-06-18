#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "TaraCore.h"

// ─── Globals ─────────────────────────────────────────────────────────────────
String     robotId;
String     wifiSSID;
String     wifiPassword;
String     serverUrl;
String     mqttHost;
uint16_t   mqttPort      = 1883;
RobotState currentState  = STATE_BOOTING;
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

static unsigned long lastHeartbeat = 0;
static unsigned long lastSensor    = 0;

// ─── Setup ───────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    Serial.printf("\n[Tara] %s v%s\n", DEVICE_NAME, FW_VERSION);

    // Derive robotId from MAC
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    char macStr[13];
    snprintf(macStr, sizeof(macStr),
        "%02X%02X%02X%02X%02X%02X",
        mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    robotId = String(macStr);
    Serial.printf("Robot ID: %s\n", robotId.c_str());

    setupDeviceHardware();
    tlog("Boot v" FW_VERSION);

    loadWiFiConfig();
    connectToWiFi();

    registerRobot();
    connectMQTT();

    loadLocalConfig();

    setState(STATE_IDLE);
    Serial.println("Tara ready.");
}

// ─── Loop ────────────────────────────────────────────────────────────────────
void loop() {
    if (WiFi.status() != WL_CONNECTED) {
        connectToWiFi();
        connectMQTT();
    }

    loopMQTT(); // dispatches incoming messages to handlers

    unsigned long now = millis();

    if (now - lastHeartbeat > HEARTBEAT_INTERVAL) {
        lastHeartbeat = now;
        publishHeartbeat();
    }

    if (now - lastSensor > SENSOR_INTERVAL) {
        lastSensor = now;
        publishSensor();
    }

    // Idle face rendered only when nothing else is happening
    if (currentState == STATE_IDLE) {
        renderIdleFace();
    }

    delay(10);
}
