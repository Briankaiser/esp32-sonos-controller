## Purpose

Delta spec for `sonos-playback`. Extends the polled playback state with volume and mute fields, and extends the command set with mute toggle.

## MODIFIED Requirements

### Requirement: Device Polls Playback State from the Home Room Speaker

(Existing requirement — extending the polled fields.)

After this change, the device SHALL additionally poll the home room speaker's current volume level (0–100) and mute state alongside the existing transport state, metadata, and position. The volume and mute state SHALL be included in the shared playback state accessible to other subsystems.

#### Scenario: Home room volume and mute are polled

- **WHEN** the home room speaker is resolved and a poll cycle runs
- **THEN** the recorded volume matches what the speaker reports
- **AND** the recorded mute state matches what the speaker reports

### Requirement: Transport Commands Are Executed Without Blocking the UI Task

(Existing requirement — extending the command set.)

In addition to the existing play, pause, next, and previous commands, the device SHALL accept a mute-toggle command from the UI task. The command SHALL be executed on the sonos task and SHALL toggle the home room speaker's mute state.

#### Scenario: Mute toggle command is enqueued and executed

- **WHEN** a ToggleMute command is enqueued and the sonos task drains it
- **THEN** the home room speaker's mute state is inverted via a `SetMute` call
- **AND** the next poll reflects the updated mute state
