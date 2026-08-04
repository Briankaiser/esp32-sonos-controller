/**
 * LVGL 9.x configuration for the ESP32-S3 Sonos Controller.
 *
 * Draw buffers are allocated explicitly in PSRAM in board.cpp; this file
 * governs LVGL's internal heap and feature set only.
 */

#if 1  /* guard against multiple inclusion */

#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/* ── Color ─────────────────────────────────────────────────────────────────── */

#define LV_COLOR_DEPTH 16           /* RGB565 — matches 16-bit RGB parallel interface */

/* ── Memory ─────────────────────────────────────────────────────────────────── */

/* Internal heap for LVGL objects (lv_obj_t, styles, timers).
 * Keep in internal SRAM for speed; 128 KB is sufficient for ~200 objects.
 * Draw buffers are allocated separately via heap_caps_malloc(MALLOC_CAP_SPIRAM). */
#define LV_MEM_CUSTOM 0
#define LV_MEM_SIZE   (128 * 1024U)

/* ── Tick ───────────────────────────────────────────────────────────────────── */

/* lv_tick_set_cb(millis) is called in bsp::display_init() */
#define LV_TICK_CUSTOM 0

/* ── Display ────────────────────────────────────────────────────────────────── */

#define LV_DRAW_BUF_ALIGN           4
#define LV_DRAW_BUF_STRIDE_ALIGN    1

/* ── Features ───────────────────────────────────────────────────────────────── */

#define LV_USE_STDLIB_MALLOC    LV_STDLIB_BUILTIN
#define LV_USE_STDLIB_STRING    LV_STDLIB_BUILTIN
#define LV_USE_STDLIB_SPRINTF   LV_STDLIB_BUILTIN

/* ── Widgets ────────────────────────────────────────────────────────────────── */

#define LV_USE_ARC         1
#define LV_USE_BUTTON      1
#define LV_USE_IMAGE       1
#define LV_USE_LABEL       1
#define LV_USE_LINE        1
#define LV_USE_SLIDER      1
#define LV_USE_SPAN        0
#define LV_USE_SWITCH      1
#define LV_USE_TEXTAREA    0
#define LV_USE_KEYBOARD    0   /* requires LV_USE_TEXTAREA */
#define LV_USE_TABLE       0
#define LV_USE_ANIMIMG     1
#define LV_USE_CANVAS      0
#define LV_USE_CHART       0
#define LV_USE_METER       0
#define LV_USE_ROLLER      0
#define LV_USE_LIST        1
#define LV_USE_DROPDOWN    0
#define LV_USE_MSGBOX      0
#define LV_USE_SPINBOX     0
#define LV_USE_TILEVIEW    1
#define LV_USE_WIN         0
#define LV_USE_TABVIEW     1
#define LV_USE_CALENDAR    0   /* calendar requires LV_USE_DROPDOWN; disable both */

/* ── Animations ─────────────────────────────────────────────────────────────── */

#define LV_USE_ANIM        1

/* ── Input devices ──────────────────────────────────────────────────────────── */

#define LV_USE_INDEV       1

/* ── Fonts (medium density class: FONTSET_MEDIUM) ───────────────────────────── */
/* These built-in fonts cover all semantic roles for the initial bootstrap.
 * Custom subset fonts (Latin-1 + icons) will replace them in a later change. */

#define LV_FONT_MONTSERRAT_12 1    /* Caption  */
#define LV_FONT_MONTSERRAT_16 1    /* Body     */
#define LV_FONT_MONTSERRAT_20 1    /* Title    */
#define LV_FONT_MONTSERRAT_28 1    /* Display  (time, big numbers) */
#define LV_FONT_MONTSERRAT_36 1    /* Display+ (screensaver clock)  */
#define LV_FONT_MONTSERRAT_14 1    /* spare / icon fallback         */

#define LV_FONT_DEFAULT &lv_font_montserrat_16

/* Disable fonts we don't use to save flash */
#define LV_FONT_MONTSERRAT_8  0
#define LV_FONT_MONTSERRAT_10 0
#define LV_FONT_MONTSERRAT_18 0
#define LV_FONT_MONTSERRAT_22 0
#define LV_FONT_MONTSERRAT_24 0
#define LV_FONT_MONTSERRAT_30 0
#define LV_FONT_MONTSERRAT_32 0
#define LV_FONT_MONTSERRAT_34 0
#define LV_FONT_MONTSERRAT_38 0
#define LV_FONT_MONTSERRAT_40 0
#define LV_FONT_MONTSERRAT_42 0
#define LV_FONT_MONTSERRAT_44 0
#define LV_FONT_MONTSERRAT_46 0
#define LV_FONT_MONTSERRAT_48 0

/* ── Logging ────────────────────────────────────────────────────────────────── */

#define LV_USE_LOG      1
#define LV_LOG_LEVEL    LV_LOG_LEVEL_WARN
#define LV_LOG_PRINTF   1

/* ── Debug / perf helpers ───────────────────────────────────────────────────── */

#define LV_USE_PERF_MONITOR  0   /* enabled at runtime when OPENSONOS_DEBUG_OVERLAY=1 */
#define LV_USE_MEM_MONITOR   0

#endif /* LV_CONF_H */
#endif /* guard */
