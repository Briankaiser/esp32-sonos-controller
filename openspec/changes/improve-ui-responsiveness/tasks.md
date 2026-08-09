## 1. Strip Hot-Path Logging (Tier 1)

- [x] 1.1 In `src/bsp/crowpanel_21_round/board.cpp`, remove the `Serial.printf("[Touch] x=%d y=%d ...")` call inside `lvgl_touch_cb` (the per-read log)
- [x] 1.2 In `src/app/sonos/playback.cpp`, remove the per-poll `Serial.printf("[Playback] title=%s artist=%s state=%s")` line at the end of `poll()`
- [x] 1.3 In `src/app/sonos/soap.cpp`, remove the multi-line `[SOAP] fault[0/200/400]` dump; keep the single concise `[SOAP] HTTP <code>` error line
- [x] 1.4 Scan for any other per-frame/per-poll `Serial.print*` in the hot paths (touch callback, `pop_event`, `poll`, `drain_commands`) and remove or gate behind `OPENSONOS_DEBUG_OVERLAY`; keep rare one-shot/error logs

## 2. Tighten UI Cadence (Tier 1)

- [x] 2.1 In `lib/lv_conf.h`, set an explicit refresh period: `#define LV_DEF_REFR_PERIOD 16` (≈60 Hz) in the Features/timing section
- [x] 2.2 In `src/main.cpp` `ui_task` loop, lower the post-frame yield cap: change the clamp `(next_ms > 20 ? 20 : next_ms)` to a `~8` ms cap so input is drained more frequently

## 3. Dedicated Command Task (Tier 2)

- [x] 3.1 In `src/app/sonos/playback.h`, add `void set_cmd_notify_task(void* handle)` and `void set_poll_notify_task(void* handle)` to the `playback` namespace (replaces the single `set_notify_task`); keep a `drain_commands(const SpeakerInfo&)` public signature
- [x] 3.2 In `src/app/sonos/playback.cpp`, replace `s_notify_task_handle` with two handles: `s_cmd_task_handle` and `s_poll_task_handle`. `enqueue_command` and `enqueue_volume_delta` call `xTaskNotifyGive(s_cmd_task_handle)`. Add `notify_poll_task()` that gives to `s_poll_task_handle` (used by cmd_task after draining)
- [x] 3.3 In `src/main.cpp`, add a `cmd_task` function (core 0):
  ```cpp
  static void cmd_task(void*) {
      sonos::playback::set_cmd_notify_task(xTaskGetCurrentTaskHandle());
      for (;;) {
          ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
          SonosState st = sonos::state::snapshot();
          if (st.home_room_idx >= 0) {
              sonos::playback::drain_commands(st.speakers[st.home_room_idx]);
              sonos::playback::notify_poll_task();  // trigger boost re-poll
          }
      }
  }
  ```
- [x] 3.4 In `sonos_task`, at startup call `sonos::playback::set_poll_notify_task(xTaskGetCurrentTaskHandle())` (instead of `set_notify_task`); remove the three inline `sonos::playback::drain_commands(hr)` calls from the poll loop (keep `poll` and `poll_volume`)
- [x] 3.5 In `setup()`, spawn `cmd_task` pinned to core 0 at priority 4 (add `kCmdTaskPriority = 4`, stack ~4 KB) before/after the sonos task spawn; ensure `kSonosTaskPriority` stays 3
- [x] 3.6 Verify `drain_commands` reads no `sonos_task`-local state that is now gone (it takes `speaker` by arg and uses file-scope statics/atomics — confirm it is self-contained on the new task)

## 4. Verification

- [x] 4.1 `pio run -e crowpanel_21_round` exits 0
- [x] 4.2 `pio run -e ref_rect_stub` exits 0
- [x] 4.3 `openspec validate --all --strict` is clean
- [x] 4.4 On hardware: rapidly tap transport and favorites — confirm no dropped taps and visibly snappier response
- [x] 4.5 On hardware: tap play/pause during an active poll — confirm the command fires within ~150 ms (not delayed until the next poll)
- [x] 4.6 On hardware: turn the volume encoder — arc tracks the finger and SetVolume fires promptly
- [x] 4.7 On hardware: confirm serial no longer floods while touching the screen
- [x] 4.8 On hardware: `curl http://<ip>/screenshot -o s.bmp` still returns a valid frame (UI task not starved)
- [x] 4.9 Add `CHANGELOG.md` entry under `## [Unreleased]`: "Interface is significantly more responsive: removed blocking debug logging from the touch path, tightened the render cadence, and moved Sonos commands to a dedicated high-priority task so taps no longer wait behind background polling."
