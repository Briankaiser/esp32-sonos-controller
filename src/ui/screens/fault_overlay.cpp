#include "fault_overlay.h"
#include "../layout/layout.h"
#include "../theme/fonts.h"
#include "../theme/accent.h"
#include "../../services/prefs.h"
#include "../../app/sonos/state.h"
#include <lvgl.h>
#include <Arduino.h>
#include <WiFi.h>

// ── State (UI task only) ──────────────────────────────────────────────────────

namespace {

enum class Fault { None, NoWifi, NoSonos, HomeRoomOffline };

lv_obj_t* s_overlay      = nullptr;    // current fault overlay (on lv_layer_top), or null
lv_obj_t* s_confirm      = nullptr;    // re-setup confirm dialog, or null
Fault     s_shown        = Fault::None;
Fault     s_candidate    = Fault::None;
int       s_candidate_ct = 0;
uint32_t  s_last_eval_ms = 0;
uint32_t  s_wifi_zero_since_ms = 0;    // when WiFi-up-with-zero-speakers began (0 = not in that state)
String    s_home_name;                 // last-known home-room friendly name (for the offline screen)

constexpr uint32_t kEvalIntervalMs = 500;   // re-evaluate cadence
constexpr uint32_t kNoSonosWindowMs = 15000; // bounded search before declaring "no Sonos"

// ── Fault diagnosis (local signals only) ──────────────────────────────────────

Fault evaluate() {
    if (WiFi.status() != WL_CONNECTED) {
        s_wifi_zero_since_ms = 0;
        return Fault::NoWifi;
    }
    SonosState st = sonos::state::snapshot();
    if (st.speakers.empty()) {
        if (s_wifi_zero_since_ms == 0) s_wifi_zero_since_ms = millis();
        if (millis() - s_wifi_zero_since_ms >= kNoSonosWindowMs) return Fault::NoSonos;
        return Fault::None;  // still within the initial search window — transient
    }
    s_wifi_zero_since_ms = 0;
    if (st.home_room_idx >= 0 && st.home_room_idx < (int)st.speakers.size()) {
        s_home_name = st.speakers[st.home_room_idx].friendly_name;  // cache for offline naming
        return Fault::None;
    }
    // Home room unresolved: a fault only if one is actually configured.
    if (!prefs::home_room().isEmpty()) return Fault::HomeRoomOffline;
    return Fault::None;
}

// ── Overlay construction ──────────────────────────────────────────────────────

void close_confirm() {
    if (s_confirm) { lv_obj_delete(s_confirm); s_confirm = nullptr; }
}

// Full-screen dark overlay that absorbs background touches. Content is placed on
// a card constrained to SLOT_LIST (the inscribed safe-area square) so all text
// and controls stay within the round display's safe area.
lv_obj_t* make_overlay_card(lv_obj_t** overlay_out) {
    lv_color_t ac = accent::get();
    lv_obj_t* ov = lv_obj_create(lv_layer_top());
    lv_obj_set_size(ov, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(ov, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(ov, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(ov, 0, 0);
    lv_obj_set_style_pad_all(ov, 0, 0);
    lv_obj_remove_flag(ov, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(ov, LV_OBJ_FLAG_CLICKABLE);  // swallow taps meant for the screen beneath

    const LayoutSlot& ls = slot(SLOT_LIST);
    lv_obj_t* card = lv_obj_create(ov);
    lv_obj_set_pos(card, ls.x, ls.y);
    lv_obj_set_size(card, ls.w, ls.h);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x1A1A1A), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, ac, 0);
    lv_obj_set_style_border_opa(card, LV_OPA_40, 0);
    lv_obj_set_style_radius(card, 18, 0);
    lv_obj_set_style_pad_all(card, 0, 0);
    lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_layout(card, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(card, 14, 0);
    lv_obj_set_style_pad_hor(card, 18, 0);

    *overlay_out = ov;
    return card;
}

void add_title(lv_obj_t* card, const char* text) {
    lv_color_t ac = accent::get();
    lv_obj_t* t = lv_label_create(card);
    lv_label_set_text(t, text);
    lv_obj_set_style_text_font(t, font(FontRole::Title), 0);
    lv_obj_set_style_text_color(t, ac, 0);
    lv_obj_set_style_text_align(t, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(t, lv_pct(90));
    lv_label_set_long_mode(t, LV_LABEL_LONG_WRAP);
}

void add_body(lv_obj_t* card, const char* text) {
    lv_obj_t* b = lv_label_create(card);
    lv_label_set_text(b, text);
    lv_obj_set_style_text_font(b, font(FontRole::Body), 0);
    lv_obj_set_style_text_color(b, lv_color_hex(0xBBBBBB), 0);
    lv_obj_set_style_text_align(b, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(b, lv_pct(90));
    lv_label_set_long_mode(b, LV_LABEL_LONG_WRAP);
}

// ── Re-setup confirmation ─────────────────────────────────────────────────────

void open_confirm() {
    if (s_confirm) return;
    lv_color_t ac = accent::get();
    lv_obj_t* card = make_overlay_card(&s_confirm);

    add_title(card, "Reset WiFi Setup?");
    add_body(card, "This clears saved WiFi and restarts the device into setup mode.");

    lv_obj_t* row = lv_obj_create(card);
    lv_obj_set_size(row, lv_pct(96), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_layout(row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Cancel
    lv_obj_t* cancel = lv_button_create(row);
    lv_obj_set_style_bg_color(cancel, lv_color_hex(0x333333), 0);
    lv_obj_set_style_radius(cancel, 10, 0);
    lv_obj_t* cl = lv_label_create(cancel);
    lv_label_set_text(cl, "Cancel");
    lv_obj_set_style_text_font(cl, font(FontRole::Body), 0);
    lv_obj_center(cl);
    lv_obj_add_event_cb(cancel, [](lv_event_t*) { close_confirm(); }, LV_EVENT_CLICKED, nullptr);

    // Confirm — destructive: full factory reset + reboot to portal
    lv_obj_t* confirm = lv_button_create(row);
    lv_obj_set_style_bg_color(confirm, ac, 0);
    lv_obj_set_style_radius(confirm, 10, 0);
    lv_obj_t* fl = lv_label_create(confirm);
    lv_label_set_text(fl, "Reset");
    lv_obj_set_style_text_font(fl, font(FontRole::Body), 0);
    lv_obj_set_style_text_color(fl, lv_color_hex(0x111111), 0);
    lv_obj_center(fl);
    lv_obj_add_event_cb(confirm, [](lv_event_t*) {
        prefs::factory_reset();  // wipes NVS → unprovisioned, then ESP.restart()
    }, LV_EVENT_CLICKED, nullptr);
}

// ── Overlay show/hide per fault ───────────────────────────────────────────────

void rebuild(Fault f) {
    close_confirm();
    if (s_overlay) { lv_obj_delete(s_overlay); s_overlay = nullptr; }
    if (f == Fault::None) return;

    lv_obj_t* card = make_overlay_card(&s_overlay);
    switch (f) {
        case Fault::NoWifi: {
            add_title(card, "No WiFi Connection");
            add_body(card, "Can't reach your WiFi network.");
            lv_color_t ac = accent::get();
            lv_obj_t* btn = lv_button_create(card);
            lv_obj_set_style_bg_color(btn, ac, 0);
            lv_obj_set_style_radius(btn, 10, 0);
            lv_obj_t* bl = lv_label_create(btn);
            lv_label_set_text(bl, "Re-run WiFi Setup");
            lv_obj_set_style_text_font(bl, font(FontRole::Body), 0);
            lv_obj_set_style_text_color(bl, lv_color_hex(0x111111), 0);
            lv_obj_center(bl);
            lv_obj_add_event_cb(btn, [](lv_event_t*) { open_confirm(); }, LV_EVENT_CLICKED, nullptr);
            break;
        }
        case Fault::NoSonos:
            add_title(card, "No Sonos Found");
            add_body(card, "No Sonos system on the network. Check that your Sonos is powered on and connected to WiFi.");
            break;
        case Fault::HomeRoomOffline: {
            add_title(card, "Room Offline");
            String msg = s_home_name.isEmpty()
                ? String("Your home room isn't responding. Reconnecting automatically.")
                : (s_home_name + " isn't responding. Reconnecting automatically.");
            add_body(card, msg.c_str());
            break;
        }
        case Fault::None:
            break;
    }
}

} // namespace

// ── Public entry point ────────────────────────────────────────────────────────

void screens::fault_overlay_tick() {
    // Freeze evaluation while the destructive confirm dialog is up — the user is
    // mid-decision and the screen must not be rebuilt out from under them.
    if (s_confirm) return;

    uint32_t now = millis();
    if (now - s_last_eval_ms < kEvalIntervalMs) return;
    s_last_eval_ms = now;

    Fault f = evaluate();
    if (f == s_shown) { s_candidate = f; s_candidate_ct = 0; return; }

    // Debounce: require the changed state to persist two consecutive evaluations
    // (~1s) before switching, so a momentary WiFi bounce or discovery blip does
    // not flash the overlay in or out.
    if (f == s_candidate) {
        s_candidate_ct++;
    } else {
        s_candidate = f;
        s_candidate_ct = 1;
    }
    if (s_candidate_ct >= 2) {
        s_shown = f;
        s_candidate_ct = 0;
        rebuild(f);
    }
}
