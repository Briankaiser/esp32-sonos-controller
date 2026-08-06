## Context

The firmware runs two FreeRTOS tasks: `ui_task` on core 1 owns all LVGL calls; `worker_task` on core 0 handles I/O. The rule that all `lv_*` calls happen on core 1 creates the primary design constraint for the screenshot service: `lv_snapshot_take()` cannot be called from the HTTP handler on core 0.

The `WebServer` library from the ESP32 Arduino core is already a dependency (used by the captive portal). The `Update` library is also bundled with the core. No new library dependencies are needed.

## Goals / Non-Goals

**Goals:**
- WiFi station connection in `setup()`, shared by all subsequent network services (Sonos, weather, OTA, screenshot).
- `/screenshot` returns a BMP of whatever the display currently shows, callable from `curl` with no tooling beyond HTTP.
- `/update` accepts a firmware binary gated by the OTA password from NVS; rejects before writing on wrong password.
- Both services compile on `ref_rect_stub` without modification.

**Non-Goals:**
- HTTPS / TLS (local network only, no certificate infrastructure).
- Authentication on `/screenshot` (read-only, no sensitive data).
- mDNS / Bonjour hostname (IP-from-serial is sufficient for v1).
- Concurrent OTA and screenshot requests (serialize at the web task level).

## Decisions

### WiFi connection location: `setup()`, blocking with 15 s timeout

WiFi connect belongs in `setup()` before tasks start. All subsequent tasks (web, future Sonos, future weather) depend on the connection being up before they run. A 15 s blocking wait with a 250 ms poll loop is simple and avoids task synchronization on the connection state. If the connection fails, the device boots without the web server but otherwise runs normally — the operator sees "WiFi failed" on serial and can still use the device.

Alternative considered: connect inside a dedicated `net_task`. Rejected — adds task lifecycle complexity before anything downstream needs it, and all current consumers (`add-sonos-discovery`, weather) would need to wait on a connection event anyway.

### Web server task: new `web_task` on core 0

A dedicated `web_task` runs `server.handleClient()` in a tight loop. This is separate from `worker_task` because OTA update processing is blocking and must not stall the worker loop that future Sonos/art/weather I/O will use.

### Screenshot cross-task signaling: flag + binary semaphore

`lv_snapshot_take()` is an LVGL call; it must run on core 1 (`ui_task`). The web task signals a request via an `atomic<bool>` flag (`s_screenshot_request`) and blocks on a `SemaphoreHandle_t` (`s_screenshot_done`). The UI task checks the flag after each `lv_timer_handler()` call; if set, calls `lv_snapshot_take()` into a PSRAM buffer, clears the flag, then gives the semaphore. The web task unblocks, sends the BMP response from the shared buffer, and releases a mutex that serializes concurrent screenshot requests.

A 3-second semaphore timeout on the web side guards against the UI task being stuck or the device being under high load. On timeout the web task returns HTTP 503.

Alternative considered: a FreeRTOS queue carrying a buffer pointer. Equivalent complexity; the flag+semaphore pattern is slightly simpler because the buffer is statically allocated (one at a time).

### BMP encoding: inline, no external library

A 480×480 24-bit BMP is 691 254 bytes (54-byte header + 480×480×3). `lv_snapshot_take()` returns an `lv_draw_buf_t` in RGB565 (the display's native format). A small inline encoder converts each RGB565 pixel to BGR24 (BMP is bottom-up, BGR) while streaming bytes into the response buffer. No JPEG encoder needed — BMP is lossless, widely viewable, and the encoder is ~30 lines.

The snapshot buffer (~692 KB) is allocated in PSRAM at init time and held for the lifetime of the server. PSRAM has 8 MB; this is ~8.5% of it.

### OTA password check: before `Update.begin()`

The `X-OTA-Password` header is read and compared against `prefs::ota_pass()` before calling `Update.begin()`. If it does not match, the handler returns 401 immediately. This ensures no flash write occurs on a wrong-password request, satisfying the spec requirement.

The comparison uses a constant-time string compare to avoid timing oracle attacks (low risk on a local network, but costs nothing).

## Risks / Trade-offs

- **Screenshot latency**: `lv_snapshot_take()` on a 480×480 display allocates and fills a full-frame buffer. On a loaded UI task this may take one or two render cycles (~33 ms each). The 3 s web-side timeout is generous; real latency should be under 100 ms.
- **PSRAM snapshot buffer held permanently**: 692 KB reserved in PSRAM even between requests. Acceptable — PSRAM is 8 MB and future album-art decode is the larger consumer. Alternative (allocate per-request) risks fragmentation under load.
- **Web task stack**: OTA upload streams up to 1.4 MB of firmware binary through the `Update` library. The library buffers internally; the web task needs ~8 KB stack.
- **No HTTPS**: firmware binary in transit is unauthenticated beyond the password header. Acceptable for a local-network-only debug tool; the OTA password prevents unauthenticated writes.

## Open Questions

- mDNS (`opensonos.local`) for future convenience — deferred; IP-from-serial is sufficient and mDNS adds a dependency. Revisit in a later polish change.
