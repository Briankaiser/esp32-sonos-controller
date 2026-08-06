## 1. Boot Sequence

- [x] 1.1 In `src/main.cpp`, move the `bsp::init()` call to before the `if (!prefs::is_provisioned())` guard so hardware is initialized on both the portal and normal boot paths

## 2. Portal Setup Screen

- [x] 2.1 In `src/services/portal.cpp`, add includes for `bsp.h` and `<lvgl.h>`
- [x] 2.2 At the start of `portal::run()`, before WiFi and DNS setup, call `lv_init()` followed by `bsp::display_init()` to bring up LVGL and the display
- [x] 2.3 After `lv_init()` / `bsp::display_init()`, create a minimal LVGL screen: a root `lv_obj_t*` set to full display size, a title label ("Initial Setup") centered horizontally at ~35% vertical, and a body label ("Please connect to OpenSonos-Setup to start setup.") centered horizontally at ~52% vertical with word-wrap enabled; use percentage-based positioning so no raw pixel coordinates appear
- [x] 2.4 After building the screen, enable the backlight at full brightness via `bsp::backlight(255)`
- [x] 2.5 In the portal's `for(;;)` event loop, add `lv_timer_handler()` followed by `delay(5)` after the existing DNS and web server calls so LVGL gets CPU time to flush the screen

## 3. Verification

- [x] 3.1 `pio run -e crowpanel_21_round` exits 0
- [x] 3.2 `pio run -e ref_rect_stub` exits 0
- [x] 3.3 `openspec validate --all --strict` is clean
- [x] 3.4 On hardware: factory reset the device; confirm the screen displays "Initial Setup" and the connection instruction before any phone connects to the AP
- [x] 3.5 Add a CHANGELOG.md entry under `## [Unreleased]` noting that the device now shows a setup screen during first-boot portal mode
