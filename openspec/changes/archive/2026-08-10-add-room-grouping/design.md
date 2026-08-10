## Context

`SonosState` already contains the full zone-group topology (`std::vector<ZoneGroup>`), updated every discovery cycle. Each `ZoneGroup` has a `coordinator_uuid` and a list of `member_uuids`. From this we can immediately determine whether the home room is standalone, a coordinator, or a member.

The existing `drain_commands()` / `PlaybackCommand` queue already handles Play, Pause, Next, Previous, and ToggleMute via SOAP. JoinGroup and LeaveGroup are two more SOAP calls on the same path; no new IPC mechanism is needed.

For the join command, the UUID of the target coordinator must travel from the UI task (where the user taps) to the sonos task (where the SOAP call runs). The existing queue carries `uint8_t` items — not big enough for a UUID string. The simplest safe solution is a file-scope `static String` written by the UI task and read by the sonos task, guarded by the existing command-queue send as a memory barrier (the enqueue itself provides ordering on the ESP32 since both tasks run on core 0 and core 1 respectively, and the queue send is the synchronisation point).

## Goals / Non-Goals

**Goals:**
- Join home room to any other speaker's group
- Leave any group and go standalone
- Visible status indicator on the now-playing screen
- Full-screen group selection overlay

**Non-Goals:**
- Group volume scaling across members (deferred; requires RenderingControl on every member)
- Dragging speakers between groups
- Creating or naming groups

## Decisions

### D1: JoinGroup carries target UUID via a static String, not the queue item

**Choice:** Add `static String s_group_target_uuid` to `playback.cpp`. The UI task calls `playback::set_group_target(uuid)` then `enqueue_command(JoinGroup)`. The sonos task reads `s_group_target_uuid` inside `drain_commands()` before issuing the SOAP call.

**Why:** The queue item is `uint8_t`; enlarging it would require changing `kWorkerQueueItemBytes` and the drain loop. A static written before enqueue and read after dequeue is safe on the ESP32: the queue send/receive pair acts as the memory barrier. Since only one JoinGroup can be in the queue at a time (queue depth 4, user taps one item), there is no race.

**Alternative considered:** A separate `QueueHandle_t` carrying a struct with a `char[64]` UUID. Rejected: adds a second queue object for one use-case.

### D2: LeaveGroup uses `BecomeCoordinatorOfStandaloneGroup` regardless of coordinator/member status

**Choice:** Always call `BecomeCoordinatorOfStandaloneGroup` on the home room speaker when the user selects "Go Solo".

**Why:** This call is idempotent: if the speaker is already standalone it is a no-op on the Sonos side. If it is a coordinator it dissolves the group. If it is a member it detaches from the coordinator. One call handles all three cases.

### D3: Status strip (SLOT_STATUS) is the grouping entry-point; tappable via lv_obj_add_event_cb

**Choice:** An `lv_label_t` in `SLOT_STATUS` shows "Solo" or "Group · N". The label's parent container receives `LV_EVENT_CLICKED`. Opening the overlay sets a flag (`s_group_overlay_open`) so accidental double-taps are ignored.

**Why:** `SLOT_STATUS` is already empty after the loading-overlay rework. It sits at the very top of the round display where a user naturally glances for status. Making it tappable is zero-cost — LVGL buttons are not needed; any `lv_obj_t` can receive click events when `LV_OBJ_FLAG_CLICKABLE` is set.

### D4: Group overlay is a full-screen lv_obj on lv_layer_top(), listed using lv_list

**Choice:** When the status strip is tapped, create an `lv_obj_t` on `lv_layer_top()` (same as the factory-reset overlay). Inside it, create an `lv_list_t` sized to `SLOT_LIST` geometry (inner square). Add "Go Solo" as the first item, followed by one item per other speaker showing its zone name. Tapping an item enqueues the command and deletes the overlay. Tapping outside the list (on the dark backdrop) deletes the overlay without a command.

**Why:** `lv_layer_top()` guarantees the overlay is always on top, including over the volume arc. `lv_list` handles scroll automatically when there are more than ~5 items (a household with many speakers). SLOT_LIST is already in the archetype for this purpose.

### D5: `PlaybackState` gains `group_coordinator_uuid`; populated from topology in the sonos task

**Choice:** After each `sonos::state::update()` call in `discovery.cpp`, the sonos task reads the topology from the snapshot and sets `group_coordinator_uuid` on the next `PlaybackState` update. If `home_room_idx` is a coordinator of its own group, `group_coordinator_uuid` is set to the home room's own UUID. If it is a member, `group_coordinator_uuid` is the coordinator's UUID. If standalone (not in any group), it is empty.

**Why:** The UI only needs one field to determine status: empty = solo, non-empty = grouped. The screen reads `playback::snapshot().group_coordinator_uuid` to set the status label and populate the overlay list.

**Implementation note:** The existing inner poll loop already calls `sonos::state::snapshot()` to get the home room. After each discovery cycle, `run_cycle()` calls `state::update()`. The new field is updated by the sonos task in the inner loop alongside existing playback polls — no separate poll needed.
