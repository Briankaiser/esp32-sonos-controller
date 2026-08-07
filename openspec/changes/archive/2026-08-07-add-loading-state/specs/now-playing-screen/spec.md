## Purpose

Delta spec for `now-playing-screen`. Adds a loading overlay that covers the screen until the first real playback state is received.

## ADDED Requirements

### Requirement: Now-Playing Screen Shows a Loading Overlay Until First State Is Received

When the now-playing screen is first displayed and no playback state has been received from the home room speaker, the device SHALL overlay the screen with a dimmed layer and a visible connecting indicator. The overlay SHALL cover all screen content. When the first non-unknown transport state is received, the overlay SHALL be removed with a brief animated transition. The overlay and its text SHALL be positioned within the display's safe area on round displays.

#### Scenario: Screen first displayed before any state

- **WHEN** the now-playing screen is shown and `transport_state` is Unknown
- **THEN** a semi-transparent overlay covers the screen contents
- **AND** a connecting indicator text is visible and centred within the safe area

#### Scenario: First state received — overlay dismisses

- **WHEN** a `PlaybackState` with a non-Unknown `transport_state` is passed to the screen for the first time
- **THEN** the overlay begins fading out
- **AND** the overlay is fully removed within 300 ms

#### Scenario: Overlay does not reappear after first dismissal

- **WHEN** the overlay has been dismissed and a subsequent poll returns any transport state
- **THEN** the overlay remains absent
