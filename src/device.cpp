// Tara Robot — device logic
// Display: SH1106 128x64 via U8g2 SW_I2C (SDA=21, SCL=22)
//
// Faces are runtime JSON received via MQTT — no hardcoded graphics.
// Idle animation is handled by TaraExpressions via the U8g2Display adapter.

#include "TaraCore.h"
#include <ArduinoJson.h>
#include <U8g2lib.h>
#include "U8g2Display.h"
#include "TaraExpressions.h"

// ─── Display + TaraExpressions ───────────────────────────────────────────────
static const int I2C_SCL = 22;
static const int I2C_SDA = 21;

static U8G2_SH1106_128X64_NONAME_F_SW_I2C
    u8g2(U8G2_R0, I2C_SCL, I2C_SDA, U8X8_PIN_NONE);

// Adapter wraps u8g2 into IDisplay so TaraExpressions stays display-agnostic
static U8g2Display<U8G2_SH1106_128X64_NONAME_F_SW_I2C> displayAdapter(&u8g2);
static TaraExpressions taraFace(&displayAdapter);

// ─── Config ───────────────────────────────────────────────────────────────────
static int  displayBrightness = 128;
static int  volume            = 70;
static int  idleTimeout       = 300;

// ─── Emotion state ────────────────────────────────────────────────────────────
struct RobotEmotionState { String state = "idle"; int energy = 50; unsigned long since = 0; };
static RobotEmotionState emotion;

// ─── Cached face JSON per state (loaded from server at boot via config) ────────
// Keys match RobotState names; stored as compact JSON strings
static String cachedFaces[11]; // indexed by RobotState enum

static const char* STATE_NAMES[] = {
    "booting", "connecting", "registering", "waiting_config", "configuring",
    "idle", "listening", "thinking", "speaking", "sleeping", "error"
};
static const int STATE_COUNT = sizeof(STATE_NAMES) / sizeof(STATE_NAMES[0]);

// ─── JSON draw engine ─────────────────────────────────────────────────────────

static void execCmd(const JsonObject& cmd) {
    const char* t = cmd["t"] | "";

    if      (strcmp(t, "disc")   == 0) u8g2.drawDisc  (cmd["x"], cmd["y"], cmd["r"]);
    else if (strcmp(t, "circle") == 0) u8g2.drawCircle(cmd["x"], cmd["y"], cmd["r"]);
    else if (strcmp(t, "hline")  == 0) u8g2.drawHLine (cmd["x"], cmd["y"], cmd["w"]);
    else if (strcmp(t, "vline")  == 0) u8g2.drawVLine (cmd["x"], cmd["y"], cmd["h"]);
    else if (strcmp(t, "pixel")  == 0) u8g2.drawPixel (cmd["x"], cmd["y"]);
    else if (strcmp(t, "rbox")   == 0) u8g2.drawRBox  (cmd["x"], cmd["y"], cmd["w"], cmd["h"], cmd["r"] | 0);
    else if (strcmp(t, "rect")   == 0) u8g2.drawFrame (cmd["x"], cmd["y"], cmd["w"], cmd["h"]);
    else if (strcmp(t, "text")   == 0) {
        const char* font = cmd["font"] | "small";
        if (strcmp(font, "large") == 0)  u8g2.setFont(u8g2_font_ncenB14_tr);
        else                             u8g2.setFont(u8g2_font_6x10_tf);
        u8g2.drawStr(cmd["x"], cmd["y"], cmd["s"] | "");
    }
}

static void renderCmds(const String& json) {
    JsonDocument doc;
    if (deserializeJson(doc, json) != DeserializationError::Ok) return;

    u8g2.clearBuffer();
    JsonArray cmds = doc["cmds"].as<JsonArray>();
    for (JsonObject cmd : cmds) execCmd(cmd);
    u8g2.sendBuffer();
}


// ─── Boot log ─────────────────────────────────────────────────────────────────
static const int LOG_Y_START = 20;
static const int LOG_LINE_H  = 11;
static const int LOG_MAX     = 4;

static String logLines[LOG_MAX];
static int    logCount = 0;

static void redrawBootScreen() {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB14_tr);
    int logoW = u8g2.getStrWidth("TARA");
    u8g2.drawStr((128 - logoW) / 2, 15, "TARA");
    u8g2.drawHLine(0, 18, 128);
    u8g2.setFont(u8g2_font_6x10_tf);
    int start = (logCount > LOG_MAX) ? logCount - LOG_MAX : 0;
    for (int i = start; i < logCount; i++) {
        int row = i - start;
        u8g2.drawStr(0, LOG_Y_START + row * LOG_LINE_H + 8, logLines[i % LOG_MAX].c_str());
    }
    u8g2.sendBuffer();
}

void tlog(const String& msg) {
    TLOG(LOG_INFO, "%s", msg.c_str());
    logLines[logCount % LOG_MAX] = msg;
    logCount++;
    redrawBootScreen();
}

// ─── Public API ───────────────────────────────────────────────────────────────

void setupDeviceHardware() {
    // Scan I2C before u8g2 takes the bus (Wire.end() is called inside)
    discoverComponents(I2C_SDA, I2C_SCL);

    // Register GPIO components that aren't on the I2C bus
    addComponent("TouchSensor", "input", "GPIO", {
        { "GPIO18", "TOUCH", "input" }
    });

    u8g2.begin();
    u8g2.setContrast((uint8_t)displayBrightness);
    redrawBootScreen();
    TLOG(LOG_INFO, "Hardware ready — SH1106 128x64");
}

void setState(RobotState s) {
    currentState = s;
}

void renderIdleFace() {
    taraFace.animateIdle();
}

void renderConfusedFace() {
    u8g2.clearBuffer();

    // Asymmetric brows — left raised, right furrowed
    u8g2.drawHLine(22, 10, 16);               // left brow — raised flat
    u8g2.drawHLine(22, 11, 16);
    u8g2.drawLine(74, 13, 90, 9);             // right brow — angled down-to-up
    u8g2.drawLine(74, 14, 90, 10);

    // Round eyes — left open, right squinted
    u8g2.drawCircle(30, 28, 9);               // left eye open
    u8g2.drawDisc(30, 28, 4);
    u8g2.drawDisc(82, 32, 6);                 // right eye squinted (smaller, lower)

    // Wavy mouth — confused squiggle
    u8g2.drawLine(44, 50, 52, 46);
    u8g2.drawLine(52, 46, 60, 50);
    u8g2.drawLine(60, 50, 68, 46);
    u8g2.drawLine(68, 46, 76, 50);

    // Question mark top-right corner
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(112, 14, "?");

    u8g2.sendBuffer();
}

void applyRobotConfig(const JsonDocument& doc) {
    displayBrightness = doc["displayBrightness"] | displayBrightness;
    volume            = doc["volume"]            | volume;
    idleTimeout       = doc["idleTimeout"]       | idleTimeout;
    u8g2.setContrast((uint8_t)displayBrightness);

    // Load cached face JSON for each state from config
    // Server pushes: { "faces": { "idle": "{cmds:[...]}", "happy": "...", ... } }
    JsonObjectConst faces = doc["faces"].as<JsonObjectConst>();
    if (faces) {
        for (int i = 0; i < STATE_COUNT; i++) {
            const char* name = STATE_NAMES[i];
            if (faces[name].is<const char*>()) {
                cachedFaces[i] = faces[name].as<String>();
            }
        }
        TLOG(LOG_INFO, "Face cache updated from config");
    }
}

void handleDisplay(const String& json) {
    JsonDocument doc;
    if (deserializeJson(doc, json) != DeserializationError::Ok) return;

    // If "cmds" array present: render directly
    if (doc["cmds"].is<JsonArray>()) {
        TLOG(LOG_DEBUG, "Display: inline cmds");
        renderCmds(json);
        return;
    }

    // If "face" name present: look up in cache and render
    String faceName = doc["face"] | String("idle");
    TLOG(LOG_DEBUG, "Display: face=%s", faceName.c_str());

    // Find matching state index
    for (int i = 0; i < STATE_COUNT; i++) {
        if (faceName == STATE_NAMES[i] && cachedFaces[i].length() > 0) {
            renderCmds(cachedFaces[i]);
            return;
        }
    }
    // Unknown face: fallback to STATE_IDLE
}

void handleEmotion(const String& json) {
    JsonDocument doc;
    if (deserializeJson(doc, json) != DeserializationError::Ok) return;

    emotion.state  = doc["state"]  | emotion.state;
    emotion.energy = doc["energy"] | emotion.energy;
    emotion.since  = millis();

    TLOG(LOG_INFO, "Emotion: %s energy=%d", emotion.state.c_str(), emotion.energy);

    // Map emotion to robot state and render
    RobotState s = STATE_IDLE;
    if      (emotion.state == "listening") s = STATE_LISTENING;
    else if (emotion.state == "thinking")  s = STATE_THINKING;
    else if (emotion.state == "speaking")  s = STATE_SPEAKING;
    else if (emotion.state == "sleeping")  s = STATE_SLEEPING;
    setState(s);
}

void handleSpeech(const String& json) {
    JsonDocument doc;
    if (deserializeJson(doc, json) != DeserializationError::Ok) return;
    String text = doc["text"] | String("");

    TLOG(LOG_INFO, "Speech: %s", text.c_str());
    setState(STATE_SPEAKING);
    delay(constrain((int)text.length() * 80, 500, 6000));
    setState(STATE_IDLE);
}
