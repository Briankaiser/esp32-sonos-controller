# Tasks: add-error-state-handling

## 1. Fault detection

- [x] 1.1 Define a `ConnectionFault` enum (`None`, `NoWifi`, `NoSonos`, `HomeRoomOffline`) in a small UI-side evaluator module.
- [x] 1.2 Implement `evaluate_fault()` reading `WiFi.status()` + `sonos::state::snapshot()` (speaker count, `home_room_idx`, whether a home room is configured) and returning the single highest-priority fault (NoWifi > NoSonos > HomeRoomOffline > None).
- [x] 1.3 Implement the ~15 s "no Sonos" bounded window: track WiFi-connected-with-zero-speakers since a timestamp; only return `NoSonos` after it elapses, else `None` (transient searching).
- [x] 1.4 Add short hysteresis so a one-evaluation blip (WiFi bounce / intermittent discovery) does not flash or clear the overlay.

## 2. Fault overlay UI

- [x] 2.1 Create a fault-overlay module under `src/ui/screens/` (single full-screen overlay on `lv_layer_top()`), reusing the modal card / accent-illuminated styling.
- [x] 2.2 Render per-fault content: "No WiFi connection", "No Sonos system found" (+ verify-power/network hint), "Home room <name> is offline" (name from stored home room).
- [x] 2.3 Ensure all overlay text/controls sit within the round display's inscribed safe-area circle (no hardcoded pixels / board names; use layout slots).
- [x] 2.4 Show the overlay when `fault != None`, hide/remove it when `None` (silent auto-recovery); overlay preempts any open settings/favorites modal.

## 3. Re-provisioning path

- [x] 3.1 Re-setup uses the existing `prefs::factory_reset()` (full NVS wipe → unprovisioned); no new clear helper needed.
- [x] 3.2 Add a "Re-run WiFi Setup" control to the No-WiFi fault screen.
- [x] 3.3 Add a confirm dialog (Cancel / Confirm) reusing modal styling; only on Confirm call `prefs::factory_reset()` and `ESP.restart()`.
- [ ] 3.4 Verify the post-reboot first-boot path detects missing credentials and starts the `OpenSonos-Setup` portal unchanged.

## 4. Now-playing handoff

- [x] 4.1 Update `now_playing.cpp` so the "Connecting…" overlay yields to the fault overlay when a fault is diagnosed, and no longer implies an indefinite hang.
- [x] 4.2 Confirm normal now-playing content resumes once the fault clears and real state is received (overlay-dismiss behavior otherwise unchanged).

## 5. Integration

- [x] 5.1 Call the evaluator on the UI tick and drive overlay show/hide; keep all `lv_*` on the UI task and avoid any blocking network calls there.
- [x] 5.2 Confirm the boot WiFi-connect failure in `main.cpp` now surfaces as the No-WiFi fault (evaluator covers boot-fail and mid-run drop uniformly — no special boot handoff needed).

## 6. Verification & documentation

- [x] 6.1 Both PlatformIO envs build (`crowpanel_21_round`, `ref_rect_stub`).
- [ ] 6.2 **[hardware]** No-WiFi: with the AP off/unreachable, the No-WiFi screen appears; `/screenshot` captured. Confirm → full factory reset → reboots into `OpenSonos-Setup`. Cancel → no change.
- [x] 6.3 **[hardware]** No-Sonos: WiFi up, all speakers powered off (or off-network) → "No Sonos system found" after the bounded window; `/screenshot` captured; auto-recovers when a speaker returns.
- [ ] 6.4 **[hardware]** Home-room-offline: put the configured home room offline (e.g., sleeping Roam) while other speakers remain → named "offline" screen; `/screenshot` captured; auto-recovers when it returns.
- [ ] 6.5 **[hardware]** Auto-recovery has no flicker/thrash across a WiFi bounce and an intermittent-discovery cycle.
- [x] 6.6 `openspec validate --all --strict` clean; CHANGELOG.md entry under `[Unreleased]`.
