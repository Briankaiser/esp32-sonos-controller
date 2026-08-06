## Context

The sonos task currently runs a discovery-only loop: `run_cycle()` then `vTaskDelay(60 s)`. The UI task loads `placeholder_create()` and then idles through an event loop that drains LVGL timer events, input intents, and the placeholder worker queue — all of which currently do nothing useful. The placeholder queue (`s_worker_to_ui`, 64-byte items) exists but is never populated.

`sonos::soap::call()` is already implemented and handles the HTTP POST / SOAPAction / envelope mechanics. Both `GetTransportInfo` and `GetPositionInfo` are straightforward calls on `urn:schemas-upnp-org:service:AVTransport:1`. Track metadata arrives as a URL-encoded DIDL-Lite XML block in the `<TrackMetaData>` element of the `GetPositionInfo` response.

The existing placeholder screen demonstrates that every slot (`SLOT_ALBUM_ART`, `SLOT_TRACK_INFO`, `SLOT_TRANSPORT`, `SLOT_PROGRESS`, `SLOT_VOLUME_ARC`, `SLOT_STATUS`) is already wired into the round/rect layout archetypes. The now-playing screen reuses all six slots without adding new ones.

## Goals / Non-Goals

**Goals:**
- Produce a device that shows what is playing and responds to play/pause/next/prev
- Keep all Sonos network calls on core 0; the UI task touches only LVGL and the playback snapshot
- Keep `now_playing.cpp` free of pixel coordinates, board names, or service URLs — those belong in the sonos layer and the layout layer respectively

**Non-Goals:**
- Volume control (encoder wiring, `RenderingControl`) — that is `add-encoder-volume-control`
- Album art fetch, decode, or dominant-colour extraction — that is `add-album-art-accent-theming`
- Home room selection screen — still pending for the case where no home room UUID is stored
- Track position seeking (seek bar drag)
- UPnP NOTIFY subscriptions (polling is sufficient for v1, consistent with the established decision)

## Decisions

### D1: PlaybackState is shared via a mutex-protected snapshot, not the existing queue

**Choice:** Add `src/app/sonos/playback.h/.cpp` with a `PlaybackState` struct (containing `std::string` fields for title/artist/album), a FreeRTOS mutex, `update()` (sonos task only), and `snapshot()` (any task). The UI task polls `playback::snapshot()` on each event loop iteration and compares a monotonic version counter to detect changes.

**Why:** The existing `s_worker_to_ui` queue uses 64-byte fixed-size items. Passing variable-length strings through a fixed-size queue requires either truncation at copy time or a pointer that is valid across tasks (unsafe without a PSRAM ring buffer). The `SonosState` design — mutex + full copy on snapshot — solves the same problem correctly. Consistency with the existing pattern also means any future reader can use the same approach.

**Alternative considered:** Extend the existing queue with a new 256-byte item type for playback. Rejected: it adds two distinct item types to one queue (requiring a tag byte), complicates the drain loop, and still truncates long titles.

### D2: sonos_task restructured into a discovery outer loop with a polling inner loop

**Choice:** Replace the current `vTaskDelay(kDiscoveryIntervalMs)` with an inner loop that wakes every `kPollIntervalMs` (2 s), calls `playback::drain_commands()` then `playback::poll()`, and exits when `kDiscoveryIntervalMs` has elapsed since the discovery cycle started.

**Why:** The alternative — a separate `playback_task` — would require its own core 0 stack and its own access to the home room IP, which it would have to read from `sonos::state::snapshot()` on every iteration. A second core 0 task risks stack contention with the web task and adds FreeRTOS object overhead. Keeping playback inside `sonos_task` keeps the home room data access local and eliminates an extra mutex round-trip. The inner-loop structure is straightforward and testable.

**Risk:** If a single discovery cycle takes longer than 60 s (network congestion, many speakers), the inner polling loop never runs. Mitigation: log a warning if `run_cycle()` exceeds 15 s; 15 s is still well below 60 s.

### D3: Transport commands travel via a small FreeRTOS queue inside playback.cpp

**Choice:** `playback::init()` creates a queue of depth 4 holding `uint8_t` command codes. `playback::enqueue_command()` (called from the UI task's button callback) sends to this queue with zero timeout — if full, the command is dropped and logged. `playback::drain_commands()` (called on the sonos task before each poll) dequeues all pending commands and executes them via `sonos::soap::call()`.

**Why:** The button callback runs in the LVGL event system on the UI task. A queue with zero-timeout send is the standard pattern for signalling a service task from an ISR-like callback. Depth 4 is sufficient — the user cannot physically enqueue more than one command per human reaction time, and each drain happens every 2 s.

**Alternative considered:** Direct SOAP call from a short-lived task spawned on the button press. Rejected: spawning a FreeRTOS task per button press has ~1 KB overhead per task and risks stack exhaustion if the user taps rapidly.

### D4: Position interpolation on a 1 s LVGL timer owned by the screen

**Choice:** `now_playing_create()` registers a 1 s `lv_timer_t`. On each tick, if the transport state is Playing, the timer callback computes `estimated_position_s = polled_position_s + (millis() - poll_timestamp_ms) / 1000` and updates the progress bar. The `millis()` call is safe on the UI task. On each `now_playing_update()` call the polled position and timestamp replace the interpolation base.

**Why:** LVGL timers fire on the UI task, so the `lv_bar_set_value()` call is safe. A 1 s interval is the coarsest grain that produces smooth-looking progress without burning CPU on updates faster than what the user perceives. Using `millis()` rather than a frame counter avoids the bar freezing during low-FPS periods.

### D5: DIDL metadata parsed with substring search, consistent with D3 from add-sonos-discovery

**Choice:** Extract `dc:title`, `upnp:artist`, `upnp:album` from the URL-decoded `<TrackMetaData>` DIDL block using `String::indexOf()` / substring extraction, matching the approach used for device description and zone group XML.

**Why:** Same rationale as D3 in `add-sonos-discovery`. The DIDL schema for Sonos track metadata has been stable for over a decade. Pulling in a full XML library for three fields is unjustified. A URL-decode helper (percent-decode `%XX` sequences) is the only addition required; it is a small function that fits in `playback.cpp`.

## Risks / Trade-offs

- **Track metadata missing or malformed** → Some Sonos content (radio, line-in) may produce empty or non-standard DIDL. Mitigation: fall back to empty strings for title/artist/album; the screen displays "—" rather than crashing.
- **Polling at 2 s means up to 2 s lag on state changes** → Acceptable for v1. UPnP event subscriptions (which would give sub-second notification) are explicitly deferred.
- **Inner loop timing drift** → `vTaskDelay(kPollIntervalMs)` is not wall-clock accurate under load. The progress bar correction on each poll resync prevents visible drift accumulation. The 60 s discovery interval is approximate, not SLA-bound.
- **Command queue depth 4** → If the user taps very rapidly or a command SOAP call blocks for a long time, excess commands are dropped. Logged on drop. Acceptable for v1.
