#include "TaraLog.h"
#include "TaraCore.h"
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <WiFiClient.h>
#include <stdarg.h>

// ─── Dedicated log MQTT client ────────────────────────────────────────────────

static WiFiClient   _logWifi;
static PubSubClient _logMqtt(_logWifi);

static String _projectId;
static String _deviceName;
static String _topic;

// Simple ring buffer — avoids dynamic allocation
static const int QUEUE_SIZE = 16;
static String    _queue[QUEUE_SIZE];
static int       _qHead  = 0;
static int       _qTail  = 0;
static int       _qCount = 0;

static void enqueue(const String& msg) {
    if (_qCount >= QUEUE_SIZE) return; // drop if full
    _queue[_qTail] = msg;
    _qTail = (_qTail + 1) % QUEUE_SIZE;
    _qCount++;
}

void taraLogInit(PubSubClient* /*unused*/, const String& projectId, const String& deviceName) {
    _projectId  = projectId;
    _deviceName = deviceName;
    _topic      = projectId + "/log";

    if (mqttHost.length() == 0) return;

    String clientId = robotId + "-log";
    _logMqtt.setBufferSize(512);
    _logMqtt.setServer(mqttHost.c_str(), mqttPort);

    int attempts = 0;
    while (!_logMqtt.connected() && attempts++ < 5) {
        if (_logMqtt.connect(clientId.c_str())) {
            Serial.printf("[Log MQTT] connected, publishing to %s\n", _topic.c_str());
        } else {
            Serial.printf("[Log MQTT] failed (%d), retrying...\n", _logMqtt.state());
            delay(1000);
        }
    }
}

void taraLog(const char* level, const char* file, int line, const char* fmt, ...) {
    char buf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    Serial.printf("[%s] %s\n", level, buf);

    if (_projectId.length() == 0) return;

    JsonDocument doc;
    doc["projectID"]  = _projectId;
    doc["deviceName"] = _deviceName;
    doc["timestamp"]  = String(millis());
    doc["level"]      = String(level);
    const char* slash = strrchr(file, '/');
    if (!slash) slash = strrchr(file, '\\');
    doc["logger"]  = slash ? (slash + 1) : file;
    doc["message"] = buf;

    String out;
    serializeJson(doc, out);
    enqueue(out);
}

void taraLogFlush() {
    if (!_logMqtt.connected()) {
        if (mqttHost.length() > 0 && _projectId.length() > 0) {
            _logMqtt.connect((robotId + "-log").c_str());
        }
        return;
    }
    // Keep broker connection alive even when there's nothing to send
    _logMqtt.loop();
    if (_qCount == 0) return;
    // Publish one entry per flush to avoid blocking
    const String& msg = _queue[_qHead];
    if (_logMqtt.publish(_topic.c_str(), msg.c_str(), false)) {
        _qHead = (_qHead + 1) % QUEUE_SIZE;
        _qCount--;
    }
}
