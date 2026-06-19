#pragma once
#include <Arduino.h>
#include <PubSubClient.h>

// Log levels
#define LOG_DEBUG "DEBUG"
#define LOG_INFO  "INFO"
#define LOG_WARN  "WARN"
#define LOG_ERROR "ERROR"

// TLOG(level, fmt, ...) — logs to Serial + queues MQTT publish to projectId.log
#define TLOG(level, fmt, ...) taraLog(level, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

void taraLogInit(PubSubClient* mqtt, const String& projectId, const String& deviceName);
void taraLog(const char* level, const char* file, int line, const char* fmt, ...);
void taraLogFlush();   // call from loop() to drain the send queue
