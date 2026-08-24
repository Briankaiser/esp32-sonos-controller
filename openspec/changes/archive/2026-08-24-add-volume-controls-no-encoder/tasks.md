## 1. Reposition Volume Slot in Rect Archetype

- [x] 1.1 In `src/ui/layout/archetype_landscape_wide.h`, move `SLOT_VOLUME_ARC` to `y=0%, h=10%` (top edge of the right column, flat strip).
- [x] 1.2 Shift `SLOT_TRACK_INFO` from `y=5%` to `y=11%` and adjust height to `h=26%` to fill the vacated space below the volume strip.
- [x] 1.3 Shift `SLOT_TRACK_INFO_COMPACT` and `SLOT_LYRICS` downward by the same 6% offset so stacking order is preserved.

## 2. Replace Placeholder with Bar + Buttons in No-Encoder Branch

- [x] 2.1 In `src/ui/screens/now_playing.cpp`, add `static lv_obj_t* s_volume_bar = nullptr;` alongside the existing `s_volume_arc` declaration.
- [x] 2.2 In the `else` branch of the `if constexpr (kDisplay.has_encoder)` volume block, remove the placeholder `lv_slider` and replace it with a flex-row container (LV_FLEX_FLOW_ROW, align center) holding: a `−` label button, an `lv_bar` (flex_grow=1), and a `+` label button. Assign the bar to `s_volume_bar`. Set bar range 0–100.
- [x] 2.3 Wire the `−` button's `LV_EVENT_CLICKED` handler: call `sonos::playback::enqueue_volume_delta(-5)` and immediately bump `s_volume_bar` value by −5 (clamped to 0) for optimistic feedback.
- [x] 2.4 Wire the `+` button's `LV_EVENT_CLICKED` handler: call `sonos::playback::enqueue_volume_delta(5)` and immediately bump `s_volume_bar` value by +5 (clamped to 100) for optimistic feedback.

## 3. Thread Volume State Through Update Functions

- [x] 3.1 In `now_playing_update_volume_arc()`, after the `s_volume_arc` branch, add an `else if (s_volume_bar)` branch that calls `lv_bar_set_value(s_volume_bar, v, LV_ANIM_OFF)`.
- [x] 3.2 In `now_playing_update()` volume section, apply the same mute-dimming style to `s_volume_bar` when muted (match the visual convention used for `s_volume_arc`).

## 4. Build Verification

- [x] 4.1 Build `crowpanel_21_round` env — must compile with zero errors/warnings. Confirm no changes to round-board rendering paths.
- [x] 4.2 Build `ref_rect_stub` env — must compile with zero errors/warnings.

## 5. CHANGELOG and Spec Validation

- [x] 5.1 Add a CHANGELOG.md entry under `## [Unreleased]` > `### Added` describing the volume bar and ± buttons on no-encoder boards.
- [x] 5.2 Run `openspec validate --all --strict` and confirm clean.
