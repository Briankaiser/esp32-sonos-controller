#include <Arduino.h>
#include <lvgl.h>

#include "bsp/bsp.h"
#include "bsp/input.h"
#include "ui/layout/layout.h"
#include "ui/screens/placeholder.h"

/* ── Task configuration ─────────────────────────────────────────────────────
 *
 * ui_task   — core 1, owns ALL lv_* calls.  Never call LVGL from any other task.
 * worker_task — core 0, placeholder for future Sonos/art/weather I/O.
 */

static constexpr uint32_t kUiTaskStackBytes    = 12 * 1024;
static constexpr uint32_t kWorkerTaskStackBytes = 8 * 1024;
static constexpr UBaseType_t kUiTaskPriority     = 5;
static constexpr UBaseType_t kWorkerTaskPriority  = 3;

// Queue for worker → UI results (placeholder — actual message type defined per change)
static QueueHandle_t s_worker_to_ui = nullptr;
static constexpr uint32_t kWorkerQueueDepth = 8;
static constexpr uint32_t kWorkerQueueItemBytes = 64;  // placeholder size

// Pointer to the active screen (used to guard intent delivery)
static lv_obj_t* volatile s_active_scr = nullptr;

/* ── Worker task (core 0) ───────────────────────────────────────────────────
 *
 * Placeholder.  Future changes (Sonos discovery, album art, weather) will
 * add work here.  All results are posted to s_worker_to_ui; the UI task
 * picks them up safely between lv_timer_handler() calls.
 *
 * For the bootstrap this runs a synthetic 2 Hz "heartbeat" to exercise the
 * queue under the FPS measurement (task 8.3).
 */
static void worker_task(void* /*arg*/) {
    static uint8_t msg[kWorkerQueueItemBytes] = {};
    for (;;) {
        xQueueSend(s_worker_to_ui, msg, 0);   // heartbeat — ignored by UI
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

/* ── UI task (core 1) ───────────────────────────────────────────────────────
 *
 * Owns the LVGL event loop. Never yields to blocking I/O.
 * Drains:
 *   • lv_timer_handler()
 *   • bsp::pop_event()  → delivers intents to active screen
 *   • s_worker_to_ui queue (placeholder — consumed but not yet acted on)
 */
static void ui_task(void* /*arg*/) {
    // LVGL init (must run on the task that will own it)
    lv_init();

    // Panel and input bring-up
    bsp::display_init();
    bsp::input_init();

    // Build and load the placeholder screen
    lv_obj_t* scr = screens::placeholder_create();
    lv_screen_load(scr);
    s_active_scr = scr;

    // Turn on backlight after the first frame is ready
    bsp::backlight(200);

    // Event loop
    for (;;) {
        uint32_t next_ms = lv_timer_handler();

        // Drain input events and route to active screen as intents.
        // If the active screen has changed, discard pending events.
        bsp::InputEvent evt;
        while (bsp::pop_event(evt)) {
            lv_obj_t* cur = s_active_scr;
            if (!cur) continue;
            // Route to screen — placeholder just consumes them silently.
            // Later changes replace this with intent dispatch per-screen.
            (void)evt;
        }

        // Drain worker queue (results arrive here once Sonos etc. exist)
        uint8_t worker_msg[kWorkerQueueItemBytes];
        while (xQueueReceive(s_worker_to_ui, worker_msg, 0) == pdTRUE) {
            // Placeholder: ignore
        }

        // Yield for the remaining time LVGL says it won't need the CPU
        uint32_t delay_ms = (next_ms < 1) ? 1 : (next_ms > 20 ? 20 : next_ms);
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }
}

/* ── Arduino entry points ───────────────────────────────────────────────────
 *
 * setup() runs on core 1, but we immediately hand off to FreeRTOS tasks
 * so that core assignment is explicit and not reliant on Arduino's default.
 */
void setup() {
    Serial.begin(115200);
    delay(100);
    Serial.println("[MAIN] Sonos Controller starting");

    // Hardware init (I2C, PCF8574, backlight off)
    bsp::init();

    // Worker → UI queue
    s_worker_to_ui = xQueueCreate(kWorkerQueueDepth, kWorkerQueueItemBytes);

    // Spawn tasks
    xTaskCreatePinnedToCore(worker_task, "Worker", kWorkerTaskStackBytes,
                            nullptr, kWorkerTaskPriority, nullptr, 0);
    xTaskCreatePinnedToCore(ui_task, "UI", kUiTaskStackBytes,
                            nullptr, kUiTaskPriority, nullptr, 1);

    // main task (Arduino's setup/loop) is not needed after this point
}

void loop() {
    // Worker and UI are FreeRTOS tasks; loop() just yields forever.
    vTaskDelay(portMAX_DELAY);
}
