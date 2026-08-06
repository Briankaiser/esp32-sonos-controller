## 1. Prefs Setter

- [x] 1.1 In `src/services/prefs.h`, add the declaration `void set_home_room(const String& uuid)` after the existing `home_room()` getter
- [x] 1.2 In `src/services/prefs.cpp`, implement `prefs::set_home_room()`: open the `opensonos` Preferences namespace in read-write mode, call `putString("home_room", uuid)`, close; log `[Prefs] home_room set to <uuid>` to serial

## 2. Rooms Service

- [x] 2.1 Create `src/services/rooms.h` declaring `namespace rooms` with `void register_handlers(WebServer& server)`
- [x] 2.2 Create `src/services/rooms.cpp` implementing both handlers:
  - Include `rooms.h`, `prefs.h`, `../../app/sonos/state.h`, `<WebServer.h>`, `<Arduino.h>`
  - `GET /rooms` handler: call `sonos::state::snapshot()`; build a JSON string by iterating `state.speakers`; each entry is `{"uuid":"<uuid>","name":"<friendly_name>"}`; send HTTP 200 with `Content-Type: application/json`
  - `POST /rooms/select` handler: read `server.arg("uuid")`; if empty, send HTTP 400 `{"error":"uuid required"}` and return; otherwise call `prefs::set_home_room(uuid)` and send HTTP 200 `{"ok":true,"uuid":"<uuid>"}`
  - `register_handlers()`: call `server.on("/rooms", HTTP_GET, <GET handler>)` and `server.on("/rooms/select", HTTP_POST, <POST handler>)`

## 3. Main Wiring

- [x] 3.1 In `main.cpp`, add `#include "services/rooms.h"` after the existing service includes
- [x] 3.2 In `setup()`, inside the `if (s_wifi_ok)` block alongside the existing `screenshot::register_handler` and `ota::register_handler` calls, add `rooms::register_handlers(s_web_server)`

## 4. Verification

- [x] 4.1 `pio run -e crowpanel_21_round` exits 0
- [x] 4.2 `pio run -e ref_rect_stub` exits 0
- [x] 4.3 On hardware: flash and run `curl http://<ip>/rooms` — confirm response is a JSON array with one object per discovered speaker containing `uuid` and `name`
- [x] 4.4 On hardware: `curl -X POST -d "uuid=<uuid-from-4.3>" http://<ip>/rooms/select` — confirm HTTP 200 and `{"ok":true,"uuid":"..."}` response
- [x] 4.5 On hardware: within 60 s, confirm serial log shows `[Sonos] home room resolved: <friendly-name>`
- [x] 4.6 On hardware: confirm the now-playing screen begins showing track title, artist, and a moving progress bar
- [x] 4.7 On hardware: `curl http://<ip>/rooms/select` with no body — confirm HTTP 400
- [x] 4.8 Add `CHANGELOG.md` entry under `## [Unreleased]`: "`GET /rooms` lists discovered Sonos speakers as JSON; `POST /rooms/select` sets the home room by UUID without requiring factory reset, enabling remote room override."
