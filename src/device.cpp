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

static const int   TOUCH_PIN          = 27;   // TTP223B — HIGH = touched
static const int   DEBOUNCE_COUNT     = 3;    // 3 stable reads
static const unsigned long TAP_WIN_MS = 9000; // no limit — just log hold time
static const unsigned long GAP_WIN_MS = 800;
static const unsigned long LONG_MS    = 99000; // disable long press for now

static bool          _touchDown    = false;
static unsigned long _pressAt      = 0;
static unsigned long _releaseAt    = 0;
static int           _tapCount     = 0;
static bool          _longFired    = false;
static int           _debounceCount = 0;
static bool          _stableTouch  = false;
static bool          _calibrated   = false;

static bool _isTouched() {
    return digitalRead(TOUCH_PIN) == HIGH;
}

void updateTouch() {
    unsigned long now = millis();

    if (!_calibrated) {
        _calibrated = true;
        pinMode(TOUCH_PIN, INPUT_PULLDOWN);
        LINFO("touch: ready on GPIO%d", TOUCH_PIN);
    }

    // Raw edge detection — bypasses debounce to confirm pin works at all
    static bool _rawPrev = false;
    bool rawNow = digitalRead(TOUCH_PIN) == HIGH;
    if (rawNow != _rawPrev) {
        _rawPrev = rawNow;
        LINFO("touch: GPIO%d edge → %s", TOUCH_PIN, rawNow ? "HIGH" : "LOW");
    }

    // Debounce
    bool raw = _isTouched();
    if (raw == _stableTouch) {
        _debounceCount = 0;
    } else {
        if (++_debounceCount >= DEBOUNCE_COUNT) {
            _stableTouch   = raw;
            _debounceCount = 0;
        }
    }
    bool touched = _stableTouch;

    if (touched && !_touchDown) {
        // Press start
        _touchDown = true;
        _pressAt   = now;
        _longFired = false;
        LINFO("touch: press");

    } else if (touched && _touchDown) {
        // Still held — check for long press
        if (!_longFired && now - _pressAt >= LONG_MS) {
            _longFired = true;
            _tapCount  = 0;
            LINFO("touch: long press");
        }

    } else if (!touched && _touchDown) {
        // Released
        unsigned long held = now - _pressAt;
        _touchDown = false;
        LINFO("touch: release held=%lums", held);
        if (!_longFired) {   // count all releases that aren't long press
            _tapCount++;
            _releaseAt = now;
        }

    } else {
        // Not held — resolve tap count once gap window expires
        if (_tapCount > 0 && now - _releaseAt >= GAP_WIN_MS) {
            if      (_tapCount == 1) LINFO("touch: single tap");
            else if (_tapCount == 2) LINFO("touch: double tap");
            else                     LINFO("touch: multiple tap (%d)", _tapCount);
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

    uint8_t touchPins[] = {27};
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
