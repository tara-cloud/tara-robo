#pragma once
#include <IDisplay.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>

class ST7735Display : public IDisplay {
public:
    explicit ST7735Display(Adafruit_ST7735* tft) : _tft(tft) {}

    bool begin()   override { return true; }
    void clear()   override { _tft->fillScreen(ST77XX_BLACK); }
    void show()    override {}

    int width()  const override { return _tft->width(); }
    int height() const override { return _tft->height(); }

    void fillScreen(bool w)                                            override { _tft->fillScreen(w ? ST77XX_WHITE : ST77XX_BLACK); }
    void drawPixel(int x, int y, bool w)                               override { _tft->drawPixel(x, y, c(w)); }
    void drawHLine(int x, int y, int len, bool w)                      override { _tft->drawFastHLine(x, y, len, c(w)); }
    void drawVLine(int x, int y, int len, bool w)                      override { _tft->drawFastVLine(x, y, len, c(w)); }
    void drawRect(int x, int y, int w, int h, bool col)                override { _tft->drawRect(x, y, w, h, c(col)); }
    void fillRect(int x, int y, int w, int h, bool col)                override { _tft->fillRect(x, y, w, h, c(col)); }
    void drawRoundRect(int x, int y, int w, int h, int r, bool col)    override { _tft->drawRoundRect(x, y, w, h, r, c(col)); }
    void fillRoundRect(int x, int y, int w, int h, int r, bool col)    override { _tft->fillRoundRect(x, y, w, h, r, c(col)); }
    void drawCircle(int x, int y, int r, bool col)                     override { _tft->drawCircle(x, y, r, c(col)); }
    void fillCircle(int x, int y, int r, bool col)                     override { _tft->fillCircle(x, y, r, c(col)); }
    void drawTriangle(int x0, int y0, int x1, int y1, int x2, int y2, bool col) override { _tft->drawTriangle(x0, y0, x1, y1, x2, y2, c(col)); }
    void fillTriangle(int x0, int y0, int x1, int y1, int x2, int y2, bool col) override { _tft->fillTriangle(x0, y0, x1, y1, x2, y2, c(col)); }

    void setTextSize(uint8_t s)   override { _tft->setTextSize(s); }
    void setTextColor(bool w)     override { _tft->setTextColor(c(w)); }
    void setCursor(int x, int y)  override { _tft->setCursor(x, y); }
    void print(const char* text)  override { _tft->print(text); }

private:
    Adafruit_ST7735* _tft;
    static uint16_t c(bool white) { return white ? ST77XX_WHITE : ST77XX_BLACK; }
};
