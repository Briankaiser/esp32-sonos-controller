## 1. Idle Poll Interval

- [ ] 1.1 In `src/app/sonos/playback.h`, add `void idle_mode(bool idle)` to the `playback` namespace; add `constexpr uint32_t kIdlePollIntervalMs = 10000`
- [ ] 1.2 In `src/app/sonos/playback.cpp`, add `static std::atomic<bool> s_idle{false}`
- [ ] 1.3 Implement `playback::idle_mode(bool idle)`: set `s_idle.store(idle)`; if `!idle`, store 4 boost cycles: `s_boost_polls.store(4)`
- [ ] 1.4 In `poll_interval_ms()`, return `kIdlePollIntervalMs` when `s_idle.load() && s_boost_polls.load() == 0`; otherwise return existing logic unchanged

## 2. Idle State in now_playing.cpp

- [ ] 2.1 In `src/ui/screens/now_playing.cpp`, add statics:
  - `static uint32_t s_idle_timer_start = 0`
  - `static bool s_is_idle = false`
  - `static bool s_wake_touch_consumed = false`
  - `constexpr uint32_t kIdleTimeoutMs = 5 * 60 * 1000u`
- [ ] 2.2 In `now_playing_create()`, initialise `s_idle_timer_start = millis()` just before `return scr`
- [ ] 2.3 In `interp_tick` (the 1 s LVGL timer callback), add idle check at the top:
  ```cpp
  if (!s_is_idle && millis() - s_idle_timer_start >= kIdleTimeoutMs) {
      s_is_idle = true;
      sonos::playback::idle_mode(true);
      bsp::backlight(0);
  }
  ```
- [ ] 2.4 In `now_playing_update()`, add at the very beginning (before the loading overlay guard):
  ```cpp
  // Reset idle timer while playing; wake from idle when playback resumes
  if (state.transport_state == sonos::TransportState::Playing) {
      s_idle_timer_start = millis();
      if (s_is_idle) {
          s_is_idle = false;
          sonos::playback::idle_mode(false);
          bsp::backlight(200);
      }
  }
  ```
- [ ] 2.5 Add `void screens::now_playing_get_idle_state(bool& is_idle, bool& consume_touch)` or expose via simple accessors. Simpler: expose `bool screens::now_playing_is_idle()` and `void screens::now_playing_consume_wake_touch()` in `now_playing.h`

## 3. Touch Wake Intercept in main.cpp

- [ ] 3.1 In the UI task event loop in `src/main.cpp`, before `lv_timer_handler()`, add:
  ```cpp
  // If idle and a touch is active, wake the display; suppress the event for this frame
  if (screens::now_playing_is_idle()) {
      lv_indev_t* indev = lv_indev_get_next(nullptr);
      if (indev && lv_indev_get_state(indev) == LV_INDEV_STATE_PRESSED) {
          screens::now_playing_wake_from_touch();  // wakes display, sets consume flag
      }
  }
  ```
- [ ] 3.2 In `bsp::input_init()`'s touch callback (`lvgl_touch_cb` in `board.cpp`), add at the top of the function body:
  ```cpp
  if (screens::now_playing_consume_wake_touch()) {
      data->state = LV_INDEV_STATE_RELEASED;
      return;
  }
  ```
  — where `now_playing_consume_wake_touch()` returns `true` and clears the flag if `s_wake_touch_consumed` is set

## 4. Public Interface in now_playing.h

- [ ] 4.1 In `src/ui/screens/now_playing.h`, add:
  - `bool now_playing_is_idle()`
  - `void now_playing_wake_from_touch()`  — wakes display + sets s_wake_touch_consumed
  - `bool now_playing_consume_wake_touch()` — returns and clears s_wake_touch_consumed

## 5. Implement now_playing interface functions

- [ ] 5.1 In `now_playing.cpp`, implement:
  - `bool screens::now_playing_is_idle() { return s_is_idle; }`
  - `void screens::now_playing_wake_from_touch()`:
    ```cpp
    if (s_is_idle) {
        s_is_idle = false;
        s_idle_timer_start = millis();
        sonos::playback::idle_mode(false);
        bsp::backlight(200);
        s_wake_touch_consumed = true;
    }
    ```
  - `bool screens::now_playing_consume_wake_touch()`:
    ```cpp
    if (s_wake_touch_consumed) { s_wake_touch_consumed = false; return true; }
    return false;
    ```

## 6. Verification

- [ ] 6.1 `pio run -e crowpanel_21_round` exits 0
- [ ] 6.2 `pio run -e ref_rect_stub` exits 0
- [ ] 6.3 On hardware: play a track, pause it, wait 5 minutes — confirm display turns off
- [ ] 6.4 On hardware: while display is off, resume playback from Sonos app — confirm display wakes within ~10 s and shows the track
- [ ] 6.5 On hardware: while display is off, tap the screen — confirm display wakes without triggering any button or overlay
- [ ] 6.6 On hardware: confirm that while idle, `[Playback]` serial logs appear approximately every 10 s (reduced from 2 s)
- [ ] 6.7 Add `CHANGELOG.md` entry under `## [Unreleased]`: "Display turns off after 5 minutes of inactivity; wakes on playback resumption or touch. Background polling reduces to 10 s while idle."
