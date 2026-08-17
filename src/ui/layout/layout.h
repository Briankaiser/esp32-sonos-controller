#pragma once

#include <array>
#include <cstdint>
#include <lvgl.h>
#include "board_config.h"   // resolves to the active board via include path

// ── Named slots ─────────────────────────────────────────────────────────────
//
// Screens position content exclusively through these named slot identifiers.
// No pixel value ever appears in screen code.

enum SlotId : uint8_t {
    SLOT_ALBUM_ART,     ///< Centre region — cover art or large status icon
    SLOT_TRACK_INFO,    ///< Text above the art — track title, artist
    SLOT_TRANSPORT,     ///< Playback buttons (play/pause, skip, shuffle, repeat)
    SLOT_PROGRESS,      ///< Track progress bar
    SLOT_VOLUME_ARC,    ///< Volume arc indicator (or on-screen slider if no encoder)
    SLOT_LIST,          ///< Full-screen scrollable list (Shortcuts, Room Groups)
    SLOT_STATUS,        ///< Small status line — room name, connectivity
    SLOT_TRACK_INFO_COMPACT, ///< Shrunken title/artist band, shown only when lyrics are present
    SLOT_LYRICS,        ///< Time-aligned lyric region, shown only when lyrics are present
    SLOT_COUNT,
};

struct LayoutSlot {
    int16_t  x, y;      ///< Top-left corner in pixels
    uint16_t w, h;      ///< Width and height in pixels
};

// ── Proportional helper ──────────────────────────────────────────────────────

constexpr uint16_t pct(uint16_t dimension, uint8_t percent) {
    return static_cast<uint16_t>(dimension * percent / 100u);
}

// ── Archetype selection ──────────────────────────────────────────────────────
//
// kDisplay (from board_config.h) drives the selection at compile time.
// The selected archetype header exposes:
//   constexpr const LayoutSlot& slot(SlotId id);

// Archetype selection: board_config.h defines exactly one of OPENSONOS_SHAPE_ROUND
// or OPENSONOS_SHAPE_RECT. The preprocessor cannot evaluate C++ constexpr
// expressions in #if, so we use macros here.

#if defined(OPENSONOS_SHAPE_ROUND)
#  include "archetype_round_compact.h"
#  include "safe_area.h"   // static_assert all round slots within the disc
#elif defined(OPENSONOS_SHAPE_RECT)
#  include "archetype_landscape_wide.h"
   // Rectangular profiles: all slots are within the full extent by construction.
#else
#  error "No shape macro defined. Board board_config.h must define OPENSONOS_SHAPE_ROUND or OPENSONOS_SHAPE_RECT"
#endif

// ── Layout container factory ─────────────────────────────────────────────────
//
// Screens call this once per slot at build time. The returned lv_obj_t*
// is positioned, sized, and clipped. Content is added as children.

lv_obj_t* layout_container(lv_obj_t* parent, SlotId id);
