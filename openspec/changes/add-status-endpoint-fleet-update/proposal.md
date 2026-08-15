## Why

Updating firmware on more than one controller today means finding each device's
IP by hand (watching serial, or guessing off the router), then `curl`-ing
`POST /update` at each one and hoping it came back before moving on. As the
number of controllers grows this is slow and error-prone: there is no way to
enumerate the fleet, no way to tell which firmware a device is already running,
and no safety net if an update leaves a device stuck. This change gives the
fleet a cheap, cache-only status surface and a host-side tool that discovers
every controller on the LAN and updates them safely, one at a time.

Control stays entirely on the LAN over direct HTTP — no Home Assistant, no
Sonos cloud. `/status` reports only the device's own already-cached state.

## What Changes

- **New `GET /status` endpoint** on port 80, no auth, `application/json`
  (matching the existing `/rooms` convention — the device hand-builds JSON and
  ships no YAML serializer). Every field is read from state the device already
  holds; the handler performs no SSDP, SOAP, or other network I/O. Fields:
  - `device`: constant `"opensonos-controller"` identity marker so a subnet
    scan can distinguish a controller from any other web server on port 80.
  - `firmware_version`: the running build's version string.
  - `wifi_connected`, `ip`, `wifi_ssid`: network status from the WiFi driver.
  - `speaker_count`: number of discovered Sonos speakers.
  - `home_room_name`, `home_room_uuid`: the configured/resolved home room.
  - `state_source`: `"gena"` when eventing is healthy, else `"polling"`.
- **Build-time firmware version** injected as `-DOPENSONOS_FW_VERSION` (derived
  from `git describe`, with a fallback when git metadata is unavailable). This
  is a prerequisite for the updater's "skip already-updated" and "confirm the
  new firmware booted" behavior — the codebase has no version string today.
- **New host-side fleet-update CLI** (`tools/fleet_update.py`, Python 3 standard
  library only, no pip dependencies): scans the local subnet for controllers via
  `GET /status`, lists them by home room name then IP with current firmware
  version, confirms with the operator, then updates each controller in turn via
  authenticated `POST /update`. After each device it tolerates the reboot
  drop-out, polls `/status` until the device returns **with the new version**
  (with a timeout), and holds a 15-second settle floor between devices. If a
  device does not come back on the new version within the timeout, the run
  **aborts** and reports the failed device rather than rolling a possibly-bad
  build across the whole fleet.

None of this uses Home Assistant or the Sonos cloud API — discovery is a plain
HTTP sweep of the LAN and updates go straight to each device's existing OTA
endpoint.

## Capabilities

### New Capabilities
- `fleet-update`: Host-side tooling that discovers OpenSonos controllers on the
  local network via their `/status` endpoint and applies a firmware update to
  each one sequentially, safely, with per-device verification and abort-on-failure.

### Modified Capabilities
- `web-services`: Adds the cache-only `GET /status` endpoint and the requirement
  that the running firmware version is discoverable, both on the device serial log
  and in the `/status` response.

## Impact

- **Firmware code**: new `src/services/status.{h,cpp}` module, registered on
  `s_web_server` in `src/main.cpp` alongside `screenshot`/`ota`/`rooms`. Reads
  existing `sonos::state::snapshot()`, `prefs::home_room()`, `WiFi.*`, and
  `sonos::eventing::healthy()`. No new tasks, no UI, nothing on screen.
- **Build**: `platformio.ini` gains a version build flag; a small PlatformIO
  `extra_scripts` helper computes it from `git describe`. Applies to both
  `crowpanel_21_round` and `ref_rect_stub` envs.
- **Host tooling**: new `tools/fleet_update.py`; no third-party packages. Reads
  the OTA password from `OPENSONOS_OTA_PASSWORD`, prompting (hidden input) if unset.
- **Docs**: `CHANGELOG.md` `## [Unreleased]` entry; `AGENTS.md`/README pointer to
  the new tool is optional follow-up.
- No change to the captive-portal provisioning flow or NVS schema.
