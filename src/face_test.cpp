// Face test — ST7735 128x160 V1.1 via SPI (SCK=18, MOSI=23, CS=5, DC=2, RST=4)
// pio run -e test-face -t upload
//
// Touch GPIO32 to cycle: IDLE → HAPPY → SAD → ANGRY → LOVE → IDLE

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>

#define TFT_CS  5
#define TFT_DC  2
#define TFT_RST 4

static Adafruit_ST7735 tft(TFT_CS, TFT_DC, TFT_RST);

// ─── Touch ────────────────────────────────────────────────────────────────────
static const int TOUCH_PIN = 32;
static unsigned long _lastTouch = 0;

bool isTouched() { return (int)touchRead(TOUCH_PIN) > 130; }

// ─── Face state ───────────────────────────────────────────────────────────────
enum Face { IDLE, HAPPY, SAD, ANGRY, LOVE, FACE_COUNT };
static Face currentFace = IDLE;

// ─── Idle animation ───────────────────────────────────────────────────────────
static int eyeOffsetX   = 0, eyeOffsetY = 0;
static int targetX      = 0, targetY    = 0;
static float floatX     = 0, floatY     = 0;
static unsigned long nextMove = 0;
static unsigned long lastBlink = 0;

// ─── Drawing ──────────────────────────────────────────────────────────────────

void drawEye(int x, int y, int w, int h) {
    tft.fillRoundRect(x, y, w, h, 6, ST77XX_WHITE);
}

void drawIdleFace() {
    tft.fillScreen(ST77XX_BLACK);
    drawEye(14 + eyeOffsetX, 22 + eyeOffsetY, 40, 20);
    drawEye(74 + eyeOffsetX, 22 + eyeOffsetY, 40, 20);
}

void drawHappyFace() {
    tft.fillScreen(ST77XX_BLACK);
    for (int side : {14, 74}) {
        tft.fillRoundRect(side, 22, 40, 20, 10, ST77XX_WHITE);
        tft.fillRect(side, 32, 40, 11, ST77XX_BLACK);
    }
}

void drawSadFace() {
    tft.fillScreen(ST77XX_BLACK);
    for (int side : {14, 74}) {
        tft.fillRoundRect(side, 22, 40, 20, 10, ST77XX_WHITE);
        tft.fillRect(side, 22, 40, 11, ST77XX_BLACK);
    }
}

void drawAngryFace() {
    tft.fillScreen(ST77XX_BLACK);
    for (int side : {14, 74}) {
        tft.fillRoundRect(side, 22, 40, 20, 4, ST77XX_WHITE);
        if (side == 14)
            tft.fillTriangle(side, 22, side + 40, 22, side + 40, 32, ST77XX_BLACK);
        else
            tft.fillTriangle(side, 22, side + 40, 22, side, 32, ST77XX_BLACK);
    }
}

void drawLoveFace() {
    tft.fillScreen(ST77XX_BLACK);
    auto heart = [](int cx, int cy) {
        tft.fillCircle(cx - 5, cy - 4, 5, ST77XX_RED);
        tft.fillCircle(cx + 5, cy - 4, 5, ST77XX_RED);
        tft.fillTriangle(cx - 10, cy - 2, cx + 10, cy - 2, cx, cy + 10, ST77XX_RED);
    };
    heart(34, 32);
    heart(94, 32);
}

void showFace(Face f) {
    switch (f) {
        case IDLE:  drawIdleFace();  break;
        case HAPPY: drawHappyFace(); break;
        case SAD:   drawSadFace();   break;
        case ANGRY: drawAngryFace(); break;
        case LOVE:  drawLoveFace();  break;
        default:    break;
    }
}

// ─── Idle blink (non-blocking) ────────────────────────────────────────────────
void blinkTick() {
    unsigned long now = millis();
    static bool blinkActive = false;
    static unsigned long blinkStart = 0;

    if (!blinkActive && now - lastBlink > (unsigned long)random(4000, 8000)) {
        blinkActive = true; blinkStart = now; lastBlink = now;
    }
    if (blinkActive) {
        unsigned long e = now - blinkStart;
        if (e < 350) {
            float t   = (float)e / 350.0f;
            int lidH  = (int)(t * t * 21);
            tft.fillScreen(ST77XX_BLACK);
            for (int side : {14, 74}) {
                tft.fillRoundRect(side + eyeOffsetX, 22 + eyeOffsetY, 40, 20, 6, ST77XX_WHITE);
                tft.fillRect(side + eyeOffsetX, 22 + eyeOffsetY, 40, min(lidH, 15), ST77XX_BLACK);
            }
        } else {
            blinkActive = false;
            drawIdleFace();
        }
    }
}

// ─── Idle eye drift (non-blocking) ───────────────────────────────────────────
static const int8_t POSITIONS[][2] = {
    {0,0},{0,0},{0,0},{-4,0},{4,0},{0,-3},{0,3},{-4,-3},{4,-3}
};

void driftTick() {
    unsigned long now = millis();
    if (now >= nextMove) {
        int idx = random(0, 9);
        targetX   = POSITIONS[idx][0];
        targetY   = POSITIONS[idx][1];
        nextMove  = now + 3000 + random(0, 1000);
    }
    floatX += (targetX - floatX) * 0.08f;
    floatY += (targetY - floatY) * 0.08f;
    eyeOffsetX = (int)floatX;
    eyeOffsetY = (int)floatY;
    drawIdleFace();
}

// ─── Setup / Loop ─────────────────────────────────────────────────────────────

void setup() {
    Serial.begin(115200);
    Serial.println("=== face_test — ST7735 128x160, touch GPIO32 to cycle ===");

    tft.initR(INITR_BLACKTAB);
    tft.setRotation(1);
    tft.fillScreen(ST77XX_BLACK);

    int sum = 0;
    for (int i = 0; i < 20; i++) { sum += touchRead(TOUCH_PIN); delay(10); }
    Serial.printf("Touch GPIO%d idle=%d threshold=130\n", TOUCH_PIN, sum / 20);

    nextMove = millis() + 3000;
    drawIdleFace();
}

void loop() {
    if (isTouched() && millis() - _lastTouch > 400) {
        _lastTouch  = millis();
        currentFace = (Face)((currentFace + 1) % FACE_COUNT);
        showFace(currentFace);
        Serial.printf("Face: %d\n", currentFace);
    }

    if (currentFace == IDLE) {
        driftTick();
        blinkTick();
    }

    delay(16);
}
