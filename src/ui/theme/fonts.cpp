#include "fonts.h"
#include <cassert>

/* ── FONTSET_MEDIUM density class ───────────────────────────────────────────
 *
 * Maps every FontRole to a built-in Montserrat face.
 * All faces must be enabled in lv_conf.h (LV_FONT_MONTSERRAT_xx = 1).
 *
 * To switch to custom subset fonts: replace the face pointers below and
 * include the generated font .c files in the build.  The role enum and
 * this function's signature do not change.
 */

// Compile-time check that all built-in faces are enabled
static_assert(LV_FONT_MONTSERRAT_36 == 1,
    "FontRole::Display needs LV_FONT_MONTSERRAT_36 enabled in lv_conf.h");
static_assert(LV_FONT_MONTSERRAT_20 == 1,
    "FontRole::Title needs LV_FONT_MONTSERRAT_20 enabled in lv_conf.h");
static_assert(LV_FONT_MONTSERRAT_16 == 1,
    "FontRole::Body needs LV_FONT_MONTSERRAT_16 enabled in lv_conf.h");
static_assert(LV_FONT_MONTSERRAT_12 == 1,
    "FontRole::Caption needs LV_FONT_MONTSERRAT_12 enabled in lv_conf.h");
static_assert(LV_FONT_MONTSERRAT_14 == 1,
    "FontRole::Icon needs LV_FONT_MONTSERRAT_14 enabled in lv_conf.h");

// Face table — one entry per FontRole, must stay aligned with the enum
static const lv_font_t* const kFaces[static_cast<uint8_t>(FontRole::COUNT)] = {
    &lv_font_montserrat_36,   // Display
    &lv_font_montserrat_20,   // Title
    &lv_font_montserrat_16,   // Body
    &lv_font_montserrat_12,   // Caption
    &lv_font_montserrat_14,   // Icon
};

// Compile-time table completeness check
static_assert(sizeof(kFaces) / sizeof(kFaces[0]) == static_cast<uint8_t>(FontRole::COUNT),
    "kFaces table row count does not match FontRole::COUNT — add the missing role");

const lv_font_t* font(FontRole role) {
    const auto idx = static_cast<uint8_t>(role);
    assert(idx < static_cast<uint8_t>(FontRole::COUNT));
    return kFaces[idx];
}
