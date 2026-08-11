#include "nav.h"
#include "theme/accent.h"

namespace {
    lv_obj_t* s_list  = nullptr;
    int32_t   s_index = -1;

    void clear_highlight(lv_obj_t* item) {
        if (!item) return;
        lv_obj_set_style_border_width(item, 0, 0);
    }

    void apply_highlight(lv_obj_t* item) {
        if (!item) return;
        lv_obj_set_style_border_width(item, 3, 0);
        lv_obj_set_style_border_color(item, accent::get(), 0);
        lv_obj_set_style_border_opa(item, LV_OPA_COVER, 0);
    }
}

void nav::register_list(lv_obj_t* list_obj) {
    s_list  = list_obj;
    s_index = -1;
}

void nav::clear_list() {
    if (s_list && lv_obj_is_valid(s_list) && s_index >= 0) {
        clear_highlight(lv_obj_get_child(s_list, s_index));
    }
    s_list  = nullptr;
    s_index = -1;
}

bool nav::has_modal() {
    lv_obj_t* layer = lv_layer_top();
    uint32_t  n     = lv_obj_get_child_count(layer);
    for (uint32_t i = 0; i < n; i++) {
        if (!lv_obj_has_flag(lv_obj_get_child(layer, i), LV_OBJ_FLAG_HIDDEN)) return true;
    }
    return false;
}

bool nav::has_selection() {
    return s_list != nullptr && s_index >= 0;
}

void nav::navigate(int delta) {
    if (!s_list || !lv_obj_is_valid(s_list)) {
        s_list  = nullptr;
        s_index = -1;
        return;
    }

    int32_t count = (int32_t)lv_obj_get_child_count(s_list);
    if (count <= 0) return;

    int32_t prev_index = s_index;
    if (s_index < 0) {
        s_index = 0;  // first turn: land on the first item regardless of direction
    } else {
        int32_t next = s_index + (delta > 0 ? 1 : -1);
        if (next < 0) next = 0;
        if (next >= count) next = count - 1;
        s_index = next;
    }

    if (prev_index >= 0 && prev_index != s_index) {
        clear_highlight(lv_obj_get_child(s_list, prev_index));
    }
    lv_obj_t* item = lv_obj_get_child(s_list, s_index);
    apply_highlight(item);
    lv_obj_scroll_to_view(item, LV_ANIM_ON);
}

void nav::select() {
    if (!s_list || !lv_obj_is_valid(s_list)) {
        s_list  = nullptr;
        s_index = -1;
        return;
    }
    if (s_index < 0) return;

    lv_obj_t* item = lv_obj_get_child(s_list, s_index);
    if (item) lv_obj_send_event(item, LV_EVENT_CLICKED, nullptr);
}
