#pragma once
#include "IDisplay.h"
#include <Adafruit_SSD1306.h>

/**
 * SSD1306Display — IDisplay adapter for Adafruit_SSD1306.
 *
 * Swap this file for a different adapter to support SH1106, ST7735, etc.
 */
class SSD1306Display : public IDisplay {
public:
    /**
     * @param oled     Pointer to an already-constructed Adafruit_SSD1306
     * @param i2cAddr  I2C address (typically 0x3C or 0x3D)
     */
    explicit SSD1306Display(Adafruit_SSD1306* oled, uint8_t i2cAddr = 0x3C)
        : _oled(oled), _addr(i2cAddr) {}

    bool begin() override {
        return _oled->begin(SSD1306_SWITCHCAPVCC, _addr);
    }
    void clear()   override { _oled->clearDisplay(); }
    void show()    override { _oled->display(); }

    int  width()  const override { return _oled->width(); }
    int  height() const override { return _oled->height(); }

    void fillScreen(bool w)    override { _oled->fillScreen(w ? WHITE : BLACK); }
    void drawPixel(int x, int y, bool w) override { _oled->drawPixel(x, y, w ? WHITE : BLACK); }

    void drawHLine(int x, int y, int len, bool w) override { _oled->drawFastHLine(x, y, len, w ? WHITE : BLACK); }
    void drawVLine(int x, int y, int len, bool w) override { _oled->drawFastVLine(x, y, len, w ? WHITE : BLACK); }

    void drawRect(int x, int y, int pw, int ph, bool w) override { _oled->drawRect(x, y, pw, ph, w ? WHITE : BLACK); }
    void fillRect(int x, int y, int pw, int ph, bool w) override { _oled->fillRect(x, y, pw, ph, w ? WHITE : BLACK); }

    void drawRoundRect(int x, int y, int pw, int ph, int r, bool w) override { _oled->drawRoundRect(x, y, pw, ph, r, w ? WHITE : BLACK); }
    void fillRoundRect(int x, int y, int pw, int ph, int r, bool w) override { _oled->fillRoundRect(x, y, pw, ph, r, w ? WHITE : BLACK); }

    void drawCircle(int x, int y, int r, bool w) override { _oled->drawCircle(x, y, r, w ? WHITE : BLACK); }
    void fillCircle(int x, int y, int r, bool w) override { _oled->fillCircle(x, y, r, w ? WHITE : BLACK); }

    void drawTriangle(int x0, int y0, int x1, int y1, int x2, int y2, bool w) override {
        _oled->drawTriangle(x0, y0, x1, y1, x2, y2, w ? WHITE : BLACK);
    }
    void fillTriangle(int x0, int y0, int x1, int y1, int x2, int y2, bool w) override {
        _oled->fillTriangle(x0, y0, x1, y1, x2, y2, w ? WHITE : BLACK);
    }

    void setTextSize(uint8_t s)    override { _oled->setTextSize(s); }
    void setTextColor(bool w)      override { _oled->setTextColor(w ? WHITE : BLACK); }
    void setCursor(int x, int y)   override { _oled->setCursor(x, y); }
    void print(const char* t)      override { _oled->print(t); }

    void setContrast(uint8_t c)    override { _oled->ssd1306_command(SSD1306_SETCONTRAST); _oled->ssd1306_command(c); }

private:
    Adafruit_SSD1306* _oled;
    uint8_t           _addr;
};
