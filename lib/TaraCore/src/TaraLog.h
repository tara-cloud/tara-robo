#pragma once
// ─── TaraLog — shim over log4c ────────────────────────────────────────────────
// Bridges the old TLOG/LOG_* API to log4c so all TaraCore code works unchanged.
// TaraLog.cpp is no longer needed — this header is self-contained.

#include <log4c.h>

// Map old level constants → log4c levels
#define LOG_DEBUG L4C_DEBUG
#define LOG_INFO  L4C_INFO
#define LOG_WARN  L4C_WARN
#define LOG_ERROR L4C_ERROR

// Map TLOG macro → LLOG macro
#define TLOG(level, fmt, ...) LLOG(level, fmt, ##__VA_ARGS__)

// taraLogInit() — bridge to log4c_init() + device name
// mqtt param is ignored (log4c manages its own MQTT connection)
inline void taraLogInit(void* /*unused*/, const String& projectId,
                        const String& deviceName) {
    log4c_set("device",           deviceName.c_str());
    log4c_set("firmwareVersion",  FW_VERSION);
    String topic = projectId + "/" + deviceName + "/logs";
    log4c_set("mqtt.topic",   topic.c_str());
    log4c_set("mqtt.enabled", "true");
}

// No-op kept for backward compat
inline void taraLogFlush() {}
