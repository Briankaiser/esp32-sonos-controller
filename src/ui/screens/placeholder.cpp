#include "placeholder.h"
#include "../layout/layout.h"
#include "../theme/fonts.h"
#include <lvgl.h>

/* ── Placeholder root screen ────────────────────────────────────────────────
 *
 * Demonstrates that screen code positions content ONLY through named slots.
 * No lv_obj_set_pos/set_size/set_x/set_y with numeric literals appears here.
 * The CI lint rejects any such literal in src/ui/screens/ — see ci.yml.
 *
 * Round display: the root object is configured as a full-radius circle with
 * corner clipping enabled (layer-2 safe-area enforcement).  Content that
 * overflows a slot is invisible rather than rendered in the dead corners.
 */

namespace screens {

lv_obj_t* placeholder_create() {
    lv_obj_t* scr = lv_obj_create(nullptr);

    // Root object: clip to a circle on round profiles
    if constexpr (kDisplay.shape == bsp::Shape::Round) {
        lv_obj_set_style_radius(scr, LV_RADIUS_CIRCLE, LV_PART_MAIN);
        lv_obj_set_style_clip_corner(scr, true, LV_PART_MAIN);
    }
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x1A1A1A), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(scr, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(scr, 0, LV_PART_MAIN);

    // ── Album art slot — dark placeholder square ──────────────────────────
    lv_obj_t* art = layout_container(scr, SLOT_ALBUM_ART);
    lv_obj_set_style_bg_color(art, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(art, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(art, 8, LV_PART_MAIN);

    // ── Track info slot — label using Title role font ─────────────────────
    lv_obj_t* info = layout_container(scr, SLOT_TRACK_INFO);
    lv_obj_t* lbl  = lv_label_create(info);
    lv_label_set_text(lbl, "Sonos Controller");
    lv_obj_set_style_text_font(lbl, font(FontRole::Title), LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0xEEEEEE), LV_PART_MAIN);
    lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 0);

    // ── Transport slot — placeholder row of dots ──────────────────────────
    lv_obj_t* transport = layout_container(scr, SLOT_TRANSPORT);
    lv_obj_t* dots = lv_label_create(transport);
    lv_label_set_text(dots, "● ● ●");
    lv_obj_set_style_text_font(dots, font(FontRole::Caption), LV_PART_MAIN);
    lv_obj_set_style_text_color(dots, lv_color_hex(0x888888), LV_PART_MAIN);
    lv_obj_align(dots, LV_ALIGN_CENTER, 0, 0);

    // ── Progress slot — thin bar ──────────────────────────────────────────
    lv_obj_t* prog_cont = layout_container(scr, SLOT_PROGRESS);
    lv_obj_t* bar = lv_bar_create(prog_cont);
    lv_obj_set_size(bar, lv_pct(100), lv_pct(100));
    lv_bar_set_value(bar, 35, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0x444444), LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0x1DB954), LV_PART_INDICATOR);

    // ── Volume arc slot — thin arc ────────────────────────────────────────
    lv_obj_t* vol_cont = layout_container(scr, SLOT_VOLUME_ARC);
    if constexpr (kDisplay.has_encoder) {
        lv_obj_t* arc = lv_arc_create(vol_cont);
        lv_obj_set_size(arc, lv_pct(100), lv_pct(100));
        lv_arc_set_rotation(arc, 135);
        lv_arc_set_bg_angles(arc, 0, 270);
        lv_arc_set_value(arc, 50);
        lv_obj_set_style_arc_color(arc, lv_color_hex(0x1DB954), LV_PART_INDICATOR);
        lv_obj_set_style_arc_color(arc, lv_color_hex(0x333333), LV_PART_MAIN);
        lv_obj_set_style_arc_width(arc, 6, LV_PART_MAIN);
        lv_obj_set_style_arc_width(arc, 6, LV_PART_INDICATOR);
        lv_obj_remove_flag(arc, LV_OBJ_FLAG_CLICKABLE);
    } else {
        // No encoder — show a slider instead
        lv_obj_t* slider = lv_slider_create(vol_cont);
        lv_obj_set_size(slider, lv_pct(90), lv_pct(40));
        lv_obj_align(slider, LV_ALIGN_CENTER, 0, 0);
        lv_slider_set_value(slider, 50, LV_ANIM_OFF);
    }

    // ── Status slot — small label ─────────────────────────────────────────
    lv_obj_t* status_cont = layout_container(scr, SLOT_STATUS);
    lv_obj_t* status_lbl  = lv_label_create(status_cont);
    lv_label_set_text(status_lbl, "bootstrap");
    lv_obj_set_style_text_font(status_lbl, font(FontRole::Caption), LV_PART_MAIN);
    lv_obj_set_style_text_color(status_lbl, lv_color_hex(0x666666), LV_PART_MAIN);
    lv_obj_align(status_lbl, LV_ALIGN_CENTER, 0, 0);

#if OPENSONOS_DEBUG_OVERLAY
    // ── FPS + heap overlay (debug builds only) ────────────────────────────
    // Uses LVGL's built-in perf/mem monitors; positioned in status slot so
    // it stays within the safe area on round displays.
    lv_obj_t* overlay_cont = layout_container(scr, SLOT_STATUS);
    lv_obj_t* fps_lbl = lv_label_create(overlay_cont);
    lv_label_set_text(fps_lbl, "FPS: --");
    lv_obj_set_style_text_font(fps_lbl, font(FontRole::Caption), LV_PART_MAIN);
    lv_obj_set_style_text_color(fps_lbl, lv_color_hex(0xFFFF00), LV_PART_MAIN);
    lv_obj_align(fps_lbl, LV_ALIGN_TOP_LEFT, 0, 0);

    // Timer to refresh the fps label every 500 ms
    lv_timer_create([](lv_timer_t* t) {
        lv_obj_t* lbl = static_cast<lv_obj_t*>(lv_timer_get_user_data(t));
        uint32_t fps = lv_display_get_fps(lv_display_get_default());
        uint32_t free_kb = heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024;
        char buf[32];
        snprintf(buf, sizeof(buf), "%u fps  %uk", fps, free_kb);
        lv_label_set_text(lbl, buf);
    }, 500, fps_lbl);
#endif // OPENSONOS_DEBUG_OVERLAY

    return scr;
}

} // namespace screens
