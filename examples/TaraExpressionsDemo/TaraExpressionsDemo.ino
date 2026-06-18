/**
 * TaraExpressionsDemo.ino
 *
 * Demonstrates all expressions, eye movements, animations, and the
 * emotion engine on a 128x64 SSD1306 OLED (I2C, SDA=21, SCL=22).
 *
 * Dependencies:
 *   Adafruit SSD1306  ^2.5
 *   Adafruit GFX      ^1.11
 *
 * ── Swapping the display ────────────────────────────────────────────────────
 * 1. Remove: #include "SSD1306Display.h" / SSD1306Display adapter(...)
 * 2. Add your own adapter that extends IDisplay
 * 3. Pass it to TaraExpressions — nothing else changes.
 */

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// HAL
#include "IDisplay.h"
#include "SSD1306Display.h"

// Expressions engine
#include "TaraExpressions.h"

// ── Hardware config ──────────────────────────────────────────────────────────
#define SDA_PIN     21
#define SCL_PIN     22
#define OLED_ADDR   0x3C
#define SCREEN_W    128
#define SCREEN_H    64
#define OLED_RESET  -1

// ── Objects ──────────────────────────────────────────────────────────────────
Adafruit_SSD1306 oled(SCREEN_W, SCREEN_H, &Wire, OLED_RESET);
SSD1306Display   adapter(&oled, OLED_ADDR);
TaraExpressions  face(&adapter);

// ── Demo state ────────────────────────────────────────────────────────────────
static const Expression SEQUENCE[] = {
    EXPR_IDLE, EXPR_HAPPY, EXPR_SAD, EXPR_LISTENING,
    EXPR_THINKING, EXPR_SPEAKING, EXPR_SLEEPING,
    EXPR_SURPRISED, EXPR_ANGRY, EXPR_ERROR
};
static const uint8_t SEQ_LEN = sizeof(SEQUENCE) / sizeof(SEQUENCE[0]);
static uint8_t       seqIdx  = 0;
static unsigned long nextSwap = 0;

void setup() {
    Serial.begin(115200);
    Wire.begin(SDA_PIN, SCL_PIN);

    if (!adapter.begin()) {
        Serial.println("OLED not found — check wiring");
        while (true) delay(1000);
    }

    Serial.println("TaraExpressions demo started");
    face.idle();
    nextSwap = millis() + 2000;
}

void loop() {
    unsigned long now = millis();

    // Cycle through expressions every 2 s
    if (now >= nextSwap) {
        seqIdx  = (seqIdx + 1) % SEQ_LEN;
        face.show(SEQUENCE[seqIdx]);
        Serial.printf("Expression: %d\n", SEQUENCE[seqIdx]);

        // After showing, demonstrate gaze for thinking/speaking
        switch (SEQUENCE[seqIdx]) {
            case EXPR_THINKING:  nextSwap = now + 4000; break; // animateThinking plays longer
            case EXPR_SPEAKING:  nextSwap = now + 3000; break;
            default:             nextSwap = now + 2000; break;
        }
    }

    // Run animation loop for animated states
    switch (SEQUENCE[seqIdx]) {
        case EXPR_IDLE:     face.animateIdle();     break;
        case EXPR_THINKING: face.animateThinking();  break;
        case EXPR_SPEAKING: face.animateSpeaking();  break;
        default: break;
    }

    delay(10);
}
