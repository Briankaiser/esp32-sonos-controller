#include "now_playing.h"
#include "settings.h"
#include "../nav.h"
#include "../../app/sonos/favorites.h"
#include <algorithm>
#include "../layout/layout.h"
#include "../theme/fonts.h"
#include "../theme/accent.h"
#include "list_btn.h"
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
static lv_obj_t*          s_transport_cont = nullptr;
static lv_obj_t*          s_volume_arc     = nullptr;
static lv_timer_t*        s_interp_timer          = nullptr;
static lv_timer_t*        s_transport_restore_tmr = nullptr;
static lv_obj_t*          s_art_img        = nullptr;
static lv_image_dsc_t     s_art_dsc        = {};
static uint32_t           s_art_version    = 0;
static lv_obj_t*          s_loading_overlay    = nullptr;
static bool               s_loading_dismissed  = false;
static lv_obj_t*          s_status_cont        = nullptr;
static lv_obj_t*          s_favorites_overlay       = nullptr;
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

// ── Transport dim feedback ────────────────────────────────────────────────────

static void dim_transport() {
    if (!s_transport_cont) return;
    lv_obj_set_style_opa(s_transport_cont, LV_OPA_30, 0);
    if (s_transport_restore_tmr) {
        lv_timer_reset(s_transport_restore_tmr);
    } else {
        s_transport_restore_tmr = lv_timer_create([](lv_timer_t* t) {
            if (s_transport_cont) lv_obj_set_style_opa(s_transport_cont, LV_OPA_COVER, 0);
            lv_timer_delete(t);
            s_transport_restore_tmr = nullptr;
        }, 1000, nullptr);
        lv_timer_set_repeat_count(s_transport_restore_tmr, 1);
    }
}

void screens::now_playing_dim_transport() { dim_transport(); }

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
    // Frosted dark backdrop so art is visible but text stays readable — same
    // "illuminated surface" language as the modal overlays: dark fill, accent
    // hairline border, and a soft accent glow (both track the album-art accent).
    lv_obj_set_style_bg_color(info, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(info, LV_OPA_50, 0);
    lv_obj_set_style_radius(info, 18, 0);
    lv_obj_set_style_border_width(info, 1, 0);
    lv_obj_set_style_border_opa(info, LV_OPA_40, 0);
    accent::bind(info, LV_STYLE_BORDER_COLOR);
    lv_obj_set_style_shadow_width(info, 16, 0);
    lv_obj_set_style_shadow_opa(info, LV_OPA_20, 0);
    accent::bind(info, LV_STYLE_SHADOW_COLOR);
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
    lv_obj_set_size(s_title_lbl, lv_pct(100), lv_font_get_line_height(font(FontRole::Display)));

    s_artist_lbl = lv_label_create(info);
    lv_label_set_text(s_artist_lbl, "");
    lv_obj_set_style_text_font(s_artist_lbl, font(FontRole::Title), 0);
    lv_obj_set_style_text_color(s_artist_lbl, lv_color_hex(0xAAAAAA), 0);
    lv_obj_set_style_text_align(s_artist_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(s_artist_lbl, LV_LABEL_LONG_DOT);
    lv_obj_set_size(s_artist_lbl, lv_pct(100), lv_font_get_line_height(font(FontRole::Title)));

    s_album_lbl = lv_label_create(info);
    lv_label_set_text(s_album_lbl, "");
    lv_obj_set_style_text_font(s_album_lbl, font(FontRole::Body), 0);
    lv_obj_set_style_text_color(s_album_lbl, lv_color_hex(0x777777), 0);
    lv_obj_set_style_text_align(s_album_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(s_album_lbl, LV_LABEL_LONG_DOT);
    lv_obj_set_size(s_album_lbl, lv_pct(100), lv_font_get_line_height(font(FontRole::Body)));

    // ── SLOT_TRANSPORT — prev / play-pause / next as equal circular discs ────
    // Widen the row symmetrically about the display centre so the outer discs
    // reach the bezel by an equal amount on each side — an intentional,
    // symmetric bleed rather than the lopsided clip of a left-packed flex row.
    // All geometry derives from the slot (already centred), so no pixel
    // literals or board names leak into screen code.
    s_transport_cont = slot_container(scr, SLOT_TRANSPORT);
    {
        const LayoutSlot& ts = slot(SLOT_TRANSPORT);
        int32_t grow = ts.w / 10;  // gentle symmetric widen per side
        lv_obj_set_pos(s_transport_cont, ts.x - grow, ts.y);
        lv_obj_set_size(s_transport_cont, ts.w + 2 * grow, ts.h);
    }
    lv_obj_set_layout(s_transport_cont, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(s_transport_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(s_transport_cont,
        LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // One circular transport disc. All three share a size and icon colour; the
    // primary (play/pause) carries a thin accent ring + soft accent glow — the
    // same "illuminated" emphasis the modal overlays use — so it reads as
    // primary without a brighter glyph. Ring and glow track the album-art
    // accent via accent::bind.
    auto make_disc = [&](const char* symbol, bool primary) -> lv_obj_t* {
        int32_t d = slot(SLOT_TRANSPORT).h * 6 / 5;  // discs a touch larger than the row
        lv_obj_t* btn = lv_button_create(s_transport_cont);
        lv_obj_set_size(btn, d, d);
        lv_obj_set_style_bg_color(btn, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(btn, LV_OPA_40, 0);  // translucent disc so icon pops on light art
        lv_obj_set_style_radius(btn, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(btn, 0, 0);
        lv_obj_set_style_shadow_width(btn, 0, 0);
        if (primary) {
            lv_obj_set_style_border_width(btn, 2, 0);
            lv_obj_set_style_border_opa(btn, LV_OPA_60, 0);
            accent::bind(btn, LV_STYLE_BORDER_COLOR);
            lv_obj_set_style_shadow_width(btn, 16, 0);
            lv_obj_set_style_shadow_opa(btn, LV_OPA_30, 0);
            accent::bind(btn, LV_STYLE_SHADOW_COLOR);
        }
        lv_obj_t* lbl = lv_label_create(btn);
        lv_label_set_text(lbl, symbol);
        lv_obj_set_style_text_font(lbl, font(FontRole::Display), 0);
        lv_obj_set_style_text_color(lbl, lv_color_hex(0xCCCCCC), 0);
        lv_obj_center(lbl);
        return btn;
    };

    lv_obj_t* prev_btn = make_disc(LV_SYMBOL_PREV, false);
    lv_obj_add_event_cb(prev_btn, [](lv_event_t*) {
        sonos::playback::enqueue_command(sonos::PlaybackCommand::Previous);
        dim_transport();
    }, LV_EVENT_PRESSED, nullptr);

    lv_obj_t* play_btn = make_disc(LV_SYMBOL_PLAY, true);
    s_play_btn_lbl = lv_obj_get_child(play_btn, 0);
    lv_obj_add_event_cb(play_btn, [](lv_event_t*) {
        bool playing = s_last.transport_state == sonos::TransportState::Playing;
        sonos::playback::enqueue_command(
            playing ? sonos::PlaybackCommand::Pause : sonos::PlaybackCommand::Play);
        sonos::playback::optimistic_transport(
            playing ? sonos::TransportState::Paused : sonos::TransportState::Playing);
        dim_transport();
    }, LV_EVENT_PRESSED, nullptr);

    lv_obj_t* next_btn = make_disc(LV_SYMBOL_NEXT, false);
    lv_obj_add_event_cb(next_btn, [](lv_event_t*) {
        sonos::playback::enqueue_command(sonos::PlaybackCommand::Next);
        dim_transport();
    }, LV_EVENT_PRESSED, nullptr);

    // ── SLOT_PROGRESS — progress bar ─────────────────────────────────────────
    lv_obj_t* prog_cont = slot_container(scr, SLOT_PROGRESS);
    s_progress_bar = lv_bar_create(prog_cont);
    lv_obj_set_size(s_progress_bar, lv_pct(100), lv_pct(100));
    lv_bar_set_range(s_progress_bar, 0, 100);
    lv_bar_set_value(s_progress_bar, 0, LV_ANIM_OFF);
    // Stadium caps on both track and fill to match the pill vocabulary
    lv_obj_set_style_radius(s_progress_bar, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_radius(s_progress_bar, LV_RADIUS_CIRCLE, LV_PART_INDICATOR);
    accent::bind_indicator(s_progress_bar);      // fill tracks accent color
    accent::bind_contrast_track(s_progress_bar); // track is white/black for contrast

    // ── SLOT_VOLUME_ARC — visual placeholder (no command wiring until add-encoder-volume) ──
    lv_obj_t* vol_cont = layout_container(scr, SLOT_VOLUME_ARC);
    lv_obj_remove_flag(vol_cont, LV_OBJ_FLAG_CLICKABLE);  // must not absorb touch meant for the art
    if constexpr (kDisplay.has_encoder) {
        s_volume_arc = lv_arc_create(vol_cont);
        // 97% of the 480×480 slot → 466×466 arc, visual radius ≈232px (near display edge)
        lv_obj_set_size(s_volume_arc, lv_pct(97), lv_pct(97));
        lv_obj_align(s_volume_arc, LV_ALIGN_CENTER, 0, 0);
        lv_arc_set_rotation(s_volume_arc, 135);
        lv_arc_set_bg_angles(s_volume_arc, 0, 270);
        lv_arc_set_value(s_volume_arc, 50);
        lv_obj_set_style_arc_color(s_volume_arc, lv_color_hex(0xD4A843), LV_PART_INDICATOR);
        lv_obj_set_style_arc_opa(s_volume_arc, LV_OPA_COVER, LV_PART_INDICATOR);
        lv_obj_set_style_arc_color(s_volume_arc, lv_color_hex(0x2A2A2A), LV_PART_MAIN);
        lv_obj_set_style_arc_opa(s_volume_arc, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_arc_width(s_volume_arc, 12, LV_PART_MAIN);
        lv_obj_set_style_arc_width(s_volume_arc, 12, LV_PART_INDICATOR);
        lv_obj_set_style_arc_rounded(s_volume_arc, true, LV_PART_INDICATOR);  // rounded fill cap
        lv_obj_set_style_arc_rounded(s_volume_arc, true, LV_PART_MAIN);       // rounded track ends
        lv_obj_remove_flag(s_volume_arc, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_opa(s_volume_arc, LV_OPA_TRANSP, LV_PART_KNOB);
    } else {
        lv_obj_t* slider = lv_slider_create(vol_cont);
        lv_obj_set_size(slider, lv_pct(90), lv_pct(40));
        lv_obj_align(slider, LV_ALIGN_CENTER, 0, 0);
        lv_slider_set_value(slider, 50, LV_ANIM_OFF);
    }
    // Drop the arc just above the album-art background so the transport discs
    // (and everything else) layer visually on top of it where they overlap at
    // the edge, rather than the arc drawing over the discs.
    lv_obj_move_to_index(vol_cont, 1);

    // ── SLOT_STATUS — settings entry point (gear icon) ──────────────────────
    s_status_cont = slot_container(scr, SLOT_STATUS);
    lv_obj_add_flag(s_status_cont, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_status_cont, [](lv_event_t*) {
        screens::settings_open();
    }, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* gear_lbl = lv_label_create(s_status_cont);
    lv_label_set_text(gear_lbl, LV_SYMBOL_SETTINGS);
    lv_obj_set_style_text_font(gear_lbl, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(gear_lbl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_opa(gear_lbl, LV_OPA_30, 0);
    lv_obj_align(gear_lbl, LV_ALIGN_CENTER, 0, 0);

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
    if (s_favorites_overlay) return;

    FavoritesState fav = favorites::snapshot();

    s_favorites_overlay = lv_obj_create(lv_layer_top());
    lv_obj_set_size(s_favorites_overlay, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(s_favorites_overlay, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(s_favorites_overlay, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_favorites_overlay, 0, 0);
    lv_obj_remove_flag(s_favorites_overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(s_favorites_overlay, [](lv_event_t* e) {
        if (lv_event_get_target(e) == s_favorites_overlay) {
            lv_obj_delete(s_favorites_overlay);
            s_favorites_overlay = nullptr;
            nav::clear_list();
        }
    }, LV_EVENT_CLICKED, nullptr);

    const LayoutSlot& ls = slot(SLOT_LIST);
    lv_color_t ac = accent::get();
    const int32_t cw = ls.w - 16;
    const int32_t ch = ls.h - 16;

    lv_obj_t* card = lv_obj_create(s_favorites_overlay);
    lv_obj_set_pos(card, ls.x + 8, ls.y + 8);
    lv_obj_set_size(card, cw, ch);
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

    lv_obj_t* hd = lv_obj_create(card);
    lv_obj_set_pos(hd, 0, 0);
    lv_obj_set_size(hd, cw, 52);
    lv_obj_set_style_bg_color(hd, ac, 0);
    lv_obj_set_style_bg_opa(hd, LV_OPA_30, 0);
    lv_obj_set_style_border_side(hd, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_width(hd, 2, 0);
    lv_obj_set_style_border_color(hd, ac, 0);
    lv_obj_set_style_border_opa(hd, LV_OPA_50, 0);
    lv_obj_set_style_radius(hd, 0, 0);
    lv_obj_set_style_pad_all(hd, 0, 0);
    lv_obj_remove_flag(hd, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t* hd_lbl = lv_label_create(hd);
    lv_label_set_text(hd_lbl, "Favorites");
    lv_obj_set_style_text_font(hd_lbl, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(hd_lbl, ac, 0);
    lv_obj_align(hd_lbl, LV_ALIGN_LEFT_MID, 20, 0);

    lv_obj_t* list = lv_list_create(card);
    lv_obj_set_pos(list, 0, 52);
    lv_obj_set_size(list, cw, ch - 52 - 72);
    lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(list, 0, 0);
    lv_obj_set_style_radius(list, 0, 0);
    lv_obj_set_style_pad_all(list, 0, 0);
    lv_obj_set_style_pad_row(list, 4, 0);

    if (!fav.fetched || fav.items.empty()) {
        lv_obj_t* empty_btn = lv_list_add_button(list, nullptr,
            fav.fetched ? "No favorites" : "Loading...");
        lv_obj_set_style_bg_color(empty_btn, lv_color_hex(0x1A1A1A), 0);
        lv_obj_set_style_bg_opa(empty_btn, LV_OPA_COVER, 0);
        lv_obj_set_style_text_color(empty_btn, lv_color_hex(0x888888), 0);
        lv_obj_clear_flag(empty_btn, LV_OBJ_FLAG_CLICKABLE);
    } else {
        // Use index stored as intptr — no heap allocation needed
        for (int idx = 0; idx < (int)fav.items.size(); ++idx) {
            lv_obj_t* btn = lv_list_add_button(list, nullptr,
                fav.items[idx].title.c_str());
            apply_list_btn_style(btn, lv_color_hex(0xEEEEEE));
            lv_obj_set_ext_click_area(btn, 10);
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
                lv_obj_delete(s_favorites_overlay);
                s_favorites_overlay = nullptr;
                nav::clear_list();
            }, LV_EVENT_CLICKED, nullptr);
        }
    }

    // Cancel — fixed at the bottom of the card
    lv_obj_t* cancel_btn = lv_obj_create(card);
    lv_obj_set_pos(cancel_btn, 0, ch - 72);
    lv_obj_set_size(cancel_btn, cw, 72);
    lv_obj_set_style_bg_color(cancel_btn, lv_color_hex(0x141414), 0);
    lv_obj_set_style_bg_opa(cancel_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_side(cancel_btn, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_border_width(cancel_btn, 1, 0);
    lv_obj_set_style_border_color(cancel_btn, ac, 0);
    lv_obj_set_style_border_opa(cancel_btn, LV_OPA_20, 0);
    lv_obj_set_style_radius(cancel_btn, 0, 0);
    lv_obj_set_style_pad_all(cancel_btn, 0, 0);
    lv_obj_remove_flag(cancel_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t* cancel_lbl = lv_label_create(cancel_btn);
    lv_label_set_text(cancel_lbl, "Cancel");
    lv_obj_set_style_text_font(cancel_lbl, font(FontRole::Body), 0);
    lv_obj_set_style_text_color(cancel_lbl, lv_color_hex(0x888888), 0);
    lv_obj_align(cancel_lbl, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_event_cb(cancel_btn, [](lv_event_t*) {
        lv_obj_delete(s_favorites_overlay);
        s_favorites_overlay = nullptr;
        nav::clear_list();
    }, LV_EVENT_CLICKED, nullptr);

    nav::register_list(list);
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

    // Full-screen overlay is open — visual updates are invisible; skip the churn.
    // (transport dim timer keeps running independently — no need to guard it here)
    // Idle tracking and loading-dismiss above still run normally.
    // NOTE: favorites overlay stays as a hidden child of lv_layer_top() between opens,
    // so we must check visibility, not just child count.
    {
        bool covered = false;
        lv_obj_t* layer = lv_layer_top();
        uint32_t n = lv_obj_get_child_count(layer);
        for (uint32_t i = 0; i < n; i++) {
            if (!lv_obj_has_flag(lv_obj_get_child(layer, i), LV_OBJ_FLAG_HIDDEN)) {
                covered = true;
                break;
            }
        }
        if (covered) return;
    }

    if (!s_transport_restore_tmr)  // don't cut short a pending dim
        lv_obj_set_style_opa(s_transport_cont, LV_OPA_COVER, 0);

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

    // Album art is applied by now_playing_refresh_art(), driven every UI frame
    // from the main loop — decoupled from playback-state changes so a freshly
    // decoded frame appears within a frame rather than waiting for the next
    // pb.version bump (which, under eventing, may be tens of seconds away).
    now_playing_refresh_art();
}

// ── now_playing_refresh_art ─────────────────────────────────────────────────
// Applies the latest decoded album art (and its accent) to the screen when a
// new decode is ready. Idempotent: no-ops when the art version is unchanged,
// so it is safe — and cheap — to call every UI frame. MUST run on the UI task
// (core 1) only, like all other lv_* calls here.
void screens::now_playing_refresh_art() {
    if (!s_art_img) return;  // screen not built yet
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
}
