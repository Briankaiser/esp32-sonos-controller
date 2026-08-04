#pragma once
#include <stdint.h>

namespace bsp {

enum class Shape : uint8_t { Rect, Round };

/**
 * Compile-time description of a display.
 *
 * This is the ONLY thing UI code sees about hardware geometry. Screens
 * must derive all sizes and positions from the active profile — never from
 * hardcoded pixel values or board names.
 *
 * One instance lives in each board's board_config.h as:
 *   inline constexpr DisplayProfile kDisplay{ ... };
 */
struct DisplayProfile {
    uint16_t w;             ///< Framebuffer width in pixels
    uint16_t h;             ///< Framebuffer height in pixels
    uint16_t diag_mm_x10;  ///< Physical diagonal × 10 (e.g. 21 = 2.1 inch → 533 tenths-mm)
    uint16_t ppi;          ///< Pixels per inch → selects font density bucket
    Shape    shape;        ///< Panel shape — determines safe-area geometry
    bool     has_encoder;  ///< True if a rotary encoder is present
    bool     has_touch;    ///< True if a touch digitizer is present
};

} // namespace bsp
