# Changelog — tara-robo

## [3.0.0] — 2026-06-28

### Changed

- **Breaking:** replaced MQTT (ota4h + config4h + health_check) with direct
  TCP socket via new `lib/socket4h/` library
- Captive portal: removed `servicePort` and `mqttPort` fields; added `port`
  (TCP socket port, default 3001); kept `ssid`, `password`, `projectId`, `host`
- `lib/socket4h/`: new TCP client — connect, send/receive JSON lines, reconnect
- `lib/TaraCore/src/TaraCore.cpp`: registration now sends `register` message
  via socket instead of HTTP POST
- `src/main.cpp`: socket connect + handlers for `config`, `display`, `emotion`,
  `speech`, `ota`; heartbeat every 30s, health every 60s
- `src/device.cpp`: `applyRobotConfig()` → `applySocketConfig(const JsonDocument&)`
- `platformio.ini`: removed `ota4h`, `config4h`, `health-check`, `PubSubClient`,
  `face`, `display4h`, `tara-face`

---

## [2.2.1] — 2026-06-26

### Fixed

- `src/main.cpp`: drive GPIO16 `HIGH` as first line of `setup()` — prevents
  BL floating low during serial/log init before `setupDeviceHardware()` runs
- `src/device.cpp`: moved `pinMode(TFT_BL, OUTPUT)` + `digitalWrite HIGH`
  to top of `setupDeviceHardware()`, before component registration and touch init

---

## [2.2.0] — 2026-06-26

### Added

- `src/device.cpp`: backlight control via GPIO 16 (`TFT_BL = 16`)
  - `pinMode(TFT_BL, OUTPUT)` + `digitalWrite(TFT_BL, HIGH)` in `setupDeviceHardware()`
  - `setBacklight(bool on)` function for software on/off control
- `TaraCore.h`: added `void setBacklight(bool on)` declaration

---

## [2.1.0] — 2026-06-26

### Changed

- `src/device.cpp`: reverted display driver from TFT_eSPI back to
  **Adafruit ST7735** (working); `renderEye()` now uses
  `tft.drawRGBBitmap()` — no sprite allocator needed
- `src/Eye.cpp`: populated with full 38400-byte RGB565 pixel data
  extracted from `lib/face/Eye.c`
- `platformio.ini`: swapped `bodmer/TFT_eSPI` back to
  `adafruit/Adafruit ST7735 and ST7789 Library@^1.10.4`
- Removed `include/User_Setup.h` (TFT_eSPI pin config, no longer needed)

---

## [2.0.1] — 2026-06-26

### Changed

- `.gitignore`: added `lib/face/` — raw LVGL sprite source no longer tracked
  (data is compiled into `src/Eye.cpp`)

---

## [2.0.0] — 2026-06-26

### Changed

- Display driver migrated from Adafruit ST7735 to **TFT_eSPI** + `TFT_eSprite`
- `renderFace(toFaceState(currentState))` replaced with `renderEye()` —
  renders a raw RGB565 bitmap sprite directly to the TFT
- `src/Eye.cpp` + `src/Eye.h`: static 160×120 RGB565 eye image data
- `include/User_Setup.h`: TFT_eSPI pin config for ST7735 V1.1
  (SCK=18, MOSI=23, CS=5, DC=2, RST=4)
- `platformio.ini`: swapped `Adafruit ST7735 and ST7789 Library` →
  `bodmer/TFT_eSPI@^2.5.43`
- `TaraCore.h`: added `renderEye()` declaration; removed face lib deps
- `main.cpp`: removed `face.h` include; loop calls `renderEye()`

---

## [1.9.3] — 2026-06-25

### Fixed

- `platformio.ini`: `DEVICE_TYPE` corrected to `tara-robo` (was `robot`)
- `platformio.ini`: `FW_VERSION` default corrected to `1.8.5` (was `3.5.0`)

---

## [1.9.2] — 2026-06-25

### Fixed

- `main.cpp`: moved `registerRobot()` before `ota4h_init` and `config4h_init`
  so `projectId` is resolved before MQTT clients subscribe — fixes stuck
  `STATE_WAITING_CONFIG` on first boot
- `device.cpp`: added `ST7735Display` adapter and `TaraFace` instance;
  `taraFace.begin()` now called in `setupDeviceHardware()` — fixes blank
  screen after registration (face renderer was never registered)
- `lib/ST7735Display`: new local adapter implementing `IDisplay` for
  Adafruit ST7735, bridging tara-face to the TFT driver

---

## [1.9.1] — 2026-06-25

### Changed

- `src/`: removed `face_test.cpp`, `factory_reset.cpp`, `touch_test.cpp` — only `main.cpp` and `device.cpp` remain
- `platformio.ini`: removed `factory-reset`, `test-touch`, `test-face`, `expressions-demo` build envs and `build_src_filter` exclusion list

---

## [1.9.0] — 2026-06-25

### Changed

- Display migrated from SH1106 128×64 OLED (I2C) to ST7735 128×160 V1.1 TFT (SPI)
  - Pins: SCK=18, MOSI=23, CS=5, DC=2, RST=4
  - `device.cpp`: replaced U8g2 driver with Adafruit ST7735;
    all draw commands, boot screen, and face rendering ported
  - `face_test.cpp`: rewritten for ST7735; love face renders in red
- `platformio.ini`: replaced `U8g2` + `Adafruit SSD1306` with
  `Adafruit ST7735 and ST7789 Library`; removed `test-oled` env
- All `tara-pi.local` references updated to `pi-nest.local`

---

## [1.8.5] — 2026-06-25

### Fixed

- `release.yml`: replace hardcoded Pocket host with `POCKET_BASE_URL` org secret
- `release.yml`: change upload method from `POST` to `PUT` (fixes HTTP 405)

---

## [1.8.4] — 2026-06-25

### Fixed

- `release.yml`: add `permissions: contents: write` so `GITHUB_TOKEN` can create releases
- `release.yml`: merge `publish` job into release workflow — compile, package, and upload to Pocket runs as a second job after release creation; skipped automatically if release already exists

---

## [1.8.3] — 2026-06-24

### Added

- `ci.yml`: compile-check workflow on every PR and non-main branch push
  - Injects version from `VERSION` file into `FW_VERSION` build flag
  - Compiles `robot` env — must pass before PR can be merged
- `release.yml`: auto-create GitHub release on every merge to `main`
  - Tags release as `v{VERSION}` from the `VERSION` file
  - Idempotent — skips if tag already exists
- `publish.yml`: build and publish firmware to Pocket on release
  - Checks out exact release tag, compiles `robot` env
  - Packages firmware as `tara-robo-{VERSION}.bin`
  - Uploads to Pocket via `POCKET_API_KEY` org-level secret

### Changed

- Pocket lib dependency URLs updated to use `pi-nest.local` hostname instead of hardcoded IP
- Pocket lib dependency URLs no longer require API token (public download enabled)

---

## [1.5.0] — 2026-06-19

### Added

- OTA MQTT client starts after `registerRobot()` — subscribes to
  `{projectId}.{deviceName}.ota` (QoS 1), loops in `main.cpp`
- `otaMqttLoop()` called every iteration to keep connection alive
- Captive portal redesigned: Host / Service Port / MQTT Port fields
  replace raw Server URL — `serverUrl` and `mqttHost` auto-constructed
- `loadWiFiConfig()` reconstructs `serverUrl`+`mqttHost` from stored host/port
- `STATE_WAITING_CONFIG` + `renderConfusedFace()` post-registration state
- Factory reset: `WiFi.disconnect(true, true)` erases SDK credentials
  before full NVS erase — fixes loop where WiFi reconnected after reset

### Changed

- `FW_VERSION` bumped to 1.5.0

---

## [1.4.0] — 2026-06-19

### Added

- Dynamic I2C component discovery at boot (`discoverComponents()`)
  - Scans bus addr 1–126 before peripheral init
  - Matches against known-device table (OLED, MPU6050, BME280, INA219, LCD)
  - Unknown addresses reported as `Unknown(0x??)` — nothing silently dropped
- `DiscoveredComponent` / `DiscoveredPin` runtime structs replace static compile-time descriptors
- `registerRobot()` sends nested `components[]{name, type, protocol, address, pins[]}` payload
- `Wire.end()` called after scan so U8g2 SW-I2C can take the bus cleanly

### Changed

- Removed hardcoded `deviceComponents[]` / `devicePins[]` from `device.cpp`
- `setupDeviceHardware()` now calls `discoverComponents(SDA, SCL)` before `u8g2.begin()`

### Internal

- Firmware moved to standalone repo `tara-cloud/tara-robo` (split from `tara-cloud/electro`)
- Added `.gitignore` to exclude `.pio/` build artifacts

---

## [1.3.0] — 2026-06-18

### Added

- Component-aware registration: `registerRobot()` sends nested `components[]{name, type, protocol, pins[]}` payload
- `ComponentPin` + `ComponentInfo` structs in `TaraCore.h`
- Hardcoded OLED component descriptor in `device.cpp` (replaced in 1.4.0)

---

## [1.2.0] — 2026-06-18

### Added

- Pin-aware registration: `registerRobot()` sends flat `pins[]` array
- `PinInfo` struct with `pin`, `label`, `direction`, `component`, `protocol`

---

## [1.1.0] — 2026-06-18

### Added

- Initial full firmware: WiFi, captive portal, MQTT, config, OTA
- SH1106 OLED via U8g2 SW-I2C (GPIO21/22)
- TaraExpressions idle face animation
- JSON draw-command engine for server-pushed faces
- Emotion, speech, display, OTA MQTT handlers

---

## [1.0.0] — 2026-06-18

### Added

- Initial project scaffold
