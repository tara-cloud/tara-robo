#include "TaraCore.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Wire.h>

// ─── Component discovery ──────────────────────────────────────────────────────

DiscoveredComponent discoveredComponents[MAX_COMPONENTS];
int                 discoveredComponentCount = 0;

struct KnownI2CDevice {
    uint8_t     addr;
    const char* name;
    const char* type;       // "output" | "input" | "io"
    // pin labels: index 0 = SDA, 1 = SCL
};

static const KnownI2CDevice KNOWN_I2C[] = {
    { 0x3C, "OLED",   "output" },   // SSD1306 / SH1106 (most common addr)
    { 0x3D, "OLED",   "output" },   // SSD1306 / SH1106 (alt addr)
    { 0x68, "MPU6050","io"     },   // gyro/accel
    { 0x69, "MPU6050","io"     },
    { 0x76, "BME280", "input"  },   // temp/humidity/pressure
    { 0x77, "BME280", "input"  },
    { 0x40, "INA219", "input"  },   // current/voltage sensor
    { 0x27, "LCD",    "output" },   // PCF8574 I2C LCD backpack
    { 0x3F, "LCD",    "output" },
};
static const int KNOWN_I2C_COUNT = sizeof(KNOWN_I2C) / sizeof(KNOWN_I2C[0]);

void discoverComponents(int sdaPin, int sclPin) {
    discoveredComponentCount = 0;

    Wire.begin(sdaPin, sclPin);
    tlog("Scanning I2C...");
    TLOG(LOG_INFO, "I2C scan SDA=%d SCL=%d", sdaPin, sclPin);

    for (uint8_t addr = 1; addr < 127 && discoveredComponentCount < MAX_COMPONENTS; addr++) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() != 0) continue;

        TLOG(LOG_DEBUG, "I2C found 0x%02X", addr);

        DiscoveredComponent& c = discoveredComponents[discoveredComponentCount++];
        c.address  = addr;
        c.protocol = "I2C";
        c.pinCount = 2;
        c.pins[0]  = { "GPIO" + String(sdaPin), "SDA", "io"     };
        c.pins[1]  = { "GPIO" + String(sclPin), "SCL", "output" };

        // Match against known-device table
        bool matched = false;
        for (int k = 0; k < KNOWN_I2C_COUNT; k++) {
            if (KNOWN_I2C[k].addr == addr) {
                c.name = KNOWN_I2C[k].name;
                c.type = KNOWN_I2C[k].type;
                matched = true;
                break;
            }
        }
        if (!matched) {
            char buf[12];
            snprintf(buf, sizeof(buf), "0x%02X", addr);
            c.name = String("Unknown(") + buf + ")";
            c.type = "io";
        }

        tlog(c.name + "@0x" + String(addr, HEX));
        TLOG(LOG_INFO, "Component: %s (%s)", c.name.c_str(), c.type.c_str());
    }

    Wire.end();
    TLOG(LOG_INFO, "I2C scan done — %d component(s)", discoveredComponentCount);
}

void addComponent(const String& name, const String& type, const String& protocol,
                  std::initializer_list<DiscoveredPin> pins) {
    if (discoveredComponentCount >= MAX_COMPONENTS) return;
    DiscoveredComponent& c = discoveredComponents[discoveredComponentCount++];
    c.name     = name;
    c.type     = type;
    c.protocol = protocol;
    c.address  = 0;
    c.pinCount = 0;
    for (const auto& p : pins) {
        if (c.pinCount >= 4) break;
        c.pins[c.pinCount++] = p;
    }
    TLOG(LOG_INFO, "Component added: %s (%s)", name.c_str(), type.c_str());
}

// ─── WiFi ────────────────────────────────────────────────────────────────────

void loadWiFiConfig() {
    Preferences prefs;
    prefs.begin(PREF_WIFI, true);
    wifiSSID     = prefs.getString("ssid",      "");
    wifiPassword = prefs.getString("password",  "");
    projectId    = prefs.getString("projectId", "");
    String host  = prefs.getString("host",      "");
    uint16_t svcPort = prefs.getUShort("servicePort", 4000);
    mqttPort         = prefs.getUShort("mqttPort",    1883);
    prefs.end();

    if (host.length() > 0) {
        serverUrl = "http://" + host + ":" + String(svcPort);
        mqttHost  = host;
    }
}

void connectToWiFi() {
    setState(STATE_CONNECTING);

    if (wifiSSID.length() == 0) {
        startSetupHotspot();
        return;
    }

    tlog("WiFi: connecting...");
    TLOG(LOG_INFO, "WiFi connecting to %s", wifiSSID.c_str());
    WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        attempts++;
    }

    if (WiFi.status() != WL_CONNECTED) {
        TLOG(LOG_WARN, "WiFi failed — starting hotspot");
        tlog("WiFi: failed");
        startSetupHotspot();
    } else {
        TLOG(LOG_INFO, "WiFi connected: %s", WiFi.localIP().toString().c_str());
        tlog("WiFi: " + WiFi.localIP().toString());
    }
}

static const char PORTAL_HTML[] PROGMEM = R"(<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>Tara Setup</title>
  <style>
    body { font-family: sans-serif; max-width: 360px; margin: 40px auto; padding: 0 16px; }
    h2   { text-align: center; }
    label { display: block; margin-top: 12px; font-size: 14px; }
    input { width: 100%; box-sizing: border-box; padding: 8px; margin-top: 4px; font-size: 15px; }
    button { width: 100%; margin-top: 20px; padding: 12px; font-size: 16px;
             background: #333; color: #fff; border: none; border-radius: 4px; cursor: pointer; }
    hr { margin: 18px 0; border: none; border-top: 1px solid #ccc; }
    .section { font-size: 12px; color: #888; text-transform: uppercase; letter-spacing: .05em; margin-top: 16px; }
  </style>
</head>
<body>
  <h2>&#x1F916; Tara Setup</h2>
  <form method="POST" action="/save">
    <p class="section">Project</p>
    <label>Project ID<input name="projectId" placeholder="e.g. tara-home" required></label>

    <p class="section">WiFi</p>
    <label>SSID<input name="ssid" required></label>
    <label>Password<input name="password" type="password"></label>

    <p class="section">Server</p>
    <label>Host<input name="host" placeholder="192.168.0.107" required></label>
    <label>Service Port<input name="servicePort" type="number" value="4000" required></label>
    <label>MQTT Port<input name="mqttPort" type="number" value="1883" required></label>

    <button type="submit">Save &amp; Reboot</button>
  </form>
</body>
</html>)";

void startSetupHotspot() {
    String apName = "Tara-" + robotId.substring(6); // last 6 chars of MAC
    WiFi.softAP(apName.c_str(), AP_PASSWORD);
    String apIp = WiFi.softAPIP().toString();
    TLOG(LOG_INFO, "AP: %s  IP: %s", apName.c_str(), apIp.c_str());
    tlog("AP: " + apName);
    tlog("IP: " + apIp);

    static DNSServer  dns;
    static WebServer  server(80);

    // Captive portal: redirect everything to the setup page
    dns.start(53, "*", WiFi.softAPIP());

    server.on("/", HTTP_GET, [&]() {
        server.send_P(200, "text/html", PORTAL_HTML);
    });

    // iOS / Android captive portal probe endpoints
    server.on("/generate_204",        HTTP_GET, [&]() { server.sendHeader("Location", "/"); server.send(302); });
    server.on("/hotspot-detect.html", HTTP_GET, [&]() { server.send_P(200, "text/html", PORTAL_HTML); });
    server.on("/connecttest.txt",     HTTP_GET, [&]() { server.sendHeader("Location", "/"); server.send(302); });

    server.on("/save", HTTP_POST, [&]() {
        String pid         = server.arg("projectId");
        String ssid        = server.arg("ssid");
        String password    = server.arg("password");
        String host        = server.arg("host");
        uint16_t svcPort   = (uint16_t)server.arg("servicePort").toInt();
        uint16_t mqPort    = (uint16_t)server.arg("mqttPort").toInt();

        if (pid.length() == 0 || ssid.length() == 0 || host.length() == 0) {
            server.send(400, "text/plain", "Missing required fields");
            return;
        }
        if (svcPort == 0) svcPort = 4000;
        if (mqPort  == 0) mqPort  = 1883;

        Preferences prefs;
        prefs.begin(PREF_WIFI, false);
        prefs.putString("projectId",   pid);
        prefs.putString("ssid",        ssid);
        prefs.putString("password",    password);
        prefs.putString("host",        host);
        prefs.putUShort("servicePort", svcPort);
        prefs.putUShort("mqttPort",    mqPort);
        prefs.end();

        TLOG(LOG_INFO, "Portal: credentials saved — rebooting");
        tlog("Saved! Rebooting...");
        server.send(200, "text/html",
            "<html><body style='font-family:sans-serif;text-align:center;padding:40px'>"
            "<h2>&#x2705; Saved!</h2><p>Tara is rebooting and will connect to your network.</p>"
            "</body></html>");
        delay(1500);
        ESP.restart();
    });

    // Catch-all redirect for captive portal detection on any unknown path
    server.onNotFound([&]() {
        server.sendHeader("Location", "/");
        server.send(302);
    });

    server.begin();
    TLOG(LOG_INFO, "Portal: serving captive portal");

    while (true) {
        dns.processNextRequest();
        server.handleClient();
    }
}

// ─── Registration ────────────────────────────────────────────────────────────

void registerRobot() {
    setState(STATE_REGISTERING);
    if (WiFi.status() != WL_CONNECTED) {
        TLOG(LOG_WARN, "Register: no WiFi — skipping");
        return;
    }
    if (serverUrl.length() == 0) {
        TLOG(LOG_WARN, "Register: no serverUrl — skipping");
        tlog("No server URL!");
        return;
    }

    tlog("Registering...");
    HTTPClient http;
    http.begin(serverUrl + "/device/register");
    http.addHeader("Content-Type", "application/json");

    JsonDocument doc;
    doc["deviceId"]        = robotId;
    doc["deviceName"]      = DEVICE_NAME;
    doc["deviceType"]      = DEVICE_TYPE;
    doc["firmwareVersion"] = FW_VERSION;
    doc["ip"]              = WiFi.localIP().toString();
    if (projectId.length() > 0) doc["projectId"] = projectId;

    JsonArray components = doc["components"].to<JsonArray>();
    for (int i = 0; i < discoveredComponentCount; i++) {
        const DiscoveredComponent& c = discoveredComponents[i];
        JsonObject co = components.add<JsonObject>();
        co["name"]     = c.name;
        co["type"]     = c.type;
        co["protocol"] = c.protocol;
        if (c.address) co["address"] = String("0x") + String(c.address, HEX);

        JsonArray pins = co["pins"].to<JsonArray>();
        for (int j = 0; j < c.pinCount; j++) {
            JsonObject po = pins.add<JsonObject>();
            po["pin"]       = c.pins[j].pin;
            po["label"]     = c.pins[j].label;
            po["direction"] = c.pins[j].direction;
        }
    }

    String body;
    serializeJson(doc, body);
    TLOG(LOG_INFO, "Register: POST %s/device/register", serverUrl.c_str());
    http.setTimeout(10000);
    int code = http.POST(body);

    if (code == 200 || code == 201) {
        String resp = http.getString();
        JsonDocument rdoc;
        if (deserializeJson(rdoc, resp) == DeserializationError::Ok) {
            String srvProjectId = rdoc["projectId"] | String("");
            if (srvProjectId.length() > 0 && srvProjectId != projectId) {
                projectId = srvProjectId;
                Preferences prefs;
                prefs.begin(PREF_WIFI, false);
                prefs.putString("projectId", projectId);
                prefs.end();
                TLOG(LOG_INFO, "Register: projectId updated: %s", projectId.c_str());
            }
        }
        tlog("Registered: OK");
    } else {
        String errBody = http.getString();
        TLOG(LOG_ERROR, "Register failed: code=%d body=%s", code, errBody.c_str());
        tlog("Register: fail " + String(code));
    }
    http.end();
}

// ─── MQTT config fetch ────────────────────────────────────────────────────────

void fetchMqttConfig() {
    if (WiFi.status() != WL_CONNECTED || serverUrl.length() == 0) return;

    tlog("Fetching MQTT cfg...");
    HTTPClient http;
    http.begin(serverUrl + "/device/mqtt-config/" + robotId);
    int code = http.GET();

    if (code == 200) {
        String body = http.getString();
        JsonDocument doc;
        if (deserializeJson(doc, body) == DeserializationError::Ok) {
            mqttHost    = doc["mqttHost"]    | mqttHost;
            mqttPort    = doc["mqttPort"]    | mqttPort;
            otaTopic    = doc["otaTopic"]    | otaTopic;
            configTopic = doc["configTopic"] | configTopic;

            // If server hasn't configured an MQTT host, extract it from serverUrl
            // e.g. "http://192.168.0.107:30400" → "192.168.0.107"
            if (mqttHost.length() == 0 && serverUrl.length() > 0) {
                String s = serverUrl;
                if (s.startsWith("http://"))  s = s.substring(7);
                if (s.startsWith("https://")) s = s.substring(8);
                int colonIdx = s.indexOf(':');
                int slashIdx = s.indexOf('/');
                int end = s.length();
                if (colonIdx > 0) end = min(end, colonIdx);
                if (slashIdx > 0) end = min(end, slashIdx);
                mqttHost = s.substring(0, end);
                TLOG(LOG_INFO, "mqttHost derived from serverUrl: %s", mqttHost.c_str());
            }

            TLOG(LOG_INFO, "MQTT config: %s:%d ota=%s config=%s",
                mqttHost.c_str(), mqttPort, otaTopic.c_str(), configTopic.c_str());
            tlog("MQTT cfg: OK");
        }
    } else if (code == 404) {
        tlog("No project assigned");
        TLOG(LOG_WARN, "mqtt-config: device not assigned to a project");
    } else {
        TLOG(LOG_ERROR, "mqtt-config fetch failed: %d", code);
        tlog("MQTT cfg: fail " + String(code));
    }
    http.end();
}

// ─── MQTT ────────────────────────────────────────────────────────────────────

String robotTopic(const String& suffix) {
    return "tara/robot/" + robotId + "/" + suffix;
}

void connectMQTT() {
    setState(STATE_CONFIGURING);
    mqttClient.setServer(mqttHost.c_str(), mqttPort);
    mqttClient.setCallback(mqttCallback);

    tlog("MQTT: connecting...");
    TLOG(LOG_INFO, "MQTT connecting to %s:%d", mqttHost.c_str(), mqttPort);

    while (!mqttClient.connected()) {
        if (mqttClient.connect(robotId.c_str())) {
            TLOG(LOG_INFO, "MQTT connected");
            tlog("MQTT: connected");

            mqttClient.subscribe(robotTopic("config").c_str(),    1);
            mqttClient.subscribe(robotTopic("display").c_str(),   0);
            mqttClient.subscribe(robotTopic("emotion").c_str(),   0);
            mqttClient.subscribe(robotTopic("speech").c_str(),    0);
            mqttClient.subscribe(robotTopic(configTopic).c_str(), 1);
        } else {
            TLOG(LOG_WARN, "MQTT connect failed (%d), retrying", mqttClient.state());
            tlog("MQTT: retry " + String(mqttClient.state()));
            delay(3000);
        }
    }
}

void loopMQTT() {
    if (!mqttClient.connected()) {
        tlog("MQTT: reconnecting");
        connectMQTT();
    }
    mqttClient.loop();
}

void mqttCallback(const char* topic, byte* payload, unsigned int length) {
    String t   = String(topic);
    String msg = String((char*)payload, length);
    TLOG(LOG_DEBUG, "MQTT [%s]: %s", t.c_str(), msg.c_str());

    if      (t.endsWith("/config") || t.endsWith("/" + configTopic)) applyConfig(msg);
    else if (t.endsWith("/display"))  handleDisplay(msg);
    else if (t.endsWith("/emotion"))  handleEmotion(msg);
    else if (t.endsWith("/speech"))   handleSpeech(msg);
}

void publishHeartbeat() {
    JsonDocument doc;
    doc["status"]   = "ONLINE";
    doc["firmware"] = FW_VERSION;
    doc["uptime"]   = millis() / 1000;
    doc["ip"]       = WiFi.localIP().toString();

    String msg;
    serializeJson(doc, msg);
    mqttClient.publish(robotTopic("heartbeat").c_str(), msg.c_str(), false);
}

void publishSensor() {
    JsonDocument doc;
    doc["temperature"] = 0;
    doc["battery"]     = 100;

    String msg;
    serializeJson(doc, msg);
    mqttClient.publish(robotTopic("sensor").c_str(), msg.c_str(), false);
}

// ─── Config ───────────────────────────────────────────────────────────────────

void loadLocalConfig() {
    Preferences prefs;
    prefs.begin(PREF_CONFIG, true);
    String json = prefs.getString("configJson", "");
    prefs.end();
    if (json.length() > 0) applyConfig(json);
}

void applyConfig(const String& json) {
    Preferences prefs;
    prefs.begin(PREF_CONFIG, false);
    prefs.putString("configJson", json);
    prefs.end();

    JsonDocument doc;
    if (deserializeJson(doc, json) != DeserializationError::Ok) return;
    TLOG(LOG_INFO, "Config applied: v%d", (int)doc["version"]);
    extern void applyRobotConfig(const JsonDocument&);
    applyRobotConfig(doc);
}
