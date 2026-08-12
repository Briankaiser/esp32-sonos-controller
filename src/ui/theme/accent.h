#pragma once
#include <lvgl.h>
#include <cstdint>

/* ── Accent colour token ─────────────────────────────────────────────────────
 *
 * One colour token drives emphasis across the whole interface.
 * Setting it transitions smoothly to the new colour (LVGL animation).
 *
 * Updated by the album-art change after dominant-colour extraction.
 * Used by: volume arc, progress bar, control button highlights.
 */

namespace accent {

/// Set the accent colour.  Transition is animated over `duration_ms`.
void set(lv_color_t color, uint32_t duration_ms = 400);

/// Return the current accent colour (pre-transition value).
lv_color_t get();

/// Register an lv_obj_t* whose bg/border/arc/shadow colour tracks the accent
/// token. The object's style is updated automatically when set() is called.
/// Supported props: LV_STYLE_BG_COLOR, LV_STYLE_BORDER_COLOR,
/// LV_STYLE_ARC_COLOR (INDICATOR), LV_STYLE_SHADOW_COLOR.
void bind(lv_obj_t* obj, lv_style_prop_t prop);

/// Bind the LV_PART_INDICATOR bg_color to the accent (for bars).
void bind_indicator(lv_obj_t* obj);

/// Bind the LV_PART_MAIN bg_color to a luminance-derived contrast colour
/// (black when accent is light, white when accent is dark) so the bar track
/// always reads clearly against the fill.
void bind_contrast_track(lv_obj_t* obj);

/// Ensure the accent colour has adequate contrast against the given background.
/// If too close, nudges lightness until the contrast ratio is acceptable.
lv_color_t adjusted_for_legibility(lv_color_t accent, lv_color_t background);

} // namespace accent
