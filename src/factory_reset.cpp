#include <Arduino.h>
#include <Preferences.h>
#include <nvs_flash.h>

void setup() {
    Serial.begin(115200);
    Serial.println("\n[Factory Reset] Clearing all NVS...");

    // Clear individual namespaces
    Preferences prefs;
    prefs.begin("tara-wifi",   false); prefs.clear(); prefs.end();
    prefs.begin("tara-device", false); prefs.clear(); prefs.end();

    // Erase entire NVS partition for a truly clean slate
    nvs_flash_erase();
    nvs_flash_init();

    Serial.println("[Factory Reset] Done — rebooting in 2s");
    delay(2000);
    ESP.restart();
}

void loop() {}
