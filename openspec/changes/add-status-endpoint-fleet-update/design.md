## Context

See proposal.md — Why. Two mechanisms need pinning down before implementation:
(1) how the firmware version string gets into the build and out through
`/status`, and (2) how the host tool decides a device has finished updating,
given that `POST /update` responds `200` and then immediately `ESP.restart()`s
(see `src/services/ota.cpp`), so the device disappears from the network for an
unknown interval before rejoining WiFi.

Relevant existing state, all already cached and cheap to read on core 0 / in the
web-server handler:
- `sonos::state::snapshot()` → `speakers` (count), `home_room_idx` (resolution).
- `prefs::home_room()` → configured home room UUID.
- `WiFi.status()` / `WiFi.localIP()` / `WiFi.SSID()` → network fields.
- `sonos::eventing::healthy()` → GENA vs polling.

The runtime web server is `s_web_server` in `src/main.cpp`; handlers are
registered by `screenshot`, `ota`, and `rooms` modules. `/status` follows the
same shape: a `status::register_handler(WebServer&)` module.

## Goals / Non-Goals

**Goals:**
- A version string that is unique per build, requires no manual bump, and is
  identical whether read from serial or `/status`.
- Completion detection that survives the reboot drop-out and confirms the *new*
  firmware is running, not merely that *a* device answered.
- Host tool with zero third-party dependencies (matches the existing
  stdlib-only `tools/mock_gena_speaker.py` / `scripts/mock_sonos.py`).

**Non-Goals:**
- No parallel/batched updates — sequential only, by design (safety over speed).
- No rollback of firmware on a device (the device's dual-slot OTA already keeps
  the prior slot; the tool's safety is *stopping the fleet*, not un-flashing a
  device).
- No persistent fleet inventory or scheduling — discovery is live each run.

## Decisions

### Version injection: `git describe` at build time via an `extra_scripts` helper

A small PlatformIO `extra_scripts` Python helper runs `git describe --tags
--always --dirty` at build time and generates
`src/services/fw_version_generated.h` defining `OPENSONOS_FW_VERSION`, which
`src/services/fw_version.h` includes. Firmware exposes it as
`const char* fw::version()` returning the macro, or a compile-time fallback
(`"unknown"`) via `#ifndef` when the generated header is absent (e.g. a build
that skips the script) so a tarball build without git still yields a non-empty
string. A generated header is used rather than a `-DOPENSONOS_FW_VERSION` build
flag because appending `CPPDEFINES` from an extra script does **not** reliably
reach the `src/` (projenv) compilation environment across PlatformIO/SCons
versions — verified empirically: the flag never landed on the source compile
line and the string was absent from the ELF. A header the firmware `#include`s
always compiles in, and (written only when the value changes, content-hashed by
SCons) re-triggers just the two TUs that use it. The generated header is
git-ignored. Both envs (`crowpanel_21_round`, `ref_rect_stub`) run the helper;
the existing `exclude_arm_asm.py` shows the `extra_scripts` pattern already in use.

- *Alternative — hand-maintained version constant:* rejected; it drifts from
  reality the moment someone forgets to bump it, defeating "skip already-updated".
- *Alternative — CHANGELOG top version:* brittle to parse and only changes on
  release, so two different dev builds would report the same version and the
  updater's verification could false-positive. `git describe` changes per commit
  and marks `-dirty`.

### `/status` is a flat JSON object built by hand, cache-only

Consistent with `/rooms` (`src/services/rooms.cpp` hand-builds JSON with a
`json_str` escaper). Reuse that escaping approach for the string fields. The
handler takes one `sonos::state::snapshot()` and reads WiFi/eventing/prefs
directly — no SSDP/SOAP. `state_source` is `"gena"` when
`sonos::eventing::healthy()` else `"polling"`. `home_room_name` is the
`friendly_name` at `home_room_idx` when `>= 0`, else empty.

### Completion detection: tolerate-drop → poll-for-new-version → settle floor

Per device, the tool runs this state machine:
1. **Upload.** `POST /update`, multipart body, `X-OTA-Password` header. A `200`
   means accepted-and-rebooting. Connection errors/resets *during or right after*
   the response are expected (the device reboots mid/again after responding) and
   are treated as "upload accepted, now verifying", not failure. A `401`/`400`
   *before* the reboot is a hard failure (bad password / bad image).
2. **Verify.** Poll `GET /status` on the same IP on a fixed interval. Success =
   a JSON response whose `firmware_version` equals the target version. Keep
   polling through connection-refused/timeout (device still rebooting/rejoining
   WiFi) until either success or the **verification timeout** elapses.
3. **Settle.** On success, wait at least 15 s (the operator-specified floor)
   before the next device, so a freshly-rebooted controller has fully settled.
4. **Abort.** On verification timeout, stop the whole run, report the failed
   device and the count already updated, exit non-zero.

The **target version** must equal, character-for-character, the string
`/status` reports (`git describe --tags --always --dirty`, e.g.
`v1.2.3-4-gabcdef-dirty`). Requiring the operator to *type* that is a footgun:
a near-miss (`v1.2.3`) flashes every device fine but then waits for a version
that never appears, times out on device 1, and aborts the whole run — reporting
a failure that did not happen. To close that loop, the build helper (task 1.1)
SHALL also write the computed version to a sidecar file
(`.pio/build/<env>/fw_version.txt`) next to the built `.bin`, and the tool's
`--version` SHALL default to reading that file, so the target is exactly the
string that was built. `--version` remains overridable for hand-built images.
Deriving the version from the `.bin` itself is not reliably possible, so the
sidecar is the source of truth. A device already reporting the target version at
discovery is reported "already up to date" and skipped.

- *Alternative — blind 15 s delay only:* rejected; assumes the device is back
  when it often is not (WiFi rejoin can exceed 15 s), and never confirms the new
  image actually booted. The 15 s is kept as a floor *on top of* verification,
  honoring the operator's stated constraint without relying on it for safety.
- *Alternative — continue past a failed device:* rejected per the proposal — a
  bad build would then roll across the entire fleet. Abort-and-report is the
  safety property.

### Subnet scan: concurrent `/status` probes, auto-detected subnet

Determine the local `/24` (or accept `--subnet`/CIDR) and probe each host's
`GET /status` concurrently (thread pool, stdlib `concurrent.futures` +
`urllib`) with a short per-host timeout (~0.5–1 s). Match on
`device == "opensonos-controller"`. Everything else is ignored.

## Risks / Trade-offs

- **Multiple subnets / VLANs** → controllers on another segment are missed.
  Mitigation: auto-detection reads the interface's real netmask (via `ifconfig`
  / `ip`, stdlib subprocess) rather than assuming `/24` — verified on a real
  `/22` network where the device sat outside the local `/24`. Ranges larger than
  `MAX_AUTO_HOSTS` are narrowed to a `/24` with a note, and `--subnet` overrides
  for anything else.
- **`git describe` with no tags** → falls back to short SHA (`--always`), still
  unique per commit; `--dirty` flags uncommitted builds so a dev image is never
  mistaken for a clean one during verification.
- **Verification timeout too short on a slow WiFi rejoin** → a healthy device
  could be misjudged failed and abort the run. Mitigation: generous default
  (e.g. 120 s) and a `--timeout` override; the abort is safe (stops, does not
  brick), and the operator can re-run to continue the remaining devices.
- **`/status` exposes SSID/IP unauthenticated** → same trust model as the
  existing unauthenticated `/rooms` and `/screenshot` on the home LAN; no
  secrets (no password) are exposed, and OTA itself stays authenticated.

## Open Questions

- Whether to add a `--dry-run` (scan + list, no update) convenience flag — does
  not affect specs or the task breakdown; can be added freely later.
