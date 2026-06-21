#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
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

// ─── Globals (defined in main.cpp) ───────────────────────────────────────────
extern String     robotId;
extern String     serverUrl;
extern String     projectId;
extern String     mqttHost;
extern uint16_t   mqttPort;
extern RobotState currentState;

// ─── Registration (HTTP) ─────────────────────────────────────────────────────
void registerRobot();

// ─── Boot log (device.cpp) ────────────────────────────────────────────────────
void tlog(const String& msg);

// ─── Device logic (device.cpp) ───────────────────────────────────────────────
void setupDeviceHardware();
void applyRobotConfig();
void handleDisplay(const String& json);
void handleEmotion(const String& json);
void handleSpeech(const String& json);
void renderIdleFace();
void renderConfusedFace();
void setState(RobotState s);
