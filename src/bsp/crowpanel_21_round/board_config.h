#pragma once
#include "../display_profile.h"

/* ── Elecrow CrowPanel 2.1" ESP32-S3 Rotary Display ─────────────────────────
 *
 * Panel:    480×480 round IPS, ST7701S, RGB parallel interface
 * Touch:    CST816 capacitive, I²C @ 0x15
 * Encoder:  quadrature A/B on GPIO, button via PCF8574
 * PSRAM:    8 MB (OPI), Flash: 16 MB
 *
 * Pin assignments verified against:
 *   mkvesala/ESP32-Crowpanel-compass (CrowPanelApplication.cpp)
 *   Elecrow Arduino lesson 1 (RotaryEncoder.h)
 */

/* ── Display profile ─────────────────────────────────────────────────────────*/

inline constexpr bsp::DisplayProfile kDisplay{
    .w            = 480,
    .h            = 480,
    .diag_mm_x10  = 533,    ///< 2.1" × 25.4 mm/in × 10 ≈ 533
    .ppi          = 340,    ///< 480 px / (2.1" × 0.707) ≈ 323; rounded for density bucket
    .shape        = bsp::Shape::Round,
    .has_encoder  = true,
    .has_touch    = true,
};

/* ── RGB parallel interface (ST7701S) ────────────────────────────────────────*/

namespace pins {
    // Control signals
    constexpr int LCD_DE    = 40;
    constexpr int LCD_VSYNC = 7;
    constexpr int LCD_HSYNC = 15;
    constexpr int LCD_PCLK  = 41;

    // Red channel R[4:0]
    constexpr int LCD_R0 = 46;
    constexpr int LCD_R1 = 3;
    constexpr int LCD_R2 = 8;
    constexpr int LCD_R3 = 18;
    constexpr int LCD_R4 = 17;

    // Green channel G[5:0]
    constexpr int LCD_G0 = 14;
    constexpr int LCD_G1 = 13;
    constexpr int LCD_G2 = 12;
    constexpr int LCD_G3 = 11;
    constexpr int LCD_G4 = 10;
    constexpr int LCD_G5 = 9;

    // Blue channel B[4:0]
    constexpr int LCD_B0 = 5;
    constexpr int LCD_B1 = 45;
    constexpr int LCD_B2 = 48;
    constexpr int LCD_B3 = 47;
    constexpr int LCD_B4 = 21;

    // ST7701S init command bus (3-wire software SPI)
    // -1 == GFX_NOT_DEFINED (from Arduino_GFX_Library); defined here without
    // the library header to avoid pulling in GFX headers into board_config.h.
    constexpr int LCD_SPI_CS  = -1;   ///< chip-select not used (3-wire mode)
    constexpr int LCD_SPI_CLK = 16;
    constexpr int LCD_SPI_SDA = 2;
    constexpr int LCD_SPI_MISO = 1;   ///< 4th arg to Arduino_SWSPI(dc,cs,sck,mosi,miso)

    // PCF8574 I/O expander (I²C address 0x21)
    // Named kSda/kScl to avoid clash with Arduino board variant macros I2C_SDA/I2C_SCL
    constexpr int kSda       = 38;
    constexpr int kScl       = 39;
    constexpr int PCF_ADDR   = 0x21;
    constexpr int PCF_TP_RST  = 0;  ///< P0 — touch reset
    constexpr int PCF_TP_INT  = 2;  ///< P2 — touch interrupt
    constexpr int PCF_LCD_PWR = 3;  ///< P3 — LCD power enable
    constexpr int PCF_LCD_RST = 4;  ///< P4 — LCD reset
    constexpr int PCF_ENC_BTN = 5;  ///< P5 — encoder press button

    // Backlight PWM
    constexpr int BACKLIGHT = 6;

    // Rotary encoder (direct GPIO)
    constexpr int ENC_A = 42;
    constexpr int ENC_B = 4;

    // CST816 touch (I²C address 0x15, shares the main I²C bus)
    constexpr int TOUCH_ADDR = 0x15;
} // namespace pins

// Shape macro — used by layout.h and safe_area.h for #if selection.
// Also defined via build_flags; guard prevents redefinition warning.
#ifndef OPENSONOS_SHAPE_ROUND
#define OPENSONOS_SHAPE_ROUND
#endif
