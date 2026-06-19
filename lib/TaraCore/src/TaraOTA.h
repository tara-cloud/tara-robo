#pragma once
#include <Arduino.h>

// ─── OTA over MQTT ────────────────────────────────────────────────────────────
// Dedicated MQTT client — independent from the main mqttClient in TaraCore.
//
// Inbound:  {projectId}.{deviceName}.ota  { "version": "1.5.0", "url": "http://...", "apiKey": "..." }
// Outbound: {projectId}.{deviceName}.ota_status { "state": "downloading|progress|ok|failed",
//                                                  "version": "...", "progress": 0-100,
//                                                  "error": "..." }

// Call once after WiFi is up — connects own MQTT client and subscribes to /ota
void otaMqttConnect();

// Call in loop() — keeps the OTA MQTT client alive and dispatches inbound messages
void otaMqttLoop();

// Publish current OTA state back to the server
void publishOTAStatus(const String& state, const String& version,
                      int progress = -1, const String& error = "");

// Inbound handler — called by the OTA MQTT callback
void handleOTA(const String& json);
