## 1. Prerequisites

- [x] 1.1 In `lib/lv_conf.h`, add `#define LV_USE_SNAPSHOT 1` under the Features section so `lv_snapshot_take()` is compiled in

## 2. WiFi Station Connection

- [x] 2.1 In `main.cpp::setup()`, after `bsp::init()`, add a WiFi station connect block: call `WiFi.begin(prefs::wifi_ssid(), prefs::wifi_pass())`, poll `WiFi.status()` every 250 ms for up to 15 s; on success print `[WiFi] connected, IP: <ip>`; on timeout print `[WiFi] connection failed — web services unavailable`
- [x] 2.2 Store a `bool s_wifi_ok` flag set from the connect result; use it to guard web server start so the server only starts when WiFi is up

## 3. Screenshot Service

- [x] 3.1 Create `src/services/screenshot.h` declaring:
  - `screenshot::init()` — allocates the PSRAM BMP buffer, creates the cross-task semaphore and mutex
  - `screenshot::register_handler(WebServer& server)` — registers `GET /screenshot`
  - `screenshot::poll()` — called from the UI task after each `lv_timer_handler()`; services any pending snapshot request
- [x] 3.2 Implement `src/services/screenshot.cpp`:
  - Static globals: `uint8_t* s_bmp_buf` (692 054 bytes: 54-byte header + 480×480×3), `SemaphoreHandle_t s_done`, `SemaphoreHandle_t s_mutex`, `volatile bool s_request`
  - `screenshot::init()`: allocates `s_bmp_buf` from PSRAM with `heap_caps_malloc(..., MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)`; creates binary semaphore `s_done` and mutex `s_mutex`; writes the static BMP file header into `s_bmp_buf[0..53]` (signature `BM`, file size, pixel-array offset, DIB header, 480×480, 24 bpp, bottom-up)
  - `screenshot::poll()`: if `!s_request` return; call `lv_draw_buf_t* snap = lv_snapshot_take(lv_screen_active(), LV_COLOR_FORMAT_RGB888)`; convert RGB888 rows bottom-up to BGR24 into `s_bmp_buf[54..]`; call `lv_draw_buf_destroy(snap)`; clear `s_request`; give `s_done`
  - Handler (registered by `register_handler`): acquire `s_mutex`; set `s_request = true`; wait on `s_done` with 3000 ms timeout; on timeout send HTTP 503 and release mutex; on success send HTTP 200 with `Content-Type: image/bmp` and the full `s_bmp_buf`; release `s_mutex`

## 4. OTA Service

- [x] 4.1 Create `src/services/ota.h` declaring `ota::register_handler(WebServer& server)` — registers `POST /update`
- [x] 4.2 Implement `src/services/ota.cpp`:
  - Handler: read `X-OTA-Password` header; compare to `prefs::ota_pass()` using a constant-time compare; on mismatch send HTTP 401 and return without calling `Update`
  - On match: call `Update.begin(server.header("Content-Length").toInt())`; stream body through `Update.write()`; call `Update.end()`; if `Update.hasError()` send HTTP 400 with `Update.errorString()`; else send HTTP 200 and call `ESP.restart()`

## 5. Web Task and Integration

- [x] 5.1 In `main.cpp`, add a static `WebServer s_web_server(80)` and a `web_task` function (core 0, 8 KB stack) that loops calling `s_web_server.handleClient()` with a 1 ms yield
- [x] 5.2 In `setup()`, after the WiFi connect block, if `s_wifi_ok`: call `screenshot::init()`, `screenshot::register_handler(s_web_server)`, `ota::register_handler(s_web_server)`, `s_web_server.begin()`, then spawn `web_task` pinned to core 0
- [x] 5.3 In `ui_task`'s event loop, add `screenshot::poll()` immediately after each `lv_timer_handler()` call so snapshot requests are serviced on the UI task

## 6. Verification

- [x] 6.1 `pio run -e crowpanel_21_round` exits 0
- [x] 6.2 `pio run -e ref_rect_stub` exits 0
- [x] 6.3 `openspec validate --all --strict` is clean
- [x] 6.4 On hardware: flash and confirm `[WiFi] connected, IP: <ip>` appears in serial log within 15 s of boot
- [x] 6.5 On hardware: `curl http://<ip>/screenshot -o s.bmp` returns HTTP 200 and `s.bmp` is a valid 480×480 BMP image of the current display
- [x] 6.6 On hardware: `curl -X POST -H "X-OTA-Password: wrongpass" http://<ip>/update` returns HTTP 401
- [x] 6.7 Add `CHANGELOG.md` entry under `## [Unreleased]`: "Screenshot endpoint (`GET /screenshot`) returns the current display as a BMP image for visual review; OTA firmware update (`POST /update`) enables wireless reflashing without USB."
