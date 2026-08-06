## Context

See `proposal.md — Why` for motivation. The firmware skeleton already has BSP init, LVGL, and FreeRTOS tasks, but `main.cpp::setup()` assumes WiFi credentials exist. The portal must intercept the boot path before any BSP, LVGL, or FreeRTOS initialization occurs — the display isn't needed for text-form provisioning and adding LVGL to the portal path would waste flash and complicate the task boundary.

NVS access uses the Arduino `Preferences` library (bundled with ESP32 core). WiFi AP and HTTP are Arduino's `WiFi`, `DNSServer`, and `WebServer` (all bundled). No new library dependencies are introduced.

## Goals / Non-Goals

**Goals:**
- Boot-path check that diverges to portal when credentials are absent
- Captive portal that any phone/laptop can reach without typing an IP
- Type-safe NVS wrapper (`prefs.h`) used by all subsystems to read/write settings
- Factory reset path that restores portal mode without a PC
- Both board envs compile with no board-specific portal code

**Non-Goals:**
- A touchscreen provisioning UI — portal is plain HTML served over HTTP
- WiFi connection verification before reboot — storing and rebooting is sufficient for v1
- Multi-network or enterprise WiFi (WPA2-Enterprise deferred)
- Secure HTTPS for the portal — credentials travel over the AP's own LAN segment, no internet path

## Decisions

### Boot-path check uses `Preferences` before any hardware init

`main.cpp::setup()` reads NVS via `prefs::is_provisioned()` as the very first operation. If not provisioned, it calls `portal::run()` which blocks until the device restarts. Normal init (BSP, LVGL, FreeRTOS) is unreachable during portal mode.

**Why not post-init?** Initializing the display and LVGL only to then tear it down for WiFi AP mode wastes ~300 ms and flash stack. Portal is a distinct operating mode; keeping it before any hardware init makes the code path unambiguous.

### `prefs.h` wraps Preferences with named typed accessors

A single `src/services/prefs.h` / `prefs.cpp` owns all NVS I/O under namespace `opensonos`. It exposes:
- `prefs::is_provisioned()` — `wifi_ssid` key is non-empty
- `prefs::wifi_ssid()`, `wifi_pass()`, `home_room()`, `lat()`, `lon()`, `ota_pass()` — getters with typed returns
- `prefs::store_provisioning(ssid, pass, room, lat, lon, ota)` — single transactional write of all portal fields
- `prefs::factory_reset()` — clears the `opensonos` namespace

**Why a wrapper vs. direct Preferences calls?** Any subsystem that later reads `home_room` or `ota_pass` would otherwise independently open the same NVS namespace with its own string keys, risking typos and divergence. Centralizing it means one place to audit and one place to add migration logic later.

**Why one namespace?** All keys belong to the same application; partitioning them across namespaces adds no benefit here and complicates factory reset (a single `Preferences::clear()` on the namespace covers everything).

### Portal runs a simple blocking loop with Arduino `WebServer` + `DNSServer`

`portal::run()` does:
1. `WiFi.softAP("OpenSonos-Setup")`
2. Starts `DNSServer` answering all queries with `192.168.4.1`
3. Starts `WebServer` on port 80 with two handlers: `GET /` → form HTML, `POST /save` → validate + store + confirm + `ESP.restart()`
4. Loops `dnsServer.processNextRequest()` + `server.handleClient()` until restart

**Why blocking / no FreeRTOS tasks?** Portal mode is a single-concern operating mode; there is nothing else running. A task-based approach adds overhead and lifecycle complexity for a path that simply blocks until it reboots.

**Why not a third-party captive portal library?** The Arduino `DNSServer` + `WebServer` combination is ~50 lines for this use case. A library would add a dependency without simplifying anything.

### NVS write is all-or-nothing within `store_provisioning`

All six fields are written in a single `Preferences` begin/end block. If any field is blank at form-validation time, `store_provisioning` is not called — the HTTP handler returns an error page instead.

**Why not per-field writes?** Partial writes could leave NVS in a state where `is_provisioned()` returns true but some fields are missing. A single validated write keeps the invariant: either fully provisioned or not provisioned at all.

### Factory reset trigger: 30-second encoder button hold with on-screen countdown

The encoder press button is PCF8574 P5 (confirmed via `mkvesala/ESP32-Crowpanel-compass`). The BSP already reads PCF8574 in `board.cpp`; a button-down / button-up pair of `InputEvent` intents will be added alongside the existing encoder-delta events.

The UI task tracks hold duration using `millis()` on `EncoderButtonDown`. At 10 seconds it creates a countdown LVGL overlay (a full-screen semi-transparent panel with a centered label). A 1-second `lv_timer` updates the remaining seconds. At 30 seconds, `prefs::factory_reset()` is called from the UI task; `ESP.restart()` is safe from any task. On `EncoderButtonUp` before 30 seconds, the timer and overlay are deleted.

**Why not a separate task?** The countdown is a UI concern — it creates and destroys LVGL objects, so it must run on the UI task. Polling button state on core 0 and posting a series of events to the UI queue would work but adds ~40 lines of inter-task plumbing for a feature that is entirely within the LVGL event model.

**Why 10 s / 30 s?** 10 seconds is long enough to be unambiguous (not a navigation gesture) but short enough for the user to understand what is happening before committing. 30 seconds gives a clear, unhurried window to abort. These thresholds are `constexpr` in `portal.h` so they can be adjusted without a rebuild.

## Risks / Trade-offs

- **No credential verification** — the portal stores whatever the user types and reboots. If the WiFi password is wrong, the device will fail to connect silently. Mitigation: a future change can add a post-boot WiFi-connect timeout that falls back to portal mode if connection fails within N seconds.
- **Plain HTTP portal** — credentials are unencrypted over the AP. Acceptable because the AP has no internet path and the device is on a private LAN, but worth noting for security review.
- **`WebServer` on port 80 persists** — if `portal::run()` were ever reached on a normally-booted device (bug), it would conflict with the web-services HTTP server added in change #3. Mitigation: `portal::run()` is only reachable before BSP init; after `ESP.restart()` into normal mode, the portal code never runs.

## Open Questions

None. The factory reset trigger is now specified (encoder button, PCF8574 P5, 30 s hold). On-hardware validation of the GPIO remains a task-level verification step, not a design blocker.
