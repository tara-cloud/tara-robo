#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <initializer_list>
#include "TaraOTA.h"
#include "TaraLog.h"

// ─── Robot states ─────────────────────────────────────────────────────────────
enum RobotState {
    STATE_BOOTING,
    STATE_CONNECTING,
    STATE_REGISTERING,
    STATE_WAITING_CONFIG,
    STATE_CONFIGURING,
    STATE_IDLE,
    STATE_LISTENING,
    STATE_THINKING,
    STATE_SPEAKING,
    STATE_SLEEPING,
    STATE_ERROR,
};

// ─── Timing ──────────────────────────────────────────────────────────────────
static const unsigned long HEARTBEAT_INTERVAL = 30000; // 30 s
static const unsigned long SENSOR_INTERVAL    = 45000; // 45 s

// ─── NVS namespaces ───────────────────────────────────────────────────────────
static const char* PREF_WIFI   = "tara-wifi";
static const char* PREF_CONFIG = "tara-device";
static const char* AP_PASSWORD = "12345678";

// ─── Runtime discovered component (populated by discoverComponents()) ────────
struct DiscoveredPin {
    String pin;
    String label;
    String direction;
};

struct DiscoveredComponent {
    String name;
    String type;
    String protocol;
    uint8_t address;            // I2C address (0 = not I2C)
    DiscoveredPin pins[4];
    int pinCount;
};

static const int MAX_COMPONENTS = 8;
extern DiscoveredComponent discoveredComponents[];
extern int                 discoveredComponentCount;

// Called from setupDeviceHardware() before peripheral init
void discoverComponents(int sdaPin, int sclPin);

// Register a non-I2C component (GPIO, SPI, UART, etc.) manually
// Call from device.cpp after discoverComponents()
void addComponent(const String& name, const String& type, const String& protocol,
                  std::initializer_list<DiscoveredPin> pins);

// ─── Globals (defined in main.cpp) ───────────────────────────────────────────
extern String     robotId;
extern String     wifiSSID;
extern String     wifiPassword;
extern String     serverUrl;
extern String     projectId;
extern String     mqttHost;
extern uint16_t   mqttPort;
extern String     otaTopic;
extern String     configTopic;
extern RobotState currentState;
extern WiFiClient wifiClient;
extern PubSubClient mqttClient;

// ─── WiFi / Hotspot ──────────────────────────────────────────────────────────
void loadWiFiConfig();
void connectToWiFi();
void startSetupHotspot();

// ─── Registration (HTTP) ─────────────────────────────────────────────────────
void registerRobot();
void fetchMqttConfig();

// ─── MQTT ────────────────────────────────────────────────────────────────────
void connectMQTT();
void loopMQTT();
void mqttCallback(const char* topic, byte* payload, unsigned int length);
void publishHeartbeat();
void publishSensor();
String robotTopic(const String& suffix);

// ─── Config ───────────────────────────────────────────────────────────────────
void loadLocalConfig();
void applyConfig(const String& json);

// ─── Boot log (device.cpp) ────────────────────────────────────────────────────
// Prints a line to the OLED boot log. Logo stays at top, lines scroll up.
void tlog(const String& msg);

// ─── Device logic (robot/device.cpp) ─────────────────────────────────────────
void setupDeviceHardware();
void handleDisplay(const String& json);
void handleEmotion(const String& json);
void handleSpeech(const String& json);
void renderIdleFace();
void renderConfusedFace();
void setState(RobotState s);
