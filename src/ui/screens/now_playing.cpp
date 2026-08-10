#include "now_playing.h"
#include "../../app/sonos/favorites.h"
#include <algorithm>
#include "../layout/layout.h"
#include "../theme/fonts.h"
#include "../theme/accent.h"
#include "../../app/sonos/art.h"
#include "../../app/sonos/state.h"
#include "../../bsp/bsp.h"
#include <lvgl.h>

// ── Static widget handles (UI task only) ──────────────────────────────────────

static lv_obj_t*          s_title_lbl      = nullptr;
static lv_obj_t*          s_artist_lbl     = nullptr;
static lv_obj_t*          s_album_lbl      = nullptr;
static lv_obj_t*          s_play_btn_lbl   = nullptr;
static lv_obj_t*          s_progress_bar   = nullptr;
static lv_obj_t*          s_status_lbl     = nullptr;
static lv_obj_t*          s_transport_cont = nullptr;
static lv_obj_t*          s_volume_arc     = nullptr;
static lv_timer_t*        s_interp_timer   = nullptr;
static lv_obj_t*          s_art_img        = nullptr;
static lv_image_dsc_t     s_art_dsc        = {};
static uint32_t           s_art_version    = 0;
static lv_obj_t*          s_loading_overlay    = nullptr;
static bool               s_loading_dismissed  = false;
static lv_obj_t*          s_status_cont        = nullptr;
static lv_obj_t*          s_group_overlay      = nullptr;
static lv_obj_t*          s_favorites_overlay       = nullptr;
static lv_obj_t*          s_favorites_list          = nullptr;
static uint32_t           s_favorites_version_built = 0;
static lv_obj_t*          s_art_cont                = nullptr;
static sonos::PlaybackState s_last;

// ── Idle state ────────────────────────────────────────────────────────────────
static uint32_t           s_idle_timer_start   = 0;
static bool               s_is_idle            = false;
static constexpr uint32_t kIdleTimeoutMs       = 5u * 60u * 1000u;

// ── Interpolation timer (fires every 1 s on UI task) ─────────────────────────

static void interp_tick(lv_timer_t* /*t*/) {
    // Enter idle after kIdleTimeoutMs of non-Playing state
    if (!s_is_idle && millis() - s_idle_timer_start >= kIdleTimeoutMs) {
        s_is_idle = true;
        sonos::playback::idle_mode(true);
        bsp::backlight(0);
    }
    if (s_last.transport_state != sonos::TransportState::Playing) return;
    if (s_last.duration_s == 0) return;
    uint32_t elapsed_s = (millis() - s_last.poll_timestamp_ms) / 1000u;
    uint32_t pos = s_last.position_s + elapsed_s;
    if (pos > s_last.duration_s) pos = s_last.duration_s;
    int32_t pct = (int32_t)(pos * 100u / s_last.duration_s);
    lv_bar_set_value(s_progress_bar, pct, LV_ANIM_OFF);
}

// ── Helper: make a transparent, non-scrollable container for a slot ───────────

static lv_obj_t* slot_container(lv_obj_t* parent, SlotId id) {
    lv_obj_t* c = layout_container(parent, id);
    lv_obj_set_style_bg_opa(c, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(c, 0, 0);
    lv_obj_set_style_pad_all(c, 0, 0);
    lv_obj_remove_flag(c, LV_OBJ_FLAG_SCROLLABLE);
    return c;
}

// ── now_playing_create ────────────────────────────────────────────────────────

lv_obj_t* screens::now_playing_create() {
    // ── Root screen ──────────────────────────────────────────────────────────
    lv_obj_t* scr = lv_obj_create(nullptr);
    if constexpr (kDisplay.shape == bsp::Shape::Round) {
        lv_obj_set_style_radius(scr, LV_RADIUS_CIRCLE, LV_PART_MAIN);
        lv_obj_set_style_clip_corner(scr, true, LV_PART_MAIN);
    }
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x1A1A1A), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(scr, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(scr, 0, LV_PART_MAIN);

    // ── SLOT_ALBUM_ART — full-screen background; tap opens favorites ─────────
    s_art_cont = layout_container(scr, SLOT_ALBUM_ART);
    lv_obj_set_style_bg_color(s_art_cont, lv_color_hex(0x1A1A1A), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_art_cont, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_remove_flag(s_art_cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_art_cont, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_art_cont, [](lv_event_t*) {
        screens::now_playing_open_favorites_overlay();
    }, LV_EVENT_CLICKED, nullptr);

    s_art_img = lv_image_create(s_art_cont);
    lv_obj_set_size(s_art_img, lv_pct(100), lv_pct(100));
    lv_image_set_inner_align(s_art_img, LV_IMAGE_ALIGN_COVER);
    lv_image_set_src(s_art_img, nullptr);
    lv_obj_add_flag(s_art_img, LV_OBJ_FLAG_HIDDEN);

    // ── SLOT_TRACK_INFO — frosted overlay band with three stacked labels ──────
    lv_obj_t* info = slot_container(scr, SLOT_TRACK_INFO);
    // Frosted dark backdrop so art is visible but text stays readable
    lv_obj_set_style_bg_color(info, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(info, LV_OPA_50, 0);
    lv_obj_set_style_radius(info, 12, 0);
    lv_obj_set_layout(info, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(info, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(info, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(info, 8, 0);
    lv_obj_set_style_pad_row(info, 2, 0);

    s_title_lbl = lv_label_create(info);
    lv_label_set_text(s_title_lbl, "\xe2\x80\x94");
    lv_obj_set_style_text_font(s_title_lbl, font(FontRole::Display), 0);  // large title
    lv_obj_set_style_text_color(s_title_lbl, lv_color_hex(0xEEEEEE), 0);
    lv_obj_set_style_text_align(s_title_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(s_title_lbl, LV_LABEL_LONG_DOT);
    lv_obj_set_width(s_title_lbl, lv_pct(100));

    s_artist_lbl = lv_label_create(info);
    lv_label_set_text(s_artist_lbl, "");
    lv_obj_set_style_text_font(s_artist_lbl, font(FontRole::Title), 0);
    lv_obj_set_style_text_color(s_artist_lbl, lv_color_hex(0xAAAAAA), 0);
    lv_obj_set_style_text_align(s_artist_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(s_artist_lbl, LV_LABEL_LONG_DOT);
    lv_obj_set_width(s_artist_lbl, lv_pct(100));

    s_album_lbl = lv_label_create(info);
    lv_label_set_text(s_album_lbl, "");
    lv_obj_set_style_text_font(s_album_lbl, font(FontRole::Body), 0);
    lv_obj_set_style_text_color(s_album_lbl, lv_color_hex(0x777777), 0);
    lv_obj_set_style_text_align(s_album_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(s_album_lbl, LV_LABEL_LONG_DOT);
    lv_obj_set_width(s_album_lbl, lv_pct(100));

    // ── SLOT_TRANSPORT — prev / play-pause / next button row ─────────────────
    s_transport_cont = slot_container(scr, SLOT_TRANSPORT);
    lv_obj_set_layout(s_transport_cont, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(s_transport_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(s_transport_cont,
        LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Previous
    lv_obj_t* prev_btn = lv_button_create(s_transport_cont);
    lv_obj_set_flex_grow(prev_btn, 1);
    lv_obj_set_height(prev_btn, lv_pct(100));
    lv_obj_set_style_bg_color(prev_btn, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(prev_btn, LV_OPA_40, 0);  // translucent disc so icon pops on light art
    lv_obj_set_style_radius(prev_btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_margin_hor(prev_btn, 4, 0);
    lv_obj_set_style_border_width(prev_btn, 0, 0);
    lv_obj_set_style_shadow_width(prev_btn, 0, 0);
    lv_obj_t* prev_lbl = lv_label_create(prev_btn);
    lv_label_set_text(prev_lbl, LV_SYMBOL_PREV);
    lv_obj_set_style_text_font(prev_lbl, font(FontRole::Display), 0);
    lv_obj_set_style_text_color(prev_lbl, lv_color_hex(0xCCCCCC), 0);
    lv_obj_center(prev_lbl);
    lv_obj_add_event_cb(prev_btn, [](lv_event_t*) {
        sonos::playback::enqueue_command(sonos::PlaybackCommand::Previous);
        lv_obj_set_style_opa(s_transport_cont, LV_OPA_40, 0);
    }, LV_EVENT_PRESSED, nullptr);

    // Play/Pause
    lv_obj_t* play_btn = lv_button_create(s_transport_cont);
    lv_obj_set_flex_grow(play_btn, 1);
    lv_obj_set_height(play_btn, lv_pct(100));
    lv_obj_set_style_bg_color(play_btn, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(play_btn, LV_OPA_40, 0);
    lv_obj_set_style_radius(play_btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_margin_hor(play_btn, 4, 0);
    lv_obj_set_style_border_width(play_btn, 0, 0);
    lv_obj_set_style_shadow_width(play_btn, 0, 0);
    s_play_btn_lbl = lv_label_create(play_btn);
    lv_label_set_text(s_play_btn_lbl, LV_SYMBOL_PLAY);
    lv_obj_set_style_text_font(s_play_btn_lbl, font(FontRole::Display), 0);
    lv_obj_set_style_text_color(s_play_btn_lbl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(s_play_btn_lbl);
    lv_obj_add_event_cb(play_btn, [](lv_event_t*) {
        sonos::playback::enqueue_command(
            s_last.transport_state == sonos::TransportState::Playing
                ? sonos::PlaybackCommand::Pause
                : sonos::PlaybackCommand::Play);
        lv_obj_set_style_opa(s_transport_cont, LV_OPA_40, 0);
    }, LV_EVENT_PRESSED, nullptr);

    // Next
    lv_obj_t* next_btn = lv_button_create(s_transport_cont);
    lv_obj_set_flex_grow(next_btn, 1);
    lv_obj_set_height(next_btn, lv_pct(100));
    lv_obj_set_style_bg_color(next_btn, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(next_btn, LV_OPA_40, 0);
    lv_obj_set_style_radius(next_btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_margin_hor(next_btn, 4, 0);
    lv_obj_set_style_border_width(next_btn, 0, 0);
    lv_obj_set_style_shadow_width(next_btn, 0, 0);
    lv_obj_t* next_lbl = lv_label_create(next_btn);
    lv_label_set_text(next_lbl, LV_SYMBOL_NEXT);
    lv_obj_set_style_text_font(next_lbl, font(FontRole::Display), 0);
    lv_obj_set_style_text_color(next_lbl, lv_color_hex(0xCCCCCC), 0);
    lv_obj_center(next_lbl);
    lv_obj_add_event_cb(next_btn, [](lv_event_t*) {
        sonos::playback::enqueue_command(sonos::PlaybackCommand::Next);
        lv_obj_set_style_opa(s_transport_cont, LV_OPA_40, 0);
    }, LV_EVENT_PRESSED, nullptr);

    // ── SLOT_PROGRESS — progress bar ─────────────────────────────────────────
    lv_obj_t* prog_cont = slot_container(scr, SLOT_PROGRESS);
    s_progress_bar = lv_bar_create(prog_cont);
    lv_obj_set_size(s_progress_bar, lv_pct(100), lv_pct(100));
    lv_bar_set_range(s_progress_bar, 0, 100);
    lv_bar_set_value(s_progress_bar, 0, LV_ANIM_OFF);
    accent::bind_indicator(s_progress_bar);      // fill tracks accent color
    accent::bind_contrast_track(s_progress_bar); // track is white/black for contrast

    // ── SLOT_VOLUME_ARC — visual placeholder (no command wiring until add-encoder-volume) ──
    lv_obj_t* vol_cont = layout_container(scr, SLOT_VOLUME_ARC);
    lv_obj_remove_flag(vol_cont, LV_OBJ_FLAG_CLICKABLE);  // sits above transport — must not absorb touch
    if constexpr (kDisplay.has_encoder) {
        s_volume_arc = lv_arc_create(vol_cont);
        // 97% of the 480×480 slot → 466×466 arc, visual radius ≈232px (near display edge)
        lv_obj_set_size(s_volume_arc, lv_pct(97), lv_pct(97));
        lv_obj_align(s_volume_arc, LV_ALIGN_CENTER, 0, 0);
        lv_arc_set_rotation(s_volume_arc, 135);
        lv_arc_set_bg_angles(s_volume_arc, 0, 270);
        lv_arc_set_value(s_volume_arc, 50);
        lv_obj_set_style_arc_color(s_volume_arc, lv_color_hex(0x1DB954), LV_PART_INDICATOR);
        lv_obj_set_style_arc_opa(s_volume_arc, LV_OPA_COVER, LV_PART_INDICATOR);
        lv_obj_set_style_arc_color(s_volume_arc, lv_color_hex(0x2A2A2A), LV_PART_MAIN);
        lv_obj_set_style_arc_opa(s_volume_arc, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_arc_width(s_volume_arc, 12, LV_PART_MAIN);
        lv_obj_set_style_arc_width(s_volume_arc, 12, LV_PART_INDICATOR);
        lv_obj_remove_flag(s_volume_arc, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_opa(s_volume_arc, LV_OPA_TRANSP, LV_PART_KNOB);
    } else {
        lv_obj_t* slider = lv_slider_create(vol_cont);
        lv_obj_set_size(slider, lv_pct(90), lv_pct(40));
        lv_obj_align(slider, LV_ALIGN_CENTER, 0, 0);
        lv_slider_set_value(slider, 50, LV_ANIM_OFF);
    }

    // ── SLOT_STATUS — group state indicator (tappable) ───────────────────────
    s_status_cont = slot_container(scr, SLOT_STATUS);
    lv_obj_add_flag(s_status_cont, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_status_cont, [](lv_event_t*) {
        Serial.println("[UI] status tapped");
        screens::now_playing_open_group_overlay();
    }, LV_EVENT_PRESSED, nullptr);

    s_status_lbl = lv_label_create(s_status_cont);
    lv_label_set_text(s_status_lbl, "");
    lv_obj_set_style_text_font(s_status_lbl, font(FontRole::Body), 0);
    lv_obj_set_style_text_color(s_status_lbl, lv_color_hex(0xCCCCCC), 0);
    lv_obj_align(s_status_lbl, LV_ALIGN_CENTER, 0, 0);

    // 1 s interpolation timer
    s_interp_timer = lv_timer_create(interp_tick, 1000, nullptr);

    // Loading overlay — covers all screen content until first real state arrives.
    // Created last so it has the highest z-order within the screen.
    s_loading_overlay = lv_obj_create(scr);
    lv_obj_set_size(s_loading_overlay, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(s_loading_overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_loading_overlay, LV_OPA_80, 0);
    lv_obj_set_style_border_width(s_loading_overlay, 0, 0);
    lv_obj_remove_flag(s_loading_overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t* loading_lbl = lv_label_create(s_loading_overlay);
    lv_label_set_text(loading_lbl, "Connecting...");
    lv_obj_set_style_text_font(loading_lbl, font(FontRole::Caption), 0);
    lv_obj_set_style_text_color(loading_lbl, lv_color_hex(0xAAAAAA), 0);
    lv_obj_align(loading_lbl, LV_ALIGN_CENTER, 0, 0);

    s_idle_timer_start = millis();

    return scr;
}

// ── now_playing_open_favorites_overlay ───────────────────────────────────────

void screens::now_playing_open_favorites_overlay() {
    FavoritesState fav = favorites::snapshot();

    // Build or rebuild the overlay if it doesn't exist yet or favorites changed
    if (!s_favorites_overlay) {
        // Opaque background — no per-pixel blending with art = fast render
        s_favorites_overlay = lv_obj_create(lv_layer_top());
        lv_obj_set_size(s_favorites_overlay, lv_pct(100), lv_pct(100));
        lv_obj_set_style_bg_color(s_favorites_overlay, lv_color_hex(0x111111), 0);
        lv_obj_set_style_bg_opa(s_favorites_overlay, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(s_favorites_overlay, 0, 0);
        lv_obj_remove_flag(s_favorites_overlay, LV_OBJ_FLAG_SCROLLABLE);
        // Tap outside list → hide (not delete; preserved for fast re-open)
        lv_obj_add_event_cb(s_favorites_overlay, [](lv_event_t* e) {
            if (lv_event_get_target(e) == s_favorites_overlay) {
                lv_obj_add_flag(s_favorites_overlay, LV_OBJ_FLAG_HIDDEN);
            }
        }, LV_EVENT_CLICKED, nullptr);

        const LayoutSlot& ls = slot(SLOT_LIST);
        s_favorites_list = lv_list_create(s_favorites_overlay);
        lv_obj_set_pos(s_favorites_list, ls.x + 8, ls.y + 8);
        lv_obj_set_size(s_favorites_list, ls.w - 16, ls.h - 16);
        lv_obj_set_style_bg_color(s_favorites_list, lv_color_hex(0x1E1E1E), 0);
        lv_obj_set_style_border_width(s_favorites_list, 0, 0);
        lv_obj_set_style_radius(s_favorites_list, 16, 0);

        s_favorites_version_built = 0;  // force populate below
    }

    // Rebuild list contents when favorites data changes
    if (fav.version != s_favorites_version_built) {
        s_favorites_version_built = fav.version;
        lv_obj_clean(s_favorites_list);  // remove all children

        if (!fav.fetched || fav.items.empty()) {
            lv_obj_t* empty_btn = lv_list_add_button(s_favorites_list, nullptr,
                fav.fetched ? "No favorites" : "Loading...");
            lv_obj_set_style_text_color(empty_btn, lv_color_hex(0x888888), 0);
            lv_obj_clear_flag(empty_btn, LV_OBJ_FLAG_CLICKABLE);
        } else {
            // Use index stored as intptr — no heap allocation, safe across hide/show
            for (int idx = 0; idx < (int)fav.items.size(); ++idx) {
                lv_obj_t* btn = lv_list_add_button(s_favorites_list, nullptr,
                    fav.items[idx].title.c_str());
                lv_obj_set_height(btn, 84);
                lv_obj_set_style_text_color(btn, lv_color_hex(0xEEEEEE), 0);
                lv_obj_set_style_text_font(btn, font(FontRole::Title), 0);
                lv_obj_set_style_pad_ver(btn, 16, 0);
                lv_obj_set_style_pad_hor(btn, 16, 0);
                lv_obj_set_ext_click_area(btn, 10);
                lv_obj_t* lbl = lv_obj_get_child(btn, 0);
                if (lbl) { lv_label_set_long_mode(lbl, LV_LABEL_LONG_DOT); lv_obj_set_width(lbl, lv_pct(100)); }
                lv_obj_set_user_data(btn, (void*)(intptr_t)idx);
                lv_obj_add_event_cb(btn, [](lv_event_t* e) {
                    lv_obj_t* b = static_cast<lv_obj_t*>(lv_event_get_target(e));
                    int i = (int)(intptr_t)lv_obj_get_user_data(b);
                    FavoritesState f = favorites::snapshot();
                    if (i >= 0 && i < (int)f.items.size()) {
                        sonos::playback::set_favorite(f.items[i].uri, f.items[i].metadata,
                                                      f.items[i].metadata_ready);
                        sonos::playback::enqueue_command(sonos::PlaybackCommand::PlayFavorite);
                    }
                    lv_obj_add_flag(s_favorites_overlay, LV_OBJ_FLAG_HIDDEN);
                }, LV_EVENT_CLICKED, nullptr);
            }
        }
    }

    lv_obj_remove_flag(s_favorites_overlay, LV_OBJ_FLAG_HIDDEN);
}

// ── now_playing_open_group_overlay ───────────────────────────────────────────

void screens::now_playing_open_group_overlay() {
    if (s_group_overlay) return;  // already open

    // Full-screen opaque backdrop on lv_layer_top() — opaque avoids compositing cost
    s_group_overlay = lv_obj_create(lv_layer_top());
    lv_obj_set_size(s_group_overlay, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(s_group_overlay, lv_color_hex(0x111111), 0);
    lv_obj_set_style_bg_opa(s_group_overlay, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_group_overlay, 0, 0);
    lv_obj_remove_flag(s_group_overlay, LV_OBJ_FLAG_SCROLLABLE);

    // Tap backdrop (not list) to dismiss without action
    lv_obj_add_event_cb(s_group_overlay, [](lv_event_t* e) {
        if (lv_event_get_target(e) == s_group_overlay) {
            lv_obj_delete(s_group_overlay);
            s_group_overlay = nullptr;
        }
    }, LV_EVENT_CLICKED, nullptr);

    // Scrollable speaker list using SLOT_LIST geometry (inner square)
    const LayoutSlot& ls = slot(SLOT_LIST);
    lv_obj_t* list = lv_list_create(s_group_overlay);
    lv_obj_set_pos(list, ls.x, ls.y);
    lv_obj_set_size(list, ls.w, ls.h);
    lv_obj_set_style_bg_color(list, lv_color_hex(0x1E1E1E), 0);
    lv_obj_set_style_border_width(list, 0, 0);
    lv_obj_set_style_radius(list, 12, 0);

    // Helper: if name is still "IP - Sonos Model - UUID" extract just the model portion
    auto clean_name = [](const String& raw) -> String {
        int first = raw.indexOf(" - ");
        if (first < 0) return raw;
        int second = raw.indexOf(" - ", first + 3);
        if (second < 0) return raw;
        return raw.substring(first + 3, second);  // "Sonos Roam", "Sonos Amp", etc.
    };

    // Helper: make a list button with large, non-scrolling text and generous hit area
    auto make_btn = [&](const char* text, lv_color_t color) -> lv_obj_t* {
        lv_obj_t* btn = lv_list_add_button(list, nullptr, text);
        lv_obj_set_height(btn, 72);
        lv_obj_set_style_text_color(btn, color, 0);
        lv_obj_set_style_text_font(btn, font(FontRole::Title), 0);
        lv_obj_set_style_pad_all(btn, 12, 0);
        lv_obj_set_ext_click_area(btn, 8);  // extend hit area by 8px on each side
        // Get the text label and stop scrolling
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

    // Determine which UUIDs are currently in the same group as home room
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

    // "Go Solo" — only relevant when grouped
    if (is_grouped) {
        lv_obj_t* solo_btn = make_btn("Go Solo", lv_color_hex(0xFFDD44));
        lv_obj_add_event_cb(solo_btn, [](lv_event_t*) {
            sonos::playback::enqueue_command(sonos::PlaybackCommand::LeaveGroup);
            if (s_group_overlay) { lv_obj_delete(s_group_overlay); s_group_overlay = nullptr; }
        }, LV_EVENT_CLICKED, nullptr);
    }

    // One checkbox entry per other speaker
    for (const auto& sp : st.speakers) {
        if (sp.uuid == home_uuid) continue;

        String display_name = sp.friendly_name;
        if (display_name.indexOf(" - ") >= 0) display_name = clean_name(display_name);

        bool in_group = std::find(group_member_uuids.begin(), group_member_uuids.end(),
                                  sp.uuid) != group_member_uuids.end();
        const char* icon = in_group ? LV_SYMBOL_OK : nullptr;
        lv_color_t color = in_group ? lv_color_hex(0xFFFFFF) : lv_color_hex(0xBBBBBB);

        // Store UUID + in_group flag packed into pointer (use heap struct)
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
        }, LV_EVENT_CLICKED, nullptr);
    }
}

// ── now_playing_update_volume_arc ────────────────────────────────────────────
// Called from UI task on encoder turn for immediate visual feedback before poll.

void screens::now_playing_update_volume_arc(int volume_0_100) {
    if (!s_volume_arc) return;
    int v = volume_0_100 < 0 ? 0 : volume_0_100 > 100 ? 100 : volume_0_100;
    lv_arc_set_value(s_volume_arc, v);
}

// ── Idle state interface ──────────────────────────────────────────────────────

bool screens::now_playing_is_idle() {
    return s_is_idle;
}

void screens::now_playing_wake_from_touch() {
    if (s_is_idle) {
        s_is_idle = false;
        s_idle_timer_start = millis();
        sonos::playback::idle_mode(false);
        bsp::backlight(200);
        // Consume the waking tap so it doesn't trigger a button/overlay
        bsp::suppress_touch_until_release();
    }
}

// ── now_playing_init_art ──────────────────────────────────────────────────────

void screens::now_playing_init_art() {
    // Bind volume arc indicator to the accent token.
    // Progress bar bindings (indicator + contrast track) are set up in now_playing_create().
    if (s_volume_arc) accent::bind(s_volume_arc, LV_STYLE_ARC_COLOR);
}

// ── now_playing_update ────────────────────────────────────────────────────────

void screens::now_playing_update(const sonos::PlaybackState& state) {
    // Reset idle timer while playing; wake from idle when playback resumes
    if (state.transport_state == sonos::TransportState::Playing) {
        s_idle_timer_start = millis();
        if (s_is_idle) {
            s_is_idle = false;
            sonos::playback::idle_mode(false);
            bsp::backlight(200);
        }
    }

    // Dismiss loading overlay on first real state (one-shot, never reappears)
    if (!s_loading_dismissed && state.transport_state != sonos::TransportState::Unknown) {
        s_loading_dismissed = true;
        if (s_status_lbl) lv_label_set_text(s_status_lbl, "");  // clear "Connecting..." from status slot
        if (s_loading_overlay) {
            static lv_anim_t s_fade;
            lv_anim_init(&s_fade);
            lv_anim_set_var(&s_fade, s_loading_overlay);
            lv_anim_set_exec_cb(&s_fade, [](void* obj, int32_t v) {
                lv_obj_set_style_opa(static_cast<lv_obj_t*>(obj), (lv_opa_t)v, 0);
            });
            lv_anim_set_completed_cb(&s_fade, [](lv_anim_t* a) {
                lv_obj_delete(static_cast<lv_obj_t*>(a->var));
                s_loading_overlay = nullptr;
            });
            lv_anim_set_values(&s_fade, LV_OPA_80, LV_OPA_TRANSP);
            lv_anim_set_duration(&s_fade, 300);
            lv_anim_set_path_cb(&s_fade, lv_anim_path_ease_out);
            lv_anim_start(&s_fade);
        }
    }

    s_last = state;
    lv_obj_set_style_opa(s_transport_cont, LV_OPA_COVER, 0);  // clear any pending dim

    lv_label_set_text(s_title_lbl,
        state.title.isEmpty() ? "\xe2\x80\x94" : state.title.c_str());
    lv_label_set_text(s_artist_lbl,
        state.artist.c_str());
    lv_label_set_text(s_album_lbl,
        state.album.c_str());

    lv_label_set_text(s_play_btn_lbl,
        state.transport_state == sonos::TransportState::Playing
            ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY);

    int32_t pct = (state.duration_s > 0)
        ? (int32_t)(state.position_s * 100u / state.duration_s)
        : 0;
    lv_bar_set_value(s_progress_bar, pct, LV_ANIM_OFF);

    if (s_volume_arc) {
        lv_arc_set_value(s_volume_arc, state.volume);
        lv_obj_set_style_arc_color(s_volume_arc,
            state.muted ? lv_color_hex(0x555555) : accent::get(),
            LV_PART_INDICATOR);
    }

    // Update album art and accent when a new decode is ready
    ArtState art = art::snapshot();
    if (art.version != s_art_version && art.pixels != nullptr) {
        s_art_version               = art.version;
        s_art_dsc.header.magic      = LV_IMAGE_HEADER_MAGIC;
        s_art_dsc.header.cf         = LV_COLOR_FORMAT_RGB565;  // matches TJpgDec JD_FORMAT=1
        s_art_dsc.header.w          = art.w;
        s_art_dsc.header.h          = art.h;
        s_art_dsc.header.stride     = (uint32_t)art.w * 2u;
        s_art_dsc.data_size         = (uint32_t)art.w * art.h * 2u;
        s_art_dsc.data              = art.pixels;
        lv_image_set_src(s_art_img, &s_art_dsc);
        lv_obj_remove_flag(s_art_img, LV_OBJ_FLAG_HIDDEN);

        lv_color_t adjusted = accent::adjusted_for_legibility(
            art.accent, lv_color_hex(0x1A1A1A));
        accent::set(adjusted, 400);
    }

    // Update group status label
    if (s_status_lbl) {
        if (state.group_coordinator_uuid.isEmpty()) {
            lv_label_set_text(s_status_lbl, "Solo");
        } else {
            // Find coordinator's friendly name
            SonosState st = sonos::state::snapshot();
            String coordinator_name;
            for (const auto& sp : st.speakers) {
                if (sp.uuid == state.group_coordinator_uuid) {
                    coordinator_name = sp.friendly_name;
                    break;
                }
            }
            lv_label_set_text(s_status_lbl,
                coordinator_name.isEmpty() ? "Group" : coordinator_name.c_str());
        }
    }
}
