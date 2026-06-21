#include "TaraCore.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <wifi4h.h>
#include <reg4h.h>

// ─── Registration ────────────────────────────────────────────────────────────

void registerRobot() {
    setState(STATE_REGISTERING);
    if (WiFi.status() != WL_CONNECTED) {
        LWARN( "Register: no WiFi — skipping");
        return;
    }
    if (serverUrl.length() == 0) {
        LWARN( "Register: no serverUrl — skipping");
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
    for (int i = 0; i < reg4h_component_count(); i++) {
        const Reg4hComponent* c = reg4h_get_component(i);
        JsonObject co = components.add<JsonObject>();
        co["name"]     = c->name;
        co["type"]     = c->type;
        co["protocol"] = c->protocol;
        if (c->address) co["address"] = String("0x") + String(c->address, HEX);

        JsonArray pins = co["pins"].to<JsonArray>();
        for (int j = 0; j < c->pinCount; j++) {
            JsonObject po = pins.add<JsonObject>();
            po["pin"]       = c->pins[j].pin;
            po["label"]     = c->pins[j].label;
            po["direction"] = c->pins[j].direction;
        }
    }

    String body;
    serializeJson(doc, body);
    LINFO( "Register: POST %s/device/register", serverUrl.c_str());
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
                prefs.begin("tara-wifi", false);
                prefs.putString("projectId", projectId);
                prefs.end();
                LINFO( "Register: projectId updated: %s", projectId.c_str());
            }
        }
        tlog("Registered: OK");
    } else {
        String errBody = http.getString();
        LERROR( "Register failed: code=%d body=%s", code, errBody.c_str());
        tlog("Register: fail " + String(code));
    }
    http.end();
}

