## Why

The firmware skeleton boots but has no way to acquire network credentials or know which Sonos room to control. Without provisioning, every device must be hard-coded before flashing — this change makes the device self-configuring on first boot, with credentials never baked into firmware.

## What Changes

- On first boot (no stored WiFi credentials), the device starts a captive portal:
  - Emits a WiFi AP named `OpenSonos-Setup`
  - Intercepts all DNS queries and redirects to the device's own HTTP server
  - Serves a setup form collecting WiFi SSID, WiFi password, home room UUID, latitude/longitude (for weather), and OTA password
- On submission the device stores all values in NVS, confirms the WiFi connection, and reboots into normal operating mode
- On subsequent boots, the device reads NVS, connects to WiFi, and skips the portal entirely
- A factory-reset path clears NVS and reboots back into the portal, triggered by holding the rotary encoder button for 30 seconds; a countdown overlay appears on screen after 10 seconds of holding so the user can see the pending reset and abort by releasing early
- No credentials are compiled into firmware. Provisioning is the only path to a working device.

## Capabilities

### New Capabilities

- `device-provisioning`: First-boot captive portal (WiFi AP, DNS redirect, HTTP form), NVS persistence of WiFi credentials and device settings, factory-reset clear, and boot-time credential check.

### Modified Capabilities

<!-- None — this change adds new behaviour without altering any existing spec requirements. -->

## Impact

- New source files: `src/services/portal.cpp`, `src/services/prefs.cpp` + `prefs.h`
- `src/main.cpp`: boot-path check — provisioned vs. first-boot, hand off to portal or normal init
- Dependencies added: Arduino `Preferences` library (bundled with esp32 core, no extra install), Arduino `DNSServer`, Arduino `WebServer` (both bundled)
- No LVGL or UI changes in this phase — the portal is plain HTTP, not a touchscreen UI
- NVS keys established here become the canonical reference for all later reads (home room, lat/lon, OTA password)
