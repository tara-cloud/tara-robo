#pragma once
#include <Arduino.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>
#include <functional>

using Socket4hCallback = std::function<void(const JsonDocument&)>;

void socket4h_on_message(const char* type, Socket4hCallback cb);
bool socket4h_connect(const String& host, uint16_t port);
void socket4h_loop();
void socket4h_send(const char* type, JsonDocument& doc);
bool socket4h_connected();
