# Changelog — tara-robo

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
