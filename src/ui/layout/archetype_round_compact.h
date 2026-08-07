#pragma once
#include "layout.h"

/* ── Round compact archetype (480×480) ──────────────────────────────────────
 *
 * All positions are derived proportionally from the display dimensions.
 * Slot geometry is constexpr — resolved once at screen build, never per frame.
 *
 * safe_area.h places a static_assert that every slot's bounding rectangle
 * lies within the inscribed disc (center 240,240, radius 240).  A slot that
 * escapes the disc fails the build naming the offending slot.
 *
 * EXCEPTIONS: SLOT_ALBUM_ART and SLOT_VOLUME_ARC intentionally span the full
 * 480×480 framebuffer.  SLOT_ALBUM_ART is a full-bleed background layer; the
 * round display clips to the disc in hardware.  SLOT_VOLUME_ARC holds a
 * circular arc widget whose content is inherently confined to the disc.
 * Both are excluded from the safe-area static_assert in safe_area.h.
 *
 * Coordinate verification for constrained slots (disc: cx=240, cy=240, R²=57600):
 *   All corners satisfy (x-240)²+(y-240)² ≤ 57000 (safety margin ~600).
 */

static constexpr uint16_t kW = kDisplay.w;   // 480
static constexpr uint16_t kH = kDisplay.h;   // 480

inline constexpr std::array<LayoutSlot, SLOT_COUNT> kArchetypeSlots = {{
    // SLOT_ALBUM_ART — full framebuffer; round display clips to disc.
    // EXCLUDED from safe-area check — intentional full-bleed background.
    {
        /*.x=*/ 0,
        /*.y=*/ 0,
        /*.w=*/ kW,   // 480
        /*.h=*/ kH,   // 480
    },
    // SLOT_TRACK_INFO — frosted overlay band, centre-lower, y≈48%, 22% tall.
    // Larger slot for bigger fonts and frosted backdrop.
    // Worst corner (432,336): dx=192,dy=96, d²=36864+9216=46080 ✓
    {
        /*.x=*/ pct(kW, 10),   // 48
        /*.y=*/ pct(kH, 48),   // 230
        /*.w=*/ pct(kW, 80),   // 384
        /*.h=*/ pct(kH, 22),   // 106
    },
    // SLOT_TRANSPORT — control row, y≈72%, 16% tall (unchanged).
    // Worst corner (392, 421): dx=152,dy=181, d²=55865 ✓
    {
        /*.x=*/ pct(kW, 18),   // 86
        /*.y=*/ pct(kH, 72),   // 345
        /*.w=*/ pct(kW, 64),   // 307
        /*.h=*/ pct(kH, 16),   // 76
    },
    // SLOT_PROGRESS — thin bar at bottom (unchanged).
    // Worst corner (360, 445): dx=120,dy=205, d²=56425 ✓
    {
        /*.x=*/ pct(kW, 25),   // 120
        /*.y=*/ pct(kH, 90),   // 432
        /*.w=*/ pct(kW, 50),   // 240
        /*.h=*/ pct(kH, 3),    // 14
    },
    // SLOT_VOLUME_ARC — full framebuffer; arc widget is circular, no corner content.
    // EXCLUDED from safe-area check — arc content inherently within disc.
    {
        /*.x=*/ 0,
        /*.y=*/ 0,
        /*.w=*/ kW,   // 480
        /*.h=*/ kH,   // 480
    },
    // SLOT_LIST — inner square (used on other screens, unchanged).
    // Corner (407,407): dx=167,dy=167, d²=55778 ✓
    {
        /*.x=*/ pct(kW, 15),   // 72
        /*.y=*/ pct(kH, 15),   // 72
        /*.w=*/ pct(kW, 70),   // 336
        /*.h=*/ pct(kH, 70),   // 336
    },
    // SLOT_STATUS — narrow strip at top centre
    // Worst corner (139, 24): dx=101,dy=216, d²=56857 ✓
    {
        /*.x=*/ pct(kW, 29),   // 139
        /*.y=*/ pct(kH, 5),    // 24
        /*.w=*/ pct(kW, 42),   // 201
        /*.h=*/ pct(kH, 8),    // 38
    },
}};

constexpr const LayoutSlot& slot(SlotId id) {
    return kArchetypeSlots[id];
}

static_assert(kArchetypeSlots.size() == SLOT_COUNT,
    "archetype_round_compact: slot count mismatch");
