## Purpose

Allows the user to adjust the home room speaker's volume and toggle mute using the rotary encoder. Volume level and mute state are polled from the speaker and reflected on the volume arc of the now-playing screen.

## Requirements

### Requirement: Rotating the Encoder Adjusts the Home Room Speaker's Volume

When the user rotates the encoder, the device SHALL adjust the home room speaker's volume in the direction and proportion of the rotation. Volume SHALL be clamped to the range 0–100. The adjustment SHALL apply to the home room speaker only, not to other members of its group. When no home room is resolved, encoder rotation SHALL have no effect.

#### Scenario: Encoder rotated clockwise while playing

- **WHEN** the home room speaker is resolved and the user rotates the encoder clockwise
- **THEN** the speaker's volume increases by a proportional amount
- **AND** the volume does not exceed 100

#### Scenario: Encoder rotated counter-clockwise at low volume

- **WHEN** the home room speaker is resolved and the user rotates the encoder counter-clockwise while the volume is already near 0
- **THEN** the speaker's volume decreases toward 0 but does not go below 0

#### Scenario: No home room resolved

- **WHEN** no home room speaker is resolved and the user rotates the encoder
- **THEN** no volume command is issued
- **AND** the device continues operating normally

### Requirement: Short Encoder Button Press Toggles Mute

A short press of the encoder button (button held and released in under 1 second) SHALL toggle the mute state of the home room speaker. Pressing while unmuted SHALL mute; pressing while muted SHALL unmute. When no home room is resolved, the short press SHALL have no effect.

#### Scenario: Short press while unmuted

- **WHEN** the home room speaker is unmuted and the user presses and releases the encoder button in under 1 second
- **THEN** the speaker becomes muted

#### Scenario: Short press while muted

- **WHEN** the home room speaker is muted and the user presses and releases the encoder button in under 1 second
- **THEN** the speaker becomes unmuted

### Requirement: Volume Arc Reflects Current Volume and Mute State

The volume arc on the now-playing screen SHALL display the volume level last reported by the home room speaker. When the speaker is muted, the arc SHALL indicate the muted state visually. The arc SHALL update whenever a new playback state snapshot is received. The arc SHALL be positioned within the display's safe area on round displays.

#### Scenario: Arc reflects polled volume

- **WHEN** a new playback state is received with volume 70
- **THEN** the volume arc displays at approximately 70% of its full travel

#### Scenario: Arc indicates mute

- **WHEN** the playback state reports the speaker is muted
- **THEN** the volume arc uses a distinct visual style to indicate the muted state
