## Why

All future UI changes need visual verification, but the device has no HDMI output and `pio device monitor` only shows serial text. Without a way to capture what the display actually shows, reviewing UI changes means physically looking at the device. A `/screenshot` HTTP endpoint solves this permanently: `curl http://<ip>/screenshot -o s.bmp` gives a reviewable image in one command, from anywhere on the home network.

A `/update` OTA endpoint solves the complementary problem: once the device is mounted on a wall or in a case, reflashing over USB is impractical. Password-protected over-the-air firmware update from the home network removes the physical dependency for all future deploys.

Both services share the same WiFi station connection that every subsequent change (Sonos discovery, weather, album art) also needs, so standing up WiFi station mode is a natural part of this change.

## What Changes

- Connect to the home WiFi network on boot using credentials from NVS (SSID + password stored during provisioning).
- Start an HTTP server on the WiFi station interface (home network only — not on the AP interface used by the captive portal).
- `GET /screenshot` — captures the current LVGL framebuffer as a BMP image and returns it as `image/bmp`. Requires no authentication (read-only, local network only).
- `POST /update` — accepts a firmware binary, validates the OTA password from the `X-OTA-Password` request header against the value stored in NVS, applies the update with the Arduino `Update` library, and reboots on success. Rejects with 401 on wrong password, 400 on malformed upload.
- Serial log prints the device's IP address after WiFi connects so the user can find the endpoints.

## Capabilities

### New Capabilities

- `web-services`: HTTP server running on the WiFi station interface; exposes `/screenshot` (BMP capture) and `/update` (OTA firmware update).

### Modified Capabilities

- `device-provisioning`: Boot sequence gains a WiFi station connect step between the provisioning check and normal init. The `prefs` accessors for `wifi_ssid`, `wifi_pass`, and `ota_pass` are already defined; no spec-level requirement change is needed.

## Impact

- `src/main.cpp`: WiFi station connect call added to `setup()` after `bsp::init()`.
- `src/services/screenshot.h` / `.cpp`: new — `screenshot::init(WebServer&)` registers the `/screenshot` handler; screenshot capture crosses the UI/worker task boundary via a shared flag + mutex.
- `src/services/ota.h` / `.cpp`: new — `ota::init(WebServer&)` registers the `/update` handler.
- `lib/lv_conf.h`: `LV_USE_SNAPSHOT 1` required for `lv_snapshot_take()`.
- Dependencies: `WiFi`, `WebServer` (already present from provisioning change).
- The `ref_rect_stub` env must compile all new service files (they are board-agnostic).
