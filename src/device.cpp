// Tara Robot — device logic
// Display: SH1106 128x64 via U8g2 SW_I2C (SDA=21, SCL=22)
// Faces dispatched via face lib — tara-face provides RoboEyes-style rendering.

#include "TaraCore.h"
#include <ArduinoJson.h>
#include <U8g2lib.h>
#include <config4h.h>
#include <reg4h.h>
#include <face.h>
#include <tara_face.h>
#include <U8g2Display.h>

// ─── Display ──────────────────────────────────────────────────────────────────
static const int I2C_SCL = 22;
static const int I2C_SDA = 21;

static U8G2_SH1106_128X64_NONAME_F_SW_I2C
    u8g2(U8G2_R0, I2C_SCL, I2C_SDA, U8X8_PIN_NONE);

static U8g2Display<U8G2_SH1106_128X64_NONAME_F_SW_I2C> displayAdapter(&u8g2);
static TaraFace taraFace(&displayAdapter);

// ─── Config ───────────────────────────────────────────────────────────────────
static int  displayBrightness = 128;
static int  volume            = 70;
static int  idleTimeout       = 300;

// ─── Emotion state ────────────────────────────────────────────────────────────
struct RobotEmotionState { String state = "idle"; int energy = 50; unsigned long since = 0; };
static RobotEmotionState emotion;

// ─── Cached face JSON per state (loaded from server via config) ───────────────
static String cachedFaces[11];

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
    LINFO("%s", msg.c_str());
    logLines[logCount % LOG_MAX] = msg;
    logCount++;
    redrawBootScreen();
}

// ─── Touch detection ──────────────────────────────────────────────────────────
// Pin: GPIO18 (capacitive touch — reads LOW when touched on ESP32)
// Events logged to LINFO:
//   single tap  — press < 400 ms, no second press within 350 ms
//   double tap  — two presses within 350 ms of each other
//   long press  — held >= 600 ms (fires once on release)
//   padding     — held >= 600 ms, then fires every 300 ms while still held

static const int   TOUCH_PIN          = 18;
static const int   TOUCH_THRESHOLD    = 40;   // capacitive threshold (0 = touched)
static const unsigned long TAP_MAX_MS = 400;  // press must be shorter to count as tap
static const unsigned long DBL_WIN_MS = 350;  // max gap between two taps for double
static const unsigned long LONG_MS    = 600;  // hold time to trigger long press
static const unsigned long PAD_MS     = 300;  // padding repeat interval

static bool          _touchDown      = false;
static unsigned long _pressAt        = 0;     // when current press started
static unsigned long _releaseAt      = 0;     // when last release happened
static int           _tapCount       = 0;     // taps in current window
static bool          _longFired      = false; // long press event already sent
static unsigned long _lastPadAt      = 0;

static bool _isTouched() {
    return touchRead(TOUCH_PIN) < TOUCH_THRESHOLD;
}

void updateTouch() {
    unsigned long now = millis();
    bool touched = _isTouched();

    if (touched && !_touchDown) {
        // ── Press start ───────────────────────────────────────────────────────
        _touchDown  = true;
        _pressAt    = now;
        _longFired  = false;
        _lastPadAt  = now;

    } else if (touched && _touchDown) {
        // ── Still held ────────────────────────────────────────────────────────
        unsigned long held = now - _pressAt;
        if (!_longFired && held >= LONG_MS) {
            _longFired = true;
            _tapCount  = 0;   // cancel any pending tap
            LINFO("touch: long press");
        }
        // Padding — repeat while held after long press
        if (_longFired && now - _lastPadAt >= PAD_MS) {
            _lastPadAt = now;
            LINFO("touch: padding");
        }

    } else if (!touched && _touchDown) {
        // ── Release ───────────────────────────────────────────────────────────
        _touchDown = false;
        unsigned long held = now - _pressAt;

        if (!_longFired && held < TAP_MAX_MS) {
            _tapCount++;
            _releaseAt = now;
        }

    } else {
        // ── Not touched — resolve pending tap after double-tap window expires ─
        if (_tapCount > 0 && now - _releaseAt >= DBL_WIN_MS) {
            if (_tapCount == 1)
                LINFO("touch: single tap");
            else
                LINFO("touch: double tap");
            _tapCount = 0;
        }
    }
}

// ─── Public API ───────────────────────────────────────────────────────────────

void setupDeviceHardware() {
    uint8_t i2cPins[] = {(uint8_t)I2C_SDA, (uint8_t)I2C_SCL};
    reg4h_add_component("", "", "I2C", i2cPins, 2);

    for (int i = 0; i < reg4h_component_count(); i++) {
        const Reg4hComponent* c = reg4h_get_component(i);
        if (c->protocol == "I2C")
            tlog(c->name + "@0x" + String(c->address, HEX));
    }

    uint8_t touchPins[] = {18};
    reg4h_add_component("TouchSensor", "input", "GPIO", touchPins, 1);

    u8g2.begin();
    u8g2.setContrast((uint8_t)displayBrightness);
    redrawBootScreen();

    taraFace.begin();   // registers FACE_IDLE with face.h dispatcher

    LINFO("Hardware ready — SH1106 128x64");
}

void setState(RobotState s) {
    currentState = s;
}

void applyRobotConfig() {
    displayBrightness = config4h_get("displayBrightness").asInt(displayBrightness);
    volume            = config4h_get("volume").asInt(volume);
    idleTimeout       = config4h_get("idleTimeout").asInt(idleTimeout);
    u8g2.setContrast((uint8_t)displayBrightness);

    JsonVariant faces = config4h_get("faces").raw();
    if (faces.is<JsonObject>()) {
        for (int i = 0; i < STATE_COUNT; i++) {
            const char* name = STATE_NAMES[i];
            if (faces[name].is<const char*>())
                cachedFaces[i] = faces[name].as<String>();
        }
        LINFO("applyRobotConfig: face cache updated");
    }
}

void handleDisplay(const String& json) {
    JsonDocument doc;
    if (deserializeJson(doc, json) != DeserializationError::Ok) return;

    if (doc["cmds"].is<JsonArray>()) {
        LDEBUG("Display: inline cmds");
        renderCmds(json);
        return;
    }

    String faceName = doc["face"] | String("idle");
    LDEBUG("Display: face=%s", faceName.c_str());

    for (int i = 0; i < STATE_COUNT; i++) {
        if (faceName == STATE_NAMES[i] && cachedFaces[i].length() > 0) {
            renderCmds(cachedFaces[i]);
            return;
        }
    }
}

void handleEmotion(const String& json) {
    JsonDocument doc;
    if (deserializeJson(doc, json) != DeserializationError::Ok) return;

    emotion.state  = doc["state"]  | emotion.state;
    emotion.energy = doc["energy"] | emotion.energy;
    emotion.since  = millis();

    LINFO("Emotion: %s energy=%d", emotion.state.c_str(), emotion.energy);

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

    LINFO("Speech: %s", text.c_str());
    setState(STATE_SPEAKING);
    delay(constrain((int)text.length() * 80, 500, 6000));
    setState(STATE_IDLE);
}
