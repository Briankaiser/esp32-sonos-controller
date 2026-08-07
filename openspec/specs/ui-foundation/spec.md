# UI Foundation Specification

## Purpose

Provides the shared interface substrate every screen builds on: named layout slots resolved from a per-shape archetype, enforcement that content stays inside the physically visible area, semantic type roles, and a single accent colour token.

## Requirements

### Requirement: Screens Position Content Only Through Named Slots

Screens SHALL position content by requesting named layout slots, and SHALL NOT specify pixel coordinates or dimensions directly. Each slot SHALL resolve to a container whose geometry is derived from the active display profile. Slot geometry SHALL be resolved once when a screen is built, and SHALL NOT be recomputed per frame.

#### Scenario: Composing the primary screen

- **WHEN** the primary screen is built
- **THEN** each element is placed inside a container obtained from a named slot
- **AND** no pixel coordinate or dimension is specified by the screen

#### Scenario: Slot geometry is stable across frames

- **WHEN** the primary screen renders repeatedly without a state change
- **THEN** slot geometry is not recomputed between frames

#### Scenario: Pixel literals are rejected

- **WHEN** continuous integration checks the screen composition sources
- **THEN** any numeric literal passed to a positioning or sizing call is reported as a failure

### Requirement: Content Cannot Occupy Physically Invisible Regions

No content SHALL be rendered outside the safe area declared by the active display profile. This SHALL be enforced at build time: a layout archetype placing any slot outside the safe area SHALL fail the build. It SHALL additionally be enforced at run time by clipping, so that content which overflows a slot is not drawn outside the safe area. On a round display, the safe area is the inscribed disc; no content is permitted in the framebuffer corners that the panel does not render.

#### Scenario: Archetype violating the safe area fails to build

- **WHEN** a layout archetype for a round profile declares a slot extending outside the inscribed disc
- **THEN** compilation fails
- **AND** the diagnostic identifies the offending slot

#### Scenario: Overflowing content is clipped at run time

- **WHEN** content inside a slot is larger than the slot
- **THEN** the overflow is clipped
- **AND** nothing is drawn outside the safe area

#### Scenario: Long text on a round display

- **WHEN** a track title longer than its slot is displayed on a round profile
- **THEN** the title is truncated or scrolled within the slot
- **AND** no glyph is drawn outside the inscribed disc

### Requirement: One Layout Archetype Per Panel Shape Class

Each display profile SHALL name a layout archetype. Each archetype SHALL define geometry for the same complete set of named slots, so that any screen composes against any archetype.

#### Scenario: Archetype defines every slot

- **WHEN** a layout archetype is compiled
- **THEN** it declares geometry for every named slot
- **AND** omitting a slot fails the build

#### Scenario: Same screen composes against a different archetype

- **WHEN** the same screen is built for a profile naming a different archetype
- **THEN** it composes successfully
- **AND** the screen's own source is unchanged

### Requirement: Type Is Selected By Semantic Role

Screens SHALL select type by semantic role rather than by point or pixel size. Each supported pixel-density class SHALL supply a face for every role.

#### Scenario: Screen requests a role

- **WHEN** a screen renders a track title
- **THEN** it requests the title role
- **AND** no numeric type size appears in the screen's source

#### Scenario: Density class missing a role

- **WHEN** a pixel-density class omits a face for a declared role
- **THEN** the build fails

### Requirement: A Single Accent Colour Token Drives Interface Emphasis

The interface SHALL expose one accent colour token. Emphasised elements SHALL derive their colour from that token. Updating the token SHALL update every element deriving from it, and the transition SHALL be animated over approximately 400 milliseconds rather than instantaneous. The token SHALL be updated when album art dominant colour is available. If the derived colour is not sufficiently distinguishable from the interface background, the device SHALL apply a legibility adjustment before using it as the accent.

#### Scenario: Accent updated from album art

- **WHEN** a new album art dominant colour is available
- **THEN** the accent-derived elements begin transitioning toward the new colour
- **AND** the transition completes within approximately 400 ms

#### Scenario: Accent contrast is legible after update

- **WHEN** the derived dominant colour has low contrast against the dark background
- **THEN** the applied accent colour is adjusted to remain distinguishable
- **AND** the animated transition still occurs

### Requirement: Input Is Delivered To Screens As Intents

Physical input SHALL be translated into device-independent intents before reaching a screen. Screens SHALL respond to intents and SHALL NOT read input hardware directly.

#### Scenario: Rotation becomes a volume intent

- **WHEN** the user rotates the encoder on a board that has one
- **THEN** a volume-change intent carrying the delta is delivered to the active screen

#### Scenario: Screens do not read hardware

- **WHEN** a screen handles user input
- **THEN** it reads only intents
- **AND** it does not reference an input device directly

#### Scenario: Input during a screen transition

- **WHEN** an intent arrives while a screen transition is in progress
- **THEN** the intent is either delivered to the incoming screen or discarded
- **AND** it is never delivered to a screen that has been torn down
