## Context

`now_playing_update()` is called from the UI task whenever `playback::snapshot().version` changes. The very first call will carry whatever `transport_state` the first poll returned. Before that call, `PlaybackState` stays at its zero-value defaults (`transport_state = Unknown`). The overlay just needs to survive until that first non-Unknown update arrives.

The overlay is a plain `lv_obj_t` created as a child of the root screen object (`scr`). Because it is created last, it has the highest z-order within the screen and paints on top of all other children — no separate layer required, and no interference with the factory-reset overlay that lives on `lv_layer_top()`.

## Goals / Non-Goals

**Goals:** visible loading state from boot until first Sonos poll; animated removal.

**Non-Goals:** progress animation (spinner, etc.); retry logic; handling WiFi loss.

## Decisions

### D1: Child of screen, not lv_layer_top()

**Choice:** The overlay is a direct child of `scr`, not `lv_layer_top()`.

**Why:** `lv_layer_top()` is shared globally. The factory-reset countdown overlay already uses it. Creating another object there risks z-order conflicts. A screen-child object is cleaned up automatically when the screen is deleted and only covers this screen's content — exactly the correct scope.

### D2: One-shot removal on first non-Unknown update; does not reappear

**Choice:** A `static bool s_loading_dismissed` flag is set to `true` when the overlay is first removed. `now_playing_update()` checks this flag and skips the overlay logic entirely on all subsequent calls.

**Why:** `transport_state` can legitimately return to Unknown if the sonos task loses its state (not the current case, but defensive). Without a one-shot flag, the overlay could reappear. Since the only semantically meaningful moment is "first time we know the speaker state", the flag is the right gate.

### D3: 300 ms opacity fade-out via lv_anim_t, then delete

**Choice:** `lv_anim_t` animates `LV_STYLE_OPA` from 204 (≈80%) to 0 over 300 ms with ease-out. The completion callback deletes the overlay object and clears `s_loading_overlay`.

**Why:** Hard-deleting without animation is jarring. 300 ms is short enough not to delay the user but long enough to feel deliberate. Using LVGL's built-in animation path keeps the transition consistent with the accent colour animation used elsewhere.
