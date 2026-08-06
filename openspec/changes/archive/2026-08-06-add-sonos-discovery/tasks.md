## 1. Shared State Types

- [x] 1.1 Create `src/app/sonos/state.h` declaring: `struct SpeakerInfo { String uuid, friendly_name, ip, control_url_base; uint16_t port; }`, `struct ZoneGroup { String coordinator_uuid; std::vector<String> member_uuids; }`, `struct SonosState { std::vector<SpeakerInfo> speakers; std::vector<ZoneGroup> groups; int home_room_idx = -1; }`, and in `namespace sonos::state`: `void init()`, `void update(const SonosState& next)` (sonos task only), `SonosState snapshot()` (any task, copies under mutex), and `SemaphoreHandle_t mutex()`
- [x] 1.2 Create `src/app/sonos/state.cpp` implementing the FreeRTOS mutex and state singleton: `init()` creates the mutex; `update()` takes the mutex, replaces the stored state, releases; `snapshot()` takes the mutex, copies the state, releases and returns the copy; `mutex()` returns the raw handle for callers that need longer-held locks

## 2. SOAP HTTP Helper

- [x] 2.1 Create `src/app/sonos/soap.h` declaring `namespace sonos::soap` with `String call(const String& ip, uint16_t port, const String& path, const String& service_type, const String& action, const String& args_xml)` — returns the response body on HTTP 200, empty string on any error, and logs failures to serial
- [x] 2.2 Create `src/app/sonos/soap.cpp` implementing `call()`: open a `WiFiClient` to `ip:port`; send an HTTP POST to `path` with headers `Content-Type: text/xml; charset="utf-8"` and `SOAPAction: "<service_type>#<action>"`; body is a UPnP SOAP envelope wrapping `<u:<action> xmlns:u="<service_type>"><args_xml></u:<action>>`; read the response, return the body on 200, log and return `""` on error or timeout (5 s connect + 5 s read)

## 3. SSDP Discovery and Device Description

- [x] 3.1 Create `src/app/sonos/discovery.h` declaring `namespace sonos::discovery` with: `constexpr uint32_t kDiscoveryIntervalMs = 60000`, `void run_cycle()` — runs one full discovery + topology refresh
- [x] 3.2 Implement the M-SEARCH phase in `discovery.cpp`: open `WiFiUDP`, join multicast group 239.255.255.250, send an M-SEARCH packet with `ST: urn:schemas-upnp-org:device:ZonePlayer:1` and `MX: 3`, listen for UDP responses for 3 s, collect unique LOCATION URLs from the `LOCATION:` header of each response, close the UDP socket
- [x] 3.3 For each collected LOCATION URL, open a `WiFiClient` HTTP GET, parse the device description XML: extract the `<UDN>` value (strip leading `uuid:` prefix to get the bare UUID), `<friendlyName>`, `<URLBase>` (or derive IP/port from the LOCATION URL if URLBase is absent); skip any entry where UUID extraction fails and log a warning
- [x] 3.4 After building the new speaker list, log `[Sonos] found N speaker(s)`; if `prefs::home_room().isEmpty()` set `home_room_idx = -1` and log `[Sonos] home room not yet configured`; otherwise iterate the list for a UUID match: on match set `home_room_idx` and log `[Sonos] home room resolved: <friendly_name>`; on no match set `home_room_idx = -1` and log `[Sonos] home room UUID <uuid> not found`

## 4. Zone-Group Topology

- [x] 4.1 If `home_room_idx >= 0` after step 3.4, call `sonos::soap::call()` with the home room speaker's IP/port/control-URL-base targeting `urn:schemas-upnp-org:service:ZoneGroupTopology:1` service, action `GetZoneGroupState`, empty args XML; if the call returns empty, log the error, keep previous topology, and skip 4.2
- [x] 4.2 Parse the `ZoneGroupState` XML from the SOAP response body: find each `<ZoneGroup Coordinator="..." ID="...">` element and its nested `<ZoneMember UUID="...">` elements; build `std::vector<ZoneGroup>` from the result; log `[Sonos] topology updated: N group(s)`

## 5. Sonos Task Wiring

- [x] 5.1 Call `sonos::state::init()` in `setup()` in `main.cpp` before the worker/UI task spawn, so the mutex exists before either task starts
- [x] 5.2 Rename the `worker_task` function to `sonos_task` in `main.cpp` (rename the FreeRTOS task name string too); replace the heartbeat body with: call `sonos::discovery::run_cycle()`, then `vTaskDelay(pdMS_TO_TICKS(sonos::discovery::kDiscoveryIntervalMs))`; update the `xTaskCreatePinnedToCore` call accordingly
- [x] 5.3 Add `#include "app/sonos/discovery.h"` and `#include "app/sonos/state.h"` to `main.cpp`

## 6. Verification

- [x] 6.1 `pio run -e crowpanel_21_round` exits 0
- [x] 6.2 `pio run -e ref_rect_stub` exits 0
- [x] 6.3 `openspec validate --all --strict` is clean
- [x] 6.4 On hardware: flash and confirm serial log shows `[Sonos] found N speaker(s)` (N ≥ 1) and `[Sonos] home room resolved: <name>` within 15 s of boot (requires Sonos speakers on the same LAN and the correct home room UUID stored in NVS from provisioning)
- [x] 6.5 On hardware: confirm `[Sonos] topology updated: N group(s)` appears immediately after home room resolution
- [x] 6.6 Add `CHANGELOG.md` entry under `## [Unreleased]`: "Sonos speaker discovery via SSDP on the local network: the device now finds all Sonos speakers, resolves the provisioned home room, and fetches zone-group topology on boot and every 60 seconds."
