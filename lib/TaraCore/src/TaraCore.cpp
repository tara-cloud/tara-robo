#include "TaraCore.h"
#include <reg4h.h>
#include <socket4h.h>

// ─── Registration via TCP socket ─────────────────────────────────────────────

void registerRobot() {
    setState(STATE_REGISTERING);
    if (WiFi.status() != WL_CONNECTED) {
        LWARN("Register: no WiFi — skipping");
        return;
    }
    if (!socket4h_connected()) {
        LWARN("Register: socket not connected — skipping");
        tlog("Register: no socket");
        return;
    }

    tlog("Registering...");

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

    socket4h_send("register", doc);
    LINFO("Register: sent via socket");
    tlog("Registered: OK");
}
