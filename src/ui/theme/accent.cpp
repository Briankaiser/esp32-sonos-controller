#include "accent.h"
#include <lvgl.h>
#include <vector>

/* ── Implementation ─────────────────────────────────────────────────────────
 *
 * Bootstrap version: colour updates are immediate (no animation).
 * The add-album-art-accent-theming change will add animated crossfades
 * once dominant-colour extraction is wired up.
 *
 * Binding approach: each bound object stores its own lv_style_t and a
 * function pointer for the LVGL 9 property setter (e.g. lv_obj_set_style_arc_color).
 * When set() is called we update every bound style and trigger a redraw.
 */

namespace {

using StyleColorSetter = void(*)(lv_obj_t*, lv_color_t, lv_style_selector_t);

struct Binding {
    lv_obj_t*          obj;
    StyleColorSetter   set_fn;
    lv_style_selector_t selector;
};

static lv_color_t         s_current = lv_color_hex(0x1DB954);  // default: Spotify green
static std::vector<Binding> s_bindings;

static constexpr uint8_t kMinLumDiff = 48;

} // anonymous namespace

void accent::set(lv_color_t color, uint32_t /*duration_ms*/) {
    s_current = color;
    for (const auto& b : s_bindings) {
        b.set_fn(b.obj, color, b.selector);
        lv_obj_invalidate(b.obj);
    }
}

lv_color_t accent::get() {
    return s_current;
}

void accent::bind(lv_obj_t* obj, lv_style_prop_t prop) {
    // Map the generic property to a concrete LVGL 9 setter.
    // Only the subset of properties we actually use needs to be handled.
    StyleColorSetter fn = nullptr;
    lv_style_selector_t sel = LV_PART_MAIN;

    switch (prop) {
        case LV_STYLE_ARC_COLOR:
            fn = [](lv_obj_t* o, lv_color_t c, lv_style_selector_t s) {
                lv_obj_set_style_arc_color(o, c, s);
            };
            sel = LV_PART_INDICATOR;
            break;
        case LV_STYLE_BG_COLOR:
            fn = [](lv_obj_t* o, lv_color_t c, lv_style_selector_t s) {
                lv_obj_set_style_bg_color(o, c, s);
            };
            break;
        case LV_STYLE_BORDER_COLOR:
            fn = [](lv_obj_t* o, lv_color_t c, lv_style_selector_t s) {
                lv_obj_set_style_border_color(o, c, s);
            };
            break;
        default:
            return; // unsupported property — ignore silently
    }

    s_bindings.push_back({obj, fn, sel});
    fn(obj, s_current, sel);
}

lv_color_t accent::adjusted_for_legibility(lv_color_t ac, lv_color_t bg) {
    auto lum = [](lv_color_t c) -> int32_t {
        return (299 * c.red + 587 * c.green + 114 * c.blue) / 1000;
    };
    int32_t diff = lum(ac) - lum(bg);
    if (diff < 0) diff = -diff;
    if (diff >= kMinLumDiff) return ac;

    bool darken = lum(ac) > 128;
    uint8_t step = kMinLumDiff / 2;
    lv_color_t adj = ac;
    for (int i = 0; i < 10; ++i) {
        int32_t d = lum(adj) - lum(bg);
        if (d < 0) d = -d;
        if (d >= kMinLumDiff) break;
        if (darken) {
            adj.red   = adj.red   > step ? adj.red   - step : 0;
            adj.green = adj.green > step ? adj.green - step : 0;
            adj.blue  = adj.blue  > step ? adj.blue  - step : 0;
        } else {
            adj.red   = static_cast<uint8_t>(adj.red   + step < 255 ? adj.red   + step : 255);
            adj.green = static_cast<uint8_t>(adj.green + step < 255 ? adj.green + step : 255);
            adj.blue  = static_cast<uint8_t>(adj.blue  + step < 255 ? adj.blue  + step : 255);
        }
    }
    return adj;
}
