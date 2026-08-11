## 1. nav module

- [x] 1.1 Create `src/ui/nav.h` — declare `register_list(lv_obj_t*)`, `clear_list()`, `navigate(int delta)`, `select()`, `has_modal()`, `has_selection()`
- [x] 1.2 Create `src/ui/nav.cpp` — implement: `s_list` / `s_index` statics; `navigate` applies selection indicator to new item, removes it from previous, and calls `lv_obj_scroll_to_view(child, LV_ANIM_ON)` to keep the selected item visible, with `lv_obj_is_valid` guard for dangling pointer safety; `select` fires `LV_EVENT_CLICKED` on the selected child; `has_modal` returns true when `lv_layer_top()` has at least one non-hidden child; `has_selection` returns `s_list != nullptr && s_index >= 0`

## 2. Encoder routing in main.cpp

- [x] 2.1 In `ui_task` `VolumeDelta` case: wrap existing volume logic in `else`; if `nav::has_modal()`, call `nav::navigate(-evt.value)` instead
- [x] 2.2 In `ui_task` `EncoderButtonUp` short-press block: if `nav::has_modal()`, call `nav::select()` when `nav::has_selection()` else no-op — do NOT fall through to transport; only reach existing play/pause logic when no modal is open

## 3. Overlay list registration

- [x] 3.1 Favorites overlay (`now_playing.cpp` `now_playing_open_favorites_overlay`): after building `s_favorites_list`, call `nav::register_list(s_favorites_list)`; in the overlay delete/close path, call `nav::clear_list()`
- [x] 3.2 Group overlay (`settings.cpp` `settings_open_group_overlay`): after building the group `lv_list`, call `nav::register_list(list)`; in the overlay close callback and explicit delete paths, call `nav::clear_list()`
- [x] 3.3 Room picker (`settings.cpp` `settings_open_room_picker`): after building the picker list, call `nav::register_list(picker_list)`; on picker close, call `nav::clear_list()`
- [x] 3.4 Settings overlay (`settings.cpp` `screens::settings_open`): assess whether the main settings panel uses an `lv_list` or custom button rows; if `lv_list`, add `register_list` / `clear_list`; if custom rows, document that this overlay is not keyboard-navigable in v1

## 4. Verification

- [x] 4.1 Build both PlatformIO envs (`crowpanel_21_round` and `ref_rect_stub`) — confirm zero errors and warnings
- [x] 4.2 Flash and manually verify: open favorites overlay → turn encoder → selection indicator appears on first item; subsequent turns move indicator; press selects item; modal close destroys indicator; volume control resumes after close
- [x] 4.3 Capture `/screenshot` with selection indicator visible on a favorites item and attach to the PR
- [x] 4.4 Add CHANGELOG.md entry under `## [Unreleased]`: rotary encoder navigates modal overlays
