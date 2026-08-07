## Why

The rotary encoder has been physically wired and interrupt-driven since the firmware bootstrap, but the `Intent::VolumeDelta` events it fires fall through the UI task's input handler unhandled. The now-playing screen displays a volume arc that is hard-coded to 50% and never updates. This change closes both gaps: the encoder controls Sonos volume in real time, and the arc reflects the actual level reported by the speaker.

Volume is handled separately from playback because it uses a distinct UPnP service (`RenderingControl` rather than `AVTransport`), a distinct input device (the encoder rather than touch), and carries room-versus-group semantics that differ from transport commands.

No phone app, no Sonos cloud API, no Home Assistant. All volume and mute control is direct UPnP SOAP over the LAN.

## What Changes

- Add `volume` (0–100) and `muted` fields to `PlaybackState`; extend `playback::poll()` to call `GetVolume` and `GetMute` on the `RenderingControl` service after each AVTransport poll.
- Add `PlaybackCommand::ToggleMute` to the command enum; add `sonos::playback::enqueue_volume_delta(int16_t delta)` as a UI-callable function backed by a `std::atomic<int16_t>` accumulator; extend `drain_commands()` to flush any pending volume delta as a single `SetVolume` call and to execute `SetMute` on `ToggleMute`.
- Wire `Intent::VolumeDelta` in the UI task event loop to `playback::enqueue_volume_delta()`.
- Wire encoder short-press (button held < 1 s, released before the factory-reset countdown starts) to `playback::enqueue_command(ToggleMute)` in the `EncoderButtonUp` handler.
- Add `static lv_obj_t* s_volume_arc` to the now-playing screen statics; store the arc handle at screen creation; update it in `now_playing_update()` to show the polled volume level and indicate mute state with a distinct arc color.

## Capabilities

### New Capabilities

- `sonos-volume`: The rotary encoder adjusts the home room speaker's volume in real time. Encoder ticks are accumulated into an atomic delta and flushed as a single `SetVolume` call on the sonos task before each 2 s poll. A short encoder-button press (< 1 s) toggles mute. Volume level and mute state are polled from the `RenderingControl` service alongside existing AVTransport polling and reflected on the volume arc. Volume control applies to the home room speaker only (room semantics, not whole-group); group volume is deferred.

### Modified Capabilities

- `sonos-playback` (DELTA): `PlaybackState` gains `volume` and `muted` fields; `poll()` fetches them from `RenderingControl`; `PlaybackCommand` gains `ToggleMute`; `drain_commands()` handles it.

## Impact

- `src/app/sonos/playback.h/.cpp`: extend `PlaybackState`, `PlaybackCommand`, and `playback::` functions as described.
- `src/main.cpp`: two new event-loop lines (VolumeDelta → `enqueue_volume_delta`; short-press ButtonUp → `enqueue_command(ToggleMute)`); add `prefs::kMuteShortPressMs` comparison.
- `src/services/prefs.h`: add `constexpr uint32_t kMuteShortPressMs = 1000`.
- `src/ui/screens/now_playing.h/.cpp`: add `s_volume_arc` static; update arc in `now_playing_update()`.
- No changes to `src/bsp/`, discovery, or the web services layer.
