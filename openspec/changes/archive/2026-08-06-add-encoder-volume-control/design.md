## Context

The encoder ISR (`enc_isr` in `board.cpp`) increments a signed `s_enc_delta` counter on every detent. The BSP event pump converts accumulated delta ticks into `Intent::VolumeDelta` events with `value = signed delta`. These events arrive in the UI task's event loop but currently fall through the `default: (void)evt` case.

The encoder button press/release already produces `Intent::EncoderButtonDown` and `Intent::EncoderButtonUp`. The factory-reset logic in `main.cpp` fires only after 5 s of continuous hold (`kResetCountdownStartMs`). A button released before 1 s is currently ignored — this is the short-press window this change uses for mute.

`RenderingControl` is a standard Sonos UPnP service available at `/MediaRenderer/RenderingControl/Control`. `GetVolume` and `GetMute` both require `InstanceID=0` and `Channel=Master`. `SetVolume` takes `InstanceID=0`, `Channel=Master`, `DesiredVolume=<0–100>`. `SetMute` takes `InstanceID=0`, `Channel=Master`, `DesiredMute=<0|1>`.

The existing `sonos::soap::call()` handles the HTTP POST/SOAP envelope. No new network infrastructure is needed.

## Goals / Non-Goals

**Goals:**
- Encoder rotation adjusts home room speaker volume with low latency
- Short encoder press toggles mute
- Volume arc on now-playing screen tracks actual volume

**Non-Goals:**
- Group volume (adjusting all members proportionally) — deferred; requires querying all member speakers' current volumes and scaling
- Fine-grained volume per group member
- Visual volume feedback during the transient period between command and next poll (optimistic update is out of scope for v1)

## Decisions

### D1: Atomic accumulator for encoder delta, not a queue

**Choice:** Use `std::atomic<int16_t> s_pending_delta{0}` in `playback.cpp`. `enqueue_volume_delta(delta)` does `s_pending_delta.fetch_add(delta)`. `drain_commands()` reads the accumulated delta with `s_pending_delta.exchange(0)`, computes the new volume from the last polled volume clamped to 0–100, and calls `SetVolume` once. No per-tick SOAP calls.

**Why:** The encoder fires at human rotation speeds — potentially 5–10 ticks per second. A queue of individual ±1 items would generate one `SetVolume` SOAP call every 2 s poll cycle regardless, but accumulating means only one network round-trip per poll interval covers all ticks since the last drain. `std::atomic<int16_t>` is available and correct on the ESP32 C++17 toolchain without a mutex.

**Alternative considered:** One `int16_t` queue item of depth 1, overwritten on each new delta — rejected because overwriting a queue requires a mutex and misses intermediate ticks.

### D2: Volume and mute polled via RenderingControl in the existing poll() cycle

**Choice:** Append `GetVolume` and `GetMute` calls to the end of `playback::poll()`. Both share the same `ip:port` as the AVTransport calls. The results are stored in new `volume` and `muted` fields on `PlaybackState`.

**Why:** Reusing the 2 s poll cycle for volume is consistent with the existing design and avoids a separate timer or task. Volume changes initiated from outside the device (e.g., the Sonos app) are reflected within one poll cycle. The two extra SOAP calls add ~100–200 ms per poll cycle — acceptable within the 2 s interval.

**Risk:** If `GetVolume` fails (network timeout), the last polled value remains unchanged — the arc may not reflect a change made externally. Acceptable for v1.

### D3: Room semantics — home room speaker only, not group

**Choice:** `SetVolume` and `SetMute` target only `speaker.ip:speaker.port` (the home room speaker). Other speakers in the same group are not affected.

**Why:** Group volume requires fetching each member's current volume, scaling them proportionally to the target, and issuing one `SetVolume` per member. This is a non-trivial operation requiring topology awareness and multiple SOAP calls. The home room speaker is the primary control target; group volume can be added in `add-room-grouping` or a later change.

**Alternative considered:** Always use the group coordinator and issue SetVolume only once (simpler than full group scaling, but still changes only the coordinator). Rejected because it mirrors exactly what room-only semantics already do — same code, different rationale. The explicit decision here is that multi-member group volume is deferred, not that coordinators are special.

### D4: Short press threshold of 1 s for mute toggle

**Choice:** Add `constexpr uint32_t kMuteShortPressMs = 1000` to `prefs.h`. In the `EncoderButtonUp` handler, if `millis() - s_btn_down_ms < prefs::kMuteShortPressMs`, enqueue `ToggleMute`.

**Why:** 1 s is short enough that it never overlaps with the factory-reset countdown (which starts at 5 s), but long enough to be intentional. It also matches a natural tap gesture. The threshold is a `constexpr` so it can be tuned without hunting through code.

### D5: Arc color changes on mute; value tracks actual volume throughout

**Choice:** `now_playing_update()` always sets `lv_arc_set_value(s_volume_arc, state.volume)`. When muted, it additionally sets the indicator arc to a dim gray color (`0x555555`). When unmuted, it restores the indicator to the accent green (`0x1DB954`). The arc value is not forced to 0 when muted, so the user can see what level they will return to on unmute.

**Why:** Setting arc to 0 on mute is misleading — unmuting would show a jump to the actual level with no explanation. Showing the real level with a gray indicator communicates "still at this level, but silenced." The color change is a single `lv_obj_set_style_arc_color` call inside `now_playing_update()`, which already runs on the UI task.
