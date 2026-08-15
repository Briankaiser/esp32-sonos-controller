## 1. Firmware version injection

- [x] 1.1 Add a PlatformIO `extra_scripts` helper (e.g. `extra_scripts/fw_version.py`) that runs `git describe --tags --always --dirty` at build time, generates `src/services/fw_version_generated.h` defining `OPENSONOS_FW_VERSION` (a generated header, not a `-D` flag — CPPDEFINES from an extra script does not reliably reach the src/ compile), and writes the same value to a sidecar file (`.pio/build/<env>/fw_version.txt`) next to the built `.bin` so the updater can read the exact target version. Generated header is git-ignored.
- [x] 1.2 Wire the helper into both `crowpanel_21_round` and `ref_rect_stub` envs in `platformio.ini` (alongside the existing `exclude_arm_asm.py`).
- [x] 1.3 Add a `fw::version()` accessor (small `src/services/fw_version.h` or similar) returning `OPENSONOS_FW_VERSION`, with a non-empty compile-time fallback (e.g. `"unknown"`) when the macro is undefined.
- [x] 1.4 Log the firmware version to serial on boot, after WiFi connects, in `src/main.cpp`.

## 2. `/status` endpoint

- [x] 2.1 Create `src/services/status.{h,cpp}` exposing `status::register_handler(WebServer&)`, following the shape of `rooms`/`screenshot`.
- [x] 2.2 Build the cache-only JSON body from `sonos::state::snapshot()`, `prefs::home_room()`, `WiFi.status()/localIP()/SSID()`, `sonos::eventing::healthy()`, and `fw::version()`; reuse a `json_str`-style escaper for string fields. No SSDP/SOAP/network I/O in the handler.
- [x] 2.3 Emit fields `device` (constant `"opensonos-controller"`), `firmware_version`, `wifi_connected`, `ip`, `wifi_ssid`, `speaker_count`, `home_room_name`, `home_room_uuid`, `state_source` (`"gena"`/`"polling"`), with empty-string defaults for unresolved/disconnected cases; respond `200` with `Content-Type: application/json`.
- [x] 2.4 Register the handler on `s_web_server` in `src/main.cpp` next to `screenshot`/`ota`/`rooms`.

## 3. Fleet-update host tool

- [x] 3.1 Create `tools/fleet_update.py` (Python 3 stdlib only — `urllib`, `concurrent.futures`, `argparse`, `getpass`, `ipaddress`, `socket`).
- [x] 3.2 Implement concurrent subnet discovery: auto-detect the local `/24`, accept `--subnet`/CIDR override, probe `GET /status` per host with a short timeout, and match `device == "opensonos-controller"`; ignore non-responding / non-JSON / non-matching hosts.
- [x] 3.3 List discovered controllers ordered by home room name then IP, showing name, IP, and firmware version; report "none found" and exit cleanly when the scan finds nothing.
- [x] 3.4 Require explicit operator confirmation after the listing and before the first upload; exit without updating if declined.
- [x] 3.5 Read the OTA password from `OPENSONOS_OTA_PASSWORD`, prompting with hidden input (`getpass`) when unset; never require it as a CLI argument.
- [x] 3.6 Accept the firmware image path as input; default the target version to the sidecar `fw_version.txt` written by task 1.1 (with `--version` as an override for hand-built images), so the operator cannot mistype the version the tool waits for; skip and report any discovered controller already reporting the target version.
- [x] 3.7 Implement per-device update: `POST /update` multipart with `X-OTA-Password`; treat a post-`200` connection drop/reset as expected (device rebooting), and fail hard on `401`/`400`.
- [x] 3.8 Implement verified completion: poll `GET /status` (through connection-refused/timeout) until the device reports the target `firmware_version`, bounded by a `--timeout` (default ~120 s); then wait a ≥15 s settle floor before the next device.
- [x] 3.9 Implement abort-on-failure: on verification timeout, stop the run, report the failed controller and the count already updated, and exit non-zero. Updates run strictly sequentially.

## 4. Documentation

- [x] 4.1 Add a `## [Unreleased]` entry to `CHANGELOG.md` describing the `/status` endpoint, the firmware version string, and the `tools/fleet_update.py` fleet updater.
- [x] 4.2 Document `tools/fleet_update.py` usage (env var, `--subnet`, `--version`, `--timeout`) in the tool's `--help`/module docstring, and add a pointer from `AGENTS.md` or README as appropriate.

## 5. Verification

- [x] 5.1 `pio run -e crowpanel_21_round` builds clean.
- [x] 5.2 `pio run -e ref_rect_stub` builds clean.
- [x] 5.3 `openspec validate --all --strict` is clean.
- [ ] 5.4 Hardware check (requires a physical controller — not available in this environment): on a real controller, `curl http://<ip>/status` returns the documented JSON with a non-empty `firmware_version` and the correct `device` marker; confirm the handler triggers no discovery/SOAP traffic.
- [ ] 5.5 End-to-end (requires a physical controller — not available in this environment): run `tools/fleet_update.py` against at least one controller — verify discovery listing, confirmation gate, verified reboot on the new version, the ≥15 s settle floor, and that a forced verification timeout aborts the run non-zero. NOTE: the tool's discovery/marker-match, version-verify polling, sidecar version default, and timeout paths were validated against a local mock `/status` server.
