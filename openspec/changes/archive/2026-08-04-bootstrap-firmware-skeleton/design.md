## Context

Greenfield repository. The primary board is an Elecrow CrowPanel 2.1" ESP32-S3: 480x480 round IPS, ST7701S over an RGB parallel interface, CST816 touch on I2C, rotary encoder on GPIO, 8 MB PSRAM, 16 MB Flash.

Three constraints shape everything below:

1. **The panel is physically circular.** The framebuffer is square; its corners are not visible. This must be structurally impossible to get wrong, not merely documented.
2. **Frame rate is bounded by memory bandwidth, not by code structure.** 480x480 at 16 bits per pixel, refreshed continuously over an RGB parallel bus from a framebuffer in octal PSRAM, is close to the practical ceiling of this part. Cover-art decode and colour analysis contend with LCD DMA for that bandwidth.
3. **A second display is expected, of unknown resolution and shape.** Separate firmware per board is acceptable. Editing screen code per board is not.

## Goals / Non-Goals

**Goals:**

- A board is added by creating one directory and one build environment.
- Screen code contains no pixel literals and no board identifiers.
- A layout that would place content in a physically invisible region fails the build.
- Zero abstraction overhead in the per-pixel path.
- Sustained frame rate of at least 55 fps, measurable from day one.
- Total abstraction surface small enough to read in one sitting — on the order of 300 lines.

**Non-Goals:**

- Runtime board detection or a board registry. One image, one board, decided at compile time.
- A general responsive-layout engine with runtime reflow.
- A general-purpose hardware abstraction layer over GPIO, I2C, or SPI. The Arduino core already is that layer.
- Layouts loaded from data at run time.
- Hardware verification of the rectangular reference board. It must compile; it need not be tested on silicon.
- Any Sonos, network, or persistence behavior. Later changes own that.

## Decisions

### The abstraction is compile-time; there are no virtual interfaces

Boards are represented by a `constexpr` profile plus a set of free functions resolved at link time, not by an abstract base class.

```cpp
enum class Shape : uint8_t { Rect, Round };

struct DisplayProfile {
  uint16_t w, h;
  uint16_t diag_mm_x10;   // physical size, for touch-target sizing
  uint16_t ppi;           // selects the type density class
  Shape    shape;
  bool     has_encoder;
  bool     has_touch;
};
```

```cpp
namespace bsp {
  void init();
  lv_display_t* display_init();
  void input_init();
  void backlight(uint8_t pct);
  bool pop_event(InputEvent&);
}
```

*Rationale.* The flush callback runs on every rendered region. A virtual call there is an indirect branch the compiler cannot see through, and it blocks inlining against the concrete panel handle. Making each board a single translation unit means the flush path is a `static` function calling a concrete handle in the same file — fully inlinable, and the abstraction provably costs nothing because it is not present in the compiled pixel path at all.

*Alternative rejected.* `IPanel` / `ITouch` / `IEncoder` abstract classes. Conventional, and it would permit runtime board selection — which is a capability nobody asked for, paid for with indirect dispatch in the hottest path in the firmware.

### Board selection is an include path, not a preprocessor branch

Each board owns a directory. The build environment adds that directory to the include path and filters sources so only that board compiles:

```ini
[env:crowpanel_21_round]
build_src_filter = +<*> -<bsp/*/> +<bsp/crowpanel_21_round/>
build_flags = -I src/bsp/crowpanel_21_round -D FONTSET_MEDIUM
```

Every consumer writes `#include "board_config.h"` and receives the right one. *Rationale.* `#ifdef BOARD_X` scattered through the interface code is precisely the outcome this design exists to prevent; it grows linearly with boards and rots silently. An include path makes the wrong thing unwriteable rather than merely discouraged.

A guard in `bsp.h` (`#if !defined(OPENSONOS_BOARD_SELECTED) #error ...`) produces the required diagnostic when no board is selected.

### Layout archetypes hold `constexpr` slot tables

```cpp
enum SlotId : uint8_t { SLOT_ALBUM_ART, SLOT_TRACK_INFO, SLOT_TRANSPORT,
                        SLOT_PROGRESS, SLOT_VOLUME_ARC, SLOT_LIST,
                        SLOT_STATUS, SLOT_COUNT };

struct LayoutSlot { int16_t x, y; uint16_t w, h; };

inline constexpr std::array<LayoutSlot, SLOT_COUNT> kSlots = { /* ... */ };
constexpr const LayoutSlot& slot(SlotId id);
```

Screens call `layout_container(parent, SLOT_X)` once at build time and populate the returned container using flex and percentage units.

Two decisions inside this one:

- **Slots are containers, never widgets.** A slot says "cover art goes in this region"; it does not say what a cover-art widget is. This keeps the archetype table declarative and lets content grow without the archetype knowing.
- **Geometry is `constexpr`, applied once.** No per-frame layout arithmetic, and the slot table costs no RAM.

`SLOT_COUNT` is deliberately small (7). A large slot vocabulary would become a layout engine by accretion.

### Safe area is enforced at build time, and again at run time

Three layers, in decreasing order of how much they prove and increasing order of how much they actually save:

1. **`static_assert` over the whole slot table.** A `constexpr` predicate tests all four corners of every slot against the inscribed disc; the fold feeds one `static_assert`. A round archetype with an escaping slot does not compile, and the diagnostic names the slot.
2. **Runtime clipping.** The root object is a circle with corner clipping enabled; slot containers clip their children.
3. **CI lint.** A grep rejecting numeric literals in positioning and sizing calls under `src/ui/screens/`.

*Rationale, stated plainly.* Layer 1 proves a property about rectangles and gives a fast feedback loop — but rotated arcs, shadows, glyph overhang, and scroll momentum all reach outside a widget's nominal rectangle. **Layer 2 is what actually keeps pixels off the dead corners.** Layer 1 is not sufficient and should not be trusted as though it were.

### Input is normalised to intents before it reaches a screen

The encoder is an input *source*; volume is an *intent*. On the round board rotation produces `{Intent::VolumeDelta, +2}`. On a board with no encoder an on-screen slider produces the identical intent. Screens only ever see intents.

The one place board divergence is permitted:

```cpp
if constexpr (!kDisplay.has_encoder)
    build_volume_slider(layout_container(root, SLOT_VOLUME_ARC));
```

`if constexpr` discards the untaken branch entirely, satisfying the requirement that an image for an encoder-less board contain no encoder code path.

### Type is addressed by role, with one face set per density class

Roles are `Display`, `Title`, `Body`, `Caption`, `Icon`. Each density class is one translation unit exporting the same symbol names; the build compiles exactly one. A `static_assert` over the role table catches the predictable failure where someone adds a role to one density class and not the other.

Density class is implied by the board, never independently selectable — otherwise the environment count multiplies for no benefit. Glyph coverage is subset to Latin-1 plus the icons actually used; five roles across two density classes will otherwise consume a substantial fraction of flash.

### Rendering strategy

Direct render mode against a full framebuffer in PSRAM, RGB565, with partial invalidation. Cover-art decode runs on core 0 and hands finished pixels to the interface task. Dominant-colour analysis runs on a 64x64 downsample, never the full image.

*Rationale.* The bandwidth budget is the binding constraint. Full-frame redraws for a changed track title would waste most of it, so partial invalidation matters more here than it does on an SPI panel.

## Risks / Trade-offs

**The portability claim is only partly tested.** Both boards in this change drive ST7701S over RGB parallel. The panel-driver seam is therefore exercised at only one point. The first SPI, QSPI, or MIPI board will likely force a revision of `display_init()` — buffer strategy, render mode, and tearing semantics all differ. This is accepted deliberately: designing that seam now would be speculation against an unknown part. The profile and slot layers are expected to survive; the panel-init contract is expected to move.

**Slot rectangles fit the primary screen and fight lists.** A scrolling list on a round display wants a per-row horizontal inset that varies with vertical position. A rectangle cannot express that. The list screens will need a bespoke round-list component. That belongs in the component layer, and the temptation to generalise the slot primitive to accommodate it should be refused.

**`static_assert` proves less than it appears to.** See the safe-area decision above. Runtime clipping is the real guarantee.

**Frame rate may not be reachable by construction.** If sustained 55 fps proves unattainable with cover art decoding concurrently, the available levers are, in order: reduce the invalidated region, decode progressively, drop the art crossfade, and lower the panel refresh rate. Frame rate is measured from the first milestone precisely so this is discovered now and not after nine more changes are stacked on top.

**Two density classes will drift.** The `static_assert` over the role table mitigates it but does not prevent someone adding a role and a face in one class only, then wondering why the other environment fails to link.

**Physical-size and density fields earn nothing yet.** `diag_mm_x10` and `ppi` are carried in the profile but genuinely unused with two boards. They cost nothing and will matter at board three; no sizing arithmetic should be built on them before then.
