## Why

The captive portal intentionally collects only WiFi credentials — home room selection is deliberately deferred to a post-boot UI step because the provisioning form can't know which speakers are on the network at the time of setup. Without a way to set the home room after provisioning, discovery always logs "home room not yet configured" and playback never starts.

This change adds two HTTP endpoints to the existing web server: one that lists the Sonos speakers currently discovered on the LAN, and one that sets a speaker as the home room by writing its UUID to NVS. This unblocks hardware validation of the now-playing screen immediately, and is a permanent feature because remote home room override is useful ongoing — for example, when temporarily moving the device to a different room without going through factory reset and re-provisioning.

No phone app, no Sonos cloud API, no Home Assistant dependency. Both endpoints operate on the LAN only.

## What Changes

- Add `prefs::set_home_room(const String& uuid)` to `src/services/prefs.h/.cpp` — a single NVS write for the home room UUID key, complementing the existing `prefs::home_room()` getter.
- Add `src/services/rooms.h/.cpp`: `rooms::register_handlers(WebServer& server)` registers `GET /rooms` and `POST /rooms/select` with the existing `s_web_server`.
- Wire `rooms::register_handlers()` into `setup()` in `main.cpp` alongside the existing screenshot and OTA registrations.

## Capabilities

### New Capabilities

None. This change extends `web-services`.

### Modified Capabilities

- `web-services` (DELTA): Two new endpoints — `GET /rooms` lists discovered Sonos speakers as JSON; `POST /rooms/select` accepts a speaker UUID, writes it to NVS as the home room, and the next discovery cycle (within 60 s) resolves it. Existing `/screenshot` and `/update` endpoints are unchanged.

## Impact

- `src/services/prefs.h/.cpp`: add `set_home_room()`.
- New files: `src/services/rooms.h/.cpp`.
- `src/main.cpp`: one additional `register_handlers` call in `setup()`.
- The home room takes effect on the next `sonos::discovery::run_cycle()` — no reboot required, though a reboot also works. The serial log will show `[Sonos] home room resolved: <name>` when it picks up the new UUID.
- No changes to `src/bsp/`, `src/ui/`, or `src/app/`.
