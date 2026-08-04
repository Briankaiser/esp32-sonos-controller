#pragma once
#include "layout.h"

/* ── Landscape wide archetype (800×480) ─────────────────────────────────────
 *
 * For wide rectangular panels.  Positions album art on the left half and
 * places controls on the right.
 */

static constexpr uint16_t kW = kDisplay.w;   // 800
static constexpr uint16_t kH = kDisplay.h;   // 480

inline constexpr std::array<LayoutSlot, SLOT_COUNT> kArchetypeSlots = {{
    // SLOT_ALBUM_ART: left half, square, vertically centred
    {
        /*.x=*/ pct(kW, 5),
        /*.y=*/ pct(kH, 5),
        /*.w=*/ pct(kH, 90),   // square: height-based so it's truly square
        /*.h=*/ pct(kH, 90),
    },
    // SLOT_TRACK_INFO: right column, top third
    {
        /*.x=*/ pct(kW, 54),
        /*.y=*/ pct(kH, 5),
        /*.w=*/ pct(kW, 42),
        /*.h=*/ pct(kH, 30),
    },
    // SLOT_TRANSPORT: right column, middle
    {
        /*.x=*/ pct(kW, 54),
        /*.y=*/ pct(kH, 38),
        /*.w=*/ pct(kW, 42),
        /*.h=*/ pct(kH, 24),
    },
    // SLOT_PROGRESS: right column, below transport
    {
        /*.x=*/ pct(kW, 54),
        /*.y=*/ pct(kH, 64),
        /*.w=*/ pct(kW, 42),
        /*.h=*/ pct(kH, 6),
    },
    // SLOT_VOLUME_ARC: right column, lower — shown as a slider on no-encoder boards
    {
        /*.x=*/ pct(kW, 54),
        /*.y=*/ pct(kH, 72),
        /*.w=*/ pct(kW, 42),
        /*.h=*/ pct(kH, 18),
    },
    // SLOT_LIST: full display minus a small margin
    {
        /*.x=*/ pct(kW, 2),
        /*.y=*/ pct(kH, 2),
        /*.w=*/ pct(kW, 96),
        /*.h=*/ pct(kH, 96),
    },
    // SLOT_STATUS: bottom-right corner, status line
    {
        /*.x=*/ pct(kW, 54),
        /*.y=*/ pct(kH, 91),
        /*.w=*/ pct(kW, 42),
        /*.h=*/ pct(kH, 8),
    },
}};

constexpr const LayoutSlot& slot(SlotId id) {
    return kArchetypeSlots[id];
}

static_assert(kArchetypeSlots.size() == SLOT_COUNT,
    "archetype_landscape_wide: slot count mismatch");
