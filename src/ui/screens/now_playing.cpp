#include "now_playing.h"
#include "../layout/layout.h"
#include "../theme/fonts.h"
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
static sonos::PlaybackState s_last;

// ── Interpolation timer (fires every 1 s on UI task) ─────────────────────────

static void interp_tick(lv_timer_t* /*t*/) {
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

    // ── SLOT_ALBUM_ART — dark placeholder square (art added in add-album-art) ─
    lv_obj_t* art = layout_container(scr, SLOT_ALBUM_ART);
    lv_obj_set_style_bg_color(art, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(art, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(art, 8, LV_PART_MAIN);

    // ── SLOT_TRACK_INFO — three stacked labels (title / artist / album) ──────
    lv_obj_t* info = slot_container(scr, SLOT_TRACK_INFO);
    lv_obj_set_layout(info, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(info, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(info, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(info, 0, 0);

    s_title_lbl = lv_label_create(info);
    lv_label_set_text(s_title_lbl, "\xe2\x80\x94");
    lv_obj_set_style_text_font(s_title_lbl, font(FontRole::Title), 0);
    lv_obj_set_style_text_color(s_title_lbl, lv_color_hex(0xEEEEEE), 0);
    lv_obj_set_style_text_align(s_title_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(s_title_lbl, LV_LABEL_LONG_DOT);
    lv_obj_set_width(s_title_lbl, lv_pct(100));

    s_artist_lbl = lv_label_create(info);
    lv_label_set_text(s_artist_lbl, "");
    lv_obj_set_style_text_font(s_artist_lbl, font(FontRole::Body), 0);
    lv_obj_set_style_text_color(s_artist_lbl, lv_color_hex(0xAAAAAA), 0);
    lv_obj_set_style_text_align(s_artist_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(s_artist_lbl, LV_LABEL_LONG_DOT);
    lv_obj_set_width(s_artist_lbl, lv_pct(100));

    s_album_lbl = lv_label_create(info);
    lv_label_set_text(s_album_lbl, "");
    lv_obj_set_style_text_font(s_album_lbl, font(FontRole::Caption), 0);
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
    lv_obj_set_style_bg_opa(prev_btn, LV_OPA_TRANSP, 0);
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
    }, LV_EVENT_CLICKED, nullptr);

    // Play/Pause
    lv_obj_t* play_btn = lv_button_create(s_transport_cont);
    lv_obj_set_flex_grow(play_btn, 1);
    lv_obj_set_height(play_btn, lv_pct(100));
    lv_obj_set_style_bg_opa(play_btn, LV_OPA_TRANSP, 0);
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
    }, LV_EVENT_CLICKED, nullptr);

    // Next
    lv_obj_t* next_btn = lv_button_create(s_transport_cont);
    lv_obj_set_flex_grow(next_btn, 1);
    lv_obj_set_height(next_btn, lv_pct(100));
    lv_obj_set_style_bg_opa(next_btn, LV_OPA_TRANSP, 0);
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
    }, LV_EVENT_CLICKED, nullptr);

    // ── SLOT_PROGRESS — progress bar ─────────────────────────────────────────
    lv_obj_t* prog_cont = slot_container(scr, SLOT_PROGRESS);
    s_progress_bar = lv_bar_create(prog_cont);
    lv_obj_set_size(s_progress_bar, lv_pct(100), lv_pct(100));
    lv_bar_set_range(s_progress_bar, 0, 100);
    lv_bar_set_value(s_progress_bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(s_progress_bar, lv_color_hex(0x444444), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_progress_bar, lv_color_hex(0x1DB954), LV_PART_INDICATOR);

    // ── SLOT_VOLUME_ARC — visual placeholder (no command wiring until add-encoder-volume) ──
    lv_obj_t* vol_cont = layout_container(scr, SLOT_VOLUME_ARC);
    lv_obj_remove_flag(vol_cont, LV_OBJ_FLAG_CLICKABLE);  // sits above transport — must not absorb touch
    if constexpr (kDisplay.has_encoder) {
        s_volume_arc = lv_arc_create(vol_cont);
        lv_obj_set_size(s_volume_arc, lv_pct(100), lv_pct(100));
        lv_arc_set_rotation(s_volume_arc, 135);
        lv_arc_set_bg_angles(s_volume_arc, 0, 270);
        lv_arc_set_value(s_volume_arc, 50);
        lv_obj_set_style_arc_color(s_volume_arc, lv_color_hex(0x1DB954), LV_PART_INDICATOR);
        lv_obj_set_style_arc_color(s_volume_arc, lv_color_hex(0x333333), LV_PART_MAIN);
        lv_obj_set_style_arc_width(s_volume_arc, 6, LV_PART_MAIN);
        lv_obj_set_style_arc_width(s_volume_arc, 6, LV_PART_INDICATOR);
        lv_obj_remove_flag(s_volume_arc, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_opa(s_volume_arc, LV_OPA_TRANSP, LV_PART_KNOB);
    } else {
        lv_obj_t* slider = lv_slider_create(vol_cont);
        lv_obj_set_size(slider, lv_pct(90), lv_pct(40));
        lv_obj_align(slider, LV_ALIGN_CENTER, 0, 0);
        lv_slider_set_value(slider, 50, LV_ANIM_OFF);
    }

    // ── SLOT_STATUS — room name / connectivity status ─────────────────────────
    lv_obj_t* status_cont = slot_container(scr, SLOT_STATUS);
    s_status_lbl = lv_label_create(status_cont);
    lv_label_set_text(s_status_lbl, "Connecting\xe2\x80\xa6");
    lv_obj_set_style_text_font(s_status_lbl, font(FontRole::Caption), 0);
    lv_obj_set_style_text_color(s_status_lbl, lv_color_hex(0x666666), 0);
    lv_obj_align(s_status_lbl, LV_ALIGN_CENTER, 0, 0);

    // 1 s interpolation timer
    s_interp_timer = lv_timer_create(interp_tick, 1000, nullptr);

    return scr;
}

// ── now_playing_update ────────────────────────────────────────────────────────

void screens::now_playing_update(const sonos::PlaybackState& state) {
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
            state.muted ? lv_color_hex(0x555555) : lv_color_hex(0x1DB954),
            LV_PART_INDICATOR);
    }
}
