#pragma once
#include <stdint.h>

namespace bsp {

/**
 * Device-independent input intents delivered to screens.
 *
 * Physical input sources (encoder rotation, touch, button) are translated
 * into Intents before reaching screen code. Screens must never read input
 * hardware directly.
 */
enum class Intent : uint8_t {
    None,
    VolumeDelta,        ///< Encoder rotated; value = signed delta (positive = up)
    EncoderButtonDown,  ///< Encoder button pressed (falling edge)
    EncoderButtonUp,    ///< Encoder button released (rising edge)
    TouchBegin,         ///< Finger down (internal routing use)
    TouchEnd,           ///< Finger up   (internal routing use)
};

struct InputEvent {
    Intent  intent;
    int16_t value;   ///< Signed delta for VolumeDelta; 0 otherwise
};

} // namespace bsp
