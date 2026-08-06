## Why

The device can now find Sonos speakers and know their topology, but nothing tells the user what is playing and nothing lets them control it. This change bridges that gap: the sonos task starts polling the home room speaker for transport state every 2 seconds, and the now-playing screen is built to display that state and respond to play/pause/next/previous presses. The result is the first version of the device that is actually useful — a physical controller you can glance at and touch to operate Sonos.

Discovery is a prerequisite because the home room speaker's IP and control URL must be resolved before playback polls can start. The web services change is a prerequisite because `/screenshot` is the primary way to verify the screen layout without physically looking at the device.

No phone app, no Sonos cloud API, no Home Assistant dependency. All control is direct UPnP SOAP over the LAN.

## What Changes

- Add `src/app/sonos/playback.h/.cpp`: mutex-protected `PlaybackState` struct (transport state, title, artist, album, duration, polled position, poll timestamp), `poll()` to call `GetTransportInfo` and `GetPositionInfo` on the home room speaker, `enqueue_command()` for transport commands from the UI task, and `drain_commands()` to execute queued commands via SOAP before each poll.
- Restructure `sonos_task` in `main.cpp`: after each 60 s discovery cycle, enter an inner 2 s polling loop that calls `playback::drain_commands()` and `playback::poll()`; exits the inner loop when the next discovery interval is due.
- Add `src/ui/screens/now_playing.h/.cpp`: replaces the placeholder screen with real track info labels, a play/pause/next/prev transport row, a progress bar driven by on-device interpolation between polls, and the existing volume arc stub (volume control is added in `add-encoder-volume-control`).
- Replace `screens::placeholder_create()` with `screens::now_playing_create()` in `main.cpp`; add playback version polling in the UI task event loop to call `now_playing_update()` whenever the playback state changes.

## Capabilities

### New Capabilities

- `sonos-playback`: Polls the home room speaker's AVTransport service every 2 seconds for transport state (playing / paused / stopped / transitioning), track metadata (title, artist, album), and current position. Interpolates track position on a 1 s UI timer between polls. Accepts play, pause, next, and previous commands from the UI task via an internal queue and executes them as SOAP calls on the sonos task. All network I/O runs on core 0; state is shared with the UI task through a mutex-protected snapshot.

- `now-playing-screen`: The primary screen of the device. Displays current track title, artist, album name, a progress bar that advances smoothly via on-device interpolation, and play/pause/next/previous transport controls. Positions all content through named layout slots; no pixel coordinates appear in screen source. Fully enforces the safe-area contract for round displays.

### Modified Capabilities

- `sonos-discovery` (runtime wiring only): `sonos_task` is restructured to interleave discovery cycles with playback polling. The `run_cycle()` contract is unchanged; only the outer task loop changes.

## Impact

- New files: `src/app/sonos/playback.h/.cpp`, `src/ui/screens/now_playing.h/.cpp`.
- Modified: `src/main.cpp` (sonos_task restructure, screen swap, playback version poll in UI loop).
- `src/ui/screens/placeholder.cpp` is retained as a compile target but is no longer loaded at boot.
- The 64-byte placeholder queue item size in `main.cpp` (`kWorkerQueueItemBytes`) is not used by this change; playback state is shared via `playback::snapshot()` and a version counter, not the existing queue.
- No changes to `src/bsp/`, `src/services/`, or the layout/theme layer.
- The volume arc rendered by the now-playing screen is a visual placeholder identical to the one in the placeholder screen; rotary encoder volume control is wired in `add-encoder-volume-control`.
