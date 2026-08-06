## 1. NVS Preferences Wrapper

- [x] 1.1 Create `src/services/prefs.h` declaring `prefs::is_provisioned()`, typed getters (`wifi_ssid`, `wifi_pass`, `home_room`, `lat`, `lon`, `ota_pass`), `store_provisioning(...)`, and `factory_reset()`; add `constexpr uint32_t kResetCountdownStartMs = 10000` and `kResetTotalMs = 30000`
- [x] 1.2 Implement `src/services/prefs.cpp` — open the `opensonos` NVS namespace, implement all getters with empty-string defaults, implement `store_provisioning` as a single begin/end Preferences block, implement `factory_reset` as `Preferences::clear()` + `ESP.restart()`
- [x] 1.3 Confirm `is_provisioned()` returns `false` on a clean NVS namespace (test by inspecting the condition: `wifi_ssid` key is non-empty)

## 2. Captive Portal

- [x] 2.1 Create `src/services/portal.h` declaring `portal::run()` (blocking, never returns — calls `ESP.restart()` on success)
- [x] 2.2 Implement `portal::run()` in `src/services/portal.cpp`: start `WiFi.softAP("OpenSonos-Setup")`, start `DNSServer` answering all queries with `192.168.4.1`, start `WebServer` on port 80, loop `processNextRequest()` + `handleClient()`
- [x] 2.3 Add `GET /` handler returning an HTML form with labeled fields for: WiFi SSID, WiFi password, Home Room UUID, Latitude, Longitude, OTA password
- [x] 2.4 Add `POST /save` handler: validate SSID and password are non-empty; on valid — call `prefs::store_provisioning(...)`, respond with a confirmation page, delay 2 s, call `ESP.restart()`; on invalid — respond with an error page listing missing fields, do not write NVS
- [x] 2.5 Add `GET /reset` handler calling `prefs::factory_reset()` for debug/test access

## 3. Factory Reset Trigger (Encoder Long-Press)

- [x] 3.1 Add `Intent::EncoderButtonDown` and `Intent::EncoderButtonUp` to `src/bsp/input.h`
- [x] 3.2 Update `src/bsp/crowpanel_21_round/board.cpp` PCF8574 read loop to emit `EncoderButtonDown` on P5 falling edge and `EncoderButtonUp` on rising edge (debounced)
- [x] 3.3 In the UI task's input handler, on `EncoderButtonDown` record the press start timestamp; on each LVGL 1-second timer tick while held, check elapsed time against `kResetCountdownStartMs` and `kResetTotalMs`
- [x] 3.4 At 10 s of continuous hold: create a full-screen semi-transparent LVGL overlay with a centered label showing remaining seconds (e.g., "Hold to reset: 20 s")
- [x] 3.5 Each second while held past 10 s, update the label with the remaining seconds
- [x] 3.6 On `EncoderButtonUp` before 30 s: delete the overlay and cancel the countdown — no NVS changes
- [x] 3.7 At 30 s of continuous hold: call `prefs::factory_reset()` (clears NVS, calls `ESP.restart()`)

## 4. Boot-Path Integration

- [x] 4.1 Update `main.cpp::setup()` to call `prefs::is_provisioned()` as the first operation, before any BSP, LVGL, or FreeRTOS initialization
- [x] 4.2 If not provisioned, call `portal::run()` — execution diverges here; normal init is unreachable
- [x] 4.3 After the provisioned path diverges, confirm serial output distinguishes the two modes (e.g., `"[boot] provisioned, starting normal init"` vs `"[boot] no credentials, starting portal"`)

## 5. Verification

- [x] 5.1 `pio run -e crowpanel_21_round` exits 0
- [x] 5.2 `pio run -e ref_rect_stub` exits 0
- [x] 5.3 `openspec validate --all --strict` is clean
- [x] 5.4 On hardware: flash to CrowPanel; verify `OpenSonos-Setup` WiFi network appears within 5 s of boot (requires physical device)
- [x] 5.5 On hardware: connect a phone to `OpenSonos-Setup`; tap the captive-portal prompt (or browse to any URL) and confirm the setup form loads (requires physical device)
- [x] 5.6 On hardware: submit the form with valid WiFi credentials; confirm the device reboots and connects to the home network without re-entering the portal (requires physical device)
- [x] 5.7 On hardware: reboot the already-provisioned device; confirm `OpenSonos-Setup` does NOT appear and the serial log shows `[boot] provisioned` (requires physical device)
- [x] 5.8 On hardware: on a provisioned device, hold the encoder button for 10 s and confirm the countdown overlay appears (requires physical device)
- [x] 5.9 On hardware: release the encoder button at ~20 s and confirm the overlay dismisses with no factory reset (requires physical device)
- [x] 5.10 On hardware: hold the encoder button for the full 30 s and confirm the device reboots into portal mode (requires physical device)
- [x] 5.11 Add `CHANGELOG.md` entry under `## [Unreleased]`: "First-boot captive portal configures WiFi, home room, and weather location without credentials in firmware. Long-press the rotary encoder for 30 seconds to factory reset."
