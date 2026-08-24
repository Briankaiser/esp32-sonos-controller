## MODIFIED Requirements

### Requirement: Volume Arc Reflects Current Volume and Mute State

The volume indicator on the now-playing screen SHALL display the volume level last reported by the home room speaker. On a board with a rotary encoder, the indicator SHALL be a circular arc. On a board without a rotary encoder, the indicator SHALL be a flat horizontal bar. When the speaker is muted, the indicator SHALL use a visually distinct style regardless of its form. The indicator SHALL update whenever a new playback state snapshot is received. The indicator SHALL be positioned within the display's safe area on round displays.

#### Scenario: Arc reflects polled volume on an encoder board

- **WHEN** a new playback state is received with volume 70 on a board with a rotary encoder
- **THEN** the volume arc displays at approximately 70% of its full travel

#### Scenario: Bar reflects polled volume on a no-encoder board

- **WHEN** a new playback state is received with volume 70 on a board without a rotary encoder
- **THEN** the volume bar displays at approximately 70% of its full width

#### Scenario: Indicator shows muted state

- **WHEN** the playback state reports the speaker is muted
- **THEN** the volume indicator (arc or bar) uses a distinct visual style to indicate the muted state

## ADDED Requirements

### Requirement: Volume Buttons on No-Encoder Boards Adjust Volume by Discrete Steps

On boards whose display profile declares no rotary encoder, the now-playing screen SHALL render a decrease (−) button and an increase (+) button flanking the volume bar. Tapping the decrease button SHALL lower the home room speaker's volume by 5 units. Tapping the increase button SHALL raise it by 5 units. Volume SHALL be clamped to the range 0–100. On tap, the volume bar SHALL reflect the adjusted level immediately, without waiting for the next polled state, matching the immediate feedback the encoder path provides on rotation. When no home room is resolved, the buttons SHALL have no effect. This requirement does not apply to boards with a rotary encoder.

#### Scenario: Tap increase button

- **WHEN** the home room speaker is resolved and the user taps the increase (+) button
- **THEN** a volume-up command is issued to the home room speaker
- **AND** the volume bar advances immediately by one step
- **AND** the resulting volume does not exceed 100

#### Scenario: Tap decrease button

- **WHEN** the home room speaker is resolved and the user taps the decrease (−) button
- **THEN** a volume-down command is issued to the home room speaker
- **AND** the volume bar retreats immediately by one step
- **AND** the resulting volume does not go below 0

#### Scenario: No home room resolved

- **WHEN** no home room speaker is resolved and the user taps either volume button
- **THEN** no volume command is issued
- **AND** the device continues operating normally
