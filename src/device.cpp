// Tara Robot — device logic
// Display: ST7735 128x160 V1.1 via SPI (SCK=18, MOSI=23, CS=5, DC=2, RST=4)
// Faces dispatched via face lib — tara-face provides RoboEyes-style rendering.

#include "TaraCore.h"
#include "Eye.h"
#include <ArduinoJson.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>
#include <config4h.h>
#include <reg4h.h>
#include <face.h>
#include <tara_face.h>
#include <ST7735Display.h>
#include <touch_me.h>

// ─── Display ──────────────────────────────────────────────────────────────────
static const int TFT_CS  = 5;
static const int TFT_DC  = 2;
static const int TFT_RST = 4;

static Adafruit_ST7735  tft(TFT_CS, TFT_DC, TFT_RST);
static ST7735Display    displayAdapter(&tft);
static TaraFace         taraFace(&displayAdapter);

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
    int x = cmd["x"] | 0, y = cmd["y"] | 0;
    int w = cmd["w"] | 0, h = cmd["h"] | 0;
    int r = cmd["r"] | 0;

    if      (strcmp(t, "disc")   == 0) tft.fillCircle(x, y, r, ST77XX_WHITE);
    else if (strcmp(t, "circle") == 0) tft.drawCircle(x, y, r, ST77XX_WHITE);
    else if (strcmp(t, "hline")  == 0) tft.drawFastHLine(x, y, w, ST77XX_WHITE);
    else if (strcmp(t, "vline")  == 0) tft.drawFastVLine(x, y, h, ST77XX_WHITE);
    else if (strcmp(t, "pixel")  == 0) tft.drawPixel(x, y, ST77XX_WHITE);
    else if (strcmp(t, "rbox")   == 0) tft.fillRoundRect(x, y, w, h, r, ST77XX_WHITE);
    else if (strcmp(t, "rect")   == 0) tft.drawRoundRect(x, y, w, h, r, ST77XX_WHITE);
    else if (strcmp(t, "text")   == 0) {
        const char* font = cmd["font"] | "small";
        tft.setTextColor(ST77XX_WHITE);
        tft.setTextSize(strcmp(font, "large") == 0 ? 2 : 1);
        tft.setCursor(x, y);
        tft.print(cmd["s"] | "");
    }
}

static void renderCmds(const String& json) {
    JsonDocument doc;
    if (deserializeJson(doc, json) != DeserializationError::Ok) return;
    tft.fillScreen(ST77XX_BLACK);
    JsonArray cmds = doc["cmds"].as<JsonArray>();
    for (JsonObject cmd : cmds) execCmd(cmd);
}

// ─── Boot log ─────────────────────────────────────────────────────────────────
static const int LOG_Y_START = 20;
static const int LOG_LINE_H  = 11;
static const int LOG_MAX     = 4;

static String logLines[LOG_MAX];
static int    logCount = 0;

static void redrawBootScreen() {
    tft.fillScreen(ST77XX_BLACK);
    tft.setTextColor(ST77XX_WHITE);
    tft.setTextSize(2);
    tft.setCursor((128 - 48) / 2, 4);
    tft.print("TARA");
    tft.drawFastHLine(0, 20, 128, ST77XX_WHITE);
    tft.setTextSize(1);
    int start = (logCount > LOG_MAX) ? logCount - LOG_MAX : 0;
    for (int i = start; i < logCount; i++) {
        int row = i - start;
        tft.setCursor(0, LOG_Y_START + row * LOG_LINE_H);
        tft.print(logLines[i % LOG_MAX]);
    }
}

void tlog(const String& msg) {
    LINFO("%s", msg.c_str());
    logLines[logCount % LOG_MAX] = msg;
    logCount++;
    redrawBootScreen();
}

// ─── Eye sprite renderer ──────────────────────────────────────────────────────
// Eye_map is RGB565 160×120. Screen in landscape (setRotation(1)) is 160×128.
// Centre vertically: y = (128-120)/2 = 4px.

void renderEye() {
    tft.drawRGBBitmap(0, (tft.height() - 120) / 2,
                      (const uint16_t*)Eye_map, 160, 120);
}

// ─── Touch ────────────────────────────────────────────────────────────────────
static TouchMe touch(32, true);

void touchBegin() {
    touch.begin(20, 3);
    touch.onTouch([]() {
        LINFO("touch: detected");
        renderFace(FACE_GIGGLE);
    });
}

void updateTouch() {
    touch.update();
}

// ─── Public API ───────────────────────────────────────────────────────────────

void setupDeviceHardware() {
    uint8_t spiPins[] = {18, 23, (uint8_t)TFT_CS, (uint8_t)TFT_DC, (uint8_t)TFT_RST};
    reg4h_add_component("ST7735", "display", "SPI", spiPins, 5);

    uint8_t touchPins[] = {32};
    reg4h_add_component("TouchSensor", "input", "GPIO", touchPins, 1);

    touchBegin();

    tft.initR(INITR_BLACKTAB);
    tft.setRotation(1);
    tft.fillScreen(ST77XX_BLACK);
    redrawBootScreen();

    taraFace.begin();

    LINFO("Hardware ready — ST7735 128x160");
}

void setState(RobotState s) {
    currentState = s;
}

void applyRobotConfig() {
    displayBrightness = config4h_get("displayBrightness").asInt(displayBrightness);
    volume            = config4h_get("volume").asInt(volume);
    idleTimeout       = config4h_get("idleTimeout").asInt(idleTimeout);

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
