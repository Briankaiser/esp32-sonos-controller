## Context

See proposal.md — Why. The firmware already ships a self-describing *version*:
`src/services/fw_version.cpp` places a greppable marker
`"OPENSONOS_FW_VERSION:" <value>` in `.rodata`; `extra_scripts/fw_version.py`
generates the value from `git describe` into a header the firmware includes and
writes a `fw_version.txt` sidecar beside the `.bin`; `tools/fleet_update.py`
reads the target version from the `.bin` marker (`version_from_bin`) with the
sidecar as fallback; and `GET /status` reports `firmware_version`. Board identity
follows this same three-part pattern (embedded marker + `/status` field +
sidecar). The board is fixed per PlatformIO env, unlike version (per commit).

The existing hardware-abstraction rules already treat the board as compile-time:
one board = one env = one image, selected by `build_src_filter`. There is no
runtime board registry or auto-detection, and the design must not introduce one.

## Goals / Non-Goals

**Goals:**
- Make each firmware image and each running device declare which board it is for,
  reusing the version pattern so there is one mental model, not two.
- Make `fleet_update.py` refuse to cross-board-flash by default, with explicit
  reporting of every device it skips and why.

**Non-Goals:**
- No runtime board auto-detection or probing — the board is known at build time
  and baked in, consistent with the compile-time HAL rule.
- No change to the OTA transport, confirmation, verification, or
  abort-on-failure flow — board matching only narrows the *set* of devices.
- No cross-board compatibility shim: a mismatched image is simply not sent.

## Decisions

### D1: Board identifier = the PlatformIO env name

The identifier value is the env name (`crowpanel_21_round`, `ref_rect_stub`).
It already uniquely names a board profile, appears in `bsp.h`'s "Available
boards" list, and is what the operator types in `pio run -e <env>`, so the
`/status` value matches the build command with no new vocabulary.

- **Alternative — a separate human "model" string** (e.g. "CrowPanel 2.1 Round"):
  rejected as a second identifier to keep in sync. The user described this as
  "model info"; the env name *is* the model identity here, and matching it to
  the build command is more useful to an operator than a prose label.

### D2: `/status` field name is `board`

Matches the internal vocabulary (`bsp.h`, env names) and reads correctly against
the value (`board: "crowpanel_21_round"`). `model` was considered (the user's
word) but would imply a marketing name distinct from the env id; `board` names
exactly what the value is. **Flagged for confirmation** — trivial to rename if
the operator prefers `model`.

### D3: Source the board string via per-env `build_flags`, not the git script

`platformio.ini` already sets per-env defines in `build_flags`
(`-DOPENSONOS_SHAPE_ROUND`, etc.), and those DO reach the `src/` (projenv)
compile. Add `-DOPENSONOS_BOARD_NAME=\"crowpanel_21_round\"` per env. This is
simpler than the version path, which needed a *generated header* specifically
because a define appended by an `extra_script` does not reliably reach projenv
(documented in `fw_version.py`). Since the board name is static config in
`platformio.ini`, a plain build flag is the right tool.

- **Alternative — generated header like the version:** unnecessary; the version
  needs it only because git output is dynamic and computed by a script. Reusing
  that machinery for a static string adds indirection for no benefit.

### D4: Embed the marker in `.rodata`, parallel to the version marker

Add a board marker source mirroring `fw_version.cpp`:
`static const char kBoardMarker[] = "OPENSONOS_BOARD:" OPENSONOS_BOARD_NAME;`
with a `bsp::board_name()` (or `fw::board()`) accessor returning the value part,
referenced so the linker keeps it. The build-time SHA-256 already covers
`.rodata`, so no post-build patching — same rationale as the version marker.
`fleet_update.py` gets `board_from_bin()` mirroring `version_from_bin()`
(find `b"OPENSONOS_BOARD:"`, read to the NUL).

### D5: Sidecar `fw_board.txt` beside the `.bin`

The `fw_version.py` script (or a small parallel step) writes `fw_board.txt` next
to `fw_version.txt`, as a fallback read path — though for the board the primary
source is always the embedded marker; the sidecar mainly keeps the two artifacts
symmetric and lets tooling read the target board without opening the image.

### D6: Filtering happens in the pending-set computation, reported like "already up to date"

`fleet_update.py` today splits discovered controllers into `pending` (version
differs) and `already` (version matches, reported and skipped). Board filtering
slots in at the same point: partition off `skipped_board` (mismatch) and
`skipped_unknown` (no board reported) *before* the version split, and print them
with a reason exactly as `already` is printed. This reuses the existing
report-then-narrow structure and the established "no silent caps" habit.

### D7: Safety defaults — exclude on mismatch AND on unknown; `--force` to override

An unknown board (legacy device) is treated as *not a match*, so it is excluded,
not flashed — flashing an image whose compatibility can't be confirmed is the
exact bricking risk this change exists to prevent. `--force` disables all board
filtering for the operator who knowingly wants a cross-board or legacy flash. A
legacy *image* (no marker in the `.bin`) can't express a target, so the tool
falls back to today's version-only behavior with a warning — never more
dangerous than the current tool.

## Risks / Trade-offs

- **[A stale/incorrect `board` value silently misroutes flashes]** → The value is
  a static build flag tied to the env; a wrong value would have to be a
  `platformio.ini` edit. Both envs building in CI (definition of done) exercises
  that each image carries its own env's name. Low likelihood, and a mismatch
  fails safe (excluded, not mis-flashed).
- **[Legacy devices excluded by default surprise an operator]** → They are
  reported explicitly as skipped-unknown with the reason, and `--force` is
  documented as the escape hatch. Once every device runs board-aware firmware,
  the unknown case disappears.
- **[Board name embedded as `\"...\"` in build_flags — quoting/escaping across
  shells and PlatformIO]** → Use the same quoting form already proven for string
  defines in this repo; verify the compiled `.bin` contains the marker with a
  grep in CI (a cheap post-build check parallel to how the version is trusted).
- **[Two boards on the LAN mid-migration, only one board-aware]** → Exactly the
  scenario this protects: the board-aware image only flashes its own board; the
  other board is skipped-unknown until it too is on board-aware firmware.

## Migration Plan

1. Land the firmware side (build flag + marker + `/status` field + boot log) and
   flash it to the existing fleet with the *current* board-unaware updater (the
   legacy-image / version-only path still works, so this rollout is unaffected).
2. Once devices report `board`, the board-aware updater filters correctly. No
   coordinated cutover: an updater with board support and an image with a marker
   is sufficient; older images just trigger the documented fallback.
3. Rollback: revert the updater to ignore `board` (field is additive to
   `/status`, so old tooling is unaffected — it already ignores unknown fields).
