## 1. Dependencies and Cleanup

- [x] 1.1 In `platformio.ini`, add `bitbank2/JPEGDEC @ ^2.0.0` to the `[libs]` `lib_deps` block
- [x] 1.2 In `lib/lv_conf.h`, remove the line `#define LV_USE_TJPGD 1` (and its comment)
- [x] 1.3 Delete the directory `lib/tjpgd_override/` (no longer needed now that TJpgDec is gone)

## 2. Art Task Infrastructure

- [x] 2.1 In `src/app/sonos/art.h`, add `void start_task()` to the `art` namespace (spawns `art_task`, stores handle); add `void notify_new_uri()` (calls `xTaskNotifyGive` on the stored handle)
- [x] 2.2 In `src/app/sonos/art.cpp`:
  - Add `static TaskHandle_t s_art_task_handle = nullptr`
  - Add `static constexpr uint32_t kArtTaskStackBytes = 12 * 1024`
  - Implement `art::start_task()`: call `xTaskCreatePinnedToCore(art_task_body, "Art", kArtTaskStackBytes, nullptr, 1, &s_art_task_handle, 0)`
  - Implement `art::notify_new_uri()`: `if (s_art_task_handle) xTaskNotifyGive(s_art_task_handle)`
  - Add the task body `static void art_task_body(void*)`: loop on `ulTaskNotifyTake(pdTRUE, portMAX_DELAY)`; on wake, read the current art URI from `sonos::playback::snapshot().album_art_uri`; read the current cached URI from `art::snapshot().uri`; if they differ and the URI is non-empty, call `fetch_and_decode(uri, speaker_ip, speaker_port)` — read speaker IP/port from `sonos::state::snapshot()`

## 3. JPEGDEC Decode

- [x] 3.1 Update `kArtPixelBufBytes` in `src/app/sonos/art.h` to `320u * 320u * 2u` (RGB565, no intermediate RGB888 buffer)
- [x] 3.2 Remove all TJpgDec-specific code from `src/app/sonos/art.cpp`:
  - Remove the `#include "libs/tjpgd/tjpgd.h"` line
  - Remove the `JpegIo`, `JpegOut` structs and all `tjpgd_*` static functions and globals (`s_cur_io`, `s_cur_out`)
  - Remove the TJpgDec decode block (jd_prepare / jd_decomp), the scale selection, the pixel buffer overflow check, the nearest-neighbour downsample loop, and the RGB888→RGB565 conversion loop
- [x] 3.3 Add `#include <JPEGDEC.h>` to `art.cpp`
- [x] 3.4 Add a file-scope `static JPEGDEC s_jpeg` decoder object and a draw callback:
  ```cpp
  static int jpeg_draw(JPEGDRAW* pDraw) {
      // pDraw->pPixels is a line of RGB565 pixels, pDraw->iWidth wide
      // destination stride: s_decoded_w * 2 bytes (set when decode starts)
      uint8_t* dst = s_pixel_buf
                   + (uint32_t)pDraw->y * s_decoded_w * 2u
                   + (uint32_t)pDraw->x * 2u;
      memcpy(dst, pDraw->pPixels, (uint32_t)pDraw->iWidth * 2u);
      return 1;  // continue
  }
  ```
  Add `static uint16_t s_decoded_w = 0` at file scope.
- [x] 3.5 Implement the new decode section in `fetch_and_decode()` after the HTTP fetch:
  - Call `s_jpeg.openRAM(s_jpeg_buf, (int)fetched, jpeg_draw)`
  - Read dimensions: `int img_w = s_jpeg.getWidth(), img_h = s_jpeg.getHeight()`
  - Choose scale: `int scale = (img_w > 320 || img_h > 320) ? JPEG_SCALE_HALF : JPEG_SCALE_NONE`
  - Set `s_decoded_w = (uint16_t)(img_w >> (scale == JPEG_SCALE_HALF ? 1 : 0))`; compute `dec_h` similarly
  - Verify `(uint32_t)s_decoded_w * dec_h * 2u <= kArtPixelBufBytes`; log and return on overflow
  - Call `s_jpeg.decode(0, 0, scale)` — returns 1 on success
  - On failure, log `[Art] JPEGDEC decode failed` and return
  - Extract dominant colour from the decoded RGB565 buffer (see task 3.6)
  - Update `s_state` under mutex as before
  - Log `[Art] decoded %ux%u accent=#%02X%02X%02X`
- [x] 3.6 Update `extract_dominant()` to read RGB565 input (2 bytes/pixel):
  - Change signature to `static lv_color_t extract_dominant(const uint8_t* pixels, uint16_t w, uint16_t h)`
  - Unpack each sample as `uint16_t px = *reinterpret_cast<const uint16_t*>(pixels + (y*w+x)*2); uint8_t r = (px>>11&0x1F)<<3; uint8_t g = (px>>5&0x3F)<<2; uint8_t b = (px&0x1F)<<3`

## 4. Sonos Task Wiring

- [x] 4.1 In `src/main.cpp`, replace the `art::fetch_and_decode()` block in `sonos_task` with:
  ```cpp
  sonos::PlaybackState pb = sonos::playback::snapshot();
  ArtState art_cur = art::snapshot();
  if (!pb.album_art_uri.isEmpty() && pb.album_art_uri != art_cur.uri) {
      art::notify_new_uri();
  }
  ```
- [x] 4.2 In `setup()` in `main.cpp`, add `art::start_task()` after `art::init()` and before the existing task spawn block

## 5. Verification

- [x] 5.1 `pio run -e crowpanel_21_round` exits 0
- [x] 5.2 `pio run -e ref_rect_stub` exits 0
- [x] 5.3 On hardware: rotate encoder or tap transport during a track change — confirm controls respond immediately without the 3–5 s freeze
- [x] 5.4 On hardware: confirm album art loads correctly with accurate colors on a colorful image
- [x] 5.5 On hardware: `curl http://<ip>/screenshot -o art.bmp` — attach screenshot with live album art
- [x] 5.6 Add `CHANGELOG.md` entry under `## [Unreleased]`: "Album art now loads asynchronously — transport controls and volume respond immediately during art decode. Switched to JPEGDEC for faster, more compatible JPEG decoding."
