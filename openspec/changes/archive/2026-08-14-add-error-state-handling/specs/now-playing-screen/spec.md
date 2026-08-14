# Delta Spec: now-playing-screen

## MODIFIED Requirements

### Requirement: Now-Playing Screen Shows a Loading Overlay Until First State Is Received

When the now-playing screen is first displayed and no playback state has been received from the home room speaker, the device SHALL overlay the screen with a dimmed layer and a visible connecting indicator. The overlay SHALL cover all screen content. When the first non-unknown transport state is received, the overlay SHALL be removed with a brief animated transition. The overlay SHALL NOT persist indefinitely: when a connectivity fault is diagnosed (no WiFi, no Sonos system found, or home room offline), the fault status layer SHALL take over the screen in place of the connecting overlay, and normal now-playing content SHALL resume once the fault clears and state is received. The overlay and its text SHALL be positioned within the display's safe area on round displays.

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

#### Scenario: Connecting overlay yields to a diagnosed fault

- **WHEN** no state has been received and a connectivity fault is diagnosed
- **THEN** the fault status screen is shown instead of an indefinite connecting overlay
- **AND** when the fault clears and state is received, the now-playing screen resumes normally
