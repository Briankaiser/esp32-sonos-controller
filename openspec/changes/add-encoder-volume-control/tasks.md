## 1. Prefs Constant

- [x] 1.1 In `src/services/prefs.h`, add `constexpr uint32_t kMuteShortPressMs = 1000` after the existing `kResetCountdownStartMs` and `kResetTotalMs` constants

## 2. PlaybackState and Command Extensions

- [x] 2.1 In `src/app/sonos/playback.h`, add `uint8_t volume = 50; bool muted = false;` fields to `PlaybackState` (after `version`)
- [x] 2.2 In `src/app/sonos/playback.h`, add `ToggleMute` to the `PlaybackCommand` enum
- [x] 2.3 In `src/app/sonos/playback.h`, add `void enqueue_volume_delta(int16_t delta)` to the `sonos::playback` namespace declaration

## 3. Playback Implementation

- [x] 3.1 In `src/app/sonos/playback.cpp`, add `#include <atomic>` and declare `static std::atomic<int16_t> s_pending_delta{0}` at file scope
- [x] 3.2 Implement `sonos::playback::enqueue_volume_delta(int16_t delta)` in `playback.cpp`: `s_pending_delta.fetch_add(delta, std::memory_order_relaxed)`
- [x] 3.3 In `playback::poll(const SpeakerInfo& speaker)`, after the existing `GetPositionInfo` call, add:
  - Call `sonos::soap::call(speaker.ip, speaker.port, "/MediaRenderer/RenderingControl/Control", "urn:schemas-upnp-org:service:RenderingControl:1", "GetVolume", "<InstanceID>0</InstanceID><Channel>Master</Channel>")`; parse `<CurrentVolume>` and store as `uint8_t` (clamp to 0–100); on failure log `[Playback] GetVolume failed` and keep previous value
  - Call `sonos::soap::call(...)` for `GetMute` with same args; parse `<CurrentMute>` (`"1"` → true, `"0"` → false); on failure log and keep previous
  - Set `next.volume` and `next.muted` before calling `update()`
- [x] 3.4 In `playback::drain_commands()`, add handling for `PlaybackCommand::ToggleMute`:
  - action = `"SetMute"`, args = `"<InstanceID>0</InstanceID><Channel>Master</Channel><DesiredMute>" + String(!current_mute ? 1 : 0) + "</DesiredMute>"` where `current_mute` is read from `snapshot().muted`; log `[Playback] command ToggleMute`
- [x] 3.5 In `playback::drain_commands()`, after draining the command queue, handle pending volume delta:
  - `int16_t delta = s_pending_delta.exchange(0, std::memory_order_relaxed)`
  - If `delta != 0`: read `PlaybackState cur = snapshot(); int new_vol = (int)cur.volume + delta; if (new_vol < 0) new_vol = 0; if (new_vol > 100) new_vol = 100;`
  - Call `sonos::soap::call(speaker.ip, speaker.port, "/MediaRenderer/RenderingControl/Control", "urn:schemas-upnp-org:service:RenderingControl:1", "SetVolume", "<InstanceID>0</InstanceID><Channel>Master</Channel><DesiredVolume>" + String(new_vol) + "</DesiredVolume>")` 
  - Log `[Playback] SetVolume %d\n`, new_vol; log failure if SOAP returns empty

## 4. Main Wiring

- [x] 4.1 In `main.cpp`, in the UI task input event loop under `case bsp::Intent::VolumeDelta:` (currently falls through `default`), add: `sonos::playback::enqueue_volume_delta(evt.value);` — add this as an explicit case before `default`
- [x] 4.2 In `main.cpp`, in the `case bsp::Intent::EncoderButtonUp:` handler, after deleting the reset timer and overlay, add: `if (millis() - s_btn_down_ms < prefs::kMuteShortPressMs) { sonos::playback::enqueue_command(sonos::PlaybackCommand::ToggleMute); }`

## 5. Now-Playing Screen

- [x] 5.1 In `src/ui/screens/now_playing.cpp`, add `static lv_obj_t* s_volume_arc = nullptr;` to the file-scope statics block
- [x] 5.2 In `now_playing_create()`, change the arc assignment from `lv_obj_t* arc = lv_arc_create(vol_cont)` to `s_volume_arc = lv_arc_create(vol_cont)` (and update all subsequent `arc` references in that block to `s_volume_arc`)
- [x] 5.3 In `now_playing_update(const sonos::PlaybackState& state)`, add after the existing progress bar update:
  - `if (s_volume_arc) { lv_arc_set_value(s_volume_arc, state.volume); lv_obj_set_style_arc_color(s_volume_arc, state.muted ? lv_color_hex(0x555555) : lv_color_hex(0x1DB954), LV_PART_INDICATOR); }`

## 6. Verification

- [x] 6.1 `pio run -e crowpanel_21_round` exits 0
- [x] 6.2 `pio run -e ref_rect_stub` exits 0
- [x] 6.3 `openspec validate --all --strict` is clean
- [x] 6.4 On hardware: rotate encoder clockwise — confirm speaker volume increases and arc advances; rotate counter-clockwise — confirm volume decreases
- [x] 6.5 On hardware: rotate encoder to min (volume 0) — confirm volume does not go below 0 and arc sits at minimum
- [x] 6.6 On hardware: short-press encoder button — confirm speaker mutes; arc turns gray; short-press again — confirm unmutes; arc returns to green
- [x] 6.7 On hardware: confirm that a long press (> 5 s) still shows the factory-reset countdown overlay and short press does NOT trigger the countdown
- [x] 6.8 On hardware: change volume in the Sonos app — confirm arc updates on the device within 2 s
- [x] 6.9 Add `CHANGELOG.md` entry under `## [Unreleased]`: "Rotary encoder now controls Sonos volume in real time; short-press the encoder to toggle mute. The volume arc reflects the current level and grays out when muted."
