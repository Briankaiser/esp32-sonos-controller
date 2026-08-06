## Why

The device has WiFi connectivity but no way to find the actual Sonos speakers on the LAN, and no user-friendly way to identify the home room. Sonos discovery via SSDP is the prerequisite for every playback, volume, grouping, and favorites operation — nothing can talk to speakers until we know their IP addresses and zone topology. Rather than asking users to type an opaque UUID during provisioning, the device discovers speakers and exposes their friendly names so the user can select the home room from a list. The actual selection screen is a separate downstream change; this change establishes the discovery engine and the shared state it populates.

## What Changes

- Add `src/app/sonos/discovery.h/.cpp`: SSDP multicast listener that discovers `urn:schemas-upnp-org:device:ZonePlayer:1` devices and fetches their device description XML to extract UUID, friendly name, and control URLs.
- Add `src/app/sonos/state.h/.cpp`: shared state struct (mutex-protected) holding the current speaker list, zone-group topology, and resolved home room reference. The UI task reads this struct; the sonos task writes it.
- Add `src/app/sonos/soap.h/.cpp`: thin SOAP HTTP helper for synchronous UPnP calls. Used by discovery to call `GetZoneGroupState` on the home room speaker for topology; reused by future playback and volume changes.
- Wire the existing `sonos_task` placeholder in `main.cpp` to run a discovery-and-topology polling loop on core 0.
- No UI changes. Discovery results are observable via the serial log only in this change; screen integration comes in `add-now-playing-playback`.

## Capabilities

### New Capabilities

- `sonos-discovery`: Discovers Sonos speakers on the LAN via SSDP, builds a speaker inventory with friendly names, resolves zone-group topology, and — if a home room UUID is already stored — identifies the home room speaker. All operations run on core 0; results are exposed to the UI task through a mutex-protected shared state struct.

### Modified Capabilities

## Impact

- New directory `src/app/sonos/` with `discovery`, `soap`, and `state` translation units.
- `main.cpp` `sonos_task` function body replaced (currently a heartbeat placeholder).
- New dependency on the ESP32 Arduino `WiFiUdp` library (already available in the framework).
- The `prefs::home_room()` NVS key is read after each discovery cycle. If a UUID is stored, the matching speaker is designated home room. If the key is empty (first boot after WiFi provisioning), shared state signals that home room selection is pending — the selection screen (separate downstream change) will write the UUID once the user picks.
- Does not affect `src/bsp/`, `src/ui/`, or any service in `src/services/`.
