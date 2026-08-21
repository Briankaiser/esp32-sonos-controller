## 1. Firmware: board identifier baked into the build

- [x] 1.1 Add a per-env board-name define to `platformio.ini` `build_flags`:
  `-DOPENSONOS_BOARD_NAME=\"crowpanel_21_round\"` for `crowpanel_21_round`,
  `-DOPENSONOS_BOARD_NAME=\"ref_rect_stub\"` for `ref_rect_stub` (and any other
  hardware env). Use the same string-define quoting already used in the file.
- [x] 1.2 Add a board accessor + `.rodata` marker mirroring
  `src/services/fw_version.cpp`: a `static const char kBoardMarker[] =
  "OPENSONOS_BOARD:" OPENSONOS_BOARD_NAME;` with a fallback
  (`#ifndef OPENSONOS_BOARD_NAME` → `"unknown"`) and an accessor returning the
  value part (skip the `"OPENSONOS_BOARD:"` prefix). Reference the marker so the
  linker does not garbage-collect it. Expose it via a header (e.g. `bsp::board()`
  or `fw::board()`).
- [x] 1.3 Confirm the compiled image contains the marker: build
  `crowpanel_21_round` and grep the `.bin` for `OPENSONOS_BOARD:crowpanel_21_round`.

## 2. Firmware: expose and log the board identifier

- [x] 2.1 Add a `board` field to the `GET /status` JSON in
  `src/services/status.cpp`, sourced from the accessor (2.2), placed alongside
  `firmware_version`. Ensure it is never empty (fallback covers a bad build).
- [x] 2.2 Log the board identifier to serial on boot, after WiFi connects, next
  to the existing firmware-version log line (`src/main.cpp`).
- [x] 2.3 Build both envs (`crowpanel_21_round`, `ref_rect_stub`) and verify each
  `/status` (or the string in each `.bin`) reports its own env name.

## 3. Build sidecar

- [x] 3.1 Write `fw_board.txt` beside the `.bin` at build time (extend
  `extra_scripts/fw_version.py` or add a small parallel step), containing the
  board identifier — symmetric with `fw_version.txt`. Value should come from the
  env name available to the script.

## 4. Fleet updater: board-aware targeting

- [x] 4.1 Add `board_from_bin(firmware_path)` to `tools/fleet_update.py`,
  mirroring `version_from_bin` (find `b"OPENSONOS_BOARD:"`, read to NUL). Return
  `None` when absent (legacy image).
- [x] 4.2 Record each discovered controller's `board` from `/status` during the
  scan (`probe_status`); store `None`/unknown when the field is absent.
- [x] 4.3 In `main()`, compute the target board (`board_from_bin`, sidecar
  fallback). Before the version split, partition discovered controllers into
  `skipped_board` (board present and != target) and `skipped_unknown` (target
  known but device board absent); keep the rest for version selection.
- [x] 4.4 Print skipped controllers with an explicit reason (board mismatch /
  board unknown), in the same style as the existing "already up to date" lines —
  never drop them silently. Include them in `--dry-run` output.
- [x] 4.5 Add a `--force` flag that disables board filtering (all discovered
  controllers eligible regardless of board).
- [x] 4.6 When the image has no embedded board marker (target board `None`), skip
  board filtering and print a warning that board matching is unavailable for this
  image; preserve current version-only behavior.
- [x] 4.7 Sanity-check the confirmation summary and abort/verify flow still count
  only the (now board-filtered) pending set.

## 5. Documentation

- [x] 5.1 Update the fleet-update runbook to describe board targeting, the skip
  reporting, `--force`, and the legacy-image fallback.
- [x] 5.2 Update `AGENTS.md` to note that firmware images and `/status` are
  board-tagged and the updater flashes only matching boards by default.
- [x] 5.3 Add a `CHANGELOG.md` entry under `## [Unreleased]` (board-aware fleet
  updates — user-visible tooling behavior).

## 6. Verification

- [x] 6.1 Both PlatformIO envs build.
- [x] 6.2 `openspec validate --all --strict` is clean.
- [x] 6.3 Manual/dry-run check: with a `crowpanel_21_round` image and a simulated
  or real mixed fleet, confirm only matching boards are listed as pending and
  mismatched/unknown are reported as skipped; confirm `--force` includes all.
