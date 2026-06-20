#pragma once
#include <Arduino.h>
#include <PubSubClient.h>

// Log levels
#define LOG_DEBUG "DEBUG"
#define LOG_INFO  "INFO"
#define LOG_WARN  "WARN"
#define LOG_ERROR "ERROR"

// TLOG macro — always safe to call from any context
// Logs to Serial immediately, queues MQTT publish (non-blocking)
#define TLOG(level, fmt, ...) taraLog(level, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

// Call once after MQTT is connected — starts background drain task
void taraLogInit(PubSubClient* mqtt, const String& projectId, const String& deviceName);

// Enqueue a log entry (thread-safe, never blocks)
void taraLog(const char* level, const char* file, int line, const char* fmt, ...);

// No longer needed — drain runs on its own FreeRTOS task
// Kept as no-op for backward compatibility
inline void taraLogFlush() {}
