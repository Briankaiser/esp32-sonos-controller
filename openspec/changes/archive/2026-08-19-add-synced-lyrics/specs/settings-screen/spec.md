## ADDED Requirements

### Requirement: Settings Screen Provides a Lyrics On/Off Control

The settings screen SHALL present a control to turn lyrics on or off. The control SHALL reflect the current persisted setting when the screen is shown, and toggling it SHALL update the persisted setting and take effect immediately without requiring a restart. The control SHALL be positioned within the display's safe area on round displays.

#### Scenario: Control reflects the current setting

- **WHEN** the settings screen is opened
- **THEN** the lyrics control shows the current on/off state

#### Scenario: Toggling the control takes effect immediately

- **WHEN** the user toggles the lyrics control
- **THEN** the persisted lyrics setting is updated
- **AND** the change takes effect immediately — turning lyrics off removes any lyric region now, and turning them on fetches lyrics for the current track
