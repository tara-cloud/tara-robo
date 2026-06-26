// Tara Robot — device logic
// Display: ST7735 128x160 V1.1 via SPI (SCK=18, MOSI=23, CS=5, DC=2, RST=4)

#include "TaraCore.h"
#include <ArduinoJson.h>
#include <TFT_eSPI.h>
#include <config4h.h>
#include <reg4h.h>
#include <touch_me.h>
#include "Eye.h"

// ─── Display ──────────────────────────────────────────────────────────────────
static TFT_eSPI  tft;
static TFT_eSprite eyeSprite(&tft);

// Eye image dims
static const int EYE_W = 160;
static const int EYE_H = 120;

// ─── Config ───────────────────────────────────────────────────────────────────
static int  displayBrightness = 128;
static int  volume            = 70;
static int  idleTimeout       = 300;

// ─── Emotion state ────────────────────────────────────────────────────────────
struct RobotEmotionState { String state = "idle"; int energy = 50; unsigned long since = 0; };
static RobotEmotionState emotion;

// ─── Cached face JSON per state ───────────────────────────────────────────────
static String cachedFaces[11];

static const char* STATE_NAMES[] = {
    "booting", "connecting", "registering", "waiting_config", "configuring",
    "idle", "listening", "thinking", "speaking", "sleeping", "error"
};
static const int STATE_COUNT = sizeof(STATE_NAMES) / sizeof(STATE_NAMES[0]);

// ─── Eye sprite renderer ──────────────────────────────────────────────────────
// Eye_map is RGB565 160×120. Screen in landscape (setRotation(1)) is 160×128.
// Centre vertically: y offset = (128 - 120) / 2 = 4.

void renderEye() {
    eyeSprite.pushImage(0, 0, EYE_W, EYE_H, (uint16_t*)Eye_map);
    eyeSprite.pushSprite(0, (tft.height() - EYE_H) / 2);
}

// ─── Boot log ─────────────────────────────────────────────────────────────────
static const int LOG_Y_START = 20;
static const int LOG_LINE_H  = 11;
static const int LOG_MAX     = 4;

static String logLines[LOG_MAX];
static int    logCount = 0;

static void redrawBootScreen() {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(2);
    tft.setCursor((tft.width() - 48) / 2, 4);
    tft.print("TARA");
    tft.drawFastHLine(0, 20, tft.width(), TFT_WHITE);
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

// ─── Touch ────────────────────────────────────────────────────────────────────
static TouchMe touch(32, true);

void touchBegin() {
    touch.begin(20, 3);
    touch.onTouch([]() {
        LINFO("touch: detected");
        renderEye();
    });
}

void updateTouch() {
    touch.update();
}

// ─── Public API ───────────────────────────────────────────────────────────────

void setupDeviceHardware() {
    uint8_t spiPins[] = {18, 23, 5, 2, 4};
    reg4h_add_component("ST7735", "display", "SPI", spiPins, 5);

    uint8_t touchPins[] = {32};
    reg4h_add_component("TouchSensor", "input", "GPIO", touchPins, 1);

    touchBegin();

    tft.init();
    tft.setRotation(1);   // landscape: 160×128
    tft.fillScreen(TFT_BLACK);

    // Allocate sprite for eye image
    eyeSprite.createSprite(EYE_W, EYE_H);
    eyeSprite.setColorDepth(16);

    redrawBootScreen();

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
    String faceName = doc["face"] | String("idle");
    LDEBUG("Display: face=%s — rendering eye sprite", faceName.c_str());
    renderEye();
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
