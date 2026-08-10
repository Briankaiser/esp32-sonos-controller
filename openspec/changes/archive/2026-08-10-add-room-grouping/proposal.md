## Why

The device knows the full zone-group topology (fetched every 60 s via `GetZoneGroupState`) but currently ignores it for anything other than display. The home room speaker can be standalone, a group coordinator, or a group member — yet the user has no way to change that from the device itself. Adding join and leave support turns the controller into a proper room-management surface: you can fold the home room into a party group with a tap, or pull it back to solo playback.

No phone app, no Sonos cloud, no Home Assistant. All grouping is direct UPnP SOAP over the LAN.

## What Changes

- Extend `PlaybackCommand` with `JoinGroup` and `LeaveGroup`; add `set_group_target(const String& coordinator_uuid)` to the playback namespace so the UI task can pass a target UUID before enqueuing `JoinGroup`.
- Extend `drain_commands()` in `playback.cpp` to execute the two new commands:
  - `LeaveGroup` → `AVTransport::BecomeCoordinatorOfStandaloneGroup` on the home room
  - `JoinGroup` → `AVTransport::SetAVTransportURI` with `currentURI = x-rincon:<coordinator_uuid>` on the home room, followed by `AVTransport::Play`
- Add `PlaybackState::group_coordinator_uuid` (the UUID of the coordinator the home room currently belongs to, empty if standalone or coordinator itself) extracted from the topology in the sonos task.
- Add a group-selection overlay to the now-playing screen: the `SLOT_STATUS` strip shows the current group state ("Solo" or "Group · N") and is tappable; tapping opens a full-screen overlay using `SLOT_LIST` geometry listing all other speakers/groups with a "Go Solo" option; tapping an entry executes the join or leave command.
- Trigger a boost poll after any grouping command so the updated topology is reflected quickly.

## Capabilities

### New Capabilities

- `sonos-grouping`: The home room speaker can join or leave groups. A tappable status indicator shows the current group state. An overlay lists available speakers and groups; selecting one issues the appropriate UPnP command. All network I/O runs on the sonos task via the existing command queue.

## Impact

- `src/app/sonos/playback.h/.cpp`: `PlaybackCommand` extended; `set_group_target()` added; `drain_commands()` handles two new commands; `PlaybackState` gains `group_coordinator_uuid`.
- `src/app/sonos/discovery.cpp`: after topology fetch, populate `group_coordinator_uuid` in the next playback state.
- `src/ui/screens/now_playing.h/.cpp`: status strip wired to tap; group overlay created/destroyed.
- No new files needed. No changes to BSP, layout archetype, or web services.
