## Purpose

Discovers all Sonos speakers on the local network using UPnP/SSDP, builds a speaker inventory with friendly names and zone-group topology, and — when a home room UUID is already stored — resolves the home room speaker. When no home room UUID is stored, the speaker inventory remains available for a downstream selection screen to present to the user. All discovery I/O runs off the UI task.

## Requirements

### Requirement: Device Discovers Sonos Speakers via SSDP on Boot

After connecting to WiFi, the device SHALL issue an SSDP M-SEARCH for `urn:schemas-upnp-org:device:ZonePlayer:1` on the multicast group 239.255.255.250:1900. For each LOCATION URL received in a valid SSDP response, the device SHALL fetch the device description XML and extract the speaker's UUID, friendly name, and UPnP control URL base. The resulting inventory SHALL be stored in shared state accessible to all subsystems.

#### Scenario: SSDP search finds speakers and populates inventory

- **WHEN** the device completes its initial SSDP search after WiFi connects
- **THEN** the shared speaker inventory contains one entry per responding Sonos device
- **AND** each entry includes the device UUID, human-readable friendly name, and the IP/port needed to issue SOAP commands
- **AND** the result is logged to serial in the form `[Sonos] found N speaker(s)`

#### Scenario: No speakers found leaves inventory empty

- **WHEN** the SSDP search completes and no Sonos devices respond within the search timeout
- **THEN** the shared speaker inventory is empty
- **AND** the device logs `[Sonos] no speakers found` to serial
- **AND** the device continues operating (no crash or reboot)

### Requirement: Home Room Is Resolved After Each Discovery Cycle When a UUID Is Stored

After each full discovery cycle, the device SHALL check whether a home room UUID is stored in NVS. If no UUID is stored, shared state SHALL reflect that home room selection is pending, and the device SHALL log `[Sonos] home room not yet configured`. If a UUID is stored, the device SHALL compare it against every discovered speaker UUID. If a match is found, that speaker SHALL be designated the home room speaker in shared state. If no match is found, the home room reference SHALL be cleared in shared state and the device SHALL log a warning.

#### Scenario: Home room UUID not yet stored — selection pending

- **WHEN** the SSDP search completes and no home room UUID is stored in NVS
- **THEN** shared state indicates home room selection is pending (no home room speaker designated)
- **AND** the device logs `[Sonos] home room not yet configured` to serial
- **AND** the device continues operating normally

#### Scenario: Home room UUID matches a discovered speaker

- **WHEN** the SSDP search completes and a discovered speaker's UUID matches the NVS home room UUID
- **THEN** shared state records that speaker as the home room
- **AND** the device logs `[Sonos] home room resolved: <friendly-name>` to serial

#### Scenario: Home room UUID stored but not found among discovered speakers

- **WHEN** the SSDP search completes and no discovered speaker matches the stored home room UUID
- **THEN** the home room reference in shared state is cleared
- **AND** the device logs `[Sonos] home room UUID <uuid> not found` to serial

### Requirement: Zone-Group Topology Is Fetched and Stored After Discovery

After resolving the home room speaker, the device SHALL call `GetZoneGroupState` on the home room speaker's `ZoneGroupTopology` service to retrieve the current group membership across all Sonos zones. The parsed topology SHALL be stored in shared state and updated on every subsequent discovery refresh.

#### Scenario: Topology fetch succeeds and is stored

- **WHEN** the home room speaker has been resolved and `GetZoneGroupState` returns successfully
- **THEN** shared state contains the current zone-group topology mapping each group coordinator UUID to its member UUID list
- **AND** the device logs `[Sonos] topology updated: <N> group(s)` to serial

#### Scenario: Topology fetch fails and previous state is preserved

- **WHEN** the `GetZoneGroupState` call returns an HTTP error or a malformed SOAP response
- **THEN** the previously stored topology remains unchanged in shared state
- **AND** the device logs the error to serial
- **AND** the device does not crash or reboot

### Requirement: Discovery Repeats Periodically to Track Network Changes

After the initial discovery, the device SHALL re-run the full SSDP discovery and topology refresh cycle at regular intervals so that speakers that join or leave the network are detected without requiring a device reboot.

#### Scenario: Periodic rediscovery detects a new speaker

- **WHEN** a Sonos speaker that was absent during the previous discovery cycle responds to the next SSDP search
- **THEN** the shared speaker inventory is updated to include the new speaker
- **AND** if the new speaker matches the home room UUID, the home room is resolved and logged

#### Scenario: Periodic rediscovery detects a lost speaker

- **WHEN** a Sonos speaker that was present in the previous inventory does not respond to the next SSDP search
- **THEN** that speaker is removed from the shared speaker inventory
- **AND** if it was the home room speaker, the home room reference is cleared and logged

### Requirement: Discovery State Is Accessible to Other Subsystems via a Thread-Safe Interface

The shared discovery state (speaker inventory, home room reference, zone-group topology) SHALL be readable from any task without causing data races. Writes happen only on the sonos task; reads MAY happen from other tasks.

#### Scenario: UI task reads speaker count without blocking sonos task

- **WHEN** the UI task reads the speaker count from shared state while a discovery cycle is in progress on the sonos task
- **THEN** the read completes without deadlock and returns a consistent snapshot
