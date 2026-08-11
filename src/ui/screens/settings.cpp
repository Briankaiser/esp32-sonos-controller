#include "settings.h"
#include "../nav.h"
#include "../layout/layout.h"
#include "../theme/fonts.h"
#include "../../app/sonos/playback.h"
#include "../../app/sonos/state.h"
#include "../../services/prefs.h"
#include <algorithm>
#include <lvgl.h>

// ── Statics (UI task only) ────────────────────────────────────────────────────

static lv_obj_t* s_settings_overlay = nullptr;
static lv_obj_t* s_room_btn         = nullptr;
static lv_obj_t* s_gvol_btn         = nullptr;
static lv_obj_t* s_picker           = nullptr;
static lv_obj_t* s_group_overlay    = nullptr;

// ── Helpers ───────────────────────────────────────────────────────────────────

static String speaker_label(const SpeakerInfo& sp) {
    if (sp.model_name.isEmpty()) return sp.friendly_name;
    return sp.friendly_name + " - " + sp.model_name;
}

static lv_obj_t* make_list_btn(lv_obj_t* list, const char* text) {
    lv_obj_t* btn = lv_list_add_button(list, nullptr, text);
    lv_obj_set_height(btn, 64);
    lv_obj_set_style_text_font(btn, font(FontRole::Body), 0);
    lv_obj_set_style_text_color(btn, lv_color_hex(0xDDDDDD), 0);
    lv_obj_set_style_pad_all(btn, 10, 0);
    lv_obj_t* lbl = lv_obj_get_child(btn, 0);
    if (lbl) {
        lv_label_set_long_mode(lbl, LV_LABEL_LONG_DOT);
        lv_obj_set_width(lbl, lv_pct(100));
    }
    return btn;
}

static void close_settings() {
    if (s_settings_overlay) {
        lv_obj_delete(s_settings_overlay);
        s_settings_overlay = nullptr;
    }
    s_room_btn = nullptr;
    s_gvol_btn = nullptr;
    nav::clear_list();
}

// ── settings_open_room_picker ─────────────────────────────────────────────────

static void settings_open_room_picker(lv_event_t*) {
    if (s_picker) return;

    s_picker = lv_obj_create(lv_layer_top());
    lv_obj_set_size(s_picker, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(s_picker, lv_color_hex(0x111111), 0);
    lv_obj_set_style_bg_opa(s_picker, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_picker, 0, 0);
    lv_obj_remove_flag(s_picker, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_add_event_cb(s_picker, [](lv_event_t* e) {
        if (lv_event_get_target(e) == s_picker) {
            lv_obj_delete(s_picker);
            s_picker = nullptr;
            nav::clear_list();
        }
    }, LV_EVENT_CLICKED, nullptr);

    const LayoutSlot& ls = slot(SLOT_LIST);
    lv_obj_t* list = lv_list_create(s_picker);
    lv_obj_set_pos(list, ls.x + 8, ls.y + 8);
    lv_obj_set_size(list, ls.w - 16, ls.h - 16);
    lv_obj_set_style_bg_color(list, lv_color_hex(0x1E1E1E), 0);
    lv_obj_set_style_border_width(list, 0, 0);
    lv_obj_set_style_radius(list, 16, 0);

    SonosState st = sonos::state::snapshot();

    if (st.speakers.empty()) {
        lv_obj_t* btn = lv_list_add_button(list, nullptr, "No speakers found");
        lv_obj_set_height(btn, 64);
        lv_obj_set_style_text_color(btn, lv_color_hex(0x888888), 0);
        lv_obj_set_style_text_font(btn, font(FontRole::Body), 0);
        lv_obj_remove_flag(btn, LV_OBJ_FLAG_CLICKABLE);
    } else {
        for (const auto& sp : st.speakers) {
            String label = speaker_label(sp);
            lv_obj_t* btn = lv_list_add_button(list, nullptr, label.c_str());
            lv_obj_set_height(btn, 64);
            lv_obj_set_style_text_color(btn, lv_color_hex(0xFFFFFF), 0);
            lv_obj_set_style_text_font(btn, font(FontRole::Body), 0);
            lv_obj_set_style_pad_all(btn, 10, 0);
            lv_obj_t* lbl = lv_obj_get_child(btn, 0);
            if (lbl) { lv_label_set_long_mode(lbl, LV_LABEL_LONG_DOT); lv_obj_set_width(lbl, lv_pct(100)); }

            String* uuid_copy = new String(sp.uuid);
            lv_obj_set_user_data(btn, uuid_copy);

            lv_obj_add_event_cb(btn, [](lv_event_t* e) {
                lv_obj_t* b = static_cast<lv_obj_t*>(lv_event_get_target(e));
                String* uuid = static_cast<String*>(lv_obj_get_user_data(b));
                if (uuid) {
                    prefs::set_home_room(*uuid);
                    SonosState st2 = sonos::state::snapshot();
                    String new_label = "Home Room: Not set";
                    for (const auto& s : st2.speakers) {
                        if (s.uuid == *uuid) {
                            new_label = "Home Room: " + speaker_label(s);
                            break;
                        }
                    }
                    if (s_room_btn) {
                        lv_obj_t* lbl = lv_obj_get_child(s_room_btn, 0);
                        if (lbl) lv_label_set_text(lbl, new_label.c_str());
                    }
                    delete uuid;
                    lv_obj_set_user_data(b, nullptr);
                }
                if (s_picker) { lv_obj_delete(s_picker); s_picker = nullptr; }
                nav::clear_list();
            }, LV_EVENT_CLICKED, nullptr);
        }
    }

    lv_obj_t* cancel_btn = lv_list_add_button(list, nullptr, "Cancel");
    lv_obj_set_height(cancel_btn, 64);
    lv_obj_set_style_text_color(cancel_btn, lv_color_hex(0xAAAAAA), 0);
    lv_obj_set_style_text_font(cancel_btn, font(FontRole::Body), 0);
    lv_obj_set_style_pad_all(cancel_btn, 10, 0);
    lv_obj_add_event_cb(cancel_btn, [](lv_event_t*) {
        if (s_picker) { lv_obj_delete(s_picker); s_picker = nullptr; }
        nav::clear_list();
    }, LV_EVENT_CLICKED, nullptr);

    nav::register_list(list);
}

// ── settings_open_group_overlay ───────────────────────────────────────────────

static void settings_open_group_overlay(lv_event_t*) {
    if (s_group_overlay) return;

    s_group_overlay = lv_obj_create(lv_layer_top());
    lv_obj_set_size(s_group_overlay, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(s_group_overlay, lv_color_hex(0x111111), 0);
    lv_obj_set_style_bg_opa(s_group_overlay, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_group_overlay, 0, 0);
    lv_obj_remove_flag(s_group_overlay, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_add_event_cb(s_group_overlay, [](lv_event_t* e) {
        if (lv_event_get_target(e) == s_group_overlay) {
            lv_obj_delete(s_group_overlay);
            s_group_overlay = nullptr;
            nav::clear_list();
        }
    }, LV_EVENT_CLICKED, nullptr);

    const LayoutSlot& ls = slot(SLOT_LIST);
    lv_obj_t* list = lv_list_create(s_group_overlay);
    lv_obj_set_pos(list, ls.x + 8, ls.y + 8);
    lv_obj_set_size(list, ls.w - 16, ls.h - 16);
    lv_obj_set_style_bg_color(list, lv_color_hex(0x1E1E1E), 0);
    lv_obj_set_style_border_width(list, 0, 0);
    lv_obj_set_style_radius(list, 16, 0);

    auto make_btn = [&](const char* text, lv_color_t color) -> lv_obj_t* {
        lv_obj_t* btn = lv_list_add_button(list, nullptr, text);
        lv_obj_set_height(btn, 72);
        lv_obj_set_style_text_color(btn, color, 0);
        lv_obj_set_style_text_font(btn, font(FontRole::Title), 0);
        lv_obj_set_style_pad_all(btn, 12, 0);
        lv_obj_set_ext_click_area(btn, 8);
        lv_obj_t* lbl = lv_obj_get_child(btn, 0);
        if (lbl) {
            lv_label_set_long_mode(lbl, LV_LABEL_LONG_DOT);
            lv_obj_set_width(lbl, lv_pct(100));
        }
        return btn;
    };

    sonos::PlaybackState pb = sonos::playback::snapshot();
    SonosState st = sonos::state::snapshot();
    String home_uuid = (st.home_room_idx >= 0) ? st.speakers[st.home_room_idx].uuid : "";
    bool is_grouped = !pb.group_coordinator_uuid.isEmpty();

    std::vector<String> group_member_uuids;
    if (is_grouped) {
        for (const auto& grp : st.groups) {
            bool found = false;
            for (const auto& m : grp.member_uuids) {
                if (m == home_uuid) { found = true; break; }
            }
            if (grp.coordinator_uuid == home_uuid || found) {
                group_member_uuids = grp.member_uuids;
                if (std::find(group_member_uuids.begin(), group_member_uuids.end(),
                              grp.coordinator_uuid) == group_member_uuids.end()) {
                    group_member_uuids.push_back(grp.coordinator_uuid);
                }
                break;
            }
        }
    }

    if (is_grouped) {
        lv_obj_t* solo_btn = make_btn("Go Solo", lv_color_hex(0xFFDD44));
        lv_obj_add_event_cb(solo_btn, [](lv_event_t*) {
            sonos::playback::enqueue_command(sonos::PlaybackCommand::LeaveGroup);
            if (s_group_overlay) { lv_obj_delete(s_group_overlay); s_group_overlay = nullptr; }
            nav::clear_list();
        }, LV_EVENT_CLICKED, nullptr);
    }

    for (const auto& sp : st.speakers) {
        if (sp.uuid == home_uuid) continue;

        String display_name = speaker_label(sp);
        bool in_group = std::find(group_member_uuids.begin(), group_member_uuids.end(),
                                  sp.uuid) != group_member_uuids.end();
        const char* icon = in_group ? LV_SYMBOL_OK : nullptr;
        lv_color_t color = in_group ? lv_color_hex(0xFFFFFF) : lv_color_hex(0xBBBBBB);

        struct BtnData { String uuid; bool in_group; };
        BtnData* data = new BtnData{ sp.uuid, in_group };

        lv_obj_t* btn = lv_list_add_button(list, icon, display_name.c_str());
        lv_obj_set_height(btn, 72);
        lv_obj_set_style_text_color(btn, color, 0);
        lv_obj_set_style_text_font(btn, font(FontRole::Title), 0);
        lv_obj_set_style_pad_all(btn, 12, 0);
        lv_obj_set_ext_click_area(btn, 8);
        lv_obj_t* lbl = lv_obj_get_child(btn, lv_obj_get_child_count(btn) - 1);
        if (lbl) { lv_label_set_long_mode(lbl, LV_LABEL_LONG_DOT); lv_obj_set_width(lbl, lv_pct(85)); }
        lv_obj_set_user_data(btn, data);

        lv_obj_add_event_cb(btn, [](lv_event_t* e) {
            lv_obj_t* b = static_cast<lv_obj_t*>(lv_event_get_target(e));
            BtnData* d = static_cast<BtnData*>(lv_obj_get_user_data(b));
            if (d) {
                sonos::playback::set_group_op_target(d->uuid);
                if (d->in_group) {
                    sonos::playback::enqueue_command(sonos::PlaybackCommand::RemoveGroupMember);
                } else {
                    sonos::playback::enqueue_command(sonos::PlaybackCommand::AddGroupMember);
                }
                delete d;
                lv_obj_set_user_data(b, nullptr);
            }
            if (s_group_overlay) { lv_obj_delete(s_group_overlay); s_group_overlay = nullptr; }
            nav::clear_list();
        }, LV_EVENT_CLICKED, nullptr);
    }

    nav::register_list(list);
}

// ── screens::settings_open ────────────────────────────────────────────────────

void screens::settings_open() {
    if (s_settings_overlay) return;

    s_settings_overlay = lv_obj_create(lv_layer_top());
    lv_obj_set_size(s_settings_overlay, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(s_settings_overlay, lv_color_hex(0x111111), 0);
    lv_obj_set_style_bg_opa(s_settings_overlay, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_settings_overlay, 0, 0);
    lv_obj_remove_flag(s_settings_overlay, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_add_event_cb(s_settings_overlay, [](lv_event_t* e) {
        if (lv_event_get_target(e) == s_settings_overlay) {
            close_settings();
        }
    }, LV_EVENT_CLICKED, nullptr);

    const LayoutSlot& ls = slot(SLOT_LIST);
    lv_obj_t* list = lv_list_create(s_settings_overlay);
    lv_obj_set_pos(list, ls.x + 8, ls.y + 8);
    lv_obj_set_size(list, ls.w - 16, ls.h - 16);
    lv_obj_set_style_bg_color(list, lv_color_hex(0x1E1E1E), 0);
    lv_obj_set_style_border_width(list, 0, 0);
    lv_obj_set_style_radius(list, 16, 0);

    // ── Home Room entry ───────────────────────────────────────────────────────
    SonosState st = sonos::state::snapshot();
    String room_label = "Home Room: ";
    room_label += (st.home_room_idx >= 0)
        ? speaker_label(st.speakers[st.home_room_idx])
        : String("Not set");

    s_room_btn = make_list_btn(list, room_label.c_str());
    lv_obj_set_style_text_color(s_room_btn, lv_color_hex(0xFFFFFF), 0);
    lv_obj_add_event_cb(s_room_btn, settings_open_room_picker, LV_EVENT_CLICKED, nullptr);

    // ── Manage Groups entry ───────────────────────────────────────────────────
    lv_obj_t* groups_btn = make_list_btn(list, "Manage Groups");
    lv_obj_add_event_cb(groups_btn, settings_open_group_overlay, LV_EVENT_CLICKED, nullptr);

    // ── Group Volume toggle ───────────────────────────────────────────────────
    bool gvol = prefs::group_volume_mode();
    const char* gvol_text = gvol ? "Group Volume: Whole group" : "Group Volume: Home room only";
    s_gvol_btn = make_list_btn(list, gvol_text);
    lv_obj_add_event_cb(s_gvol_btn, [](lv_event_t*) {
        bool current = prefs::group_volume_mode();
        prefs::set_group_volume_mode(!current);
        lv_obj_t* lbl = lv_obj_get_child(s_gvol_btn, 0);
        if (lbl) {
            lv_label_set_text(lbl,
                !current ? "Group Volume: Whole group" : "Group Volume: Home room only");
        }
    }, LV_EVENT_CLICKED, nullptr);

    nav::register_list(list);
}
