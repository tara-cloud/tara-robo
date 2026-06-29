#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
#include <log4c.h>
#include <ArduinoJson.h>

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

// ─── Globals (defined in main.cpp) ───────────────────────────────────────────
extern String     robotId;
extern String     serverUrl;
extern String     projectId;
extern String     socketHost;
extern uint16_t   socketPort;
extern RobotState currentState;

// ─── Registration (socket) ────────────────────────────────────────────────────
void registerRobot();

// ─── Boot log (device.cpp) ────────────────────────────────────────────────────
void tlog(const String& msg);

// ─── Device logic (device.cpp) ───────────────────────────────────────────────
void setupDeviceHardware();
void touchBegin();
void updateTouch();
void applySocketConfig(const JsonDocument& doc);
void handleDisplay(const String& json);
void handleEmotion(const String& json);
void handleSpeech(const String& json);
void setState(RobotState s);
void renderEye();
void renderRaw(const char* b64data, int w, int h);
void setBacklight(bool on);
