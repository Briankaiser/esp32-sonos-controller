#pragma once
#include "../display_profile.h"

/* ── ref_rect_stub: 800×480 rectangular reference profile ───────────────────
 *
 * This board support package exists ONLY to prove the display abstraction.
 * It declares the second layout archetype so that compiling it confirms no
 * screen code references the round board's geometry.
 *
 * It is not expected to be hardware-verified. The panel driver is a stub;
 * touch always reports "no touch"; there is no encoder.
 */

inline constexpr bsp::DisplayProfile kDisplay{
    .w            = 800,
    .h            = 480,
    .diag_mm_x10  = 889,    ///< approx 3.5" diagonal × 25.4 × 10
    .ppi          = 267,
    .shape        = bsp::Shape::Rect,
    .has_encoder  = false,
    .has_touch    = true,
};

namespace pins {
    constexpr int kSda       = 38;
    constexpr int kScl       = 39;
    constexpr int BACKLIGHT  = 6;
    constexpr int TOUCH_ADDR = 0x14;  ///< GT911 default address
} // namespace pins

#ifndef OPENSONOS_SHAPE_RECT
#define OPENSONOS_SHAPE_RECT
#endif
