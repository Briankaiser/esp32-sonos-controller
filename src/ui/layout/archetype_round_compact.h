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
 * Coordinate verification (disc: cx=240, cy=240, R²=57600):
 *   All corners satisfy (x-240)²+(y-240)² ≤ 57000 (safety margin ~600).
 */

static constexpr uint16_t kW = kDisplay.w;   // 480
static constexpr uint16_t kH = kDisplay.h;   // 480

inline constexpr std::array<LayoutSlot, SLOT_COUNT> kArchetypeSlots = {{
    // SLOT_ALBUM_ART — central square, 56% × 56%, centered
    // Corner (373,373): dx=133,dy=133, d²=35378 ✓
    {
        /*.x=*/ pct(kW, 22),   // 105
        /*.y=*/ pct(kH, 22),   // 105
        /*.w=*/ pct(kW, 56),   // 268
        /*.h=*/ pct(kH, 56),   // 268
    },
    // SLOT_TRACK_INFO — text band above art, y≈12%, 11% tall
    // Worst corner (91+297-1=387, 57): dx=147,dy=183, d²=55098 ✓
    {
        /*.x=*/ pct(kW, 19),   // 91
        /*.y=*/ pct(kH, 12),   // 57
        /*.w=*/ pct(kW, 62),   // 297
        /*.h=*/ pct(kH, 11),   // 52
    },
    // SLOT_TRANSPORT — control row below art, y≈79%, 9% tall
    // Worst corner (86+307-1=392, 421): dx=152,dy=181, d²=55865 ✓
    {
        /*.x=*/ pct(kW, 18),   // 86
        /*.y=*/ pct(kH, 79),   // 379
        /*.w=*/ pct(kW, 64),   // 307
        /*.h=*/ pct(kH, 9),    // 43
    },
    // SLOT_PROGRESS — thin bar below transport, y≈90%, 3% tall
    // Worst corner (360, 445): dx=120,dy=205, d²=56425 ✓
    {
        /*.x=*/ pct(kW, 25),   // 120
        /*.y=*/ pct(kH, 90),   // 432
        /*.w=*/ pct(kW, 50),   // 240
        /*.h=*/ pct(kH, 3),    // 14
    },
    // SLOT_VOLUME_ARC — large inner square for arc/slider widget
    // Corner (407,407): dx=167,dy=167, d²=55778 ✓
    {
        /*.x=*/ pct(kW, 15),   // 72
        /*.y=*/ pct(kH, 15),   // 72
        /*.w=*/ pct(kW, 70),   // 336
        /*.h=*/ pct(kH, 70),   // 336
    },
    // SLOT_LIST — same inner region as VOLUME_ARC (used on different screens)
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
