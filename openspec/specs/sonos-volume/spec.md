## Purpose

Allows the user to adjust the home room speaker's volume and toggle mute. On boards with a rotary encoder, the encoder is the volume input and the display shows a circular arc. On boards without an encoder, touch buttons are the volume input and the display shows a flat bar. Volume level and mute state are polled from the speaker and reflected on the volume indicator of the now-playing screen.

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
