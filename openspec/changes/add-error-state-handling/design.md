## Context

See proposal.md — Why. Today the now-playing "Connecting…" overlay (`now_playing.cpp`) is dismissed only when a non-Unknown transport state arrives, so every connectivity fault presents identically as an indefinite hang. The signals needed to tell the faults apart already exist and are cheap to read:

- **WiFi**: `WiFi.status() == WL_CONNECTED` — callable from any task.
- **Speakers / home room**: `sonos::state::snapshot()` (mutex-protected) exposes the speaker vector and `home_room_idx` (`< 0` ⇒ configured-but-unresolved or unconfigured).

Provisioning already has: `prefs::is_provisioned()` (true iff WiFi SSID stored), the blocking `portal::run()` (starts `OpenSonos-Setup`, returns only via `ESP.restart()`), and `prefs::factory_reset()` (wipes *all* NVS, behind a 30 s button-hold).

## Goals / Non-Goals

**Goals:**
- One evaluator that maps live signals → a single fault enum, with a clear priority.
- Fault presentation that reuses the existing overlay/"illuminated" styling and the round safe-area machinery.
- A re-setup path that clears WiFi credentials and reboots to the portal, guarded by confirmation.

**Non-Goals:**
- No change to discovery/eventing behavior (they already retry and expose state).
- No new persisted settings, no new network protocol, no cloud/HA involvement.
- Rotary interaction on fault screens is out of scope for v1 (touch only).

## Decisions

**1. Diagnose on the UI task from existing snapshots — no new cross-task state.**
The fault enum (`None / NoWifi / NoSonos / HomeRoomOffline`) is computed each UI tick from `WiFi.status()` + `sonos::state::snapshot()`. These reads are cheap and non-blocking, and this keeps detection next to the code that shows/hides the overlay (all `lv_*` stays on the UI task). *Alternative:* publish a fault field from core 0 through shared state — rejected as unnecessary plumbing for three integer comparisons; the inputs are already thread-safe.

**2. Single full-screen fault overlay on `lv_layer_top()`, content swapped by fault.**
One overlay object, shown when `fault != None` and hidden when `None` (this *is* the silent auto-recovery). Content (title/body/controls) is set from the current fault. Priority (NoWifi > NoSonos > HomeRoomOffline) is resolved in the evaluator so the UI only ever renders one. *Alternative:* three separate screens — rejected; a single swappable overlay is less state to manage and dismisses uniformly.

**3. The "no Sonos found" bounded window is a UI-side timer.**
Track "WiFi-connected-with-zero-speakers since T". Only raise `NoSonos` once that exceeds ~15 s; before then show the existing transient connecting indication. This prevents a normal cold-boot search from flashing an alarming screen. Home-room resolution typically lands after the first speakers appear, so the priority order plus this window avoid a NoSonos→HomeRoomOffline→normal flash.

**4. Re-setup clears WiFi credentials only (not full factory reset), then reboots.**
Add `prefs::clear_wifi()` that removes the SSID/password keys so `is_provisioned()` returns false; weather/OTA prefs survive. On reboot the *existing* first-boot path detects missing credentials and starts the portal — no new portal entry code. *Alternative:* reuse `factory_reset()` — rejected because it needlessly wipes weather/OTA; the user's intent is "redo WiFi," not "wipe the device."

**5. Confirmation reuses the modal card + accent styling.**
The destructive re-setup uses a two-button confirm (Cancel / Confirm) built with the existing modal-overlay look, satisfying the "explicit confirmation" requirement. Touch-driven.

**6. Fault overlay preempts open modals.**
If a fault is diagnosed while a settings/favorites overlay is open, the fault overlay takes precedence (a lost connection matters more than a half-finished menu). It sits at the top of `lv_layer_top()` and, on auto-recovery, is removed to reveal whatever was underneath.

## Risks / Trade-offs

- **Fault flapping (WiFi bouncing, one-cycle discovery blips)** → apply short hysteresis: require a condition to persist a beat before showing, and debounce clears by one evaluation, so a momentary blip doesn't flash the screen.
- **Boot ordering (WiFi up, speakers not yet found, home room not yet resolved)** → handled by the ~15 s NoSonos window + strict priority; documented so implementation preserves it.
- **Destructive re-setup from a stray touch** → mitigated by the mandatory confirm step (spec'd).
- **Clearing WiFi keys but leaving stale weather/OTA** → acceptable; the portal form re-collects them on submit. A future nicety is pre-filling the form from surviving prefs (see Open Questions).

## Open Questions

- Should the portal form pre-fill weather/OTA from surviving NVS after a WiFi-only clear? Deferrable — it does not change these specs or the task breakdown.
