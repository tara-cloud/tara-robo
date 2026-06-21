#include <Arduino.h>
#include <TFT_eSPI.h>

TFT_eSPI tft = TFT_eSPI();

#define FACE_COLOR TFT_CYAN
#define BG_COLOR TFT_BLACK

unsigned long lastBlink = 0;
unsigned long lastMove  = 0;

int eyeOffsetX = 0;
int eyeOffsetY = 0;

// ─── Face order ───────────────────────────────────────────────────────────────
enum Face { IDLE, HAPPY, SAD, ANGRY, LOVE, FACE_COUNT };
static Face currentFace = IDLE;

// ─── Touch ────────────────────────────────────────────────────────────────────
static const int   TOUCH_PIN  = 32;
static int         _idleVal   = 0;
static unsigned long _lastTouch = 0;

bool isTouched() {
    return (int)touchRead(TOUCH_PIN) > (_idleVal + 3);
}

// ─── Drawing ──────────────────────────────────────────────────────────────────

void clearFace() { tft.fillScreen(BG_COLOR); }

void drawEye(int x, int y, int w, int h) {
    tft.fillRoundRect(x, y, w, h, 6, FACE_COLOR);
}

void drawIdleFace() {
    clearFace();
    drawEye(32 + eyeOffsetX, 55 + eyeOffsetY, 20, 20);
    drawEye(76 + eyeOffsetX, 55 + eyeOffsetY, 20, 20);
}

void drawBlinkFace() {
    clearFace();
    tft.fillRoundRect(32 + eyeOffsetX, 65 + eyeOffsetY, 20, 4, 2, FACE_COLOR);
    tft.fillRoundRect(76 + eyeOffsetX, 65 + eyeOffsetY, 20, 4, 2, FACE_COLOR);
}

void drawHappyFace() {
    clearFace();
    for (int i = 0; i < 20; i++) {
        tft.drawPixel(32 + i, 65 - (i / 4), FACE_COLOR);
        tft.drawPixel(76 + i, 65 - (i / 4), FACE_COLOR);
    }
}

void drawSadFace() {
    clearFace();
    for (int i = 0; i < 20; i++) {
        tft.drawPixel(32 + i, 60 + (i / 4), FACE_COLOR);
        tft.drawPixel(76 + i, 60 + (i / 4), FACE_COLOR);
    }
}

void drawAngryFace() {
    clearFace();
    tft.fillTriangle(30, 65, 52, 55, 52, 70, FACE_COLOR);
    tft.fillTriangle(98, 65, 76, 55, 76, 70, FACE_COLOR);
}

void drawHeart(int x, int y);

void drawLoveFace() {
    clearFace();
    drawHeart(42, 65);
    drawHeart(86, 65);
}

void drawHeart(int x, int y) {
    tft.fillCircle(x - 4, y - 4, 4, FACE_COLOR);
    tft.fillCircle(x + 4, y - 4, 4, FACE_COLOR);
    tft.fillTriangle(x - 8, y - 2, x + 8, y - 2, x, y + 10, FACE_COLOR);
}

void showFace(Face f) {
    switch (f) {
        case IDLE:  drawIdleFace();  break;
        case HAPPY: drawHappyFace(); break;
        case SAD:   drawSadFace();   break;
        case ANGRY: drawAngryFace(); break;
        case LOVE:  drawLoveFace();  break;
        default: break;
    }
}

// ─── Idle animations ──────────────────────────────────────────────────────────

void blinkAnimation() {
    drawBlinkFace();
    delay(120);
    drawIdleFace();
}

void randomEyeMovement() {
    eyeOffsetX = random(-3, 4);
    eyeOffsetY = random(-2, 3);
    drawIdleFace();
}

void idleBehavior() {
    if (millis() - lastBlink > (unsigned long)random(3000, 7000)) {
        blinkAnimation();
        lastBlink = millis();
    }
    if (millis() - lastMove > (unsigned long)random(1500, 3000)) {
        randomEyeMovement();
        lastMove = millis();
    }
}

// ─── Setup / Loop ─────────────────────────────────────────────────────────────

void setup() {
    Serial.begin(115200);
    tft.init();
    tft.setRotation(1);
    randomSeed(micros());

    // Calibrate idle touch
    long sum = 0;
    for (int i = 0; i < 20; i++) { sum += touchRead(TOUCH_PIN); delay(10); }
    _idleVal = (int)(sum / 20);
    Serial.printf("Touch GPIO%d idle=%d\n", TOUCH_PIN, _idleVal);

    drawIdleFace();
}

void loop() {
    // Touch — cycle to next face
    if (isTouched() && millis() - _lastTouch > 400) {
        _lastTouch  = millis();
        currentFace = (Face)((currentFace + 1) % FACE_COUNT);
        showFace(currentFace);
    }

    // Run idle animations only when on idle face
    if (currentFace == IDLE) {
        idleBehavior();
    }
}
