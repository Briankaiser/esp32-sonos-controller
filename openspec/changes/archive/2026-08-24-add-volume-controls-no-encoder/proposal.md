## Why

Boards without a rotary encoder have no way to adjust volume from the device. The volume arc is also visually inappropriate on a rectangular display — a curved arc belongs on the round panel; a flat bar suits the rect layout. This change adds ± tap controls for no-encoder boards and repositions the volume widget to the top edge on rectangular boards.

## What Changes

- Board config already includes `has_encoder: bool` in `DisplayProfile` — no change needed there.
- On boards where `has_encoder = false` (e.g. `ref_rect_stub`): replace the placeholder slider in the now-playing volume slot with a horizontal bar flanked by a `−` and `+` button. Each tap adjusts volume by ±5.
- On rectangular boards (`shape = Rect`): move the volume widget slot to the top edge of the right column (flat, short strip) instead of its current lower-right position, so the layout reads top-to-bottom: volume bar → track info → controls.
- The volume arc (encoder path) is unchanged on round/encoder boards.

## Capabilities

### New Capabilities

(none)

### Modified Capabilities

- `now-playing-screen`: Volume display adapts to the board profile — flat bar with ± buttons on no-encoder boards; arc on encoder boards. Volume slot position moves to top edge on rectangular boards.
- `sonos-volume`: Touch-based ± buttons become an additional volume input path for boards without an encoder. Encoder-based control is unchanged.

## Impact

- `src/ui/layout/archetype_landscape_wide.h` — SLOT_VOLUME_ARC moved to top of right column; SLOT_TRACK_INFO, SLOT_TRACK_INFO_COMPACT, SLOT_LYRICS shifted down.
- `src/ui/screens/now_playing.cpp` — no-encoder branch replaces placeholder slider with `lv_bar` + two `lv_button` objects; `now_playing_update_volume_arc()` and `now_playing_update()` updated to handle both `s_volume_arc` and `s_volume_bar`.
- `src/app/sonos/playback.h` — uses existing `enqueue_volume_delta(int16_t)` for button callbacks; no API change.
- No BSP changes. No new board config fields. No changes to round-board code paths.
