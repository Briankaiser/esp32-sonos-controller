## 1. Enable TJpgDec

- [x] 1.1 In `lib/lv_conf.h`, find the line `#define LV_USE_TJPGD 0` (or the block where it defaults to 0) and set `#define LV_USE_TJPGD 1`

## 2. Album Art URI in PlaybackState

- [x] 2.1 In `src/app/sonos/playback.h`, add `String album_art_uri;` to `PlaybackState` after the `album` field
- [x] 2.2 In `playback::poll()` in `src/app/sonos/playback.cpp`, after extracting `dc:title`, `dc:creator`, and `upnp:album` from the decoded DIDL, add: `String art_uri = extract_xml_text(didl, "upnp:albumArtURI");` and assign `next.album_art_uri = art_uri`

## 3. Art State and Fetch/Decode

- [x] 3.1 Create `src/app/sonos/art.h` declaring:
  - `struct ArtState { uint8_t* pixels = nullptr; uint16_t w = 0; uint16_t h = 0; lv_color_t accent = lv_color_hex(0x1DB954); uint32_t version = 0; String uri; }` — note: `pixels` points into the shared PSRAM decode buffer; callers must not free it
  - `constexpr size_t kArtJpegBufBytes  = 150 * 1024`
  - `constexpr size_t kArtPixelBufBytes = 230 * 230 * 3`
  - `namespace art`: `void init()`, `void fetch_and_decode(const String& url, const String& speaker_ip, uint16_t speaker_port)` (core 0 only), `ArtState snapshot()` (any task)
- [x] 3.2 Create `src/app/sonos/art.cpp`:
  - Static globals: `uint8_t* s_jpeg_buf`, `uint8_t* s_pixel_buf`, `SemaphoreHandle_t s_mutex`, `ArtState s_state`
  - `art::init()`: allocate `s_jpeg_buf` and `s_pixel_buf` from PSRAM with `heap_caps_malloc`; create mutex; log sizes
  - `art::snapshot()`: take mutex, copy `ArtState` (shallow — `pixels` pointer is valid until next `fetch_and_decode`), release, return
- [x] 3.3 In `art.cpp`, implement `art::fetch_and_decode(url, speaker_ip, speaker_port)`:
  - HTTP GET the `url` using `WiFiClient`; read body into `s_jpeg_buf`; record byte count; close connection; on error log and return without updating state
  - If byte count > `kArtJpegBufBytes`, log `[Art] JPEG too large (%u bytes)` and return
  - Call TJpgDec: create a local read-state struct holding `{buf: s_jpeg_buf, len: fetched_len, pos: 0}`; implement `input_func` as a static lambda reading from that struct; call `jd_prepare()` with a 4096-byte work buffer allocated on the stack
  - On `JDR_OK`: read `jdec.width` and `jdec.height`; choose scale: 0 if max(w,h)≤230, 1 if ≤460, 2 if ≤920, else 3; call `jd_decomp()` with scale and an output callback that writes decoded MCU blocks into `s_pixel_buf` at the correct stride (decoded_width × 3)
  - Store actual decoded dimensions (width >> scale, height >> scale) into local vars
  - Extract dominant colour: iterate s_pixel_buf with a stride that visits ~64×64 points; accumulate R, G, B for pixels where `L = (77*R + 150*G + 29*B) >> 8` > 40 AND `|R-G| + |G-B|` > 15; if ≥ 100 such pixels, compute accent = `lv_color_make(avgR, avgG, avgB)`; else use `lv_color_hex(0x1DB954)`
  - Apply legibility guard: compute `L` of accent; if `L < 60`, scale each channel by `60.0f / L` clamped at 255
  - Update `s_state` under mutex: increment version, set `pixels = s_pixel_buf`, `w`, `h`, `accent`, `uri = url`
  - Log `[Art] decoded %ux%u accent=#%06X` to serial

## 4. Sonos Task Wiring

- [x] 4.1 In `main.cpp`, add `#include "app/sonos/art.h"` after the existing sonos includes
- [x] 4.2 In `setup()`, call `art::init()` after `sonos::playback::init()` and before the task spawn block
- [x] 4.3 In `sonos_task()` in `main.cpp`, in the inner polling loop after `sonos::playback::poll(hr)`, add:
  ```cpp
  sonos::PlaybackState pb = sonos::playback::snapshot();
  sonos::ArtState      art_cur = art::snapshot();
  if (!pb.album_art_uri.isEmpty() && pb.album_art_uri != art_cur.uri) {
      art::fetch_and_decode(pb.album_art_uri, hr.ip, hr.port);
  }
  ```

## 5. Accent Token

- [x] 5.1 Create `src/ui/theme/accent.h` declaring `namespace accent` with:
  - `void init(lv_obj_t* progress_bar, lv_obj_t* volume_arc)` — stores widget handles, sets initial colour
  - `void update(lv_color_t new_color)` — starts animated transition (UI task only)
- [x] 5.2 Create `src/ui/theme/accent.cpp`:
  - Static globals: `lv_obj_t* s_bar`, `lv_obj_t* s_arc`, `lv_color_t s_current = lv_color_hex(0x1DB954)`
  - `accent::init()`: store handles; apply `s_current` to both widgets via `lv_obj_set_style_bg_color` / `lv_obj_set_style_arc_color`
  - `accent::update(new_color)`: if `new_color` equals `s_current` (same value), return early; otherwise start an `lv_anim_t` for the progress bar indicator using a custom exec callback that calls `lv_obj_set_style_bg_color(s_bar, lv_color_mix(s_current, new_color, anim_value), LV_PART_INDICATOR)` where `anim_value` goes 0→255; and a parallel `lv_anim_t` for the arc indicator; set duration 400 ms, path `lv_anim_path_ease_out`; on the completion callback for the bar animation, update `s_current = new_color`

## 6. Now-Playing Screen

- [x] 6.1 In `src/ui/screens/now_playing.h`, add `#include "../../app/sonos/art.h"` and `#include "../theme/accent.h"`; add `void now_playing_init_art(lv_obj_t* progress_bar, lv_obj_t* volume_arc)` to the `screens` namespace
- [x] 6.2 In `src/ui/screens/now_playing.cpp`, add statics: `static lv_obj_t* s_art_img = nullptr;` and `static uint32_t s_art_version = 0;`; add `static lv_image_dsc_t s_art_dsc = {}`
- [x] 6.3 In `now_playing_create()`, replace the dark square `SLOT_ALBUM_ART` block:
  - Remove the dark `#333333` rounded square
  - Create `s_art_img = lv_image_create(scr)`; position it using the slot geometry (center in SLOT_ALBUM_ART); set `lv_image_set_src(s_art_img, nullptr)` initially (no image); add a dark background object behind it at the same position as a fallback placeholder
- [x] 6.4 Implement `screens::now_playing_init_art(progress_bar, volume_arc)`: calls `accent::init(progress_bar, volume_arc)`; call this from `ui_task()` in `main.cpp` after `now_playing_create()`, passing the two widget handles (expose them from `now_playing_create()` as needed, or store and retrieve them)
- [x] 6.5 In `now_playing_update()`, after the existing arc update, add:
  ```cpp
  sonos::ArtState art = art::snapshot();
  if (art.version != s_art_version && art.pixels != nullptr) {
      s_art_version = art.version;
      s_art_dsc.header.magic      = LV_IMAGE_HEADER_MAGIC;
      s_art_dsc.header.cf         = LV_COLOR_FORMAT_RGB888;
      s_art_dsc.header.w          = art.w;
      s_art_dsc.header.h          = art.h;
      s_art_dsc.header.stride     = art.w * 3;
      s_art_dsc.data_size         = (uint32_t)art.w * art.h * 3;
      s_art_dsc.data              = art.pixels;
      lv_image_set_src(s_art_img, &s_art_dsc);
      accent::update(art.accent);
  }
  ```

## 7. Verification

- [x] 7.1 `pio run -e crowpanel_21_round` exits 0
- [x] 7.2 `pio run -e ref_rect_stub` exits 0
- [x] 7.3 `openspec validate --all --strict` is clean
- [ ] 7.4 On hardware: play a track with album art — confirm the art appears in the album art slot within a few seconds of the track starting
- [ ] 7.5 On hardware: skip to a different track — confirm the art updates to the new track's image
- [ ] 7.6 On hardware: confirm the progress bar and volume arc colour transitions to match the album art
- [ ] 7.7 On hardware: `curl http://<ip>/screenshot -o art.bmp` — attach screenshot showing the now-playing screen with live album art and a non-green accent colour
- [x] 7.8 Add `CHANGELOG.md` entry under `## [Unreleased]`: "Now-playing screen displays album art; the interface accent colour animates to match each track's art."
