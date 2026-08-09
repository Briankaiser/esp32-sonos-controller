## Context

`bsp::backlight(n)` already exists and accepts 0–255. Setting it to 0 turns the display off completely. The UI task owns LVGL and all display operations. `playback::poll_interval_ms()` already has a boost mechanism (atomic `s_boost_polls`); idle mode extends this with a parallel `s_idle` flag that overrides the base interval.

Touch events arrive as `bsp::InputEvent` items via `bsp::pop_event()` in the UI task event loop. All types (VolumeDelta, EncoderButtonDown/Up) pass through the same loop. CST816 touch coordinates arrive directly as LVGL input device events via `lv_timer_handler()`; they are not currently visible in the BSP event queue. Intercepting idle-wake touches requires checking in the LVGL input callback or using a pre-input filter.

## Goals / Non-Goals

**Goals:** display off after 5 min idle; instant wake on playback or touch; reduced poll frequency while idle.

**Non-Goals:** screensaver content (clock, weather) — deferred from original roadmap item 10; keep it as a follow-on; this change is the idle infrastructure only.

## Decisions

### D1: Idle timer as a simple millisecond counter in the UI task, reset on each Playing state update

**Choice:** `static uint32_t s_idle_timer_start = 0; static bool s_is_idle = false`. In `now_playing_update()`, if `state.transport_state == Playing` reset `s_idle_timer_start = millis()` and clear `s_is_idle`. The 1 s interpolation timer (`interp_tick`) additionally checks `millis() - s_idle_timer_start >= kIdleTimeoutMs` and enters idle if so.

**Why:** A single millis()-based timer requires no FreeRTOS timer objects, no new LVGL timers, and is reset naturally by the existing update path. `interp_tick` fires every 1 s so the idle trigger is checked at 1 s resolution — accurate enough for a 5-minute timeout.

### D2: Idle wake from playback via existing `now_playing_update()` path

**Choice:** In `now_playing_update()`, if `s_is_idle && state.transport_state == Playing`, call `wake_from_idle()` (inline: `bsp::backlight(200); s_is_idle = false; playback::idle_mode(false)`).

**Why:** `now_playing_update()` is already called whenever the playback version changes. A Playing state detected here is the natural wake trigger. No additional polling or signalling needed.

### D3: Idle wake from touch via a pre-LVGL intercept flag

**Choice:** Add `static bool s_wake_touch_consumed = false`. In the UI task event loop, before `lv_timer_handler()`, check if `s_is_idle` and if a touch is currently active (via `lv_indev_get_state(lv_indev_get_next(nullptr)) == LV_INDEV_STATE_PRESSED`). If so, wake and set `s_wake_touch_consumed = true`. In the touch read callback (`lvgl_touch_cb`), if `s_wake_touch_consumed`, report `LV_INDEV_STATE_RELEASED` for this frame to suppress the event, then clear the flag.

**Why:** LVGL processes touch through its input device system. Injecting a "released" state for the wake touch cleanly prevents the tap from opening overlays or triggering buttons, while still letting the physical release event clear naturally.

**Alternative considered:** Disable all LVGL event processing for one frame. Rejected because it could cause missed encoder events.

### D4: Idle poll interval via `s_idle` atomic in playback.cpp

**Choice:** Add `static std::atomic<bool> s_idle{false}` alongside `s_boost_polls`. `poll_interval_ms()` returns `kIdlePollIntervalMs = 10000` when `s_idle` is true and `s_boost_polls == 0`. `idle_mode(true/false)` sets/clears `s_idle`; on `idle_mode(false)`, also stores 4 boost cycles for immediate fast recovery.

**Why:** The same atomic pattern used by boost polls. No mutex needed. Sonos rarely changes state while idle (a speaker left paused stays paused), so 10 s polling is sufficient to detect external play events within a reasonable latency.
