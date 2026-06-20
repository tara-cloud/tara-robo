#include "TaraLog.h"
#include "TaraCore.h"
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <WiFiClient.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <stdarg.h>

// ─── Queue ────────────────────────────────────────────────────────────────────

static const int QUEUE_SIZE = 32;

struct LogEntry {
    char level[8];
    char logger[32];
    char message[224];
    uint32_t timestamp;
};

static LogEntry  _queue[QUEUE_SIZE];
static int       _qHead  = 0;
static int       _qTail  = 0;
static int       _qCount = 0;
static SemaphoreHandle_t _qMutex = nullptr;

// ─── MQTT client ──────────────────────────────────────────────────────────────

static WiFiClient   _logWifi;
static PubSubClient _logMqtt(_logWifi);

static String _projectId;
static String _deviceName;
static String _topic;

// ─── Enqueue (called from any task/ISR) ──────────────────────────────────────

static void enqueue(const char* level, const char* logger, const char* message) {
    if (!_qMutex) return;
    if (xSemaphoreTake(_qMutex, pdMS_TO_TICKS(5)) != pdTRUE) return;

    if (_qCount < QUEUE_SIZE) {
        LogEntry& e = _queue[_qTail];
        strncpy(e.level,   level,   sizeof(e.level)   - 1); e.level  [sizeof(e.level)  -1] = 0;
        strncpy(e.logger,  logger,  sizeof(e.logger)  - 1); e.logger [sizeof(e.logger) -1] = 0;
        strncpy(e.message, message, sizeof(e.message) - 1); e.message[sizeof(e.message)-1] = 0;
        e.timestamp = millis();
        _qTail  = (_qTail + 1) % QUEUE_SIZE;
        _qCount++;
    }
    // Drop silently if full — never block

    xSemaphoreGive(_qMutex);
}

// ─── Drain task (dedicated FreeRTOS task on core 0) ──────────────────────────

static void _drainTask(void*) {
    for (;;) {
        // Wait until MQTT is connected and queue has entries
        if (!_logMqtt.connected() || _qCount == 0) {
            // Try to (re)connect if host is known
            if (mqttHost.length() > 0 && _projectId.length() > 0 && !_logMqtt.connected()) {
                _logMqtt.setServer(mqttHost.c_str(), mqttPort);
                _logMqtt.connect((robotId + "-log").c_str());
            }
            vTaskDelay(pdMS_TO_TICKS(200));
            continue;
        }

        _logMqtt.loop();

        // Dequeue one entry per iteration
        if (xSemaphoreTake(_qMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            if (_qCount > 0) {
                LogEntry e = _queue[_qHead]; // copy out
                _qHead  = (_qHead + 1) % QUEUE_SIZE;
                _qCount--;
                xSemaphoreGive(_qMutex);

                // Build JSON and publish
                JsonDocument doc;
                doc["projectID"]  = _projectId;
                doc["deviceName"] = _deviceName;
                doc["timestamp"]  = e.timestamp;
                doc["level"]      = e.level;
                doc["logger"]     = e.logger;
                doc["message"]    = e.message;

                String out;
                serializeJson(doc, out);
                _logMqtt.publish(_topic.c_str(), out.c_str(), false);
            } else {
                xSemaphoreGive(_qMutex);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(50)); // 20 msgs/sec max — won't flood broker
    }
}

// ─── Public API ───────────────────────────────────────────────────────────────

void taraLogInit(PubSubClient* /*unused*/, const String& projectId, const String& deviceName) {
    _projectId  = projectId;
    _deviceName = deviceName;
    _topic      = projectId + "/log";

    _qMutex = xSemaphoreCreateMutex();

    _logMqtt.setServer(mqttHost.c_str(), mqttPort);
    _logMqtt.setBufferSize(512);

    // Drain task on core 0 (Arduino loop runs on core 1)
    xTaskCreatePinnedToCore(
        _drainTask,
        "LogDrain",
        4096,   // stack bytes
        nullptr,
        1,      // priority (lower than loop)
        nullptr,
        0       // core 0
    );

    Serial.printf("[Log] drain task started → %s\n", _topic.c_str());
}

void taraLog(const char* level, const char* file, int line, const char* fmt, ...) {
    char buf[224];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    // Always print to Serial (immediate, no blocking)
    Serial.printf("[%s] %s\n", level, buf);

    if (_projectId.length() == 0) return;

    // Short filename
    const char* slash = strrchr(file, '/');
    if (!slash) slash = strrchr(file, '\\');
    const char* logger = slash ? (slash + 1) : file;

    enqueue(level, logger, buf);
}
