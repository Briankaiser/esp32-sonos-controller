#include "settings.h"
#include "../nav.h"
#include "../layout/layout.h"
#include "../theme/fonts.h"
#include "../../app/sonos/playback.h"
#include "../../app/sonos/eventing.h"
#include "../../app/sonos/state.h"
#include "../../app/sonos/discovery.h"
#include "../../app/lyrics/service.h"
#include "../../services/prefs.h"
#include "../theme/accent.h"
#include "list_btn.h"
#include <algorithm>
#include <lvgl.h>
#include <WiFi.h>

// ── Statics (UI task only) ────────────────────────────────────────────────────

static lv_obj_t* s_settings_overlay = nullptr;
static lv_obj_t* s_room_btn         = nullptr;
static lv_obj_t* s_gvol_btn         = nullptr;
static lv_obj_t* s_lyrics_btn       = nullptr;
static lv_obj_t* s_picker           = nullptr;
static lv_obj_t* s_group_overlay    = nullptr;

// ── Helpers ───────────────────────────────────────────────────────────────────

static String speaker_label(const SpeakerInfo& sp) {
    if (sp.model_name.isEmpty()) return sp.friendly_name;
    return sp.friendly_name + " - " + sp.model_name;
}

// Case-insensitive ordering by friendly name so room/speaker lists in every
// modal read alphabetically regardless of discovery order. Ties fall back to
// uuid for a stable order between rebuilds.
static bool speaker_name_less(const SpeakerInfo& a, const SpeakerInfo& b) {
    auto lc = [](char c) -> char { return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c; };
    const String& an = a.friendly_name;
    const String& bn = b.friendly_name;
    uint32_t n = an.length() < bn.length() ? an.length() : bn.length();
    for (uint32_t i = 0; i < n; i++) {
        char ca = lc(an[i]), cb = lc(bn[i]);
        if (ca != cb) return ca < cb;
    }
    if (an.length() != bn.length()) return an.length() < bn.length();
    return a.uuid < b.uuid;
}

static lv_obj_t* make_list_btn(lv_obj_t* list, const char* text) {
    lv_obj_t* btn = lv_list_add_button(list, nullptr, text);
    apply_list_btn_style(btn, lv_color_hex(0xEEEEEE));
    return btn;
}

static void apply_card_style(lv_obj_t* card, lv_color_t ac) {
    lv_obj_set_style_bg_color(card, lv_color_hex(0x1A1A1A), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, ac, 0);
    lv_obj_set_style_border_opa(card, LV_OPA_40, 0);
    lv_obj_set_style_radius(card, 18, 0);
    lv_obj_set_style_clip_corner(card, true, 0);
    lv_obj_set_style_pad_all(card, 0, 0);
    lv_obj_set_style_shadow_width(card, 24, 0);
    lv_obj_set_style_shadow_spread(card, 6, 0);
    lv_obj_set_style_shadow_color(card, ac, 0);
    lv_obj_set_style_shadow_opa(card, LV_OPA_30, 0);
    lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);
}

static lv_obj_t* make_card_header(lv_obj_t* card, int32_t card_w, const char* title,
                                   lv_color_t ac) {
    lv_obj_t* hd = lv_obj_create(card);
    lv_obj_set_pos(hd, 0, 0);
    lv_obj_set_size(hd, card_w, 52);
    lv_obj_set_style_bg_color(hd, ac, 0);
    lv_obj_set_style_bg_opa(hd, LV_OPA_30, 0);
    lv_obj_set_style_border_side(hd, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_width(hd, 2, 0);
    lv_obj_set_style_border_color(hd, ac, 0);
    lv_obj_set_style_border_opa(hd, LV_OPA_50, 0);
    lv_obj_set_style_radius(hd, 0, 0);
    lv_obj_set_style_pad_all(hd, 0, 0);
    lv_obj_remove_flag(hd, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t* lbl = lv_label_create(hd);
    lv_label_set_text(lbl, title);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(lbl, ac, 0);
    lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 20, 0);
    return hd;
}

static lv_obj_t* make_inner_list(lv_obj_t* card, int32_t card_w, int32_t y, int32_t h) {
    lv_obj_t* list = lv_list_create(card);
    lv_obj_set_pos(list, 0, y);
    lv_obj_set_size(list, card_w, h);
    lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(list, 0, 0);
    lv_obj_set_style_radius(list, 0, 0);
    lv_obj_set_style_pad_all(list, 0, 0);
    lv_obj_set_style_pad_row(list, 4, 0);
    return list;
}

static lv_obj_t* make_back_btn(lv_obj_t* card, int32_t card_w, int32_t card_h,
                                const char* label, lv_color_t ac) {
    lv_obj_t* btn = lv_obj_create(card);
    lv_obj_set_pos(btn, 0, card_h - 72);
    lv_obj_set_size(btn, card_w, 72);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x141414), 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_side(btn, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_border_width(btn, 1, 0);
    lv_obj_set_style_border_color(btn, ac, 0);
    lv_obj_set_style_border_opa(btn, LV_OPA_20, 0);
    lv_obj_set_style_radius(btn, 0, 0);
    lv_obj_set_style_pad_all(btn, 0, 0);
    lv_obj_remove_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t* lbl = lv_label_create(btn);
    lv_label_set_text(lbl, label);
    lv_obj_set_style_text_font(lbl, font(FontRole::Body), 0);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0x888888), 0);
    lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 0);
    return btn;
}

static void close_settings() {
    if (s_settings_overlay) {
        lv_obj_delete(s_settings_overlay);
        s_settings_overlay = nullptr;
    }
    s_room_btn = nullptr;
    s_gvol_btn = nullptr;
    s_lyrics_btn = nullptr;
    nav::clear_list();
}

// ── show_room_picker ─────────────────────────────────────────────────

static void show_room_picker(lv_event_t*) {
    if (s_picker) return;

    s_picker = lv_obj_create(lv_layer_top());
    lv_obj_set_size(s_picker, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(s_picker, lv_color_hex(0x000000), 0);
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
    lv_color_t ac = accent::get();
    const int32_t cw = ls.w - 16;
    const int32_t ch = ls.h - 16;

    lv_obj_t* card = lv_obj_create(s_picker);
    lv_obj_set_pos(card, ls.x + 8, ls.y + 8);
    lv_obj_set_size(card, cw, ch);
    apply_card_style(card, ac);

    make_card_header(card, cw, "Select Room", ac);

    lv_obj_t* list = make_inner_list(card, cw, 52, ch - 52 - 72);

    SonosState st = sonos::state::snapshot();
    // Alphabetical by friendly name (room picker doesn't index into speakers).
    std::sort(st.speakers.begin(), st.speakers.end(), speaker_name_less);

    if (st.speakers.empty()) {
        lv_obj_t* btn = lv_list_add_button(list, nullptr, "No speakers found");
        lv_obj_set_height(btn, 72);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x1A1A1A), 0);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
        lv_obj_set_style_text_color(btn, lv_color_hex(0x888888), 0);
        lv_obj_set_style_text_font(btn, font(FontRole::Title), 0);
        lv_obj_set_style_pad_hor(btn, 20, 0);
        lv_obj_set_style_pad_ver(btn, 0, 0);
        lv_obj_remove_flag(btn, LV_OBJ_FLAG_CLICKABLE);
    } else {
        for (const auto& sp : st.speakers) {
            // Bonded-set satellites (home theater surround/sub, non-primary side of
            // a stereo pair) aren't independently playable — never offer them as a
            // home room.
            if (sp.invisible) continue;

            String label = speaker_label(sp);
            lv_obj_t* btn = lv_list_add_button(list, nullptr, label.c_str());
            apply_list_btn_style(btn, lv_color_hex(0xEEEEEE));

            String* uuid_copy = new String(sp.uuid);
            lv_obj_set_user_data(btn, uuid_copy);

            // Ownership: this button always owns its String* until LVGL destroys
            // the widget, whether that happens via the click handler below or via
            // lv_obj_delete(s_picker) from the tap-outside/Back paths. Freeing
            // unconditionally here (rather than only on click) is what closes the
            // leak — every button not clicked previously leaked its String forever.
            lv_obj_add_event_cb(btn, [](lv_event_t* e) {
                lv_obj_t* b = static_cast<lv_obj_t*>(lv_event_get_target(e));
                delete static_cast<String*>(lv_obj_get_user_data(b));
            }, LV_EVENT_DELETE, nullptr);

            lv_obj_add_event_cb(btn, [](lv_event_t* e) {
                lv_obj_t* b = static_cast<lv_obj_t*>(lv_event_get_target(e));
                String* uuid = static_cast<String*>(lv_obj_get_user_data(b));
                if (uuid) {
                    prefs::set_home_room(*uuid);
                    // Re-resolve home_room_idx immediately so the polling loop
                    // switches rooms on its next wake rather than waiting up to
                    // kDiscoveryIntervalMs (10 min) for the next discovery cycle.
                    SonosState st2 = sonos::state::snapshot();
                    String new_label = "Home Room: Not set";
                    for (int i = 0; i < (int)st2.speakers.size(); i++) {
                        if (st2.speakers[i].uuid == *uuid) {
                            new_label = "Home Room: " + speaker_label(st2.speakers[i]);
                            st2.home_room_idx = i;
                            break;
                        }
                    }
                    sonos::state::update(st2);
                    // Drop GENA subscriptions so event_task re-subscribes to the
                    // new speaker; healthy() goes false, so sonos_task falls back
                    // to polling and fetches fresh state from the new room.
                    sonos::eventing::request_reset();
                    // Re-derive the group coordinator for the new home room: the
                    // stored coordinator UUID still points at the previous room's
                    // group until a topology refresh, which would otherwise mislead
                    // poll/art/eventing targeting until the next discovery cycle.
                    sonos::discovery::request_topology_refresh();
                    sonos::playback::notify_poll_task();
                    if (s_room_btn) {
                        lv_obj_t* lbl = lv_obj_get_child(s_room_btn, 0);
                        if (lbl) lv_label_set_text(lbl, new_label.c_str());
                    }
                }
                if (s_picker) { lv_obj_delete(s_picker); s_picker = nullptr; }
                nav::clear_list();
            }, LV_EVENT_CLICKED, nullptr);
        }
    }

    lv_obj_t* back_btn = make_back_btn(card, cw, ch, "Back", ac);
    lv_obj_add_event_cb(back_btn, [](lv_event_t*) {
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
    lv_obj_set_style_bg_color(s_group_overlay, lv_color_hex(0x000000), 0);
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
    lv_color_t ac = accent::get();
    const int32_t cw = ls.w - 16;
    const int32_t ch = ls.h - 16;

    lv_obj_t* card = lv_obj_create(s_group_overlay);
    lv_obj_set_pos(card, ls.x + 8, ls.y + 8);
    lv_obj_set_size(card, cw, ch);
    apply_card_style(card, ac);

    make_card_header(card, cw, "Groups", ac);

    lv_obj_t* list = make_inner_list(card, cw, 52, ch - 52 - 72);

    auto make_btn = [&](const char* text, lv_color_t color) -> lv_obj_t* {
        lv_obj_t* btn = lv_list_add_button(list, nullptr, text);
        apply_list_btn_style(btn, color);
        lv_obj_set_ext_click_area(btn, 8);
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

    // Alphabetical by friendly name. Safe to reorder here: home_uuid and
    // group_member_uuids were already resolved above and the loop keys off
    // sp.uuid, not speaker index.
    std::sort(st.speakers.begin(), st.speakers.end(), speaker_name_less);

    for (const auto& sp : st.speakers) {
        if (sp.uuid == home_uuid) continue;
        // Bonded-set satellites aren't independently playable — never offer them
        // as a grouping target.
        if (sp.invisible) continue;

        String display_name = speaker_label(sp);
        bool in_group = std::find(group_member_uuids.begin(), group_member_uuids.end(),
                                  sp.uuid) != group_member_uuids.end();
        const char* icon = in_group ? LV_SYMBOL_OK : nullptr;
        lv_color_t color = in_group ? lv_color_hex(0xFFFFFF) : lv_color_hex(0xBBBBBB);

        struct BtnData { String uuid; bool in_group; };
        BtnData* data = new BtnData{ sp.uuid, in_group };

        lv_obj_t* btn = lv_list_add_button(list, icon, display_name.c_str());
        apply_list_btn_style(btn, color);
        lv_obj_set_ext_click_area(btn, 8);
        lv_obj_set_user_data(btn, data);

        // Ownership: this button always owns its BtnData* until LVGL destroys the
        // widget, whether that happens via the click handler below or via
        // lv_obj_delete(s_group_overlay) from tap-outside/Go-Solo/another button's
        // click. Freeing unconditionally here (rather than only on click) is what
        // closes the leak — every button not clicked previously leaked its BtnData
        // forever, and with up to a dozen-plus discovered speakers this adds up
        // fast over weeks of normal Settings use.
        lv_obj_add_event_cb(btn, [](lv_event_t* e) {
            lv_obj_t* b = static_cast<lv_obj_t*>(lv_event_get_target(e));
            delete static_cast<BtnData*>(lv_obj_get_user_data(b));
        }, LV_EVENT_DELETE, nullptr);

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
            }
            if (s_group_overlay) { lv_obj_delete(s_group_overlay); s_group_overlay = nullptr; }
            nav::clear_list();
        }, LV_EVENT_CLICKED, nullptr);
    }

    lv_obj_t* back_btn = make_back_btn(card, cw, ch, "Back", ac);
    lv_obj_add_event_cb(back_btn, [](lv_event_t*) {
        if (s_group_overlay) { lv_obj_delete(s_group_overlay); s_group_overlay = nullptr; }
        nav::clear_list();
    }, LV_EVENT_CLICKED, nullptr);

    nav::register_list(list);
}

// ── screens::settings_open ────────────────────────────────────────────────────

void screens::settings_open() {
    if (s_settings_overlay) return;

    s_settings_overlay = lv_obj_create(lv_layer_top());
    lv_obj_set_size(s_settings_overlay, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(s_settings_overlay, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(s_settings_overlay, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_settings_overlay, 0, 0);
    lv_obj_remove_flag(s_settings_overlay, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_add_event_cb(s_settings_overlay, [](lv_event_t* e) {
        if (lv_event_get_target(e) == s_settings_overlay) {
            close_settings();
        }
    }, LV_EVENT_CLICKED, nullptr);

    const LayoutSlot& ls = slot(SLOT_LIST);
    lv_color_t ac = accent::get();
    const int32_t cw = ls.w - 16;
    const int32_t ch = ls.h - 16;

    lv_obj_t* card = lv_obj_create(s_settings_overlay);
    lv_obj_set_pos(card, ls.x + 8, ls.y + 8);
    lv_obj_set_size(card, cw, ch);
    apply_card_style(card, ac);

    make_card_header(card, cw, "Settings", ac);

    lv_obj_t* list = make_inner_list(card, cw, 52, ch - 52 - 72);

    // ── Home Room entry ───────────────────────────────────────────────────────
    SonosState st = sonos::state::snapshot();
    String room_label = "Home Room: ";
    room_label += (st.home_room_idx >= 0)
        ? speaker_label(st.speakers[st.home_room_idx])
        : String("Not set");

    s_room_btn = make_list_btn(list, room_label.c_str());
    lv_obj_set_style_text_color(s_room_btn, lv_color_hex(0xFFFFFF), 0);
    lv_obj_add_event_cb(s_room_btn, show_room_picker, LV_EVENT_CLICKED, nullptr);

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

    // ── Lyrics toggle ─────────────────────────────────────────────────────────
    bool lyrics_on = prefs::lyrics_enabled();
    const char* lyrics_text = lyrics_on ? "Lyrics: On" : "Lyrics: Off";
    s_lyrics_btn = make_list_btn(list, lyrics_text);
    lv_obj_add_event_cb(s_lyrics_btn, [](lv_event_t*) {
        bool next = !prefs::lyrics_enabled();
        prefs::set_lyrics_enabled(next);
        lyrics::service::set_enabled(next);   // clears shown lyrics / refetches for the current track
        lv_obj_t* lbl = lv_obj_get_child(s_lyrics_btn, 0);
        if (lbl) lv_label_set_text(lbl, next ? "Lyrics: On" : "Lyrics: Off");
    }, LV_EVENT_CLICKED, nullptr);

    // ── Device IP + SSID — debug aid, not interactive ────────────────────────
    String ip_text = "IP: " + WiFi.localIP().toString() + "   " + WiFi.SSID();
    lv_obj_t* ip_lbl = lv_list_add_text(list, ip_text.c_str());
    lv_obj_set_style_text_font(ip_lbl, font(FontRole::Caption), 0);
    lv_obj_set_style_text_color(ip_lbl, lv_color_hex(0x555555), 0);

    nav::register_list(list);

    lv_obj_t* back_btn = make_back_btn(card, cw, ch, "Back", ac);
    lv_obj_add_event_cb(back_btn, [](lv_event_t*) { close_settings(); },
                        LV_EVENT_CLICKED, nullptr);
}

void screens::settings_open_room_picker() {
    show_room_picker(nullptr);
}
