## Why

The device currently shows the now-playing screen at all times, even when nothing is playing. Leaving a bright display on indefinitely wastes power, causes OLED burn-in risk, and creates an odd UX when music has stopped or been paused for a long time. An idle state that dims the display (or shows a minimal clock/screensaver) when playback has been inactive for a few minutes, and wakes immediately when playback resumes or the screen is touched, matches the expected behavior of a standalone device.

The idle state also reduces unnecessary LVGL render work and SOAP poll frequency, which improves overall system responsiveness while idle.

## Behaviour Model

- **Active**: transport state is Playing → show now-playing screen normally, full backlight, normal 2 s poll interval
- **Idle trigger**: transport state is Paused, Stopped, or Unknown for 5 continuous minutes → enter idle
- **Idle state**: backlight off (or minimal), minimal or blank display content
- **Wake triggers**: transport state changes to Playing (detected by background polling) OR any touch event → return to now-playing, full backlight
- **Reduced poll interval**: while idle, poll every 10 s instead of every 2 s; boost back to normal immediately on wake

## What Changes

- Add an idle timer in the UI task: a 5-minute countdown that resets whenever `transport_state == Playing`. When it fires, the UI task dims the backlight to 0 (or minimum) and marks the screen as idle.
- In `now_playing_update()`: if the screen is idle and `transport_state == Playing`, wake the screen (restore backlight, clear idle flag, boost polls).
- In the UI task touch handler: any touch event while idle wakes the screen and resets the idle timer without forwarding the touch to LVGL (the first touch is a wake-only tap).
- Idle poll interval: expose a `playback::idle_mode(bool)` call. When entering idle, sets an atomic `s_idle` flag; `poll_interval_ms()` returns 10 000 ms when idle and normal interval otherwise. On wake, clears the flag and stores 4 boost cycles.
- The idle visual: backlight off via `bsp::backlight(0)`. No screensaver content needed for v1 — a blank screen is sufficient and avoids font/layout complexity.

## Capabilities

### Modified Capabilities

- `now-playing-screen` (DELTA): Adds idle-state behaviour — the screen enters an idle state after 5 minutes of non-Playing transport state and wakes on playback resumption or touch.

## Impact

- `src/main.cpp`: idle timer countdown in UI task; touch handler wake logic; `playback::idle_mode()` calls.
- `src/app/sonos/playback.h/.cpp`: `idle_mode(bool)` function; `poll_interval_ms()` respects idle flag (10 s when idle).
- `src/ui/screens/now_playing.h/.cpp`: `now_playing_update()` wakes on Playing from idle.
- No new files. No changes to BSP hardware other than `bsp::backlight()`.
