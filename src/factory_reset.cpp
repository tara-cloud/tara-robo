#include <Arduino.h>
#include <Preferences.h>
#include <nvs_flash.h>
#include <WiFi.h>

void setup() {
    Serial.begin(115200);
    Serial.println("\n[Factory Reset] Clearing all NVS...");

    // Erase WiFi credentials stored by the SDK
    WiFi.disconnect(true, true); // disconnect + erase credentials
    delay(200);

    // Clear our own NVS namespaces
    Preferences prefs;
    prefs.begin("tara-wifi",   false); prefs.clear(); prefs.end();
    prefs.begin("tara-device", false); prefs.clear(); prefs.end();

    // Erase entire NVS partition for a clean slate
    nvs_flash_erase();
    nvs_flash_init();

    Serial.println("[Factory Reset] Done — rebooting in 2s");
    delay(2000);
    ESP.restart();
}

void loop() {}
