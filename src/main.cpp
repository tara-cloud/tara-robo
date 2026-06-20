#include <Arduino.h>
#include <WiFi.h>
#include "TaraCore.h"
#include "TaraOTA.h"
#include "TaraConfig.h"

// ─── Globals ─────────────────────────────────────────────────────────────────
String     robotId;
String     wifiSSID;
String     wifiPassword;
String     serverUrl;
String     projectId;
String     mqttHost;
uint16_t   mqttPort     = 1883;
String     otaTopic     = "ota";
String     configTopic  = "taraConfig";
RobotState currentState = STATE_BOOTING;
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

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
    otaMqttConnect();
    configMqttConnect();

    // Init logging service — publishes to projectId.log via OTA MQTT client
    // Use the same mqttClient that's connected to the broker
    taraLogInit(&mqttClient, projectId, String(DEVICE_NAME));
    TLOG(LOG_INFO, "Tara ready. v%s id=%s", FW_VERSION, robotId.c_str());

    setState(STATE_WAITING_CONFIG);
}

// ─── Loop ────────────────────────────────────────────────────────────────────
void loop() {
    if (WiFi.status() != WL_CONNECTED) {
        connectToWiFi();
    }

    otaMqttLoop();
    configMqttLoop();

    if (currentState == STATE_WAITING_CONFIG) {
        renderConfusedFace();
    }

    if (currentState == STATE_IDLE) {
        renderIdleFace();
    }

    delay(10);
}
