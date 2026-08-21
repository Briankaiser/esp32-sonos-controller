## Why

The fleet updater discovers every OpenSonos controller on the LAN and flashes
the same firmware image to all of them. It filters only on firmware *version* —
it has no concept of *board type*. Every board reports the same
`device: "opensonos-controller"` marker, so a `crowpanel_21_round` image would
be pushed to a `ref_rect_stub` (or any future panel) sharing the network. One
board's firmware on a different board is a bricking risk: wrong display driver,
wrong pin map, wrong flash layout. As soon as a second board type exists on the
network, `fleet_update.py` is unsafe to run.

The firmware is already self-describing for version (a greppable marker in the
image + a `/status` field + a sidecar the updater reads). This change extends
that exact pattern to board identity so the updater only touches devices the
image was actually built for.

## What Changes

- The firmware image embeds a greppable board-identifier marker
  (`OPENSONOS_BOARD:<id>`) in `.rodata`, parallel to the existing
  `OPENSONOS_FW_VERSION:` marker. The build also writes a `fw_board.txt` sidecar
  beside the `.bin`, parallel to `fw_version.txt`.
- `GET /status` gains a `board` field reporting the running board identifier
  (the PlatformIO env name, e.g. `crowpanel_21_round`). The board is also logged
  to serial on boot, mirroring the firmware version.
- `tools/fleet_update.py` becomes board-aware: it reads the target board from
  the `.bin` marker, reads each discovered device's `board` from `/status`, and
  **excludes** devices whose board does not match the image's target from the
  update set. Skipped devices are reported explicitly with the reason — never
  silently dropped.
- Safety-first defaults: a device that reports no `board` (legacy firmware) or a
  mismatched board is NOT flashed. A `--force` flag overrides board filtering for
  deliberate edge cases. If the `.bin` carries no board marker (a legacy image),
  the updater falls back to today's version-only behavior with a warning, so the
  tool never gets *less* safe than it is now.
- Documentation: AGENTS.md and the fleet-update runbook describe board targeting
  and the `--force` escape hatch.

This is LAN-only host tooling and on-device state — no Home Assistant, no Sonos
cloud API involved.

## Capabilities

### New Capabilities

(none — this extends existing capabilities)

### Modified Capabilities

- `web-services`: the `/status` contract gains a required `board` field, and the
  running board identifier becomes discoverable (logged on boot, reported by
  `/status`) — parallel to the existing firmware-version discoverability
  requirement.
- `fleet-update`: discovery records each controller's board; the tool derives the
  target board from the firmware image and updates only board-matching
  controllers, reporting non-matching and unknown-board devices as skipped, with
  a documented override and a legacy fallback.

## Impact

- Build: `platformio.ini` (per-env board-name `build_flags` define);
  `src/services/fw_version.cpp` / a parallel board marker source; the
  `extra_scripts/fw_version.py` sidecar step (or a parallel step) writes
  `fw_board.txt`.
- On-device: `src/services/status.cpp` (`/status` JSON), boot logging in
  `src/main.cpp`.
- Host tooling: `tools/fleet_update.py` (board read from `.bin` and `/status`,
  pending-set filter, `--force`, skip reporting).
- Docs: `AGENTS.md`, fleet-update runbook, `CHANGELOG.md`.
- Both PlatformIO envs must still build; `openspec validate --all --strict`
  clean.
