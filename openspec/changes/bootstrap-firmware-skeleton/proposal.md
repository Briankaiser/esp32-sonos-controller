## Why

The repository is empty, so every planned capability — Sonos control, the screens, OTA — is blocked on a firmware that actually boots and drives the CrowPanel 2.1" round panel. Committing to a display abstraction now, rather than retrofitting one after the round 480x480 board is hardcoded throughout the screen code, is what makes future displays of a different resolution, physical size, and shape viable at all.

## What Changes

- Add a PlatformIO project (Arduino framework, C++17, LVGL 9.x) with two build environments: `crowpanel_21_round` and `ref_rect_stub`.
- Introduce a board support package layout — one directory per board, one translation unit, selected by `build_src_filter` plus an include path. Each board exports exactly one `constexpr DisplayProfile` (resolution, physical size, PPI, shape, input capabilities) and concrete panel/touch/encoder bindings behind a free-function contract.
- Implement the round board for real: ST7701S RGB panel init, CST816 touch as an LVGL input device, rotary encoder as an LVGL encoder input device with group navigation.
- Implement `ref_rect_stub` as a compiling reference profile at 800x480 rectangular with GT911 touch and no encoder. It exists to prove the abstraction, and is not expected to be hardware-verified.
- Establish the layout system: named slots (`SLOT_ALBUM_ART`, `SLOT_TRANSPORT`, ...) resolved from `constexpr` layout archetypes, so screen code positions content without ever naming a pixel value or a board.
- Enforce the round display's safe area three ways: a compile-time `static_assert` that **fails the build** if any slot escapes the visible disc, runtime circular clipping, and a CI lint rejecting numeric literals in positioning calls under `src/ui/screens/`.
- Establish the runtime task model: LVGL pinned to core 1 owning all `lv_*` calls, I/O workers on core 0, communication by queue, LVGL draw buffers in PSRAM.
- Add a font-role system (`Display`/`Title`/`Body`/`Caption`/`Icon`) with per-density buckets, so screens never name a pixel size.
- Add a placeholder root screen plus a debug-flag FPS/heap overlay, so frame rate and input are measurable end to end from day one.
- Add `CHANGELOG.md`, and CI that builds both envs and runs `openspec validate --all --strict`.

No Sonos, network, persistence, or provisioning behavior lands in this change. Per project scope, control will be direct UPnP/SOAP over the LAN — this change deliberately adds no Home Assistant or Sonos cloud API surface, and nothing here should introduce one.

## Capabilities

### New Capabilities

- `hardware-abstraction`: The board support package contract — one `constexpr DisplayProfile` and panel/touch/encoder bindings per board, selected at compile time, so a new display is a new directory plus a new build env rather than an edit to screen code.
- `runtime-architecture`: Task and core partitioning, the queue-based hand-off between I/O and UI work, PSRAM draw buffer allocation, and the sustained frame-rate budget.
- `ui-foundation`: LVGL runtime initialization, layout archetypes and named slots, round-display safe-area enforcement, input-intent routing, and the font-role and accent-token theme system.

### Modified Capabilities

None — `openspec/specs/` is empty; this is the first change.

## Impact

- **New code**: `platformio.ini`, `src/main.cpp`, `src/bsp/**`, `src/ui/layout/**`, `src/ui/theme/**`, `lib/lv_conf.h`, `.github/workflows/ci.yml`
- **New dependencies**: LVGL 9.x, `GFX Library for Arduino` (ST7701S RGB), a CST816 touch library, a GT911 touch library for the stub board
- **Build surface**: two firmware images from here on; every later change must keep both envs compiling
- **Constraints inherited by all later work**: no pixel literals in screen code, no `lv_*` calls off the UI task, no blocking network calls on the UI task
- **Hardware unknown**: the encoder press-button GPIO is not documented in available sources and must be confirmed against the Elecrow wiki or by probing before the encoder module is finished
