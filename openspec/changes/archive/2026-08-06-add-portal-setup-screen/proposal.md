## Why

During first-boot setup, the device screen is blank while the captive portal is active, leaving users with no visual feedback that anything is happening. A simple on-screen message confirming the AP name and next step eliminates confusion for anyone who picks up the device without prior knowledge.

## What Changes

- `portal::run()` initializes LVGL (minimally) before entering its event loop and renders a two-line setup screen: an "Initial Setup" title and a "Please connect to OpenSonos-Setup to start setup." instruction
- `main.cpp` calls `bsp::init()` before the `is_provisioned()` check so the display, touch, and LVGL subsystems are available to the portal path
- `lv_timer_handler()` is called inside the portal's `for(;;)` event loop to drive LVGL rendering during portal mode
- The portal screen is a minimal static layout — no touch input, no interaction, no routing — just the two lines of text, safe-area-compliant on the round panel

**Scope boundary:** This change is local firmware only — direct display output. No cloud, no Home Assistant, no Sonos API.

## Capabilities

### New Capabilities

_(none)_

### Modified Capabilities

- `device-provisioning`: Add requirement that the device displays a visual setup prompt on-screen while portal mode is active

## Impact

- `src/main.cpp`: Move `bsp::init()` call before the `is_provisioned()` check; ensure `bsp::init()` is idempotent or that only one path calls it
- `src/services/portal.cpp`: Add LVGL init and a minimal screen before the event loop; add `lv_timer_handler()` to the loop body
- `src/bsp/crowpanel_21_round/board.cpp`: `bsp::init()` must tolerate being called before any FreeRTOS tasks are spawned (currently true — it only initializes hardware peripherals)
- No new dependencies; uses LVGL already in the project
