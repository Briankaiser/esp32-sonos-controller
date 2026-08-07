## 1. Loading Overlay

- [ ] 1.1 In `src/ui/screens/now_playing.cpp`, add two statics after the existing statics block:
  `static lv_obj_t* s_loading_overlay = nullptr;` and `static bool s_loading_dismissed = false;`

- [ ] 1.2 At the very end of `now_playing_create()`, just before `return scr`, add:
  ```cpp
  // Loading overlay — covers all screen content until first real state arrives
  s_loading_overlay = lv_obj_create(scr);
  lv_obj_set_size(s_loading_overlay, lv_pct(100), lv_pct(100));
  lv_obj_set_style_bg_color(s_loading_overlay, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(s_loading_overlay, LV_OPA_80, 0);
  lv_obj_set_style_border_width(s_loading_overlay, 0, 0);
  lv_obj_clear_flag(s_loading_overlay, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_t* loading_lbl = lv_label_create(s_loading_overlay);
  lv_label_set_text(loading_lbl, "Connecting\xe2\x80\xa6");
  lv_obj_set_style_text_font(loading_lbl, font(FontRole::Caption), 0);
  lv_obj_set_style_text_color(loading_lbl, lv_color_hex(0xAAAAAA), 0);
  lv_obj_align(loading_lbl, LV_ALIGN_CENTER, 0, 0);
  ```

- [ ] 1.3 In `now_playing_update()`, add a guard at the very beginning of the function body (before `s_last = state`):
  ```cpp
  if (!s_loading_dismissed && state.transport_state != sonos::TransportState::Unknown) {
      s_loading_dismissed = true;
      if (s_loading_overlay) {
          // Fade out and delete
          static lv_anim_t s_fade;
          lv_anim_init(&s_fade);
          lv_anim_set_var(&s_fade, s_loading_overlay);
          lv_anim_set_exec_cb(&s_fade, [](void* obj, int32_t v) {
              lv_obj_set_style_opa(static_cast<lv_obj_t*>(obj), (lv_opa_t)v, 0);
          });
          lv_anim_set_completed_cb(&s_fade, [](lv_anim_t* a) {
              lv_obj_delete(static_cast<lv_obj_t*>(lv_anim_get_var(a)));
              s_loading_overlay = nullptr;
          });
          lv_anim_set_values(&s_fade, LV_OPA_80, LV_OPA_TRANSP);
          lv_anim_set_duration(&s_fade, 300);
          lv_anim_set_path_cb(&s_fade, lv_anim_path_ease_out);
          lv_anim_start(&s_fade);
      }
  }
  ```

## 2. Verification

- [ ] 2.1 `pio run -e crowpanel_21_round` exits 0
- [ ] 2.2 `pio run -e ref_rect_stub` exits 0
- [ ] 2.3 On hardware: power cycle the device — confirm the dimmed overlay with "Connecting…" text appears while the device is connecting and discovering speakers
- [ ] 2.4 On hardware: confirm the overlay fades out within ~300 ms of the first playback state arriving and does not reappear
- [ ] 2.5 Add `CHANGELOG.md` entry under `## [Unreleased]`: "A dimmed loading overlay with connecting text covers the now-playing screen until the first Sonos state is received."
