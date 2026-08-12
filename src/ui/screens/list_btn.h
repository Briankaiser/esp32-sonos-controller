#pragma once
#include <lvgl.h>
#include "../nav.h"
#include "../theme/fonts.h"
#include "../theme/accent.h"

// Shared helper for all modal list buttons: strips theme styles, applies the
// accent left-bar indicator for FOCUSED (touch), USER_1 (encoder nav), and
// PRESSED states, and configures the label for single-line truncation.
inline void apply_list_btn_style(lv_obj_t* btn, lv_color_t text_default) {
    lv_color_t ac = accent::get();
    lv_obj_remove_style_all(btn);
    lv_obj_set_height(btn, 72);
    lv_obj_set_layout(btn, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x1A1A1A), 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_set_style_text_color(btn, text_default, 0);
    lv_obj_set_style_text_font(btn, font(FontRole::Title), 0);
    lv_obj_set_style_pad_hor(btn, 20, 0);
    lv_obj_set_style_pad_ver(btn, 0, 0);
    lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    for (lv_state_t st : {(lv_state_t)LV_STATE_FOCUSED,
                          (lv_state_t)LV_STATE_USER_1,
                          (lv_state_t)LV_STATE_PRESSED}) {
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x222222), st);
        lv_obj_set_style_border_side(btn, LV_BORDER_SIDE_LEFT, st);
        lv_obj_set_style_border_width(btn, 4, st);
        lv_obj_set_style_border_color(btn, ac, st);
        lv_obj_set_style_border_opa(btn, LV_OPA_COVER, st);
        lv_obj_set_style_text_color(btn, ac, st);
    }
    // Last child is always the text label (accounts for optional icon child)
    lv_obj_t* lbl = lv_obj_get_child(btn, lv_obj_get_child_count(btn) - 1);
    if (lbl) {
        lv_obj_set_size(lbl, lv_pct(100), lv_font_get_line_height(font(FontRole::Title)));
        lv_label_set_long_mode(lbl, LV_LABEL_LONG_DOT);
        lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_LEFT, 0);
    }
}
