#pragma once
#include <lvgl.h>

/* ── Semantic font roles ─────────────────────────────────────────────────────
 *
 * Screens select type by role, never by pixel size.
 * Each density class (FONTSET_MEDIUM, etc.) provides one face per role.
 * Changing FONTSET via build_flags switches the whole face set.
 *
 * For the bootstrap, all roles map to LVGL's built-in Montserrat faces.
 * Custom subset fonts (Latin-1 + used icons) will replace them in a
 * later change once the icon set is finalised.
 */

enum class FontRole : uint8_t {
    Display,    ///< Large time/counter display (screensaver clock, big number)
    Title,      ///< Track title, primary labels
    Body,       ///< Artist name, secondary text
    Caption,    ///< Room name, small metadata
    Icon,       ///< Icon glyphs
    COUNT,
};

/// Return the lv_font_t* for the given role in the active density class.
const lv_font_t* font(FontRole role);

/* ── Compile-time role-completeness check ───────────────────────────────────
 *
 * The font() function is implemented in fonts.cpp.  Any density class that
 * does not provide a face for every role will fall into the default case and
 * trigger the assertion at runtime.  A link-time check is not possible for
 * runtime-selected fonts, but the test in task 10.x will exercise this path.
 */
