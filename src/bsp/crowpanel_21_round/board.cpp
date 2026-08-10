#include "board_config.h"
#include "../bsp.h"

#include <Arduino.h>
#include <Wire.h>
#include <Arduino_GFX_Library.h>
#include <PCF8574.h>
#include <lvgl.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

// ── ST7701S initialisation sequence ─────────────────────────────────────────
// Adapted from mkvesala/ESP32-Crowpanel-compass (MIT licence).
// RGB666 (0x3A=0x60) used instead of RGB565 (0x50) — 0x50 renders incorrectly
// on this panel variant.  BGR cleared (0x36=0x00) for correct RGB channel order.

static const uint8_t kST7701InitOps[] = {
    BEGIN_WRITE,
    WRITE_COMMAND_8, 0xFF,
    WRITE_BYTES, 5, 0x77, 0x01, 0x00, 0x00, 0x10,

    WRITE_C8_D16, 0xC0, 0x3B, 0x00,
    WRITE_C8_D16, 0xC1, 0x0B, 0x02,
    WRITE_C8_D16, 0xC2, 0x00, 0x02,
    WRITE_C8_D8,  0xCC, 0x10,
    WRITE_C8_D8,  0xCD, 0x08,

    WRITE_COMMAND_8, 0xB0,
    WRITE_BYTES, 16,
    0x02, 0x13, 0x1B, 0x0D, 0x10, 0x05, 0x08, 0x07,
    0x07, 0x24, 0x04, 0x11, 0x0E, 0x2C, 0x33, 0x1D,

    WRITE_COMMAND_8, 0xB1,
    WRITE_BYTES, 16,
    0x05, 0x13, 0x1B, 0x0D, 0x11, 0x05, 0x08, 0x07,
    0x07, 0x24, 0x04, 0x11, 0x0E, 0x2C, 0x33, 0x1D,

    WRITE_COMMAND_8, 0xFF,
    WRITE_BYTES, 5, 0x77, 0x01, 0x00, 0x00, 0x11,

    WRITE_C8_D8, 0xB0, 0x5D,
    WRITE_C8_D8, 0xB1, 0x43,
    WRITE_C8_D8, 0xB2, 0x81,
    WRITE_C8_D8, 0xB3, 0x80,
    WRITE_C8_D8, 0xB5, 0x43,
    WRITE_C8_D8, 0xB7, 0x85,
    WRITE_C8_D8, 0xB8, 0x20,
    WRITE_C8_D8, 0xC1, 0x78,
    WRITE_C8_D8, 0xC2, 0x78,
    WRITE_C8_D8, 0xD0, 0x88,

    WRITE_COMMAND_8, 0xE0,
    WRITE_BYTES, 3, 0x00, 0x00, 0x02,

    WRITE_COMMAND_8, 0xE1,
    WRITE_BYTES, 11,
    0x03, 0xA0, 0x00, 0x00, 0x04, 0xA0, 0x00, 0x00,
    0x00, 0x20, 0x20,

    WRITE_COMMAND_8, 0xE2,
    WRITE_BYTES, 13,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00,

    WRITE_COMMAND_8, 0xE3,
    WRITE_BYTES, 4, 0x00, 0x00, 0x11, 0x00,

    WRITE_C8_D16, 0xE4, 0x22, 0x00,

    WRITE_COMMAND_8, 0xE5,
    WRITE_BYTES, 16,
    0x05, 0xEC, 0xA0, 0xA0, 0x07, 0xEE, 0xA0, 0xA0,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

    WRITE_COMMAND_8, 0xE6,
    WRITE_BYTES, 4, 0x00, 0x00, 0x11, 0x00,

    WRITE_C8_D16, 0xE7, 0x22, 0x00,

    WRITE_COMMAND_8, 0xE8,
    WRITE_BYTES, 16,
    0x06, 0xED, 0xA0, 0xA0, 0x08, 0xEF, 0xA0, 0xA0,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

    WRITE_COMMAND_8, 0xEB,
    WRITE_BYTES, 7,
    0x00, 0x00, 0x40, 0x40, 0x00, 0x00, 0x00,

    WRITE_COMMAND_8, 0xED,
    WRITE_BYTES, 16,
    0xFF, 0xFF, 0xFF, 0xBA, 0x0A, 0xBF, 0x45, 0xFF,
    0xFF, 0x54, 0xFB, 0xA0, 0xAB, 0xFF, 0xFF, 0xFF,

    WRITE_COMMAND_8, 0xEF,
    WRITE_BYTES, 6,
    0x10, 0x0D, 0x04, 0x08, 0x3F, 0x1F,

    WRITE_COMMAND_8, 0xFF,
    WRITE_BYTES, 5, 0x77, 0x01, 0x00, 0x00, 0x13,
    WRITE_C8_D8, 0xEF, 0x08,

    WRITE_COMMAND_8, 0xFF,
    WRITE_BYTES, 5, 0x77, 0x01, 0x00, 0x00, 0x00,

    WRITE_C8_D8, 0x36, 0x00,   // BGR=0 → RGB channel order
    WRITE_C8_D8, 0x3A, 0x60,   // RGB666 (0x50 RGB565 renders incorrectly on this panel)

    WRITE_COMMAND_8, 0x11,     // Sleep out
    END_WRITE,

    DELAY, 100,

    BEGIN_WRITE,
    WRITE_COMMAND_8, 0x29,     // Display on
    END_WRITE,

    DELAY, 50,
};

// ── Module-private state ─────────────────────────────────────────────────────

// Arduino_SWSPI(dc, cs, sck, mosi, miso): dc=-1 (3-wire), cs=-1 unused
static Arduino_SWSPI       s_init_bus(-1, pins::LCD_SPI_CLK,
                                      pins::LCD_SPI_SDA, pins::LCD_SPI_MISO,
                                      -1);
static Arduino_ESP32RGBPanel s_rgb_bus(
    pins::LCD_DE,    pins::LCD_VSYNC, pins::LCD_HSYNC, pins::LCD_PCLK,
    pins::LCD_R0, pins::LCD_R1, pins::LCD_R2, pins::LCD_R3, pins::LCD_R4,
    pins::LCD_G0, pins::LCD_G1, pins::LCD_G2, pins::LCD_G3, pins::LCD_G4, pins::LCD_G5,
    pins::LCD_B0, pins::LCD_B1, pins::LCD_B2, pins::LCD_B3, pins::LCD_B4,
    1, 10, 4, 24,   // hsync: polarity, front_porch, pulse_width, back_porch
    1, 10, 4, 24,   // vsync: polarity, front_porch, pulse_width, back_porch
    0,              // pclk_active_neg
    10000000,       // prefer_speed (Hz)
    false,          // useBigEndian
    0,              // de_idle_high
    0,              // pclk_idle_high
    4800            // bounce_buffer_size_px (10 lines × 480 px — DMA reads from SRAM)
);
static Arduino_RGB_Display s_gfx(
    480, 480, &s_rgb_bus,
    0,              // rotation
    true,           // auto_flush (DMA driven)
    &s_init_bus,
    GFX_NOT_DEFINED, // RST handled by PCF8574
    kST7701InitOps, sizeof(kST7701InitOps)
);

static PCF8574 s_pcf(pins::PCF_ADDR);
static lv_display_t* s_disp = nullptr;

// Draw buffers in PSRAM (partial render, 48 lines each)
static constexpr uint32_t kBufLines  = 48;
static constexpr uint32_t kBufPixels = 480 * kBufLines;
static uint16_t* s_buf1 = nullptr;
static uint16_t* s_buf2 = nullptr;

// Input event queue (encoder deltas + button presses)
static QueueHandle_t s_evt_queue = nullptr;

// Encoder ISR state
static volatile int32_t s_enc_delta = 0;
static volatile uint8_t s_enc_last_a = 0;

// ── LVGL flush callback ──────────────────────────────────────────────────────

static void lvgl_flush_cb(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map) {
    int32_t w = area->x2 - area->x1 + 1;
    int32_t h = area->y2 - area->y1 + 1;
    s_gfx.draw16bitRGBBitmap(area->x1, area->y1, reinterpret_cast<uint16_t*>(px_map), w, h);
    lv_display_flush_ready(disp);
}

// ── LVGL touch read callback ─────────────────────────────────────────────────

// Set true by bsp::suppress_touch_until_release(); the callback reports RELEASED
// until the finger physically lifts, consuming the idle-wake tap.
static bool s_suppress_touch = false;

static void lvgl_touch_cb(lv_indev_t* indev, lv_indev_data_t* data) {
    // CST816 register layout: [0x01]=gesture, [0x02]=touch count,
    //   [0x03]=YH+event, [0x04]=YL, [0x05]=XH+touch_id, [0x06]=XL
    Wire.beginTransmission(pins::TOUCH_ADDR);
    Wire.write(0x02);
    Wire.endTransmission(false);
    Wire.requestFrom(pins::TOUCH_ADDR, 5);
    if (Wire.available() < 5) {
        static bool s_warned = false;
        if (!s_warned) {
            Serial.printf("[Touch] I2C read failed (addr=0x%02X, avail=%d)\n",
                          pins::TOUCH_ADDR, Wire.available());
            s_warned = true;
        }
        data->state = LV_INDEV_STATE_RELEASED;
        return;
    }
    uint8_t count  = Wire.read();
    uint8_t yh     = Wire.read();
    uint8_t yl     = Wire.read();
    uint8_t xh     = Wire.read();
    uint8_t xl     = Wire.read();
    if (count == 0) {
        s_suppress_touch = false;  // finger lifted — resume normal reporting
        data->state = LV_INDEV_STATE_RELEASED;
        return;
    }
    if (s_suppress_touch) {
        // Consuming the idle-wake tap: report released until the finger lifts
        data->state = LV_INDEV_STATE_RELEASED;
        return;
    }
    data->state   = LV_INDEV_STATE_PRESSED;
    // CST816 X/Y axes are swapped relative to this display's orientation
    data->point.x = static_cast<int32_t>(((yh & 0x0F) << 8) | yl);
    data->point.y = static_cast<int32_t>(((xh & 0x0F) << 8) | xl);
}

// ── Encoder ISR (GPIO CHANGE on pin A) ──────────────────────────────────────

static void IRAM_ATTR enc_isr() {
    uint8_t a = digitalRead(pins::ENC_A);
    uint8_t b = digitalRead(pins::ENC_B);
    if (a != s_enc_last_a) {
        s_enc_delta += (a != b) ? 1 : -1;
        s_enc_last_a = a;
    }
}

// ── BSP implementation ───────────────────────────────────────────────────────

// LEDC channel for backlight PWM (Arduino core 2.x requires explicit channel)
static constexpr int kBklChannel = 0;

static uint32_t lvgl_tick_ms() { return static_cast<uint32_t>(millis()); }

void bsp::init() {
    // I2C bus
    Wire.begin(pins::kSda, pins::kScl, 400000);

    // PCF8574: robtillaart library uses write()/read(), no pinMode().
    // All pins start high (input mode via pull-up resistors on the chip).
    // Pre-set output-pin values before begin() so they take effect immediately.
    s_pcf.begin();

    // LCD power on
    s_pcf.write(pins::PCF_LCD_PWR, HIGH);
    delay(100);

    // LCD reset pulse
    s_pcf.write(pins::PCF_LCD_RST, HIGH);  delay(50);
    s_pcf.write(pins::PCF_LCD_RST, LOW);   delay(120);
    s_pcf.write(pins::PCF_LCD_RST, HIGH);  delay(120);

    // Touch reset pulse
    s_pcf.write(pins::PCF_TP_RST, HIGH);   delay(50);
    s_pcf.write(pins::PCF_TP_RST, LOW);    delay(120);
    s_pcf.write(pins::PCF_TP_RST, HIGH);   delay(120);
    s_pcf.write(pins::PCF_TP_INT, HIGH);   delay(50);

    // Input event queue (holds up to 16 events)
    s_evt_queue = xQueueCreate(16, sizeof(InputEvent));

    // Backlight off until the first frame is ready (Arduino 3.x LEDC API)
    ledcAttach(pins::BACKLIGHT, 5000, 8);
    ledcWrite(pins::BACKLIGHT, 0);
}

lv_display_t* bsp::display_init() {
    // Allocate draw buffers in PSRAM
    s_buf1 = static_cast<uint16_t*>(
        heap_caps_malloc(kBufPixels * sizeof(uint16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    s_buf2 = static_cast<uint16_t*>(
        heap_caps_malloc(kBufPixels * sizeof(uint16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));

    if (!s_buf1 || !s_buf2) {
        Serial.println("[BSP] FATAL: PSRAM draw buffer allocation failed");
        while (true) delay(1000);
    }

    // Panel init
    if (!s_gfx.begin()) {
        Serial.println("[BSP] FATAL: GFX panel init failed");
        while (true) delay(1000);
    }
    s_gfx.fillScreen(0x0000);  // RGB565 black

    // LVGL display
    s_disp = lv_display_create(kDisplay.w, kDisplay.h);
    lv_display_set_flush_cb(s_disp, lvgl_flush_cb);
    lv_display_set_buffers(s_disp,
                           s_buf1, s_buf2,
                           kBufPixels * sizeof(uint16_t),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);

    lv_tick_set_cb(lvgl_tick_ms);  // millis() returns unsigned long; wrapper gives uint32_t
    return s_disp;
}

void bsp::input_init() {
    // Touch input device
    lv_indev_t* touch_dev = lv_indev_create();
    lv_indev_set_type(touch_dev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(touch_dev, lvgl_touch_cb);

    // Encoder input device (for LVGL group navigation)
    if constexpr (kDisplay.has_encoder) {
        // Encoder A ISR
        pinMode(pins::ENC_A, INPUT);
        pinMode(pins::ENC_B, INPUT);
        s_enc_last_a = digitalRead(pins::ENC_A);
        attachInterrupt(digitalPinToInterrupt(pins::ENC_A), enc_isr, CHANGE);
    }
}

void bsp::backlight(uint8_t brightness) {
    ledcWrite(pins::BACKLIGHT, brightness);
}

void bsp::suppress_touch_until_release() {
    s_suppress_touch = true;
}

bool bsp::pop_event(InputEvent& evt) {
    // Drain encoder delta into volume events
    int32_t delta = s_enc_delta;
    if (delta != 0) {
        s_enc_delta -= delta;
        evt = {InputEvent{bsp::Intent::VolumeDelta, static_cast<int16_t>(delta)}};
        return true;
    }
    // Encoder button (polled via PCF8574, active-low) — emits Down on falling edge,
    // Up on rising edge so the UI task can implement the 30-second factory-reset hold.
    if constexpr (kDisplay.has_encoder) {
        static bool s_btn_pressed = false;
        bool cur = (s_pcf.read(pins::PCF_ENC_BTN) == LOW);
        if (cur && !s_btn_pressed) {
            s_btn_pressed = true;
            evt = {bsp::Intent::EncoderButtonDown, 0};
            return true;
        }
        if (!cur && s_btn_pressed) {
            s_btn_pressed = false;
            evt = {bsp::Intent::EncoderButtonUp, 0};
            return true;
        }
    }
    return xQueueReceive(s_evt_queue, &evt, 0) == pdTRUE;
}
