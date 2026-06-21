// Touch sensor test — scans all valid touch pins simultaneously
// pio run -e test-touch -t upload
//
// Touch the IO wire/pad and watch which pin value DROPS.
// That pin is your sensor pin. Idle ~80-150, touched ~5-20.

#include <Arduino.h>

// All 10 ESP32 touch pins
static const int PINS[]  = {4,  0,  2,  15, 13, 12, 14, 27, 33, 32};
static const int NPIN    = 10;
static const int THRESHOLD = 30;

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n=== Touch Pin Scanner ===");
    Serial.println("Touch the pad — watch which value drops below 30\n");
    Serial.println("Pin:  T0   T1   T2   T3   T4   T5   T6   T7   T8   T9");
    Serial.println("GPIO:  4    0    2   15   13   12   14   27   33   32");
    Serial.println("-----------------------------------------------------------");
}

void loop() {
    char buf[160] = {};
    int pos = 0;
    bool anyTouched = false;

    for (int i = 0; i < NPIN; i++) {
        int v = (int)touchRead(PINS[i]);
        pos += snprintf(buf + pos, sizeof(buf) - pos, "%4d ", v);
        if (v < THRESHOLD && v > 0) anyTouched = true;
    }

    if (anyTouched) {
        strncat(buf, "  <<<", sizeof(buf) - strlen(buf) - 1);
        for (int i = 0; i < NPIN; i++) {
            int v = (int)touchRead(PINS[i]);
            if (v < THRESHOLD && v > 0) {
                char tmp[12];
                snprintf(tmp, sizeof(tmp), " T%d(GPIO%d)", i, PINS[i]);
                strncat(buf, tmp, sizeof(buf) - strlen(buf) - 1);
            }
        }
    }

    Serial.println(buf);
    delay(300);
}
