# Changelog — tara-robo

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

- Pocket lib dependency URLs updated to use `tara-pi.local` hostname instead of hardcoded IP
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
