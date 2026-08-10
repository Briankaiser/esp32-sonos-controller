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

/* Route the LVGL pool to PSRAM on Xtensa/ESP32 targets.
 *
 * clip_corner + LV_RADIUS_CIRCLE causes LVGL to create temporary ARGB8888 layer
 * buffers (~92 KB at 480×48) for corner compositing. These don't fit alongside
 * widget objects in a 128 KB static SRAM pool, causing a constant stream of
 * "No memory" warnings and a black display.
 *
 * LV_MEM_POOL_ALLOC redirects the entire LVGL TLSF heap to PSRAM (8 MB available).
 * Increase the pool to 256 KB since cost is now paid by PSRAM, not SRAM.
 * On non-Xtensa hosts (test_native) the static array path is used unchanged.
 */
#ifdef __XTENSA__
#  include <esp_heap_caps.h>
#  define LV_MEM_POOL_ALLOC(size) \
       heap_caps_malloc((size), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
#endif

#define LV_MEM_CUSTOM 0
/* Pool is backed by PSRAM (see LV_MEM_POOL_ALLOC above), so 2 MB is cheap.
 * lv_snapshot_take_to_draw_buf() allocates two 480×240 ARGB8888 layer buffers
 * (~460 KB each) from this pool during rendering, which exceeds the old 256 KB cap. */
#define LV_MEM_SIZE   (2 * 1024 * 1024U)

/* ── Tick ───────────────────────────────────────────────────────────────────── */

/* lv_tick_set_cb(millis) is called in bsp::display_init() */
#define LV_TICK_CUSTOM 0

/* ── Display ────────────────────────────────────────────────────────────────── */

/* Default display refresh + input-read timer period. LVGL 9 defaults to 33 ms
 * (~30 Hz); 16 ms gives ~60 Hz sampling for snappier touch and smoother render.
 * Safe now that blocking Serial logging has been removed from the touch-read path. */
#define LV_DEF_REFR_PERIOD          16

#define LV_DRAW_BUF_ALIGN           4
#define LV_DRAW_BUF_STRIDE_ALIGN    1

/* ── Features ───────────────────────────────────────────────────────────────── */

#define LV_USE_STDLIB_MALLOC    LV_STDLIB_BUILTIN
#define LV_USE_STDLIB_STRING    LV_STDLIB_BUILTIN
#define LV_USE_STDLIB_SPRINTF   LV_STDLIB_BUILTIN

/* Disable the built-in Material-style default theme.
 *
 * lv_display_create() auto-initialises lv_theme_default when LV_USE_THEME_DEFAULT=1.
 * That theme adds card-style shadows, light-grey backgrounds, and borders to every
 * lv_obj container.  Our containers override bg_opa and border_width but NOT
 * shadow_width, so the theme's shadow renders as a floating dark halo around every
 * transparent slot rectangle — visible on the dark (#1A1A1A) root screen.
 *
 * All widgets are styled explicitly in our screen code, so the default theme adds
 * no value and actively interferes.  Disabling it cuts binary size and removes the
 * visual artefacts.
 */
#define LV_USE_THEME_DEFAULT    0
#define LV_USE_SNAPSHOT         1

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
#define LV_FONT_MONTSERRAT_48 1

/* ── Logging ────────────────────────────────────────────────────────────────── */

#define LV_USE_LOG      1
#define LV_LOG_LEVEL    LV_LOG_LEVEL_WARN
#define LV_LOG_PRINTF   1

/* ── Debug / perf helpers ───────────────────────────────────────────────────── */

#define LV_USE_PERF_MONITOR  0   /* enabled at runtime when OPENSONOS_DEBUG_OVERLAY=1 */
#define LV_USE_MEM_MONITOR   0

#endif /* LV_CONF_H */
#endif /* guard */
