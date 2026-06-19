#pragma once
#include <Arduino.h>

// ─── Config over MQTT ─────────────────────────────────────────────────────────
// Dedicated MQTT client — independent from OTA and main clients.
//
// Inbound: {projectId}.{deviceName}.config  { ...config json... }

// Call once after registerRobot() — connects and subscribes to config topic
void configMqttConnect();

// Call in loop() — keeps the config MQTT client alive and dispatches messages
void configMqttLoop();

// Inbound handler — called by the config MQTT callback
void handleConfig(const String& json);
