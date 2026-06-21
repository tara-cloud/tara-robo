// Touch test with OLED feedback — auto-detects the active touch pin
// pio run -e test-touch -t upload
//
// On boot: scans all pins, picks the one with highest idle value (most responsive)
// Then shows touch events on OLED screen:
//   idle         → two white eyes (calm)
//   single tap   → eyes blink + "TAP"
//   double tap   → squinting eyes + "TAP TAP"
//   multiple tap → wide eyes + "TAP xN"
//   long press   → closed eyes + "HOLD"

#include <Arduino.h>
#include <U8g2lib.h>

// ─── Display ──────────────────────────────────────────────────────────────────
static U8G2_SH1106_128X64_NONAME_F_SW_I2C
    u8g2(U8G2_R0, /*scl*/22, /*sda*/21, U8X8_PIN_NONE);

// ─── All touch-capable pins ───────────────────────────────────────────────────
static const int ALL_PINS[] = {4, 0, 2, 15, 13, 12, 14, 27, 33, 32};
static const int N_PINS     = 10;

// Pin 33: idle=128, touched=133 → value RISES when touched
// Detection: touchRead(33) > THRESHOLD  (inverted from normal)
static const int TOUCH_PIN  = 33;
static const int IDLE_VAL   = 128;
static const int THRESHOLD  = 130;  // above idle, below touched peak

// ─── Touch state ──────────────────────────────────────────────────────────────
static const int   DEBOUNCE   = 3;
static const unsigned long TAP_WIN  = 1000;
static const unsigned long GAP_WIN  = 500;
static const unsigned long LONG_MS  = 2000;

static bool          _down      = false;
static unsigned long _pressAt   = 0;
static unsigned long _releaseAt = 0;
static int           _tapCount  = 0;
static bool          _longFired = false;
static int           _dbc       = 0;
static bool          _stable    = false;

// ─── Screen state ─────────────────────────────────────────────────────────────
enum ScreenState { S_IDLE, S_TAP, S_DOUBLE, S_MULTI, S_HOLD, S_SEARCHING };
static ScreenState   _screen    = S_SEARCHING;
static int           _multiN    = 0;
static unsigned long _showUntil = 0;

// ─── Draw helpers ─────────────────────────────────────────────────────────────

static void drawEyes(int openH, int eyeW = 36) {
    int lx = (128 - eyeW*2 - 10) / 2;
    int rx = lx + eyeW + 10;
    int ey = (64 - openH) / 2;
    if (openH > 0) {
        u8g2.drawRBox(lx, ey, eyeW, openH, 8);
        u8g2.drawRBox(rx, ey, eyeW, openH, 8);
    } else {
        // closed — thin line
        u8g2.drawHLine(lx,  32, eyeW);
        u8g2.drawHLine(lx,  33, eyeW);
        u8g2.drawHLine(rx,  32, eyeW);
        u8g2.drawHLine(rx,  33, eyeW);
    }
}

static void bigText(const char* line1, const char* line2 = nullptr) {
    u8g2.setFont(u8g2_font_ncenB14_tr);
    int w1 = u8g2.getStrWidth(line1);
    if (line2 == nullptr) {
        u8g2.drawStr((128 - w1) / 2, 38, line1);
    } else {
        int w2 = u8g2.getStrWidth(line2);
        u8g2.drawStr((128 - w1) / 2, 26, line1);
        u8g2.drawStr((128 - w2) / 2, 50, line2);
    }
}

static void drawScreen() {
    u8g2.clearBuffer();
    u8g2.setDrawColor(1);

    switch (_screen) {
        case S_SEARCHING:
            u8g2.setFont(u8g2_font_6x10_tf);
            u8g2.drawStr(10, 25, "Searching touch");
            u8g2.drawStr(20, 40, "pin...");
            break;
        case S_IDLE: {
            // Show live raw value so we can see if touching changes it
            int raw = TOUCH_PIN >= 0 ? (int)touchRead(TOUCH_PIN) : -1;
            u8g2.setFont(u8g2_font_ncenB14_tr);
            char buf[20]; snprintf(buf, sizeof(buf), "raw: %d", raw);
            int w = u8g2.getStrWidth(buf);
            u8g2.drawStr((128 - w) / 2, 26, buf);
            char buf2[20]; snprintf(buf2, sizeof(buf2), "thr: %d", THRESHOLD);
            int w2 = u8g2.getStrWidth(buf2);
            u8g2.drawStr((128 - w2) / 2, 50, buf2);
            break;
        }
        case S_TAP:
            bigText("TAP");
            break;
        case S_DOUBLE:
            bigText("TAP", "TAP");
            break;
        case S_MULTI: {
            char buf[10]; snprintf(buf, sizeof(buf), "TAP x%d", _multiN);
            bigText(buf);
            break;
        }
        case S_HOLD:
            bigText("HOLD");
            break;
    }

    u8g2.sendBuffer();
}

// ─── Setup ────────────────────────────────────────────────────────────────────

void setup() {
    Serial.begin(115200);
    u8g2.begin();
    u8g2.setContrast(128);

    // Scan all pins and show on screen — touch the wire during this
    static const int PINS[] = {4, 0, 2, 15, 13, 12, 14, 27, 33, 32};
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(0, 10, "TOUCH YOUR PAD NOW");
    u8g2.sendBuffer();
    delay(2000);   // 2 s window — touch the IO wire during this

    // Print all values while touching
    u8g2.clearBuffer();
    int row = 10;
    for (int i = 0; i < 10; i++) {
        int v = (int)touchRead(PINS[i]);
        char buf[24]; snprintf(buf, sizeof(buf), "T%d G%d=%d", i, PINS[i], v);
        u8g2.drawStr(0, row, buf);
        row += 11;
        if (row > 54) { u8g2.sendBuffer(); delay(2000); u8g2.clearBuffer(); row = 10; }
        Serial.printf("  T%d GPIO%d = %d\n", i, PINS[i], v);
    }
    u8g2.sendBuffer();
    delay(4000);   // read the screen

    _screen = S_IDLE;
    drawScreen();
}

// ─── Loop ─────────────────────────────────────────────────────────────────────

void loop() {
    unsigned long now = millis();

    // Revert to idle after 1.5 s
    if (_screen != S_IDLE && _screen != S_HOLD && now >= _showUntil) {
        _screen = S_IDLE;
    }

    // Refresh idle screen every loop tick — shows raw value in real time
    if (_screen == S_IDLE) {
        drawScreen();
    }

    if (TOUCH_PIN < 0) return;

    // Debounce
    bool raw = touchRead(TOUCH_PIN) > THRESHOLD;  // rises when touched
    if (raw == _stable) { _dbc = 0; }
    else if (++_dbc >= DEBOUNCE) { _stable = raw; _dbc = 0; }

    // Gesture state machine
    if (_stable && !_down) {
        _down = true; _pressAt = now; _longFired = false;

    } else if (_stable && _down) {
        if (!_longFired && now - _pressAt >= LONG_MS) {
            _longFired = true; _tapCount = 0;
            _screen = S_HOLD; drawScreen();
            Serial.println(">>> LONG PRESS");
        }

    } else if (!_stable && _down) {
        _down = false;
        if (_screen == S_HOLD) { _screen = S_IDLE; drawScreen(); }
        if (!_longFired && now - _pressAt < TAP_WIN) { _tapCount++; _releaseAt = now; }

    } else {
        if (_tapCount > 0 && now - _releaseAt >= GAP_WIN) {
            if (_tapCount == 1) {
                _screen = S_TAP; Serial.println(">>> SINGLE TAP");
            } else if (_tapCount == 2) {
                _screen = S_DOUBLE; Serial.println(">>> DOUBLE TAP");
            } else {
                _screen = S_MULTI; _multiN = _tapCount;
                Serial.printf(">>> MULTIPLE TAP (%d)\n", _tapCount);
            }
            _showUntil = now + 1500; _tapCount = 0; drawScreen();
        }
    }
}
