#include "layout.h"
#include <lvgl.h>

/**
 * Create a container object positioned and sized from the named slot.
 *
 * The container is placed once; LVGL flex/percent governs content
 * within it. Overflow is clipped so content cannot escape the slot
 * boundary (and therefore cannot escape the safe area on round displays).
 *
 * Screen code must only call this function — never lv_obj_set_pos() or
 * lv_obj_set_size() directly.
 */
lv_obj_t* layout_container(lv_obj_t* parent, SlotId id) {
    const LayoutSlot& s = slot(id);

    lv_obj_t* obj = lv_obj_create(parent);

    lv_obj_set_pos(obj, s.x, s.y);
    lv_obj_set_size(obj, s.w, s.h);

    // Remove default padding and border so the slot is usable edge-to-edge
    lv_obj_set_style_pad_all(obj, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, LV_PART_MAIN);

    // In LVGL 9, children are clipped to the parent by default (no flag needed).
    // To ALLOW overflow you would add LV_OBJ_FLAG_OVERFLOW_VISIBLE — we never do.

    // Remove scroll (slots are fixed containers, not scrollable by default)
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);

    return obj;
}
