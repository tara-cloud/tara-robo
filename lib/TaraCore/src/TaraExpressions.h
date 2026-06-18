#pragma once
#include "IDisplay.h"
#include <Arduino.h>

// ── Expression enum ────────────────────────────────────────────────────────
enum Expression {
    EXPR_IDLE,
    EXPR_HAPPY,
    EXPR_SAD,
    EXPR_LISTENING,
    EXPR_THINKING,
    EXPR_SPEAKING,
    EXPR_SLEEPING,
    EXPR_SURPRISED,
    EXPR_ANGRY,
    EXPR_ERROR
};

// ── Emotion state ──────────────────────────────────────────────────────────
struct EmotionState {
    int energy;       // 0–100  (low = slow blink, high = fast animation)
    int happiness;    // 0–100  (maps to happy/sad lean)
    int attention;    // 0–100  (high = wide pupils, looking alert)
    int sleepiness;   // 0–100  (high → sleeping expression)
};

// ── TaraExpressions ───────────────────────────────────────────────────────
/**
 * All drawing goes through IDisplay — swap the adapter for a new display
 * without touching any expression logic.
 *
 * Eye geometry (128×64 canvas):
 *   Left eye  centred at x=38, Right eye at x=90, baseline y=32
 *   Eye width ~30px, height ~22px with rounded rectangle
 *   Eyelid = filled rect that overlaps the top of the eye rounded rect
 */
class TaraExpressions {
public:
    explicit TaraExpressions(IDisplay* display);

    // ── Static expressions ───────────────────────────────────────────────
    void idle();
    void happy();
    void sad();
    void listening();
    void thinking();
    void speaking();
    void sleeping();
    void surprised();
    void angry();
    void error();

    // Dispatch by enum (useful for MQTT handler)
    void show(Expression expr);

    // ── Eye movements (relative to current expression) ───────────────────
    void blink();
    void lookLeft();
    void lookRight();
    void lookUp();
    void lookDown();

    // ── Animations (call in loop; they are non-blocking) ─────────────────
    void animateIdle();       // random blink + subtle drift
    void animateThinking();   // eyes scan left↔right
    void animateSpeaking();   // eye bounce rhythm

    // ── Emotion engine ───────────────────────────────────────────────────
    void update(const EmotionState& state);  // auto-select expression & speed

private:
    IDisplay* _d;

    // Pupil offset applied on top of the base expression
    int  _pupilDX = 0;
    int  _pupilDY = 0;

    // Animation state
    unsigned long _lastAnimMs   = 0;
    unsigned long _nextBlinkMs  = 0;
    int           _animStep     = 0;
    bool          _isBlinking   = false;

    // Current expression (for animation context)
    Expression    _currentExpr  = EXPR_IDLE;

    // ── Drawing helpers ──────────────────────────────────────────────────
    struct Eye {
        int cx, cy;    // centre
        int w, h;      // full outer width/height
        int r;         // corner radius
    };

    static constexpr Eye LEFT  = {38, 32, 30, 22, 6};
    static constexpr Eye RIGHT = {90, 32, 30, 22, 6};

    // Draw one eye with eyelid and pupil
    // lidFrac  0.0 = fully open, 1.0 = closed
    // pupilDX/DY = offset from centre for gaze direction
    // arch    = true → arch top (raised eyebrow look)
    // squint  = true → reduce height by 30% from bottom
    void _drawEye(const Eye& e,
                  float lidFrac,
                  int   pupilDX, int pupilDY,
                  bool  archTop  = false,
                  bool  squint   = false,
                  bool  excited  = false);

    // Draw both eyes with the same params
    void _drawEyes(float lidFrac,
                   int   pupilDX = 0, int pupilDY = 0,
                   bool  archTop = false, bool squint = false,
                   bool  excited = false);

    // Draw eyebrow above eye
    // tilt: negative = inner corner lower (angry), positive = inner corner higher (sad)
    void _drawBrow(const Eye& e, int yOffset, int tilt);

    // Mouth helpers
    void _drawSmile(int cx, int cy, int w, int depth);
    void _drawFrown(int cx, int cy, int w, int depth);
    void _drawFlat (int cx, int cy, int w);
    void _drawOvalMouth(int cx, int cy, int w, int h);

    // Render to buffer and flush
    void _render();

    unsigned long _nextBlink();
};
