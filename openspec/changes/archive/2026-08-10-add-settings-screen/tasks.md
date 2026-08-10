## 1. New Prefs Functions

- [x] 1.1 In `src/services/prefs.h`, add declarations:
  - `bool   group_volume_mode()` — returns `false` (home room only) by default
  - `void   set_group_volume_mode(bool whole_group)`
- [x] 1.2 In `src/services/prefs.cpp`, implement both functions using NVS key `"grp_vol"` in the `"opensonos"` namespace (same namespace as `home_room`)

## 2. Group Volume in flush_volume()

- [x] 2.1 In `src/app/sonos/playback.cpp`, after the existing `SetVolume` call in `flush_volume()`, add whole-group logic:
  - If `prefs::group_volume_mode()` is false, return (existing behavior)
  - Call `sonos::state::snapshot()` to get the zone topology and speaker list
  - Find the `ZoneGroup` whose `coordinator_uuid` or `member_uuids` contains `speaker.uuid`
  - For each UUID in that group that is NOT `speaker.uuid`, look up the matching `SpeakerInfo` in `st.speakers`; if found, call `sonos::soap::call(...)` with `SetVolume` and the same desired volume; log and skip on empty response
- [x] 2.2 Add `#include "../../services/prefs.h"` to `playback.cpp` if not already present

## 3. Settings Screen Files

- [x] 3.1 Create `src/ui/screens/settings.h` declaring:
  - `lv_obj_t* settings_create(lv_obj_t* return_screen)` — creates and returns the settings LVGL screen; `return_screen` is stored so the back button can reload it
  - All symbols in `namespace screens`
- [x] 3.2 Create `src/ui/screens/settings.cpp` with a stub implementation: dark background, `layout_container(scr, SLOT_LIST)`, back button as the first `lv_list` item (`LV_SYMBOL_LEFT " Back"`) that calls `lv_scr_load(s_return_screen)` and registers `LV_EVENT_SCREEN_UNLOADED` to delete the settings screen

## 4. Room-Selection Entry

- [x] 4.1 In `settings.cpp`, after the back-button row, add a "Home Room" list entry. At `settings_create()` time, call `sonos::state::snapshot()` to get `home_room_idx` and `speakers`:
  - If `home_room_idx >= 0`: display `speakers[home_room_idx].name` as the value suffix
  - Else: display "Not set"
  - Store the list button pointer as `static lv_obj_t* s_room_btn` for label refresh
- [x] 4.2 Register `LV_EVENT_CLICKED` on the room entry button that calls `settings_open_room_picker()`
- [x] 4.3 Implement `static void settings_open_room_picker(lv_event_t*)`:
  - Guard against double-open with `static lv_obj_t* s_picker = nullptr`
  - Create a full-screen dark backdrop on `lv_layer_top()`
  - Create an `lv_list_t` inside the backdrop positioned and sized to `slot(SLOT_LIST)` geometry (read `slot(SLOT_LIST).x/y/w/h` — no raw pixel values)
  - Add backdrop `LV_EVENT_CLICKED` to close picker only when click target is the backdrop itself
- [x] 4.4 Populate the picker list:
  - Call `sonos::state::snapshot()`
  - If no speakers: add a single disabled label "No speakers found"
  - For each speaker: add a list button with `speaker.name`; store `speaker.uuid` via `lv_obj_set_user_data`
  - Append a "Cancel" button
- [x] 4.5 Register `LV_EVENT_CLICKED` on each speaker button:
  - Read UUID from `lv_obj_get_user_data(btn)`
  - Call `prefs::set_home_room(uuid)`
  - Update `s_room_btn` label text to show the selected name
  - Delete the picker: `lv_obj_del(s_picker); s_picker = nullptr`
- [x] 4.6 Register `LV_EVENT_CLICKED` on the "Cancel" button: delete picker without changing prefs

## 5. Group Management Entry

- [x] 5.1 In `settings.cpp`, add a "Manage Groups" list entry after the Home Room entry
- [x] 5.2 Move the group overlay implementation from `now_playing.cpp` to `settings.cpp`:
  - Remove `now_playing_open_group_overlay()` from `now_playing.h` and `now_playing.cpp`, including `s_group_overlay` static and the status-strip tap callback that triggered it
  - Implement `static void settings_open_group_overlay(lv_event_t*)` in `settings.cpp` with the same logic: dark backdrop on `lv_layer_top()`, `lv_list` at `slot(SLOT_LIST)` geometry, "Go Solo" first, then one button per non-home-room speaker; tap enqueues the appropriate command and closes the overlay
- [x] 5.3 Register `LV_EVENT_CLICKED` on the "Manage Groups" entry to call `settings_open_group_overlay()`

## 6. Group Volume Toggle Entry

- [x] 6.1 In `settings.cpp`, add a "Group Volume" list entry after "Manage Groups"
- [x] 6.2 At `settings_create()` time, set the button label based on `prefs::group_volume_mode()`:
  - `false` → `"Group Volume: Home room only"`
  - `true`  → `"Group Volume: Whole group"`
  - Store the button pointer as `static lv_obj_t* s_gvol_btn`
- [x] 6.3 Register `LV_EVENT_CLICKED` on the group volume button:
  - Read current mode, toggle it, call `prefs::set_group_volume_mode(!current)`
  - Update `s_gvol_btn` label text to reflect the new state

## 7. Now-Playing Screen: Gear Icon in SLOT_STATUS

- [x] 7.1 In `src/ui/screens/now_playing.cpp`, remove the existing group-status label (`s_status_lbl`) and any code in `now_playing_update()` that writes to it
- [x] 7.2 In `now_playing_create()`, replace the status container content with a single gear icon:
  - Get (or create) the `SLOT_STATUS` container
  - Add a label child with text `LV_SYMBOL_SETTINGS`, aligned to center
  - Set `LV_OBJ_FLAG_CLICKABLE` on the container; register `LV_EVENT_CLICKED` callback
- [x] 7.3 In the gear click callback: call `screens::settings_create(lv_scr_act())` and load the result with `lv_scr_load()`
- [x] 7.4 Remove `now_playing_open_group_overlay()` declaration from `now_playing.h` (moved to settings.cpp internals)

## 8. Verification

- [x] 8.1 `pio run -e crowpanel_21_round` exits 0
- [x] 8.2 `pio run -e ref_rect_stub` exits 0
- [x] 8.3 On hardware: gear icon is visible and centered in the status area of the now-playing screen; no group status text is shown
- [x] 8.4 On hardware: tapping the gear navigates to the settings screen showing "Back", "Home Room", "Manage Groups", "Group Volume"
- [x] 8.5 On hardware: Home Room entry shows the current speaker name (or "Not set"); tapping it opens the picker; selecting a speaker updates the label and persists to NVS
- [x] 8.6 On hardware: tapping "Manage Groups" opens the group overlay over the settings screen; join/leave commands work as before
- [x] 8.7 On hardware: tapping "Group Volume" toggles between "Home room only" and "Whole group"; setting survives reboot
- [x] 8.8 On hardware (Whole group mode, home room in a group): rotate encoder — volume arc updates; serial confirms `SetVolume` is issued to all group members
- [x] 8.9 On hardware: tapping Back returns to now-playing with playback continuing
- [x] 8.10 Add `CHANGELOG.md` entry under `## [Unreleased]`: "Settings screen (gear icon on now-playing): select home room speaker, manage zone groups, and toggle group-wide volume control."
