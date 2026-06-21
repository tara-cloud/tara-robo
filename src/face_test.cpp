// Face test — TFT_eSPI on ST7735 128×160
// Standalone: pio run -e test-face -t upload
//
// Idle face: two cyan eyes with subtle drift every 3s
// Touch pad (GPIO32): triggers giggle — capsule eyes wobble

#include <Arduino.h>
#include <TFT_eSPI.h>

TFT_eSPI tft = TFT_eSPI();

#define FACE_COLOR TFT_CYAN
#define BG_COLOR   TFT_BLACK
#define SCR_W      128
#define SCR_H      160

// ─── Eye geometry ─────────────────────────────────────────────────────────────
static const int EYE_W  = 30;
static const int EYE_H  = 36;
static const int EYE_R  = 8;
static const int SPACE  = 10;

// ─── Drawing helpers ──────────────────────────────────────────────────────────

void clearFace() { tft.fillScreen(BG_COLOR); }

void drawEye(int x, int y, int w, int h) {
    tft.fillRoundRect(x, y, w, h, EYE_R, FACE_COLOR);
}

void drawIdleEyes(int ox, int oy) {
    int totalW = EYE_W + SPACE + EYE_W;
    int lx     = (SCR_W - totalW) / 2 + ox;
    int rx     = lx + EYE_W + SPACE;
    int ey     = (SCR_H - EYE_H) / 2 + oy;
    clearFace();
    drawEye(lx, ey, EYE_W, EYE_H);
    drawEye(rx, ey, EYE_W, EYE_H);
}

void drawGiggleEyes(int ox, int scaleH) {
    // Small capsule eyes, upper screen, inward tilt
    static const int GW = 22, GH = 14, GR = 7, GS = 14, TILT = 3;
    int totalW = GW + GS + GW;
    int lx     = (SCR_W - totalW) / 2 + ox;
    int rx     = lx + GW + GS;
    int ey     = SCR_H / 4;
    int h      = GH + scaleH;
    int r      = h / 2;

    clearFace();

    // Left eye + inward tilt mask
    tft.fillRoundRect(lx, ey, GW, h, r, FACE_COLOR);
    for (int t = 0; t < TILT; t++)
        tft.fillRect(lx + GW - (TILT - t), ey + t, TILT - t, 1, BG_COLOR);

    // Right eye + inward tilt mask
    tft.fillRoundRect(rx, ey, GW, h, r, FACE_COLOR);
    for (int t = 0; t < TILT; t++)
        tft.fillRect(rx, ey + t, TILT - t, 1, BG_COLOR);
}

// ─── Idle state ───────────────────────────────────────────────────────────────
float _idleOX = 0, _idleOY = 0;
int   _targetX = 0, _targetY = 0;
unsigned long _nextMove = 3000;

static const int8_t POSITIONS[][2] = {
    {0,0},{0,0},{0,0},    // centre weighted
    {-4,0},{4,0},
    {0,-3},{0,3},
    {-4,-3},{4,-3},
};

void updateIdle() {
    unsigned long now = millis();
    if (now >= _nextMove) {
        int idx = random(0, 9);
        _targetX = POSITIONS[idx][0];
        _targetY = POSITIONS[idx][1];
        _nextMove = now + 3000 + random(0, 1000);
    }
    _idleOX += (_targetX - _idleOX) * 0.08f;
    _idleOY += (_targetY - _idleOY) * 0.08f;
    if (abs(_idleOX - _targetX) < 0.5f) _idleOX = _targetX;
    if (abs(_idleOY - _targetY) < 0.5f) _idleOY = _targetY;
    drawIdleEyes((int)_idleOX, (int)_idleOY);
}

// ─── Giggle state ─────────────────────────────────────────────────────────────
bool          _giggling   = false;
unsigned long _giggleStart = 0;
static const unsigned long GIGGLE_STEP = 120;
static const int           GIGGLE_CYCLES = 8;

void startGiggle() {
    if (!_giggling) { _giggling = true; _giggleStart = millis(); }
}

bool updateGiggle() {
    if (!_giggling) return false;
    unsigned long elapsed = millis() - _giggleStart;
    int cycle = (int)(elapsed / GIGGLE_STEP);
    if (cycle >= GIGGLE_CYCLES) {
        _giggling = false;
        _idleOX = _idleOY = 0; _targetX = _targetY = 0;
        drawIdleEyes(0, 0);
        return false;
    }
    int ox     = (cycle % 2 == 0) ? -3 :  3;
    int scale  = (cycle % 2 == 0) ?  4 :  0;
    drawGiggleEyes(ox, scale);
    return true;
}

// ─── Touch ────────────────────────────────────────────────────────────────────
static const int TOUCH_PIN   = 32;
static const int TOUCH_THR   = 0;   // set at boot
static int       _idleVal    = 0;
static unsigned long _lastTouch = 0;

bool isTouched() {
    return (int)touchRead(TOUCH_PIN) > (_idleVal + 3);
}

// ─── Setup / Loop ─────────────────────────────────────────────────────────────

void setup() {
    Serial.begin(115200);
    tft.init();
    tft.setRotation(0);
    tft.fillScreen(BG_COLOR);

    // Calibrate idle touch value
    long sum = 0;
    for (int i = 0; i < 20; i++) { sum += touchRead(TOUCH_PIN); delay(10); }
    _idleVal = (int)(sum / 20);
    Serial.printf("Touch: GPIO%d idle=%d threshold=%d\n",
                  TOUCH_PIN, _idleVal, _idleVal + 3);

    drawIdleEyes(0, 0);
    _nextMove = millis() + 3000;
}

void loop() {
    // Touch — start giggle
    bool touched = isTouched();
    if (touched && millis() - _lastTouch > 300) {
        _lastTouch = millis();
        startGiggle();
    }

    if (!updateGiggle()) {
        updateIdle();
    }

    delay(16);   // ~60fps
}
