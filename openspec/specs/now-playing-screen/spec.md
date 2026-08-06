## Purpose

The primary screen of the device. Displays the currently playing track, provides transport controls, and shows a progress bar that advances in real time via on-device interpolation. All content is positioned through named layout slots and respects the safe area of the active display profile.

## Requirements

### Requirement: Now-Playing Screen Displays Current Track Information

The now-playing screen SHALL display the current track title, artist name, and album name. When no track metadata is available (speaker not yet polled, stopped, or transitioning), the screen SHALL show a placeholder state that makes clear the device is not yet displaying track information. All text content SHALL be positioned within the display's safe area.

#### Scenario: Track is playing with full metadata

- **WHEN** a track is playing and metadata is available
- **THEN** the screen shows the track title in the title layout slot
- **AND** the artist name and album name are visible in the track info area
- **AND** all text is clipped within the safe area on a round display

#### Scenario: Metadata is unavailable

- **WHEN** no track metadata is available
- **THEN** the track info area shows a placeholder (e.g., "—") rather than empty or missing labels

#### Scenario: Track title exceeds available width

- **WHEN** a track title is longer than its slot
- **THEN** the title is truncated or scrolled within the slot
- **AND** no glyph is drawn outside the safe area

### Requirement: Now-Playing Screen Shows a Progress Bar That Advances in Real Time

The now-playing screen SHALL include a progress bar that reflects the current elapsed position as a proportion of the track duration. When the transport state is Playing, the bar SHALL visually advance between polls using on-device interpolation. When the state is not Playing, the bar SHALL remain fixed at the last known position. When the track duration is zero or unknown, the bar SHALL remain at its minimum position.

#### Scenario: Track is playing — bar advances smoothly

- **WHEN** the transport state is Playing and time passes between polls
- **THEN** the progress bar advances in real time
- **AND** the advance rate matches one second per second

#### Scenario: Track is paused — bar is frozen

- **WHEN** the transport state is Paused
- **THEN** the progress bar does not advance

#### Scenario: New track starts — bar resets

- **WHEN** a new track begins (position resets to near zero)
- **THEN** the progress bar position updates to reflect the new elapsed position

### Requirement: Now-Playing Screen Provides Play, Pause, Next, and Previous Controls

The now-playing screen SHALL render visible controls for play/pause, next track, and previous track. Each control SHALL be activatable by touch. Tapping a control SHALL issue the corresponding transport command. The play/pause control SHALL reflect the current transport state (showing a play icon when paused and a pause icon when playing).

#### Scenario: User taps play while paused

- **WHEN** the transport state is Paused and the user taps the play/pause control
- **THEN** a Play command is issued to the home room speaker
- **AND** the play/pause control switches to the pause icon once the state updates to Playing

#### Scenario: User taps next

- **WHEN** the user taps the next control
- **THEN** a Next command is issued to the home room speaker

#### Scenario: User taps previous

- **WHEN** the user taps the previous control
- **THEN** a Previous command is issued to the home room speaker

### Requirement: Now-Playing Screen Respects the Safe Area on Round Displays

All content rendered by the now-playing screen SHALL lie within the safe area declared by the active display profile. No widget boundary, label glyph, or icon SHALL be drawn in the dead corners of the round framebuffer.

#### Scenario: Screen rendered on the round display

- **WHEN** the now-playing screen is rendered on the 480×480 round CrowPanel
- **THEN** all content is within the inscribed disc
- **AND** the corners of the framebuffer contain no visible content
