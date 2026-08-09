## Context

Two tasks share core 0 today: `sonos_task` (priority 3) runs discovery + a poll loop and also drains the command queue; `art_task` (priority 1) decodes album art. The UI task (priority 5) owns core 1 alone and runs the LVGL loop. `enqueue_command`/`enqueue_volume_delta` (UI task) push to a FreeRTOS queue and `xTaskNotifyGive` the sonos task — but if the sonos task is blocked inside `soap::call` during a `poll()`, the notification is latched and ignored until the poll's four SOAP calls return.

Each `soap::call` opens a fresh `WiFiClient` (its own TCP socket), so two tasks calling SOAP concurrently do not share connection state. `PlaybackState` is mutex-protected; `s_boost_polls` and `s_idle` are atomics.

## Goals / Non-Goals

**Goals:** eliminate blocking work on the render/input path; make user commands preempt in-flight polls.

**Non-Goals:** persistent SOAP connections and interrupt-driven touch (documented as deferred recommendations in the proposal).

## Decisions

### D1: Dedicated `cmd_task` at priority 4 on core 0

**Choice:** Add `cmd_task` pinned to core 0 at priority 4 — above `sonos_task` (3), below the UI task (5, core 1). Its body blocks on `ulTaskNotifyTake(pdTRUE, portMAX_DELAY)`; on wake it snapshots `sonos::state`, resolves the home room, and calls `drain_commands(hr)`. `enqueue_command`/`enqueue_volume_delta` notify `cmd_task` instead of `sonos_task`.

**Why:** Because `cmd_task` outranks `sonos_task` on the same core, FreeRTOS preempts `sonos_task` the moment a command arrives — including between the SOAP calls of an in-flight poll, and immediately when a SOAP call's socket wait yields. Command latency drops from "rest of the current poll" (hundreds of ms) to tens of ms. A dedicated task is chosen over draining between SOAP calls because it makes commands genuinely independent of poll structure.

**Concurrency:** `cmd_task` and `sonos_task` may both be inside `soap::call` at once; each uses a local `WiFiClient`, so there is no shared socket. Sonos accepts concurrent UPnP requests to one speaker. Shared state stays behind the existing mutex/atomics. `drain_commands` already reads only its arguments plus atomics.

### D2: `sonos_task` keeps only discovery + poll; `cmd_task` re-notifies it after draining

**Choice:** Remove the three `drain_commands` calls from `sonos_task`'s inner loop. After `cmd_task` finishes draining, it calls `xTaskNotifyGive(sonos_task_handle)` so the poll loop wakes early and refreshes state (the existing `s_boost_polls` mechanism still sets the fast interval). `sonos_task` stores its own handle at startup; `cmd_task` stores its handle for the enqueue path.

**Why:** Keeps the "command executed → state refreshed promptly" behavior without `sonos_task` owning command draining. Two handles: `s_cmd_task_handle` (notified by enqueue) and `s_sonos_task_handle` (notified by cmd_task for boost re-poll).

### D3: Remove blocking logging from the render/input path

**Choice:** Delete the `Serial.printf` in `lvgl_touch_cb` (`board.cpp`), the per-poll `[Playback] title=...` line, the `[Touch]` reads, and the multi-line `[SOAP] fault` dump. Retain concise one-line error logs (connect failed, decode failed) which fire rarely, not per frame.

**Why:** `Serial.printf` at 115200 baud blocks until the UART FIFO drains; an ~80-char line inside a callback that runs every LVGL refresh is the dominant touch-latency source. Rare error logs are fine.

### D4: Explicit LVGL cadence

**Choice:** Set an explicit refresh/read period in `lv_conf.h` (~16–20 ms) and lower the UI-loop post-frame yield cap in `main.cpp` from 20 ms toward ~5–8 ms.

**Why:** Faster input sampling and render cadence is only safe once the per-read serial cost is removed (D3). ~50–60 Hz sampling makes taps feel immediate.
