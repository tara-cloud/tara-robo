// Face test — SH1106 128x64 OLED via U8g2 SW_I2C (SDA=21, SCL=22)
// pio run -e test-face -t upload
//
// Touch GPIO32 to cycle: IDLE → HAPPY → SAD → ANGRY → LOVE → IDLE

#include <Arduino.h>
#include <U8g2lib.h>

static U8G2_SH1106_128X64_NONAME_F_SW_I2C
    u8g2(U8G2_R0, /*scl*/22, /*sda*/21, U8X8_PIN_NONE);

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
    u8g2.drawRBox(x, y, w, h, 6);
}

void drawIdleFace() {
    u8g2.clearBuffer();
    u8g2.setDrawColor(1);
    drawEye(14 + eyeOffsetX, 22 + eyeOffsetY, 40, 20);
    drawEye(74 + eyeOffsetX, 22 + eyeOffsetY, 40, 20);
    u8g2.sendBuffer();
}

void drawHappyFace() {
    u8g2.clearBuffer();
    u8g2.setDrawColor(1);
    // Arched happy eyes — fill roundrect then mask bottom half
    for (int side : {14, 74}) {
        u8g2.drawRBox(side, 22, 40, 20, 10);
        u8g2.setDrawColor(0);
        u8g2.drawBox(side, 32, 40, 11);  // mask bottom
        u8g2.setDrawColor(1);
    }
    u8g2.sendBuffer();
}

void drawSadFace() {
    u8g2.clearBuffer();
    u8g2.setDrawColor(1);
    // Drooping eyes — fill roundrect then mask top half, flip
    for (int side : {14, 74}) {
        u8g2.drawRBox(side, 22, 40, 20, 10);
        u8g2.setDrawColor(0);
        u8g2.drawBox(side, 22, 40, 11);  // mask top
        u8g2.setDrawColor(1);
    }
    u8g2.sendBuffer();
}

void drawAngryFace() {
    u8g2.clearBuffer();
    u8g2.setDrawColor(1);
    // Rectangle eyes with angry inner brow cut
    for (int side : {14, 74}) {
        u8g2.drawRBox(side, 22, 40, 20, 4);
        u8g2.setDrawColor(0);
        if (side == 14)
            u8g2.drawTriangle(side, 22, side + 40, 22, side + 40, 32); // left eye angry
        else
            u8g2.drawTriangle(side, 22, side + 40, 22, side, 32);       // right eye angry
        u8g2.setDrawColor(1);
    }
    u8g2.sendBuffer();
}

void drawLoveFace() {
    u8g2.clearBuffer();
    u8g2.setDrawColor(1);
    // Two hearts
    auto heart = [](int cx, int cy) {
        u8g2.drawDisc(cx - 5, cy - 4, 5);
        u8g2.drawDisc(cx + 5, cy - 4, 5);
        u8g2.drawTriangle(cx - 10, cy - 2, cx + 10, cy - 2, cx, cy + 10);
    };
    heart(34, 32);
    heart(94, 32);
    u8g2.sendBuffer();
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
            u8g2.clearBuffer(); u8g2.setDrawColor(1);
            for (int side : {14, 74}) {
                u8g2.drawRBox(side + eyeOffsetX, 22 + eyeOffsetY, 40, 20, 6);
                u8g2.setDrawColor(0);
                u8g2.drawBox(side + eyeOffsetX, 22 + eyeOffsetY, 40, min(lidH, 15));
                u8g2.setDrawColor(1);
            }
            u8g2.sendBuffer();
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
    Serial.println("=== face_test v4 — SH1106 OLED, touch GPIO32 to cycle ===");

    u8g2.begin();
    u8g2.setContrast(200);

    int sum = 0;
    for (int i = 0; i < 20; i++) { sum += touchRead(TOUCH_PIN); delay(10); }
    Serial.printf("Touch GPIO%d idle=%d threshold=130\n", TOUCH_PIN, sum / 20);

    nextMove = millis() + 3000;
    drawIdleFace();
}

void loop() {
    // Touch — cycle face
    if (isTouched() && millis() - _lastTouch > 400) {
        _lastTouch  = millis();
        currentFace = (Face)((currentFace + 1) % FACE_COUNT);
        showFace(currentFace);
        Serial.printf("Face: %d\n", currentFace);
    }

    // Idle animations only on idle face
    if (currentFace == IDLE) {
        driftTick();
        blinkTick();
    }

    delay(16);
}
