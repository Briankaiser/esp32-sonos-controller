## Context

The firmware already has a `sonos_task` placeholder pinned to core 0 that runs a synthetic heartbeat. WiFi station connect is complete. The home room UUID in `prefs::home_room()` may or may not be set — the captive portal now collects only WiFi credentials; home room selection happens via a UI screen after discovery (separate downstream change). What's missing is everything between "WiFi is up" and "we have a speaker inventory and optionally a resolved home room."

SSDP on ESP32 Arduino uses `WiFiUDP` — no external library required. The device description XML is fetched with `WiFiClient` over plain HTTP. SOAP is also plain HTTP POST with a specific `SOAPAction` header and an XML body. The ESP32 Arduino `WiFiClient` handles both.

There is no UPnP event subscription support in this change. All topology awareness comes from polling `GetZoneGroupState`; UPnP NOTIFY subscriptions are explicitly deferred as noted in the project plan.

## Goals / Non-Goals

**Goals:**
- Replace the `sonos_task` heartbeat with a real discovery-and-topology loop
- Provide a stable, mutex-protected shared state that playback/volume/grouping can build on without knowing when discovery last ran
- Keep all network I/O on core 0; shared state access from any task is safe via a mutex

**Non-Goals:**
- Playback control, volume, grouping, or favorites — those come in later changes
- UPnP NOTIFY event subscriptions (polling is sufficient for v1)
- Discovery of non-Sonos UPnP devices
- Persisting discovered state across reboots (NVS already has the UUID; the IP is re-resolved each discovery cycle)

## Decisions

### D1: Active SSDP M-SEARCH over passive NOTIFY listening

**Choice:** Send an M-SEARCH multicast and collect unicast responses, rather than passively listening for NOTIFY messages from speakers.

**Why:** NOTIFY is sent by speakers at arbitrary intervals (usually every 1800 s) and on state changes. A device that boots mid-interval would wait up to 30 minutes before seeing all speakers. M-SEARCH gets responses within 2–5 seconds and is the correct discovery mechanism for a new joiner.

**Alternative considered:** NOTIFY-only listener — rejected because of the boot-time gap.

### D2: Flat speaker list in shared state; no in-memory graph

**Choice:** Store discovered speakers as a `std::vector<SpeakerInfo>` (UUID, friendly name, IP, port, control-URL base). Zone-group topology is stored as a separate `std::vector<ZoneGroup>` (coordinator UUID + member UUID list). The home room is tracked as `int home_room_idx` (-1 = not resolved). Callers that need to distinguish "not yet configured" from "configured but not found" check `prefs::home_room().isEmpty()` themselves — there is no need to duplicate that NVS state in the shared struct.

**Why:** The flat list is sufficient for everything planned through `add-room-grouping`. A graph structure would pay complexity now for no near-term benefit. Using an index rather than a pointer-into-vector avoids a dangling-pointer class of bug when the vector is rebuilt on rediscovery.

**Alternative considered:** `std::map<String, SpeakerInfo>` keyed by UUID — would simplify lookup but adds heap allocation cost per discovery cycle on a device where PSRAM is plentiful but IRAM is not. Vector with a linear UUID scan is acceptable for ≤20 speakers.

### D3: XML parsing with minimal string search, not a DOM parser

**Choice:** Extract fields from device description XML and `GetZoneGroupState` responses using `String::indexOf()` / substring extraction rather than pulling in a full XML library.

**Why:** The device description and zone group XML schemas are stable and narrow. A DOM parser (TinyXML2, etc.) adds ~40 KB of flash and heap overhead for minimal benefit. The two fields we need (UUID inside `<UDN>`, `LOCATION` from SSDP header, group XML from SOAP response) are extractable with simple substring operations.

**Risk:** If Sonos ever restructures the XML layout (they haven't in 10+ years), the parser breaks silently. Mitigation: add serial log on failed parse so the operator can diagnose.

### D4: Single `sonos::state` mutex shared by all subsystems

**Choice:** One `SemaphoreHandle_t` in `state.cpp` guards the entire `SonosState` struct. Callers lock, copy what they need, unlock.

**Why:** Discovery, playback, and volume all need different parts of the same state. Multiple fine-grained mutexes would risk inversion. One mutex with a short hold time (copy a handful of strings) is simple and correct.

**Caller convention:** Never hold the mutex across a network call. Copy needed values out first, release, then do I/O.

### D5: Periodic rediscovery interval of 60 seconds

**Choice:** Re-run the full M-SEARCH + topology cycle every 60 seconds.

**Why:** Sonos speakers rarely change IP (DHCP leases are long), but a speaker could reboot or be physically moved. 60 s is short enough to detect a lost/rejoined speaker before the user notices stale state, and long enough that the ~300 ms discovery traffic is negligible. The interval is a `constexpr` in `discovery.h` so it can be changed without hunting through the code.

## Risks / Trade-offs

- **SSDP multicast reliability on some routers** → Some home routers block or rate-limit 239.255.255.250 multicast. Mitigation: log a clear warning; the operator can diagnose via the serial console. There is no fallback without manual IP entry (out of scope for v1).
- **String-based XML parse is fragile** → Covered in D3. The risk is acceptable because Sonos has not changed these XML schemas in over a decade, and a parse failure produces a serial log, not a silent wrong result.
- **Linear UUID search over speaker list** → Acceptable for ≤20 speakers. If a household somehow has more, the 60 s rediscovery cycle would be the bottleneck long before the linear scan.
- **`home_room_idx` ambiguity** → -1 means both "UUID not configured yet" and "UUID configured but not found in current scan." Callers that need to distinguish the two cases call `prefs::home_room().isEmpty()`. This keeps the state struct simple; the two cases require different UI responses (show selection screen vs. show a warning), which is the responsibility of downstream changes, not this one.
