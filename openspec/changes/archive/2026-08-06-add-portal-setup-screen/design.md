## Context

`portal::run()` is called from Arduino's `setup()` on core 1, before `bsp::init()` and before any FreeRTOS tasks are spawned. Normally LVGL is initialized inside `ui_task` (core 1) after provisioning; in portal mode `ui_task` never runs.

## Goals / Non-Goals

**Goals:**
- Drive the display during portal mode with a minimal static LVGL screen
- Satisfy the all-`lv_*`-on-core-1 constraint without spawning any tasks

**Non-Goals:**
- Routing or interaction on the portal screen (it is static)
- Reusing any of the normal screen/layout infrastructure

## Decisions

### D1: Move `bsp::init()` before the provisioning check

`bsp::init()` currently runs after the portal guard. Moving it before is safe — it only initializes I2C, PCF8574, LCD power/reset, event queue, and backlight; none of these depend on FreeRTOS tasks being up. This gives the portal path access to the hardware before branching.

Alternative considered: keep `bsp::init()` in its original position and call it again from inside `portal::run()`. Rejected because `bsp::init()` creates a FreeRTOS queue and attaches an LEDC channel — calling it twice would leak the first queue handle and re-initialize LEDC unnecessarily.

### D2: Portal initializes LVGL inline before entering its event loop

`portal::run()` calls `lv_init()`, then `bsp::display_init()`, builds a minimal two-label screen, enables the backlight, and then enters its existing `for(;;)` event loop. No separate task or function boundary is needed.

This is correct because:
- Arduino `setup()` runs on core 1 — the same core that owns LVGL in normal operation.
- `portal::run()` never returns (it calls `ESP.restart()`), so `lv_init()` and `bsp::display_init()` are called exactly once per boot regardless of path taken.
- No other task is running at this point, so the LVGL mutex constraint is trivially satisfied.

Alternative considered: move LVGL init into `main.cpp` before `portal::run()` so the portal just builds its screen. Rejected as unnecessary layering — the portal is already a self-contained blocking function; calling `lv_init()` inside it keeps the boot sequence in one place.

### D3: Portal event loop drives LVGL with `lv_timer_handler()`

The existing `for(;;)` loop calls `s_dns.processNextRequest()` and `s_server.handleClient()`. Adding `lv_timer_handler()` after both gives LVGL the time slices it needs to flush the screen. A `delay(5)` (or `vTaskDelay`) at the end keeps the loop from busy-spinning.

### D4: Safe area via LVGL percentage positioning

The two labels are centered horizontally and placed at ~35% and ~52% of the display height. For a 480×480 round panel the inscribed circle has radius 240; those vertical positions keep both labels well within the safe disc. No raw pixel coordinates are used.

## Risks / Trade-offs

- [Risk] `bsp::display_init()` allocates two PSRAM draw buffers (≈46 KB each). On a fresh boot with no prior allocations this is fine; on a rapid-restart loop the heap resets with `ESP.restart()`, so there is no accumulation risk.
- [Trade-off] The portal screen uses LVGL directly rather than the `src/ui/` layout system. That's acceptable because the portal is a one-screen dead-end; it never needs to be themed or routed.
