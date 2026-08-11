#pragma once
/* lvgl.h stub for native host tests.
 *
 * Provides only the types and inline no-op functions that the pure-logic
 * modules (layout.cpp, accent.cpp, fonts.cpp) reference. No rendering
 * takes place; all LVGL calls are silent no-ops.
 *
 * Font variables use inline constexpr (C++17) so their addresses are
 * stable across translation units. This lets fonts.cpp take &lv_font_*
 * addresses without a separate object file.
 *
 * Do not add implementations here that simulate real behaviour — stubs
 * that are too clever hide bugs in the modules under test.
 */

#include <cstdint>
#include <cstddef>
#include <climits>

// ── Macros from lv_conf.h that modules check with static_assert ─────────────
#define LV_COLOR_DEPTH            16
#define LV_FONT_MONTSERRAT_12     1
#define LV_FONT_MONTSERRAT_14     1
#define LV_FONT_MONTSERRAT_16     1
#define LV_FONT_MONTSERRAT_20     1
#define LV_FONT_MONTSERRAT_28     1
#define LV_FONT_MONTSERRAT_36     1

// ── Color ───────────────────────────────────────────────────────────────────
typedef struct {
    uint8_t blue;
    uint8_t green;
    uint8_t red;
} lv_color_t;

typedef uint8_t lv_opa_t;

#define LV_OPA_COVER   255
#define LV_OPA_TRANSP  0

inline lv_color_t lv_color_hex(uint32_t c) {
    return { (uint8_t)(c & 0xFF), (uint8_t)((c >> 8) & 0xFF), (uint8_t)((c >> 16) & 0xFF) };
}
inline lv_color_t lv_color_make(uint8_t r, uint8_t g, uint8_t b) {
    return { b, g, r };
}
inline uint32_t lv_color_to_u32(lv_color_t c) {
    return ((uint32_t)c.red << 16) | ((uint32_t)c.green << 8) | (uint32_t)c.blue;
}

// ── Font ────────────────────────────────────────────────────────────────────
typedef struct { int _unused; } lv_font_t;

// Inline definitions so accent.cpp and fonts.cpp can take their addresses
inline constexpr lv_font_t lv_font_montserrat_12 = {};
inline constexpr lv_font_t lv_font_montserrat_14 = {};
inline constexpr lv_font_t lv_font_montserrat_16 = {};
inline constexpr lv_font_t lv_font_montserrat_20 = {};
inline constexpr lv_font_t lv_font_montserrat_28 = {};
inline constexpr lv_font_t lv_font_montserrat_36 = {};

// ── Style ───────────────────────────────────────────────────────────────────
typedef struct { int _unused; } lv_style_t;
typedef uint32_t lv_style_prop_t;
typedef uint32_t lv_style_selector_t;
typedef uint32_t lv_part_t;

#define LV_PART_MAIN       0
#define LV_PART_INDICATOR  2

// Style property IDs used in accent.cpp
#define LV_STYLE_ARC_COLOR      ((lv_style_prop_t)0x0A3)
#define LV_STYLE_BG_COLOR       ((lv_style_prop_t)0x020)
#define LV_STYLE_BORDER_COLOR   ((lv_style_prop_t)0x030)

inline void lv_style_init(lv_style_t*)                                           {}
inline void lv_style_set_color(lv_style_t*, lv_style_prop_t, lv_color_t)         {}
inline lv_color_t lv_style_get_color(lv_style_t*, lv_style_prop_t, void*)        { return {}; }

// ── Object ──────────────────────────────────────────────────────────────────
typedef struct lv_obj_t lv_obj_t;  // opaque forward declaration
typedef uint32_t lv_obj_flag_t;

#define LV_OBJ_FLAG_SCROLLABLE       ((lv_obj_flag_t)(1 << 4))
#define LV_OBJ_FLAG_OVERFLOW_VISIBLE ((lv_obj_flag_t)(1 << 20))
#define LV_OBJ_FLAG_CLICKABLE        ((lv_obj_flag_t)(1 << 1))

inline lv_obj_t* lv_obj_create(lv_obj_t*)                                         { return nullptr; }
inline void lv_obj_set_pos(lv_obj_t*, int32_t, int32_t)                           {}
inline void lv_obj_set_size(lv_obj_t*, int32_t, int32_t)                          {}
inline void lv_obj_set_style_pad_all(lv_obj_t*, int32_t, lv_style_selector_t)     {}
inline void lv_obj_set_style_border_width(lv_obj_t*, int32_t, lv_style_selector_t){}
inline void lv_obj_set_style_bg_opa(lv_obj_t*, lv_opa_t, lv_style_selector_t)    {}
inline void lv_obj_add_flag(lv_obj_t*, lv_obj_flag_t)                             {}
inline void lv_obj_remove_flag(lv_obj_t*, lv_obj_flag_t)                          {}
inline void lv_obj_add_style(lv_obj_t*, lv_style_t*, lv_part_t)                  {}
inline void lv_obj_report_style_change(lv_style_t*)                               {}
inline void lv_obj_invalidate(lv_obj_t*)                                           {}
inline void lv_obj_set_style_arc_color(lv_obj_t*, lv_color_t, lv_style_selector_t)    {}
inline void lv_obj_set_style_bg_color(lv_obj_t*, lv_color_t, lv_style_selector_t)     {}
inline void lv_obj_set_style_border_color(lv_obj_t*, lv_color_t, lv_style_selector_t) {}

// ── Color mixing (accent fade) ──────────────────────────────────────────────
inline lv_color_t lv_color_mix(lv_color_t c1, lv_color_t c2, lv_opa_t mix) {
    lv_color_t out;
    out.red   = (uint8_t)((c1.red   * mix + c2.red   * (255 - mix)) / 255);
    out.green = (uint8_t)((c1.green * mix + c2.green * (255 - mix)) / 255);
    out.blue  = (uint8_t)((c1.blue  * mix + c2.blue  * (255 - mix)) / 255);
    return out;
}

// ── Animation (accent fade) ─────────────────────────────────────────────────
struct lv_anim_t { int _unused; };
typedef void (*lv_anim_exec_xcb_t)(void*, int32_t);
typedef void (*lv_anim_completed_cb_t)(lv_anim_t*);
typedef int32_t (*lv_anim_path_cb_t)(const lv_anim_t*);

inline void    lv_anim_init(lv_anim_t*)                                  {}
inline void    lv_anim_set_exec_cb(lv_anim_t*, lv_anim_exec_xcb_t)       {}
inline void    lv_anim_set_completed_cb(lv_anim_t*, lv_anim_completed_cb_t) {}
inline void    lv_anim_set_var(lv_anim_t*, void*)                        {}
inline void    lv_anim_set_values(lv_anim_t*, int32_t, int32_t)          {}
inline void    lv_anim_set_duration(lv_anim_t*, uint32_t)                {}
inline void    lv_anim_set_path_cb(lv_anim_t*, lv_anim_path_cb_t)        {}
inline void    lv_anim_start(lv_anim_t*)                                 {}
inline int32_t lv_anim_path_ease_out(const lv_anim_t*)                   { return 0; }

// ── Display ─────────────────────────────────────────────────────────────────
typedef struct lv_display_t lv_display_t;

// ── lv_pct helper (used in placeholder.cpp but included via LVGL) ───────────
inline int32_t lv_pct(int32_t x) { return -(x | 0x8000); }  // LVGL's percent encoding

// ── RADIUS ──────────────────────────────────────────────────────────────────
#define LV_RADIUS_CIRCLE (INT16_MAX)
