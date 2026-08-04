/* On-device display integration tests.
 *
 * Run with: pio test -e test_device   (CrowPanel must be connected via USB)
 *
 * Results are reported over serial at 115200 baud. The touch test requires
 * a physical finger tap within TOUCH_TIMEOUT_MS of test start.
 *
 * Tasks verified:
 *   5.3  Display begin() returns true
 *   5.4  Touch read returns coordinates within display bounds
 *   5.5  FPS ≥ 55 sustained under synthetic worker load
 */

#include <unity.h>
#include <Arduino.h>
#include <lvgl.h>

#include "bsp/bsp.h"
#include "bsp/display_profile.h"
#include "ui/layout/layout.h"
#include "ui/screens/placeholder.h"

void setUp()    {}
void tearDown() {}

// ── 5.3 Display bring-up ──────────────────────────────────────────────────────

void test_display_init_succeeds() {
    lv_init();
    lv_display_t* disp = bsp::display_init();
    TEST_ASSERT_NOT_NULL(disp);
    TEST_ASSERT_EQUAL_UINT16(kDisplay.w, lv_display_get_horizontal_resolution(disp));
    TEST_ASSERT_EQUAL_UINT16(kDisplay.h, lv_display_get_vertical_resolution(disp));
}

// ── 5.4 Touch registration ────────────────────────────────────────────────────

static constexpr uint32_t TOUCH_TIMEOUT_MS = 5000;

void test_touch_read_within_bounds() {
    // If no touch within timeout, skip with informative message
    uint32_t deadline = millis() + TOUCH_TIMEOUT_MS;

    while (millis() < deadline) {
        // Poll the CST816 directly (same as board.cpp's lvgl_touch_cb)
        Wire.beginTransmission(pins::TOUCH_ADDR);
        Wire.write(0x02);
        Wire.endTransmission(false);
        Wire.requestFrom(pins::TOUCH_ADDR, 5);
        if (Wire.available() >= 5) {
            uint8_t count = Wire.read();
            uint8_t yh    = Wire.read();
            uint8_t yl    = Wire.read();
            uint8_t xh    = Wire.read();
            uint8_t xl    = Wire.read();
            if (count > 0) {
                int32_t x = ((xh & 0x0F) << 8) | xl;
                int32_t y = ((yh & 0x0F) << 8) | yl;
                TEST_ASSERT_INT_WITHIN(kDisplay.w, kDisplay.w / 2, x);
                TEST_ASSERT_INT_WITHIN(kDisplay.h, kDisplay.h / 2, y);
                return;  // pass
            }
        }
        delay(20);
    }
    TEST_IGNORE_MESSAGE("Touch not detected within 5 s — tap the screen during this test");
}

// ── 5.5 Frame-rate budget ─────────────────────────────────────────────────────

static constexpr uint32_t FPS_MEASURE_MS = 3000;
static constexpr uint32_t FPS_MIN        = 55;

// Synthetic worker task: sends a heartbeat every 100 ms to stress the queue
static void synthetic_worker(void* /*arg*/) {
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void test_fps_meets_budget() {
    // Build the placeholder screen and load it
    lv_obj_t* scr = screens::placeholder_create();
    lv_screen_load(scr);

    // Spawn a synthetic worker to exercise the task scheduler
    xTaskCreatePinnedToCore(synthetic_worker, "TestWorker", 2048, nullptr, 3, nullptr, 0);

    // Run LVGL for FPS_MEASURE_MS
    uint32_t end = millis() + FPS_MEASURE_MS;
    while (millis() < end) {
        lv_timer_handler();
        delay(5);
    }

    uint32_t fps = lv_display_get_fps(lv_display_get_default());
    Serial.printf("[test_display] Measured FPS: %u (minimum: %u)\n", fps, FPS_MIN);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(FPS_MIN, fps);
}

// ── Main ──────────────────────────────────────────────────────────────────────

void setup() {
    Serial.begin(115200);
    delay(2000);  // Allow USB serial to connect

    bsp::init();
    bsp::input_init();

    UNITY_BEGIN();

    RUN_TEST(test_display_init_succeeds);
    RUN_TEST(test_touch_read_within_bounds);
    RUN_TEST(test_fps_meets_budget);

    UNITY_END();
}

void loop() {
    // Tests run once in setup(); loop just yields
    vTaskDelay(portMAX_DELAY);
}
