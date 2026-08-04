# Hardware Abstraction Specification

## Purpose

Isolates every board-specific display, touch, and rotary-input detail behind a single per-board display profile, so that supporting a panel of a different resolution, physical size, or shape means adding a board support package and a build environment rather than editing any screen.

## Requirements

### Requirement: Single Board Selected Per Firmware Image

Each firmware image SHALL be built for exactly one board. The build SHALL select exactly one board support package, and SHALL fail with a diagnostic naming the available board identifiers when zero or more than one is selected.

#### Scenario: Building the round board environment

- **WHEN** the `crowpanel_21_round` environment is built
- **THEN** the build succeeds
- **AND** only that board's support package is compiled into the image
- **AND** the image reports a 480x480 display of round shape

#### Scenario: Building the rectangular reference environment

- **WHEN** the `ref_rect_stub` environment is built
- **THEN** the build succeeds
- **AND** the image reports an 800x480 display of rectangular shape
- **AND** the image reports that no rotary encoder is present

#### Scenario: No board selected

- **WHEN** a build is invoked without selecting a board support package
- **THEN** compilation fails
- **AND** the diagnostic names the available board identifiers

### Requirement: Display Profile Is The Sole Hardware Contract For UI Code

Each board support package SHALL expose exactly one display profile declaring pixel dimensions, physical size, pixel density, panel shape, and which input devices are present. Code that composes screens SHALL depend only on that profile, and SHALL NOT branch on a board identifier.

#### Scenario: Screen composition reads geometry from the profile

- **WHEN** a screen is composed
- **THEN** every element's position and size derives from the active display profile
- **AND** no board identifier appears in the composition path

#### Scenario: Adding a board does not touch screen code

- **WHEN** a new board support package declares an existing layout archetype at a resolution not previously supported
- **THEN** every screen compiles and composes for the new profile
- **AND** no file that composes screens is modified

### Requirement: Panel Shape Defines A Safe Area

The display profile SHALL declare a safe area that excludes any region the panel does not physically render. For a round panel, the safe area SHALL be the inscribed disc. For a rectangular panel, the safe area SHALL be the full extent less any declared margin.

#### Scenario: Round panel excludes framebuffer corners

- **WHEN** the active profile declares a round shape
- **THEN** the declared safe area excludes the four corners of the framebuffer

#### Scenario: Rectangular panel uses full extent

- **WHEN** the active profile declares a rectangular shape
- **THEN** the declared safe area equals the panel extent less the profile's declared margin

### Requirement: Absent Input Devices Degrade Gracefully

The display profile SHALL declare whether a rotary encoder and a touch digitizer are present. Every control action reachable on a board with an encoder SHALL remain reachable on a board without one.

#### Scenario: Volume adjustment on a board with an encoder

- **WHEN** the active profile declares an encoder is present
- **AND** the user rotates the encoder
- **THEN** a volume-change intent is raised carrying the rotation delta

#### Scenario: Volume adjustment on a board without an encoder

- **WHEN** the active profile declares no encoder is present
- **THEN** an on-screen volume control is presented instead
- **AND** adjusting it raises the same volume-change intent that an encoder would raise

#### Scenario: No dead code for absent devices

- **WHEN** a firmware image is built for a board declaring no encoder
- **THEN** the image contains no encoder-handling code path
