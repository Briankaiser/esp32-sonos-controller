## Why

When something goes wrong with connectivity, the device gives the user nothing to act on: it sits on the "Connecting…" overlay indefinitely. That overlay only clears when real Sonos playback state arrives, so **no WiFi**, **no Sonos system on the network**, and an **offline home room** are all indistinguishable dead-ends with no explanation and no recovery path. A physical Sonos controller that can silently hang on a blank "Connecting…" screen reads as broken. These are the normal, recoverable failure modes of a LAN appliance (router rebooted, WiFi password changed, Sonos powered off, a battery Roam asleep) and each deserves a clear on-screen explanation and, where the user can act, an obvious next step.

## What Changes

- Add a **fault status layer** that takes over the screen with a distinct, plain-language message per cause instead of the indefinite "Connecting…" overlay:
  - **No WiFi** — the device cannot reach its configured network. The screen explains this and offers an on-screen **"Re-run WiFi Setup"** control that asks for confirmation, then performs a full reset of stored configuration (returning to an unprovisioned state) and reboots into the existing `OpenSonos-Setup` captive portal.
  - **No Sonos system found** — WiFi is up but discovery has found zero speakers for a bounded time. The screen says so and suggests checking that Sonos is powered and on the same network. Discovery keeps retrying.
  - **Home room offline** — WiFi and Sonos are fine, but the configured home-room speaker is not present in discovery. The screen names the missing room and keeps retrying.
- **Silent auto-recovery**: each fault screen clears itself and normal operation resumes the moment the underlying condition clears (WiFi reconnects, a speaker appears, the home room returns) — no user action required.
- Replace the now-playing "Connecting…" overlay's terminal behavior: it hands off to the fault layer when a fault is diagnosed, rather than displaying forever.

The fault diagnosis is derived from signals that are **already observable** — WiFi station status and the discovery layer's existing thread-safe speaker list and home-room resolution — so no change to discovery's own behavior is required.

Scope boundaries (unchanged, called out because error handling could be mistaken for needing them): detection stays entirely **local** — WiFi station status and SSDP/LAN discovery. **NO Home Assistant** and **NO Sonos cloud/OAuth API** are used to determine reachability. Re-provisioning reuses the existing on-device captive portal; no cloud account is involved.

## Capabilities

### New Capabilities
- `connection-status`: Diagnoses the current connectivity fault (none / no-WiFi / no-Sonos-found / home-room-offline) from local signals, owns the full-screen fault status screens and their safe-area layout, drives silent auto-recovery, and owns the on-device "re-run WiFi setup" recovery action.

### Modified Capabilities
- `now-playing-screen`: the loading overlay no longer persists indefinitely — it yields to the fault status layer once a fault is diagnosed, and is dismissed on first real state as today.
- `device-provisioning`: add a re-provisioning entry point reachable during normal operation (confirm → full reset → reboot into portal), in addition to first-boot detection.

(`sonos-discovery` is intentionally **not** modified: its already-exposed speaker list, home-room resolution, and existing retry/backoff behavior are sufficient inputs for fault detection — no requirement changes.)

## Impact

- **New UI**: fault status screens (round safe-area compliant), built and updated only on the UI task; a confirm dialog for the destructive re-setup action.
- **Detection**: a small connection-state evaluator fed by WiFi station status (`WiFi.status()`) and discovery results (speaker count, home-room resolution), living on core 0 and exposed through the existing thread-safe state interface — no blocking network calls on the UI task.
- **Provisioning**: a normal-operation path into the existing `prefs::factory_reset()` (full NVS wipe → unprovisioned) + reboot, distinct from the existing 30 s button-hold. (Weather config is being removed under a separate `remove-weather` change, which is what makes a full reset lossless and lets us skip a partial-clear helper.)
- **Boot flow**: `main.cpp` WiFi-connect failure currently proceeds silently into the now-playing screen; it will instead feed the no-WiFi fault state.
- **Affected code**: `src/ui/screens/now_playing.cpp` (loading overlay handoff), a new fault-screen module under `src/ui/screens/`, a small fault-evaluator reading existing `sonos::state` + `WiFi.status()`, `src/services/prefs.*` and `src/main.cpp` (re-provisioning path + boot WiFi-fail handoff). No new dependencies; no change to `discovery.cpp` behavior.
