#pragma once
#include <Arduino.h>

/**
 * IDisplay — display hardware abstraction layer.
 *
 * TaraExpressions draws only through this interface.
 * To support a new display: subclass IDisplay, implement every method,
 * pass a pointer to TaraExpressions. Nothing else changes.
 */
class IDisplay {
public:
    virtual ~IDisplay() = default;

    // Lifecycle
    virtual bool begin()    = 0;
    virtual void clear()    = 0;   // clear internal buffer
    virtual void show()     = 0;   // flush buffer to hardware

    // Canvas info
    virtual int width()  const = 0;
    virtual int height() const = 0;

    // Primitives (all coordinates in pixels, origin top-left)
    virtual void fillScreen(bool white) = 0;
    virtual void drawPixel(int x, int y, bool white) = 0;

    virtual void drawHLine(int x, int y, int w, bool white) = 0;
    virtual void drawVLine(int x, int y, int h, bool white) = 0;
    virtual void drawRect(int x, int y, int w, int h, bool white) = 0;
    virtual void fillRect(int x, int y, int w, int h, bool white) = 0;
    virtual void drawRoundRect(int x, int y, int w, int h, int r, bool white) = 0;
    virtual void fillRoundRect(int x, int y, int w, int h, int r, bool white) = 0;
    virtual void drawCircle(int x, int y, int r, bool white) = 0;
    virtual void fillCircle(int x, int y, int r, bool white) = 0;
    virtual void drawTriangle(int x0, int y0, int x1, int y1, int x2, int y2, bool white) = 0;
    virtual void fillTriangle(int x0, int y0, int x1, int y1, int x2, int y2, bool white) = 0;

    virtual void setTextSize(uint8_t s)          = 0;
    virtual void setTextColor(bool white)        = 0;
    virtual void setCursor(int x, int y)         = 0;
    virtual void print(const char* text)         = 0;

    // Optional: contrast / brightness (0–255, ignore if not supported)
    virtual void setContrast(uint8_t /*contrast*/) {}
};
