#pragma once
#include <Arduino.h>
#include <lvgl.h>
#include <freertos/FreeRTOS.h>

struct ArtState {
    uint8_t*   pixels  = nullptr;  // points into shared PSRAM buffer; do not free
    uint16_t   w       = 0;
    uint16_t   h       = 0;
    lv_color_t accent;             // dominant colour with legibility guard applied
    uint32_t   version = 0;
    String     uri;

    ArtState() { accent = lv_color_hex(0xD4A843); }
};

namespace art {
    constexpr size_t kArtJpegBufBytes  = 512u * 1024u;    // 512 KB — covers high-quality 640×640 JPEGs
    constexpr size_t kArtPixelBufBytes = 320u * 320u * 2u; // 205 KB — RGB565 native from JPEGDEC

    void     init();
    void     start_task();          // spawn art_task (core 0, priority 1)
    void     notify_new_uri();      // wake art_task to fetch/decode (sonos task only)
    ArtState snapshot();            // any task
}
