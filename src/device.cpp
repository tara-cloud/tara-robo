// Tara Robot — device logic
// Display: ST7735 128x160 V1.1 via SPI (SCK=18, MOSI=23, CS=5, DC=2, RST=4, BL=16)

#include "TaraCore.h"
#include <ArduinoJson.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>
#include <reg4h.h>
#include <touch_me.h>
#include "Eye.h"

// ─── Display ──────────────────────────────────────────────────────────────────
static const int TFT_CS  = 5;
static const int TFT_DC  = 2;
static const int TFT_RST = 4;
static const int TFT_BL  = 16;

static Adafruit_ST7735 tft(TFT_CS, TFT_DC, TFT_RST);

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
    tft.drawRGBBitmap(0, (tft.height() - EYE_H) / 2,
                      (const uint16_t*)Eye_map, EYE_W, EYE_H);
}

static size_t b64decode(const char* src, uint8_t* dst) {
    static const char tbl[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t out = 0;
    uint32_t val = 0;
    int bits = 0;
    for (; *src; src++) {
        if (*src == '=') break;
        const char* p = strchr(tbl, *src);
        if (!p) continue;
        val = (val << 6) | (uint32_t)(p - tbl);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            if (dst) dst[out] = (val >> bits) & 0xff;
            out++;
        }
    }
    return out;
}

void renderRaw(const char* b64data, int w, int h) {
    size_t binLen = b64decode(b64data, nullptr);
    uint8_t* buf = (uint8_t*)malloc(binLen);
    if (!buf) { LERROR("renderRaw: malloc failed (%d bytes)", (int)binLen); return; }
    b64decode(b64data, buf);
    int x = (tft.width()  - w) / 2;
    int y = (tft.height() - h) / 2;
    tft.drawRGBBitmap(x, y, (const uint16_t*)buf, w, h);
    free(buf);
}

// ─── Chunked raw image transfer ───────────────────────────────────────────────
static uint8_t* _rawBuf      = nullptr;
static size_t   _rawBufSize  = 0;
static size_t   _rawOffset   = 0;
static int      _rawW        = 0;
static int      _rawH        = 0;
static int      _rawChunks   = 0;
static int      _rawReceived = 0;

void rawStart(int w, int h, int chunks) {
    if (_rawBuf) { free(_rawBuf); _rawBuf = nullptr; }
    _rawBufSize  = (size_t)w * h * 2;
    _rawBuf      = (uint8_t*)malloc(_rawBufSize);
    _rawOffset   = 0;
    _rawW        = w;
    _rawH        = h;
    _rawChunks   = chunks;
    _rawReceived = 0;
    if (!_rawBuf) LERROR("rawStart: malloc failed (%d bytes)", (int)_rawBufSize);
    else          LINFO("rawStart: %dx%d %d chunks", w, h, chunks);
}

void rawChunk(int index, const char* b64data) {
    if (!_rawBuf) { LERROR("rawChunk: no buffer"); return; }
    size_t decoded = b64decode(b64data, _rawBuf + _rawOffset);
    _rawOffset += decoded;
    _rawReceived++;
    LINFO("rawChunk: %d/%d", _rawReceived, _rawChunks);
    if (_rawReceived >= _rawChunks) {
        int x = (tft.width()  - _rawW) / 2;
        int y = (tft.height() - _rawH) / 2;
        tft.drawRGBBitmap(x, y, (const uint16_t*)_rawBuf, _rawW, _rawH);
        free(_rawBuf); _rawBuf = nullptr;
        LINFO("rawChunk: rendered %dx%d", _rawW, _rawH);
    }
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

void setBacklight(bool on) {
    digitalWrite(TFT_BL, on ? HIGH : LOW);
}

void setupDeviceHardware() {
    // Drive BL HIGH first — prevents float during init
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);

    uint8_t spiPins[] = {18, 23, (uint8_t)TFT_CS, (uint8_t)TFT_DC, (uint8_t)TFT_RST};
    reg4h_add_component("ST7735", "display", "SPI", spiPins, 5);

    uint8_t touchPins[] = {32};
    reg4h_add_component("TouchSensor", "input", "GPIO", touchPins, 1);

    touchBegin();

    tft.initR(INITR_BLACKTAB);
    tft.setRotation(1);
    tft.fillScreen(ST77XX_BLACK);
    redrawBootScreen();

    LINFO("Hardware ready — ST7735 128x160 BL=GPIO16");
}

void setState(RobotState s) {
    currentState = s;
}

void applySocketConfig(const JsonDocument& doc) {
    displayBrightness = doc["displayBrightness"] | displayBrightness;
    volume            = doc["volume"]            | volume;
    idleTimeout       = doc["idleTimeout"]       | idleTimeout;

    JsonVariantConst faces = doc["faces"];
    if (faces.is<JsonObjectConst>()) {
        for (int i = 0; i < STATE_COUNT; i++) {
            const char* name = STATE_NAMES[i];
            if (faces[name].is<const char*>())
                cachedFaces[i] = faces[name].as<String>();
        }
        LINFO("applySocketConfig: face cache updated");
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
