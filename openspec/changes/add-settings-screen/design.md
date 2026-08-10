## Context

`SonosState` already carries the full discovered speaker list (`std::vector<SpeakerInfo>`), `home_room_idx`, and zone-group topology (`std::vector<ZoneGroup>`). `prefs::home_room()` and `prefs::set_home_room(uuid)` already exist. The HTTP `/rooms` endpoints are already wired; this change adds no new backend HTTP machinery.

The existing slot system covers the settings screen needs without new slots: `SLOT_LIST` provides the inner-square content area (72, 72, 336×336 on the round board), and `SLOT_STATUS` (139, 24, 201×57) is the top strip on the now-playing screen.

The group join/leave overlay (`now_playing_open_group_overlay`) was implemented in `now_playing.cpp` by the `add-room-grouping` change. This change moves the trigger point from the now-playing status strip to the settings screen, and moves the overlay implementation to `settings.cpp`. The SOAP commands and playback queue it uses are unchanged.

Navigation between screens is a simple `lv_scr_load()` pair — no router, no history stack. The settings screen is created fresh on each open and deleted on close, keeping RAM usage bounded.

## Goals / Non-Goals

**Goals:**
- `SLOT_STATUS` repurposed as a tappable gear icon on now-playing (group status label removed)
- New settings LVGL screen with a scrollable list within the safe area
- Room-selection entry with speaker picker overlay
- Group management entry: opens the group join/leave overlay from within settings
- Group volume toggle: persisted in NVS; `flush_volume()` applies to whole group when enabled

**Non-Goals:**
- Screen history / breadcrumb stack (only one settings screen exists; a single back action suffices)
- Animated screen transitions (plain `lv_scr_load()` is sufficient for v1)
- Display brightness, encoder sensitivity, weather, or other future settings (reserved for later changes)
- Per-member volume fine-tuning or group volume scaling

## Decisions

### D1: SLOT_STATUS becomes the gear icon only — group status label is removed from now-playing

**Choice:** `SLOT_STATUS` on the now-playing screen holds a single centered gear label (`LV_SYMBOL_SETTINGS`) that is `LV_OBJ_FLAG_CLICKABLE`. The group status text ("Solo" / "Group · N") is removed from now-playing entirely. Group management moves to the settings screen.

**Why:** `SLOT_STATUS` is the only named slot at the top of the disc. It was previously split between a group status label (left) and a proposed gear icon (right). Dedicating the full 201×57-px strip to a single tappable gear is a cleaner affordance: one tap target, one destination. The group status was informational; with group management in settings, the user still has full access to it one screen away.

**Consequence:** `now_playing_update()` no longer writes to the status label. The `s_status_lbl` pointer and its `group_coordinator_uuid` update path (added by `add-room-grouping`) are removed from now_playing.cpp.

### D2: Settings screen uses SLOT_LIST; back button is the first list item

**Choice:** `screens::settings_create()` returns an `lv_obj_t*` screen (parent `nullptr`). The first child is a `layout_container(scr, SLOT_LIST)`. Inside SLOT_LIST: a back button as the first `lv_list` item using `LV_SYMBOL_LEFT " Back"`, then the three settings entries below.

**Why:** `SLOT_LIST` (15%–85% of each axis on the round board) safely contains a list on the round display. The back button as the first list item gives a large touch target and scrolls with the list, needing no separate slot. No new slots or slot geometry changes are required.

### D3: Room-picker overlay uses lv_layer_top() + SLOT_LIST geometry

**Choice:** On tap of the Home Room entry, create a full-screen dark backdrop on `lv_layer_top()`. Inside it, create an `lv_list` at the same geometry as `slot(SLOT_LIST)`. Each list item shows a speaker's friendly name; a "Cancel" item is appended. Tapping a speaker calls `prefs::set_home_room(uuid)`, refreshes the entry label, and destroys the overlay. Tapping "Cancel" or the backdrop destroys the overlay without side-effects.

**Why:** Identical pattern to the group overlay. Re-uses the same dark-backdrop + `lv_list` idiom, providing visual consistency.

### D4: Group overlay implementation moves from now_playing.cpp to settings.cpp

**Choice:** `now_playing_open_group_overlay()` and its static state (`s_group_overlay`) are removed from `now_playing.cpp`. Equivalent logic is reimplemented in `settings.cpp` as `static void settings_open_group_overlay()`, called when the user taps "Manage Groups". The SOAP commands (`enqueue_command`, `set_group_target`) are unchanged.

**Why:** The overlay is now triggered from settings, not from now-playing. Moving the implementation to settings.cpp removes the cross-module call and keeps `now_playing.cpp` free of group-management logic. The `now_playing_open_group_overlay` function is removed from `now_playing.h` entirely.

**Consequence:** `now_playing_update()` no longer calls `now_playing_open_group_overlay()`. Any code that called `now_playing_open_group_overlay()` (currently only the status-strip tap callback) is deleted.

### D5: prefs::set_home_room() and group_volume_mode() called from the UI task

**Choice:** Both NVS writes happen on the UI task (core 1) directly on user gesture.

**Why:** NVS writes via `Preferences::putString` / `putBool` are synchronous but fast (< 1 ms typical). No dispatch to a background task is needed. The write happens once on a deliberate user action, not in a hot path.

### D6: Settings screen reads speaker data from sonos::state::snapshot() at build/open time

**Choice:** `settings_create()` calls `sonos::state::snapshot()` once for the initial Home Room label. The picker overlay and group overlay each call `snapshot()` at open time. No live update subscription.

**Why:** The settings screen is opened deliberately. Speaker data changes slowly (discovery every 60 s). A stale name at most refreshes on the next open. This avoids wiring settings into the update callback path.

### D7: Navigation is a two-call lv_scr_load() pair; settings screen created fresh each open

**Choice:** The gear icon tap creates the settings screen via `screens::settings_create(lv_scr_act())` and loads it with `lv_scr_load()`. The back button reloads the now-playing screen. The settings screen is deleted via `LV_EVENT_SCREEN_UNLOADED`.

**Why:** The now-playing screen already exists and holds live LVGL widgets (volume arc, progress bar timers). Keeping it alive and switching back avoids rebuilding that state. The settings screen has no persistent state worth keeping.

### D8: Group volume toggle is a two-state lv_list button cycling on tap; persisted as a bool in NVS

**Choice:** The "Group Volume" entry is an `lv_list_btn` whose label cycles between "Home room only" and "Whole group" on each tap. The current state is read from `prefs::group_volume_mode()` (returns `false` by default). On tap, the state is toggled and written back via `prefs::set_group_volume_mode(bool)`.

**Why:** A simple tap-to-toggle on a list button is the lightest possible two-state control on a small round display. `lv_checkbox` or `lv_switch` would work but consume more vertical space and require more layout care for the round safe area. A label that reads "Home room only" / "Whole group" is self-documenting without a separate value column.

### D9: flush_volume() reads group_volume_mode() and iterates zone group members for whole-group volume

**Choice:** In `flush_volume(const SpeakerInfo& speaker)`, after setting volume on the home room, if `prefs::group_volume_mode()` is true: call `sonos::state::snapshot()` to get the zone groups; find the zone group containing the home room's UUID; call `SetVolume` on every other member's IP/port. Members are identified by UUID; their `SpeakerInfo` (with IP/port) is looked up from `st.speakers`.

**Why:** `flush_volume` already runs on cmd_task (core 0). `sonos::state::snapshot()` and `prefs::group_volume_mode()` are safe to call from any task. `SonosState::zone_groups` contains the topology populated by discovery. Iterating members and calling `SetVolume` individually is the most compatible approach across all Sonos firmware versions — `SetGroupVolume` is not universally available.

**Note:** If a group member's IP lookup fails (speaker in topology but not in speakers list), that member is silently skipped and the others are still updated.
