#include "board_config.h"
#include "../bsp.h"

#include <Arduino.h>
#include <Wire.h>
#include <lvgl.h>
#include <esp_heap_caps.h>

/* ── Stub panel ─────────────────────────────────────────────────────────────
 *
 * No real panel driver is wired. The stub allocates the same PSRAM draw
 * buffers and registers the same LVGL display structure so every screen
 * file compiles against this board without modification.
 *
 * No encoder code path exists in this translation unit, satisfying the
 * requirement that a board declaring has_encoder=false contains none.
 */

static_assert(!kDisplay.has_encoder,
    "ref_rect_stub declares no encoder — check board_config.h");

static lv_display_t* s_disp = nullptr;

static constexpr uint32_t kBufLines  = 48;
static constexpr uint32_t kBufPixels = static_cast<uint32_t>(kDisplay.w) * kBufLines;
static uint16_t* s_buf1 = nullptr;
static uint16_t* s_buf2 = nullptr;

static void lvgl_flush_cb(lv_display_t* /*disp*/, const lv_area_t* /*area*/, uint8_t* /*px_map*/) {
    lv_display_flush_ready(s_disp);  // stub — no real panel
}

static void lvgl_touch_cb(lv_indev_t* /*indev*/, lv_indev_data_t* data) {
    data->state = LV_INDEV_STATE_RELEASED;  // stub — always no touch
}

void bsp::init() {
    Wire.begin(pins::kSda, pins::kScl);
    ledcAttach(pins::BACKLIGHT, 5000, 8);  // Arduino 3.x LEDC API
    ledcWrite(pins::BACKLIGHT, 0);
}

lv_display_t* bsp::display_init() {
    s_buf1 = static_cast<uint16_t*>(
        heap_caps_malloc(kBufPixels * sizeof(uint16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    s_buf2 = static_cast<uint16_t*>(
        heap_caps_malloc(kBufPixels * sizeof(uint16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));

    if (!s_buf1 || !s_buf2) {
        Serial.println("[BSP/stub] FATAL: PSRAM draw buffer allocation failed");
        while (true) delay(1000);
    }

    s_disp = lv_display_create(kDisplay.w, kDisplay.h);
    lv_display_set_flush_cb(s_disp, lvgl_flush_cb);
    lv_display_set_buffers(s_disp,
                           s_buf1, s_buf2,
                           kBufPixels * sizeof(uint16_t),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);

    static auto tick_cb = []() -> uint32_t { return static_cast<uint32_t>(millis()); };
    lv_tick_set_cb(tick_cb);
    return s_disp;
}

void bsp::input_init() {
    lv_indev_t* touch_dev = lv_indev_create();
    lv_indev_set_type(touch_dev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(touch_dev, lvgl_touch_cb);
    // No encoder on this board — if constexpr ensures no encoder code compiles in
}

void bsp::backlight(uint8_t brightness) {
    ledcWrite(pins::BACKLIGHT, brightness);
}

void bsp::suppress_touch_until_release() {
    // No touch input on the rectangular reference stub — no-op.
}

bool bsp::pop_event(InputEvent& /*evt*/) {
    return false;  // stub — no input events
}
