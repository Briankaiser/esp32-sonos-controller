## Context

`now_playing.cpp` already branches on `if constexpr (kDisplay.has_encoder)` when building the volume slot. The encoder branch creates a circular `lv_arc` (`s_volume_arc`). The no-encoder branch currently places a placeholder `lv_slider` that is not wired to any command. `archetype_landscape_wide.h` positions `SLOT_VOLUME_ARC` at `y=72%, h=18%` — near the bottom of the right column. `sonos::playback::enqueue_volume_delta(int16_t)` is the existing LVGL-task-safe API for sending volume adjustments.

## Goals / Non-Goals

**Goals:**
- Replace the no-encoder placeholder with a functional flat bar + ± buttons.
- Reposition the volume slot to the top of the right column on the rect archetype.
- Keep the encoder (round) code path completely unchanged.

**Non-Goals:**
- Long-press repeat on the buttons (not requested).
- Volume adjustment on the round board via touch (encoder is the input there).
- Any new board config fields — `has_encoder` and `shape` are already present.

## Decisions

**Bar + flanking buttons, not bar-only:**  
A bar with no affordance gives no visual hint that it is interactive. Two labeled buttons (−, +) make the interaction unambiguous and large enough to tap reliably.

**±5 step size:**  
5-unit steps match Sonos' own app behaviour and give 20 distinct levels, which is enough granularity for casual adjustment without requiring many taps.

**Optimistic bar update on tap:**  
The button callbacks call `enqueue_volume_delta(±5)` (enqueues a SOAP command) and also bump `s_volume_bar` immediately by ±5. This mirrors how the arc receives instant feedback from the encoder ISR. The next polled state will correct any drift. Without this the bar appears frozen for ~2 s per tap.

**No `s_volume_arc` / `s_volume_bar` union — separate static handles:**  
Exactly one of the two is non-null at runtime (chosen at compile time), so the update functions check which is set and route accordingly. No shared type needed.

**SLOT_VOLUME_ARC stays at top in rect archetype:**  
The archetype slot table is the single source of layout truth. Moving the slot moves the whole widget. Cascading slot adjustments (SLOT_TRACK_INFO etc.) maintain the column's stacking order without touching screen code.

## Risks / Trade-offs

- `ref_rect_stub` has a stub panel driver that does not render, so the bar + buttons cannot be screenshot-verified on hardware. Both envs must compile; visual correctness can only be confirmed when real rect hardware is available.
- The optimistic bump assumes the SOAP command succeeds. A failed command will be corrected on the next poll. This is the same behaviour the encoder already uses and is acceptable.
