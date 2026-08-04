# Capability Map and Change Roadmap

Planning reference. Not an OpenSpec artifact — the CLI does not read this file.

`openspec/specs/` is **generated** when a change is archived. Capabilities appear
there only once a change that declares them has been applied and archived. Do not
hand-author files under `openspec/specs/`.

## Capabilities

Thirteen planned. The change that introduces each one is responsible for authoring
its delta spec.

| Capability | Purpose | Introduced by |
|---|---|---|
| `hardware-abstraction` | Board support package contract — one `constexpr` display profile and panel/touch/encoder bindings per board, selected at compile time | 1 |
| `runtime-architecture` | Task and core partitioning, queue hand-off, PSRAM buffers, frame-rate budget | 1 |
| `ui-foundation` | LVGL init, layout archetypes and named slots, safe-area enforcement, input intents, type roles, accent token | 1 |
| `device-provisioning` | First-boot captive portal; persisted WiFi, home room, coordinates, OTA password | 2 |
| `web-services` | On-device HTTP: `/update` OTA and the `/screenshot` debug endpoint | 3 |
| `sonos-discovery` | SSDP discovery, speaker inventory, zone-group topology, home-room resolution | 4 |
| `sonos-playback` | Transport control, state polling, on-device position interpolation | 5 |
| `now-playing-screen` | Primary screen composition, controls, progress, state sync | 5 |
| `sonos-volume` | Volume and mute, room versus group semantics | 6 |
| `album-art` | Art fetch, decode, cache, dominant-colour extraction | 7 |
| `sonos-grouping` | Join and leave against the home room's group | 8 |
| `sonos-favorites` | Favorites browse and play | 9 |
| `screensaver` | Idle detection, clock, minimal weather | 10 |

### Granularity decisions

- **Weather lives in `screensaver`**, not its own capability — the screensaver is its
  only consumer, and a separate capability would be a spec with one reader.
- **Position interpolation lives in `sonos-playback`**, not `now-playing-screen` — it
  is state behavior, not presentation. The screen renders whatever playback reports.
- **`sonos-volume` is separate from `sonos-playback`** despite both being transport-ish.
  It has a distinct input device (the encoder), a distinct UPnP service
  (`RenderingControl`), and distinct room-versus-group semantics.
- **`now-playing-screen` is the only screen with its own capability.** The other
  screens are thin presentations of a Sonos capability and are specced there. If a
  screen grows behavior of its own, split it out then.

## Change sequence

Each change is applied and archived before anything depending on it begins.

| # | Change | Depends on | Capabilities |
|---|---|---|---|
| 1 | `bootstrap-firmware-skeleton` | — | `hardware-abstraction`, `runtime-architecture`, `ui-foundation` |
| 2 | `add-device-provisioning` | 1 | `device-provisioning` |
| 3 | `add-web-debug-services` | 1, 2 | `web-services` |
| 4 | `add-sonos-discovery` | 2 | `sonos-discovery` |
| 5 | `add-now-playing-playback` | 3, 4 | `sonos-playback`, `now-playing-screen` |
| 6 | `add-encoder-volume-control` | 5 | `sonos-volume` |
| 7 | `add-album-art-accent-theming` | 5 | `album-art`, modifies `ui-foundation` |
| 8 | `add-room-grouping` | 4, 5 | `sonos-grouping` |
| 9 | `add-favorites-shortcuts` | 4, 5 | `sonos-favorites` |
| 10 | `add-idle-screensaver-weather` | 2, 5 | `screensaver` |
| 11 | `expand-rectangular-board-profile` | 1 + all screens | modifies `hardware-abstraction`, `ui-foundation` |

### Sequencing rationale

- **`web-services` (3) lands before any screen work.** The `/screenshot` endpoint is
  the primary way to inspect interface state; building it after the screens it exists
  to debug would waste its value.
- **Provisioning (2) precedes discovery (4)** because discovery needs WiFi credentials
  and a home-room selection to resolve against.
- **Volume (6) is separate from and after playback (5)** so the encoder work lands
  against a screen that already renders state.
- **Change 11 is the real test of the display abstraction.** It builds every screen
  against the rectangular profile. The pass condition is that no file under
  `src/ui/screens/` is modified. If that fails, the abstraction did not hold and the
  finding belongs in that change's design notes.

## Deferred

Not in v1, and not blocked on anything above.

- Lyrics display
- UPnP event subscription in place of 2s polling — polling is adequate for v1
- Density-based sizing arithmetic; `ppi` and `diag_mm_x10` are carried in the profile
  but unused until a third board exists
