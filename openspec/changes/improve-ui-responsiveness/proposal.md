## Why

The now-playing interface lags: touches are slow and sometimes dropped, and Sonos commands take longer to send than expected. A code-verified investigation found the causes:

- A debug `Serial.printf` runs inside the LVGL touch-read callback (`board.cpp`), firing ~33×/s and continuously while a finger is held. At 115200 baud each line is several milliseconds of blocking *inside the input read* — the largest single touch-latency source. Per-poll `[Playback]` and verbose `[SOAP]` logs add more.
- User commands serialize behind an in-flight poll. `sonos_task` runs `drain_commands` then `poll()` (four SOAP calls) on the same task; a tap enqueued while a poll's SOAP calls are in flight cannot be drained until the whole poll returns, so the command waits hundreds of milliseconds.

These violate the existing `runtime-architecture` guarantee that rendering and input are never blocked by I/O work. This change restores that guarantee and adds an explicit guarantee that user commands execute independently of background polling.

## What Changes

- **Strip hot-path logging**: remove the touch-read `Serial.printf`, the per-poll `[Playback] title=...` line, the `[Touch]` spam, and the verbose `[SOAP] fault` dump. Keep concise error and one-shot logs only.
- **Tighten UI cadence**: lower the UI-loop post-frame yield cap and set an explicit LVGL refresh/read period so touch samples at ~50–60 Hz once the per-read serial cost is gone.
- **Dedicated command task**: add a `cmd_task` on core 0 at priority 4 (above `sonos_task`'s 3). `enqueue_command`/`enqueue_volume_delta` notify it; on wake it reads the home room from shared state and runs `drain_commands` plus the volume-delta flush. Because it outranks `sonos_task` on the same core, a tap preempts the gaps in an in-flight poll and executes within tens of milliseconds. `sonos_task` keeps only discovery and polling; after draining, `cmd_task` notifies `sonos_task` to trigger the existing boost re-poll.

## Capabilities

### Modified Capabilities

- `runtime-architecture` (DELTA): reaffirms that no work on the render/input task may block it (including diagnostic logging), and adds a requirement that user commands execute independently of background polling.

## Impact

- `src/bsp/crowpanel_21_round/board.cpp`: remove touch-read logging.
- `src/main.cpp`: UI-loop cadence; add `cmd_task`; remove `drain_commands` from `sonos_task`; task creation in `setup()`.
- `src/app/sonos/playback.h/.cpp`: retarget command/volume notifications to `cmd_task`; add `cmd_task` body and start function; `cmd_task` re-notifies `sonos_task` for boost re-poll.
- `src/app/sonos/soap.cpp`: drop the verbose fault dump (keep a concise error line).
- `lib/lv_conf.h`: explicit refresh/read period.

## Deferred / Recommended (not in this change)

- **Persistent/keep-alive SOAP connection** to the home room, eliminating the per-call TCP handshake — the biggest remaining latency win once commands are decoupled. Needs reconnect and error-recovery handling.
- **Interrupt-driven touch** via the CST816 INT line instead of polling every LVGL refresh — removes blocking I2C from the render path. Requires verifying the INT pin wiring on hardware first.
