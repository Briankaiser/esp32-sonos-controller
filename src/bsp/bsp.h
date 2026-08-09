#pragma once

/* ── Board selection guard ───────────────────────────────────────────────────
 *
 * Each PlatformIO env adds -I src/bsp/<board>/ and -DOPENSONOS_BOARD_SELECTED
 * to its build_flags.  If neither is set the following error fires, naming the
 * available board identifiers so the developer knows what to pass.
 */
#ifndef OPENSONOS_BOARD_SELECTED
#  error "No board selected. Add -I src/bsp/<board> -DOPENSONOS_BOARD_SELECTED to build_flags. \
Available boards: crowpanel_21_round, ref_rect_stub"
#endif

#include <lvgl.h>
#include "input.h"

/* ── BSP free-function contract ──────────────────────────────────────────────
 *
 * Each board provides exactly one translation unit (board.cpp) that defines
 * these functions.  They are resolved at link time — zero virtual dispatch.
 *
 * Calling order:
 *   bsp::init()
 *   bsp::display_init()   → LVGL must be initialised before this call
 *   bsp::input_init()
 *   bsp::backlight(255)   → turn on after the first frame is ready
 */
namespace bsp {

/**
 * Initialise clocks, PSRAM, I2C bus, and PCF8574 GPIO expander.
 * Holds LCD backlight off until backlight() is called.
 */
void init();

/**
 * Initialise the panel, allocate PSRAM draw buffers, create and register
 * the LVGL display.  Returns the lv_display_t* owned by the BSP.
 *
 * Precondition: lv_init() has been called.
 */
lv_display_t* display_init();

/**
 * Register touch and (if present) encoder as LVGL input devices.
 * Starts the encoder delta ISR.
 */
void input_init();

/**
 * Set backlight brightness 0–255 (0 = off, 255 = full).
 */
void backlight(uint8_t brightness);

/**
 * Suppress touch reports until the current touch is released.  Used to consume
 * the tap that wakes the display from idle so it does not also trigger a button.
 * The touch layer reports "released" until the finger physically lifts.
 */
void suppress_touch_until_release();

/**
 * Drain one pending InputEvent, if any.  Returns true and fills *evt
 * when an event is available; returns false when the queue is empty.
 *
 * Called from the UI task between lv_timer_handler() calls.
 * Never called from an ISR.
 */
bool pop_event(InputEvent& evt);

} // namespace bsp
