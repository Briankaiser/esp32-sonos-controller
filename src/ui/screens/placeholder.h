#pragma once
#include <lvgl.h>

namespace screens {

/**
 * Placeholder root screen.
 *
 * Composed entirely through named layout slots — no pixel literals.
 * Exists only to confirm that:
 *   1. Both board envs compile the screen without modification.
 *   2. Layout slots resolve correctly for each archetype.
 *   3. The round display's dead corners contain nothing (runtime clipping).
 *   4. LVGL renders and the frame-rate overlay is visible.
 */
lv_obj_t* placeholder_create();

} // namespace screens
