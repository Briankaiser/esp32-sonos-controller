## 1. PlaybackState and Command Extensions

- [x] 1.1 In `src/app/sonos/playback.h`, add `String group_coordinator_uuid` to `PlaybackState` (after `muted`)
- [x] 1.2 In `src/app/sonos/playback.h`, add `JoinGroup` and `LeaveGroup` to `PlaybackCommand`
- [x] 1.3 In `src/app/sonos/playback.h`, add `void set_group_target(const String& coordinator_uuid)` to the `playback` namespace

## 2. Playback Implementation

- [x] 2.1 In `src/app/sonos/playback.cpp`, add `static String s_group_target_uuid` at file scope
- [x] 2.2 Implement `playback::set_group_target(const String& uuid)`: `s_group_target_uuid = uuid`
- [x] 2.3 In `drain_commands()`, add handling for `PlaybackCommand::LeaveGroup`:
  - Call `sonos::soap::call(speaker.ip, speaker.port, "/MediaRenderer/AVTransport/Control", "urn:schemas-upnp-org:service:AVTransport:1", "BecomeCoordinatorOfStandaloneGroup", "<InstanceID>0</InstanceID>")`
  - Log `[Group] leaving group`; log failure if SOAP returns empty
  - Set `s_boost_polls.store(4)`
- [x] 2.4 In `drain_commands()`, add handling for `PlaybackCommand::JoinGroup`:
  - Build args: `"<InstanceID>0</InstanceID><CurrentURI>x-rincon:" + s_group_target_uuid + "</CurrentURI><CurrentURIMetaData></CurrentURIMetaData>"`
  - Call `SetAVTransportURI` with those args
  - If successful, call `Play` with `<InstanceID>0</InstanceID><Speed>1</Speed>`
  - Log `[Group] joining group coordinator <uuid>`; log failure if either SOAP call returns empty
  - Set `s_boost_polls.store(4)`

## 3. Group State in PlaybackState

- [x] 3.1 In `src/main.cpp`, in the `sonos_task` inner loop after `sonos::playback::poll(hr)`, derive and update `group_coordinator_uuid`:
  - Snapshot state: `SonosState st = sonos::state::snapshot()`
  - Find which `ZoneGroup` contains `hr.uuid` as coordinator or member
  - If `hr.uuid` is a coordinator of a group with more than 1 member: `group_coordinator_uuid = hr.uuid`
  - If `hr.uuid` is a member (not coordinator) of a group: `group_coordinator_uuid = <that group's coordinator_uuid>`
  - Otherwise (standalone or sole member): `group_coordinator_uuid = ""`
  - Update via `sonos::PlaybackState pb2 = sonos::playback::snapshot(); pb2.group_coordinator_uuid = ...; pb2.version++; sonos::playback::update(pb2)` — use the mutex directly to avoid a version bump in poll(); alternatively call a new `update_group_state(uuid)` helper

    **Implementation note:** To avoid a double-version-bump with `poll()`, add a helper `playback::update_group_uuid(const String& uuid)` that takes the mutex, updates only `group_coordinator_uuid`, increments version, and releases. Call this after `poll()` in the sonos task inner loop.

- [x] 3.2 In `src/app/sonos/playback.h`, add `void update_group_uuid(const String& uuid)` to the `playback` namespace
- [x] 3.3 In `src/app/sonos/playback.cpp`, implement `playback::update_group_uuid(const String& uuid)`:
  - Take `s_mutex`, set `s_state.group_coordinator_uuid = uuid`, increment `s_state.version`, release mutex

## 4. Group Overlay UI

- [x] 4.1 In `src/ui/screens/now_playing.h`, add `void now_playing_open_group_overlay()` and `static lv_obj_t* s_group_overlay` (or handle internally)
- [x] 4.2 In `src/ui/screens/now_playing.cpp`, add:
  - `static lv_obj_t* s_status_cont = nullptr` (pointer to the status slot container, stored at screen build for click registration)
  - `static lv_obj_t* s_group_overlay = nullptr`
- [x] 4.3 In `now_playing_create()`, store the status container: `s_status_cont = slot_container(scr, SLOT_STATUS)`. Add `LV_OBJ_FLAG_CLICKABLE` to it and register an event callback that calls `now_playing_open_group_overlay()`
- [x] 4.4 Implement `screens::now_playing_open_group_overlay()` in `now_playing.cpp`:
  - If `s_group_overlay != nullptr`, return (already open)
  - Create `s_group_overlay = lv_obj_create(lv_layer_top())` full-screen with 80% black background
  - Create `lv_obj_t* list = lv_list_create(s_group_overlay)` sized and positioned to SLOT_LIST geometry (inner square): use the slot dimensions from `slot(SLOT_LIST)` to set position and size without pixel literals
  - Add backdrop click-to-close: `lv_obj_add_event_cb(s_group_overlay, ..., LV_EVENT_CLICKED, nullptr)` with callback that deletes s_group_overlay and sets it to nullptr — check that click target is s_group_overlay itself (not list child)
  - Populate the list:
    - First item: "Go Solo" button — callback calls `sonos::playback::enqueue_command(PlaybackCommand::LeaveGroup)` then closes overlay
    - For each speaker in `sonos::state::snapshot().speakers`: skip the home room itself; add a list button with the speaker's friendly name; callback calls `playback::set_group_target(speaker.uuid)` then `enqueue_command(JoinGroup)` then closes overlay
- [x] 4.5 In `now_playing_update()`, update the status label to reflect group state:
  - If `state.group_coordinator_uuid.isEmpty()`: set `s_status_lbl` text to "Solo"
  - Else: find the coordinator's friendly name from `sonos::state::snapshot()`; set text to the coordinator name or "Group" if not found

## 5. Verification

- [x] 5.1 `pio run -e crowpanel_21_round` exits 0
- [x] 5.2 `pio run -e ref_rect_stub` exits 0
- [x] 5.3 On hardware: confirm "Solo" appears in the status strip when home room is not in a group
- [x] 5.4 On hardware: tap the status strip — confirm the group overlay appears with a list of speakers
- [x] 5.5 On hardware: tap a speaker in the overlay — confirm home room joins that group and plays in sync
- [x] 5.6 On hardware: tap status strip and select "Go Solo" — confirm home room leaves the group and plays independently
- [x] 5.7 On hardware: tap outside the list — confirm overlay closes without any action
- [x] 5.8 Add `CHANGELOG.md` entry under `## [Unreleased]`: "Tap the status strip to open the group overlay; join any speaker's group or go solo with a single tap."
