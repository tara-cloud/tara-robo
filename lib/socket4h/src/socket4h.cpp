#include "socket4h.h"
#include <map>

static WiFiClient _client;
static String     _host;
static uint16_t   _port         = 3001;
static String     _rxBuf;
static std::map<String, Socket4hCallback> _handlers;

void socket4h_on_message(const char* type, Socket4hCallback cb) {
    _handlers[String(type)] = cb;
}

bool socket4h_connect(const String& host, uint16_t port) {
    _host = host;
    _port = port;
    if (_client.connect(host.c_str(), port)) {
        Serial.printf("[socket4h] connected to %s:%d\n", host.c_str(), port);
        return true;
    }
    Serial.printf("[socket4h] connect failed to %s:%d\n", host.c_str(), port);
    return false;
}

bool socket4h_connected() {
    return _client.connected();
}

void socket4h_send(const char* type, JsonDocument& doc) {
    if (!_client.connected()) return;
    doc["type"] = type;
    String line;
    serializeJson(doc, line);
    line += '\n';
    _client.print(line);
}

void socket4h_loop() {
    // Reconnect if dropped
    if (!_client.connected()) {
        _client.stop();
        delay(1000);
        if (!_client.connect(_host.c_str(), _port)) return;
        Serial.printf("[socket4h] reconnected to %s:%d\n", _host.c_str(), _port);
    }

    // Read incoming newline-delimited JSON
    while (_client.available()) {
        char c = _client.read();
        if (c == '\n') {
            if (_rxBuf.length() > 0) {
                JsonDocument doc;
                if (deserializeJson(doc, _rxBuf) == DeserializationError::Ok) {
                    String type = doc["type"] | String("");
                    auto it = _handlers.find(type);
                    if (it != _handlers.end()) it->second(doc);
                }
                _rxBuf = "";
            }
        } else {
            _rxBuf += c;
        }
    }
}
