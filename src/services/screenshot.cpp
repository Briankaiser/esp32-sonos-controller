#include "screenshot.h"

#include <esp_heap_caps.h>
#include <lvgl.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace screenshot {

static constexpr uint32_t kW           = 480;
static constexpr uint32_t kH           = 480;
static constexpr uint32_t kPixelBytes  = kW * kH * 3;   // RGB888
static constexpr uint32_t kHeaderBytes = 54;
static constexpr uint32_t kBmpSize     = kHeaderBytes + kPixelBytes; // 691 254

// 480×480 24-bit BMP header (BITMAPFILEHEADER 14 B + BITMAPINFOHEADER 40 B)
static const uint8_t kBmpHeader[kHeaderBytes] = {
    0x42, 0x4D,              // 'BM'
    0x76, 0x8A, 0x0A, 0x00,  // file size 691 254
    0x00, 0x00, 0x00, 0x00,  // reserved
    0x36, 0x00, 0x00, 0x00,  // pixel array offset 54
    0x28, 0x00, 0x00, 0x00,  // DIB header size 40
    0xE0, 0x01, 0x00, 0x00,  // width 480
    0xE0, 0x01, 0x00, 0x00,  // height 480 (positive → bottom-up rows)
    0x01, 0x00,              // colour planes 1
    0x18, 0x00,              // bits per pixel 24
    0x00, 0x00, 0x00, 0x00,  // compression BI_RGB
    0x00, 0xF4, 0x0A, 0x00,  // image data size 691 200
    0x00, 0x00, 0x00, 0x00,  // X pixels/metre
    0x00, 0x00, 0x00, 0x00,  // Y pixels/metre
    0x00, 0x00, 0x00, 0x00,  // colours in table
    0x00, 0x00, 0x00, 0x00,  // important colours
};

static uint8_t*          s_bmp_buf     = nullptr; // 691 254 B output buffer (PSRAM)
static uint8_t*          s_snap_pixels = nullptr; // 691 200 B snapshot pixels (PSRAM)
static lv_draw_buf_t     s_snap_buf;              // LVGL draw-buf wrapper (no heap)
static SemaphoreHandle_t s_done        = nullptr; // UI task → web task signal
static SemaphoreHandle_t s_mutex       = nullptr; // serialise concurrent requests
static volatile bool     s_request     = false;
static volatile bool     s_snap_ok     = false;
static WebServer*        s_srv         = nullptr;

void init() {
    // Allocate both buffers from PSRAM directly rather than from the 256 KB
    // LVGL pool: the snapshot pixels alone (675 KB) exceed the pool size.
    s_bmp_buf     = (uint8_t*)heap_caps_malloc(kBmpSize,    MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    s_snap_pixels = (uint8_t*)heap_caps_malloc(kPixelBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    if (s_bmp_buf) {
        memcpy(s_bmp_buf, kBmpHeader, kHeaderBytes);
    }

    // Wrap s_snap_pixels in an lv_draw_buf_t for lv_snapshot_take_to_draw_buf.
    // ALLOCATED flag is NOT set, so LVGL will never try to free this memory.
    lv_draw_buf_init(&s_snap_buf, kW, kH, LV_COLOR_FORMAT_RGB888,
                     kW * 3, s_snap_pixels, kPixelBytes);

    s_done  = xSemaphoreCreateBinary();
    s_mutex = xSemaphoreCreateMutex();
}

// Called from ui_task (core 1) after each lv_timer_handler().
void poll() {
    if (!s_request) return;

    // lv_layer_top() is a sibling of the active screen, not a descendant of it —
    // a modal overlay (settings, favorites, grouping) living there is invisible
    // to a snapshot rooted at lv_screen_active(). Every overlay is full-screen
    // and fully opaque, so when one is open, snapshotting the top layer instead
    // captures the whole visible picture (same detection used by the overlay
    // guard in now_playing.cpp and nav::has_modal()).
    lv_obj_t* target = lv_screen_active();
    lv_obj_t* layer   = lv_layer_top();
    uint32_t  n       = lv_obj_get_child_count(layer);
    for (uint32_t i = 0; i < n; i++) {
        if (!lv_obj_has_flag(lv_obj_get_child(layer, i), LV_OBJ_FLAG_HIDDEN)) {
            target = layer;
            break;
        }
    }

    lv_result_t res = lv_snapshot_take_to_draw_buf(
        target, LV_COLOR_FORMAT_RGB888, &s_snap_buf);

    s_snap_ok = (res == LV_RESULT_OK);

    if (s_snap_ok) {
        // LVGL stores rows top-down; BMP (positive height) stores rows bottom-up.
        // Swap row order and convert RGB→BGR in one pass.
        for (uint32_t y = 0; y < kH; y++) {
            const uint8_t* src = s_snap_pixels + y * kW * 3;
            uint8_t*       dst = s_bmp_buf + kHeaderBytes + (kH - 1 - y) * kW * 3;
            for (uint32_t x = 0; x < kW; x++) {
                dst[x * 3 + 0] = src[x * 3 + 2]; // B
                dst[x * 3 + 1] = src[x * 3 + 1]; // G
                dst[x * 3 + 2] = src[x * 3 + 0]; // R
            }
        }
    }

    s_request = false;
    xSemaphoreGive(s_done);
}

void register_handler(WebServer& server) {
    s_srv = &server;

    server.on("/screenshot", HTTP_GET, []() {
        if (!s_bmp_buf || !s_done || !s_mutex) {
            s_srv->send(503, "text/plain", "Unavailable");
            return;
        }
        if (xSemaphoreTake(s_mutex, portMAX_DELAY) != pdTRUE) {
            s_srv->send(503, "text/plain", "Busy");
            return;
        }
        s_snap_ok = false;
        s_request = true;

        bool got = (xSemaphoreTake(s_done, pdMS_TO_TICKS(3000)) == pdTRUE);

        if (!got) {
            s_request = false; // cancel in case UI task hasn't picked it up yet
            xSemaphoreGive(s_mutex);
            s_srv->send(503, "text/plain", "Timeout");
            return;
        }

        if (!s_snap_ok) {
            xSemaphoreGive(s_mutex);
            s_srv->send(503, "text/plain", "Snapshot failed");
            return;
        }

        s_srv->setContentLength(kBmpSize);
        s_srv->send(200, "image/bmp", "");
        s_srv->sendContent((const char*)s_bmp_buf, kBmpSize);
        xSemaphoreGive(s_mutex);
    });
}

} // namespace screenshot
