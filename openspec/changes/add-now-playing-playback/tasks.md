## 1. PlaybackState and Playback Namespace

- [x] 1.1 Create `src/app/sonos/playback.h` declaring:
  - `enum class TransportState { Unknown, Stopped, Playing, Paused, Transitioning }`
  - `struct PlaybackState { TransportState transport_state = TransportState::Unknown; String title, artist, album, track_uri; uint32_t duration_s = 0; uint32_t position_s = 0; uint32_t poll_timestamp_ms = 0; uint32_t version = 0; }`
  - `enum class PlaybackCommand : uint8_t { Play, Pause, Next, Previous }`
  - `constexpr uint32_t kPollIntervalMs = 2000`
  - `namespace sonos::playback`: `void init()`, `void update(const PlaybackState& next)` (sonos task only), `PlaybackState snapshot()` (any task), `void enqueue_command(PlaybackCommand cmd)` (UI task), `void drain_commands(const SpeakerInfo& speaker)` (sonos task), `void poll(const SpeakerInfo& speaker)` (sonos task)
- [x] 1.2 Create `src/app/sonos/playback.cpp` implementing the mutex-protected singleton and command queue:
  - `init()`: creates FreeRTOS mutex and a `QueueHandle_t s_cmd_queue = xQueueCreate(4, sizeof(uint8_t))`
  - `update()`: takes mutex, increments `state.version`, replaces stored state, releases mutex
  - `snapshot()`: takes mutex, copies state, releases mutex, returns copy
  - `enqueue_command()`: `xQueueSend(s_cmd_queue, &cmd, 0)` — zero-timeout; log a warning to serial if the queue was full

## 2. Playback Polling Implementation

- [x] 2.1 In `playback.cpp`, add a static helper `static uint32_t parse_time(const String& hmmss)` that converts a `"h:mm:ss"` string (e.g., `"0:03:42"`) to total seconds; returns 0 if the string does not match the pattern
- [x] 2.2 In `playback.cpp`, add a static helper `static String url_decode(const String& s)` that percent-decodes `%XX` sequences and replaces `+` with space; used to decode the `TrackMetaData` DIDL block
- [x] 2.3 In `playback.cpp`, add a static helper `static String extract_xml_text(const String& xml, const String& tag)` that returns the text content between the first `<tag>` and `</tag>` pair; returns empty string if not found
- [x] 2.4 Implement `playback::poll(const SpeakerInfo& speaker)` in `playback.cpp`:
  - Call `sonos::soap::call(speaker.ip, speaker.port, speaker.control_url_base + "AVTransport/Control", "urn:schemas-upnp-org:service:AVTransport:1", "GetTransportInfo", "<InstanceID>0</InstanceID>")`; if empty, log `[Playback] GetTransportInfo failed` and return without updating state
  - Parse `<CurrentTransportState>` from the response to set `TransportState` (map `"PLAYING"` → `Playing`, `"PAUSED_PLAYBACK"` → `Paused`, `"STOPPED"` → `Stopped`, `"TRANSITIONING"` → `Transitioning`, anything else → `Unknown`)
  - Call `sonos::soap::call(...)` for `GetPositionInfo` with the same InstanceID arg; if empty, log `[Playback] GetPositionInfo failed` and return
  - Parse `<TrackDuration>` and `<RelTime>` using `parse_time()`; parse `<TrackMetaData>` using `url_decode()` followed by `extract_xml_text()` for `dc:title`, `upnp:artist`, `upnp:album`
  - Build a new `PlaybackState`, set `poll_timestamp_ms = millis()`, call `playback::update()`
  - Log `[Playback] title=<title> artist=<artist> state=<state>` to serial

## 3. Transport Command Execution

- [x] 3.1 Implement `playback::drain_commands(const SpeakerInfo& speaker)` in `playback.cpp`:
  - Dequeue all pending `PlaybackCommand` items from `s_cmd_queue` with zero timeout
  - For each command, call `sonos::soap::call()` targeting `AVTransport/Control`, service `urn:schemas-upnp-org:service:AVTransport:1`:
    - `Play`: action `Play`, args `<InstanceID>0</InstanceID><Speed>1</Speed>`
    - `Pause`: action `Pause`, args `<InstanceID>0</InstanceID>`
    - `Next`: action `Next`, args `<InstanceID>0</InstanceID>`
    - `Previous`: action `Previous`, args `<InstanceID>0</InstanceID>`
  - Log `[Playback] command <name>` on each execution; log `[Playback] command <name> failed` if SOAP call returns empty

## 4. sonos_task Restructure

- [x] 4.1 In `main.cpp`, add `#include "app/sonos/playback.h"` and call `sonos::playback::init()` in `setup()` after `sonos::state::init()` and before the task spawn block
- [x] 4.2 Replace the `sonos_task` function body in `main.cpp` with:
  ```
  for (;;) {
      sonos::discovery::run_cycle();
      SonosState st = sonos::state::snapshot();
      uint32_t cycle_start_ms = millis();
      while (millis() - cycle_start_ms < sonos::discovery::kDiscoveryIntervalMs) {
          if (st.home_room_idx >= 0) {
              const SpeakerInfo& hr = st.speakers[st.home_room_idx];
              sonos::playback::drain_commands(hr);
              sonos::playback::poll(hr);
          } else {
              Serial.println("[Playback] no home room — skipping poll");
          }
          vTaskDelay(pdMS_TO_TICKS(sonos::playback::kPollIntervalMs));
          // Re-snapshot in case discovery updated state (home room just resolved)
          st = sonos::state::snapshot();
      }
  }
  ```

## 5. Now-Playing Screen

- [x] 5.1 Create `src/ui/screens/now_playing.h` declaring in `namespace screens`:
  - `lv_obj_t* now_playing_create()`
  - `void now_playing_update(const sonos::PlaybackState& state)`
- [x] 5.2 Create `src/ui/screens/now_playing.cpp` with the following structure:
  - Static handles: `lv_obj_t* s_title_lbl`, `s_artist_lbl`, `s_album_lbl`, `s_play_btn_lbl`, `s_progress_bar`; `lv_timer_t* s_interp_timer`; `sonos::PlaybackState s_last`
  - `now_playing_create()`:
    - Build root screen object with the same dark background, `LV_RADIUS_CIRCLE` clip, and `LV_OBJ_FLAG_SCROLLABLE` cleared as in `placeholder_create()`
    - `SLOT_ALBUM_ART`: dark `#333333` rounded square — identical to placeholder; art is added in `add-album-art-accent-theming`
    - `SLOT_TRACK_INFO`: create three stacked `lv_label_t` children using `LV_ALIGN_TOP_MID` / `LV_ALIGN_CENTER` / `LV_ALIGN_BOTTOM_MID` offsets from center; top label uses `FontRole::Title` for track title, middle uses `FontRole::Body` for artist, bottom uses `FontRole::Caption` for album; all labels set `LV_LABEL_LONG_DOT`; assign to `s_title_lbl`, `s_artist_lbl`, `s_album_lbl`; initial text `"—"` for all three
    - `SLOT_TRANSPORT`: create a horizontal flex row; add three `lv_btn_t` children (prev, play/pause, next); use `LV_SYMBOL_PREV`, `LV_SYMBOL_PLAY`, `LV_SYMBOL_NEXT` as label text; register `lv_obj_add_event_cb` with `LV_EVENT_CLICKED` for each; prev callback calls `sonos::playback::enqueue_command(PlaybackCommand::Previous)`, next calls `Next`, play/pause calls `Play` or `Pause` depending on `s_last.transport_state`; assign the play/pause label object to `s_play_btn_lbl`
    - `SLOT_PROGRESS`: create `lv_bar_t`, `lv_obj_set_size(bar, lv_pct(100), lv_pct(100))`, `lv_bar_set_value(bar, 0, LV_ANIM_OFF)`; accent color `lv_color_hex(0x1DB954)` on indicator; assign to `s_progress_bar`
    - `SLOT_VOLUME_ARC`: identical to placeholder (encoder arc or touch slider based on `kDisplay.has_encoder`) — visual only, no command wiring yet
    - `SLOT_STATUS`: small caption label showing `"Connecting…"` initially; assign to a static `s_status_lbl`
    - Register a 1 s LVGL timer: callback updates progress bar from interpolation when `s_last.transport_state == Playing`; assign to `s_interp_timer`
    - Return root screen object
  - `now_playing_update(const sonos::PlaybackState& state)`:
    - Store `state` into `s_last`
    - Update `s_title_lbl` text to `state.title.isEmpty() ? "—" : state.title.c_str()`
    - Update `s_artist_lbl` to `state.artist.isEmpty() ? "" : state.artist.c_str()`
    - Update `s_album_lbl` to `state.album.isEmpty() ? "" : state.album.c_str()`
    - Update `s_play_btn_lbl` symbol: `LV_SYMBOL_PAUSE` when Playing, `LV_SYMBOL_PLAY` otherwise
    - Update `s_progress_bar` value: `state.duration_s > 0 ? (state.position_s * 100 / state.duration_s) : 0`; range 0–100, `LV_ANIM_OFF`

## 6. Main Wiring

- [x] 6.1 In `main.cpp`, add `#include "ui/screens/now_playing.h"` and `#include "app/sonos/playback.h"`
- [x] 6.2 In `ui_task()`, replace the `screens::placeholder_create()` call with `screens::now_playing_create()`; update `s_active_scr` assignment accordingly
- [x] 6.3 Add a static `uint32_t s_playback_version = 0` in `main.cpp` (file scope, UI task only); in the UI task event loop, after the input drain block, call `sonos::PlaybackState pb = sonos::playback::snapshot(); if (pb.version != s_playback_version) { s_playback_version = pb.version; screens::now_playing_update(pb); }`

## 7. Verification

- [x] 7.1 `pio run -e crowpanel_21_round` exits 0
- [x] 7.2 `pio run -e ref_rect_stub` exits 0
- [x] 7.3 `openspec validate --all --strict` is clean
- [x] 7.4 On hardware: flash and confirm serial shows `[Playback] title=<title> artist=<artist> state=Playing` within 5 s of discovery completing (requires Sonos speakers on the LAN with a track playing)
- [x] 7.5 On hardware: confirm the now-playing screen shows the correct title, artist, and album
- [x] 7.6 On hardware: confirm the progress bar advances visibly over ~10 s without a poll
- [x] 7.7 On hardware: tap the play/pause button — confirm Sonos pauses; tap again — confirm it resumes
- [x] 7.8 On hardware: tap next and previous — confirm track changes and the screen updates
- [x] 7.9 On hardware: `curl http://<ip>/screenshot -o s.bmp` — attach screenshot showing the now-playing screen with a live track
- [x] 7.10 Add `CHANGELOG.md` entry under `## [Unreleased]`: "Now-playing screen displays current track title, artist, album, and a live progress bar; play/pause, next, and previous controls let you operate Sonos directly from the device."
