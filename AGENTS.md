# Agent Instructions

ESP32-S3 Sonos room controller firmware. Read this before making changes.

## Spec-driven workflow

This project uses [OpenSpec](https://github.com/Fission-AI/OpenSpec). Behavior is
defined in specs before it is implemented.

- `openspec/specs/<capability>/spec.md` — **current truth.** What the firmware does
  today. Never edited by hand to record history. Generated when a change is archived.
- `openspec/changes/<name>/` — **an active proposal.** `proposal.md` (why),
  `specs/<capability>/spec.md` (behavior deltas), `design.md` (how), `tasks.md` (steps).
- `openspec/changes/archive/YYYY-MM-DD-<name>/` — **the engineering record.** Immutable.
  Why a decision was made and what was intended.
- `openspec/config.yaml` — project context and per-artifact rules, injected into
  artifact generation. Update this when a project-wide constraint changes.

Commands: `/opsx:propose`, `/opsx:apply`, `/opsx:archive`. Or the CLI directly —
`openspec list`, `openspec status --change <name>`, `openspec validate --all --strict`.

Do not implement behavior that has no spec. If you find yourself writing code whose
purpose is not covered by a requirement, stop and propose a change first.

## CHANGELOG.md

`CHANGELOG.md` is release-facing and separate from the OpenSpec archive. The three
artifacts answer different questions; do not duplicate content between them.

- One sentence per user-visible change, describing observable behavior.
- Link to the archived change for rationale. Never restate the rationale.
- Written at **release** time, when a firmware image is tagged. One release usually
  bundles several archived changes.
- Internal-only work (refactors, tooling, CI, spec cleanups) is archived but stays
  out of the changelog.
- Every `tasks.md` carries a task to add the `## [Unreleased]` entry, so the two
  never drift.

## Scope boundaries

These are settled decisions. Do not propose reversing them without discussion.

- **No Home Assistant dependency.** Control is direct UPnP/SOAP over the LAN. The
  device must work when Home Assistant is down or absent.
- **No Sonos cloud or OAuth API.**
- **No recently-played history.** It is not exposed over UPnP. Favorites only.
- **No TV or line-in source switching.**
- **No lyrics** in v1.

## Architecture rules

Violating any of these is a defect, not a style preference.

1. **The hardware abstraction is compile-time.** `constexpr DisplayProfile` plus a
   free-function BSP contract. No virtual `IPanel`/`ITouch`/`IEncoder`, no runtime
   board registry, no auto-detection. This exists to keep the LVGL flush path free
   of indirect dispatch.
2. **One board = one directory = one translation unit**, selected by
   `build_src_filter` and an include path. `#ifdef BOARD_X` must never appear in
   interface code.
3. **Screen code contains no pixel literals and no board identifiers.** Position
   content only through named layout slots. CI enforces this.
4. **Layout slots define containers, never widgets.** Slot geometry is `constexpr`
   and applied once at screen build — never recomputed per frame.
5. **All `lv_*` calls happen on the interface task (core 1).** Never call LVGL from
   another task.
6. **No blocking network calls on the interface task.** Sonos, cover art, and weather
   I/O run on core 0 and hand results back over a queue.
7. **Cover-art decode and dominant-colour analysis never run on the interface task.**
   Colour analysis operates on a 64x64 downsample, not the full image.
8. **The encoder ISR increments a counter and does nothing else.**

## The round display

The primary panel is physically circular. The framebuffer is square and its four
corners are **not visible**. This is enforced three ways, and all three must stay
working:

1. A compile-time `static_assert` over the slot table — a layout placing a slot
   outside the inscribed disc **fails the build**.
2. Runtime circular clipping. This is what actually keeps pixels off the dead
   corners; rotated arcs, shadows, glyph overhang, and scroll momentum all escape a
   widget's nominal rectangle.
3. A CI lint rejecting numeric literals in positioning calls.

Layer 1 proves a property about rectangles only. Do not treat it as sufficient.

## Build and test

```bash
pio run -e crowpanel_21_round      # primary board
pio run -e ref_rect_stub           # rectangular reference; must always compile
pio device monitor
openspec validate --all --strict
```

Debug loop:

- `curl http://<device-ip>/screenshot -o s.bmp` — framebuffer capture. This is the
  primary way to inspect interface state. Attach a capture to interface changes.
- Frame-rate and free-memory overlay, enabled by a build flag.
- Host-side mock SOAP responder replays canned Sonos XML, so playback logic is
  testable without real speakers.

Performance gate: **at least 55 fps sustained.** 60 is the aspiration. The binding
constraint is PSRAM and LCD DMA bandwidth, not code structure — do not attempt to
fix a frame-rate problem by restructuring the abstraction.

## Definition of done

1. Both PlatformIO envs build.
2. `openspec validate --all --strict` is clean.
3. Interface changes include a `/screenshot` capture.
4. A `CHANGELOG.md` entry exists under `## [Unreleased]` if user-visible.

## Hardware reference

| | CrowPanel 2.1" round (primary) | `ref_rect_stub` |
|---|---|---|
| MCU | ESP32-S3R8, 8 MB PSRAM, 16 MB Flash | ESP32-S3 |
| Panel | 480x480 round IPS, ST7701S, RGB parallel | 800x480, ST7701S, RGB parallel |
| Touch | CST816, I2C 0x15 (SDA=38, SCL=39) | GT911 |
| Encoder | A=GPIO42, B=GPIO4, press **unconfirmed** | none |
| Backlight | GPIO6 | — |

The encoder press-button GPIO is not documented in available sources. Confirm it
against the Elecrow wiki or by probing before finishing the encoder module.
