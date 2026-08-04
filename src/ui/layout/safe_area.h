#pragma once
#include "layout.h"

/* ── Round-display safe-area enforcement ────────────────────────────────────
 *
 * For round panels the safe area is the largest inscribed disc.  A slot that
 * places any corner outside this disc will fail the build with a diagnostic
 * naming the offending slot.
 *
 * Layer 1 (this file): compile-time — proves rectangles.
 * Layer 2 (board.cpp):  runtime clipping — catches arcs, shadows, overhang.
 * Layer 3 (ci.yml):     lint — rejects raw pixel literals in screen code.
 */

namespace safe_area {

// Disc geometry: centred, radius = half the short axis
static constexpr int16_t CX = static_cast<int16_t>(kDisplay.w / 2);
static constexpr int16_t CY = static_cast<int16_t>(kDisplay.h / 2);
static constexpr uint32_t kShortAxis =
    (kDisplay.w < kDisplay.h) ? kDisplay.w : kDisplay.h;
static constexpr uint32_t kRadius = kShortAxis / 2;
static constexpr uint32_t kR2 = kRadius * kRadius;

// Single-return-statement constexpr for C++11 compatibility
constexpr bool point_in_disc(int16_t px, int16_t py) {
    return static_cast<uint32_t>(
               (int32_t)(px - CX) * (int32_t)(px - CX) +
               (int32_t)(py - CY) * (int32_t)(py - CY)) <= kR2;
}

constexpr bool slot_in_disc(const LayoutSlot& s) {
    return point_in_disc(s.x,               s.y)
        && point_in_disc(s.x + (int16_t)(s.w - 1), s.y)
        && point_in_disc(s.x,               s.y + (int16_t)(s.h - 1))
        && point_in_disc(s.x + (int16_t)(s.w - 1), s.y + (int16_t)(s.h - 1));
}

} // namespace safe_area

// ── Per-slot static_assert (round profiles only) ─────────────────────────────
//
// Uses OPENSONOS_SHAPE_ROUND preprocessor macro (defined by board_config.h and
// forwarded via build_flags) rather than a C++ expression in #if, which the
// preprocessor cannot evaluate.

#ifdef OPENSONOS_SHAPE_ROUND

static_assert(safe_area::slot_in_disc(slot(SLOT_ALBUM_ART)),
    "SLOT_ALBUM_ART escapes the inscribed disc — adjust archetype_round_compact.h");
static_assert(safe_area::slot_in_disc(slot(SLOT_TRACK_INFO)),
    "SLOT_TRACK_INFO escapes the inscribed disc — adjust archetype_round_compact.h");
static_assert(safe_area::slot_in_disc(slot(SLOT_TRANSPORT)),
    "SLOT_TRANSPORT escapes the inscribed disc — adjust archetype_round_compact.h");
static_assert(safe_area::slot_in_disc(slot(SLOT_PROGRESS)),
    "SLOT_PROGRESS escapes the inscribed disc — adjust archetype_round_compact.h");
static_assert(safe_area::slot_in_disc(slot(SLOT_VOLUME_ARC)),
    "SLOT_VOLUME_ARC escapes the inscribed disc — adjust archetype_round_compact.h");
static_assert(safe_area::slot_in_disc(slot(SLOT_LIST)),
    "SLOT_LIST escapes the inscribed disc — adjust archetype_round_compact.h");
static_assert(safe_area::slot_in_disc(slot(SLOT_STATUS)),
    "SLOT_STATUS escapes the inscribed disc — adjust archetype_round_compact.h");

#endif // OPENSONOS_SHAPE_ROUND
