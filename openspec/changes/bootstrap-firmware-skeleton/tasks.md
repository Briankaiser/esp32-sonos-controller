## 1. Project scaffolding

- [x] 1.1 Create `platformio.ini` with a `[common]` base and two envs, `crowpanel_21_round` and `ref_rect_stub`, each setting `build_src_filter` and a board include path
- [x] 1.2 Pin LVGL 9.x, `GFX Library for Arduino`, and the CST816 and GT911 touch libraries as `lib_deps`
- [x] 1.3 Add `lib/lv_conf.h` configured for RGB565, PSRAM draw buffers, direct render mode, and partial invalidation
- [x] 1.4 Add `.gitignore` for `.pio/`, build output, and editor cruft
- [x] 1.5 Confirm both envs compile an empty `app_main` before any board code exists

## 2. Board support package contract

- [x] 2.1 Define `src/bsp/display_profile.h` with `Shape` and the `constexpr DisplayProfile` struct
- [x] 2.2 Define `src/bsp/input.h` with the `Intent` enum and `InputEvent` struct
- [x] 2.3 Define `src/bsp/bsp.h` free-function contract, plus the `#error` guard that fires when no board is selected and names the available board identifiers
- [x] 2.4 Verify the no-board-selected diagnostic by building with the board include path removed

## 3. Round board implementation

- [x] 3.1 Add `src/bsp/crowpanel_21_round/board_config.h` with the display profile and pin constants
- [x] 3.2 Implement ST7701S RGB panel bring-up and a `static` flush callback in `board.cpp`
- [x] 3.3 Allocate the framebuffer and draw buffers in PSRAM; report allocation failure over serial rather than rendering partially
- [x] 3.4 Implement backlight control on GPIO6
- [x] 3.5 Wire CST816 touch over I2C as an LVGL pointer input device
- [x] 3.6 **Confirm the encoder press-button GPIO** against the Elecrow wiki or by probing, then record it in `board_config.h`
  - Confirmed: button is on PCF8574 P5 (I²C expander at 0x21), NOT a direct GPIO
- [x] 3.7 Implement the encoder: trivial ISR incrementing a delta counter, drained outside interrupt context into volume-change intents
- [x] 3.8 Register the encoder as an LVGL encoder input device with group navigation

## 4. Rectangular reference board

- [x] 4.1 Add `src/bsp/ref_rect_stub/board_config.h` declaring 800x480, rectangular, no encoder
- [x] 4.2 Implement `board.cpp` against the same contract, with GT911 touch
- [x] 4.3 Confirm the env builds and that the image contains no encoder code path

## 5. Layout system

- [x] 5.1 Define `SlotId`, `LayoutSlot`, and the proportional helper in `src/ui/layout/layout.h`
- [x] 5.2 Implement `archetype_round_compact.h` with the full `constexpr` slot table
- [x] 5.3 Implement `archetype_landscape_wide.h` with the full `constexpr` slot table
- [x] 5.4 Add a compile-time check that every archetype defines all `SLOT_COUNT` slots
- [x] 5.5 Implement `safe_area.h`: the `constexpr` disc-containment predicate, the fold over the slot table, and the `static_assert` whose diagnostic names the offending slot
- [x] 5.6 Select the archetype from the active profile's shape and aspect
- [x] 5.7 Implement `layout_container()` applying position, size, and clipping once at build time
- [ ] 5.8 Make the root object a clipped circle on round profiles

## 6. Theme and type

- [x] 6.1 Define `FontRole` and the role-to-face mapping in `src/ui/theme/fonts.h`
- [x] 6.2 Generate a subset face set for the medium density class, Latin-1 plus the icons actually used
  - Bootstrap uses LVGL built-in Montserrat faces; custom subset fonts deferred to add-now-playing-playback
- [x] 6.3 Add the `static_assert` over the role table that catches a density class missing a role
- [x] 6.4 Implement the accent colour token in `src/ui/theme/accent.cpp` with animated transition and a legibility floor against the background

## 7. Runtime

- [x] 7.1 Create the interface task pinned to core 1 owning every `lv_*` call
- [x] 7.2 Create the core 0 worker task and the queue carrying results back to the interface task
- [x] 7.3 Add the LVGL tick source
- [x] 7.4 Route drained input events into intents delivered to the active screen, discarding intents aimed at a torn-down screen

## 8. Placeholder screen and diagnostics

- [x] 8.1 Build a placeholder root screen composed entirely from named slots, with no pixel literals
- [x] 8.2 Implement the frame-rate and free-memory overlay behind a build flag, absent from images built without it
- [x] 8.3 Add a synthetic load path that exercises the worker queue, to observe frame rate under contention

## 9. Continuous integration

- [x] 9.1 Add `.github/workflows/ci.yml` building both envs
- [x] 9.2 Add the lint rejecting numeric literals in positioning and sizing calls under `src/ui/screens/`
- [x] 9.3 Add `openspec validate --all --strict` to CI
- [x] 9.4 Add `CHANGELOG.md` in Keep a Changelog format with an `## [Unreleased]` section

## 10. Verification

- [x] 10.1 Both envs build clean: `pio run -e crowpanel_21_round` and `pio run -e ref_rect_stub`
- [x] 10.2 `openspec validate --all --strict` is clean
- [ ] 10.3 On hardware: the placeholder screen renders, touch registers, encoder rotation and press register
- [ ] 10.4 Overlay reports at least 55 fps sustained, including under synthetic worker load
- [x] 10.5 **Negative test:** move a slot into a dead corner of the round archetype and confirm the build fails naming that slot, then revert
- [x] 10.6 **Negative test:** add a pixel literal to the placeholder screen and confirm the CI lint rejects it, then revert
- [ ] 10.7 Confirm nothing renders in the round panel's dead corners at run time, including with an over-long track title
- [x] 10.8 Add the `CHANGELOG.md` entry under `## [Unreleased]`
