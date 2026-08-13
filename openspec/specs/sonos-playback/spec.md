## Purpose

Polls the home room Sonos speaker for transport state, track metadata, volume, and mute state; interpolates track position between polls; and executes transport commands (play, pause, next, previous, mute toggle) issued by the UI layer. All network I/O runs off the UI task; state is exposed to any task through a thread-safe snapshot interface.

## Requirements

### Requirement: Device Polls Playback State from the Home Room Speaker

While the home room speaker is resolved, the device SHALL keep its recorded playback state — transport state (playing, paused, stopped, or transitioning), current track title, artist name, album name, total track duration, elapsed position, volume level (0–100), and mute state — consistent with what the speaker reports. While push notifications are healthy, state changes SHALL be ingested from notifications and steady-state polling SHALL be suspended; the device SHALL poll only to resynchronize the elapsed track position (on transport-state or track changes, and at a slow periodic cadence) and immediately after a user command. While push notifications are unhealthy, the device SHALL poll the full playback state periodically as the sole state source. When no home room speaker is resolved, both eventing and polling SHALL be skipped and the device SHALL continue operating without error.

#### Scenario: Home room is playing a track

- **WHEN** the home room speaker is resolved and a track is playing
- **THEN** the device records the transport state as Playing
- **AND** the recorded title, artist, and album match what the speaker reports
- **AND** the recorded elapsed position and duration match what the speaker reports

#### Scenario: Home room is not yet resolved

- **WHEN** no home room speaker is resolved in shared state
- **THEN** playback polling and event ingestion are skipped
- **AND** the device logs `[Playback] no home room — skipping poll` to serial
- **AND** the device continues operating normally

#### Scenario: Home room volume and mute stay current

- **WHEN** the home room speaker is resolved and its volume or mute state changes
- **THEN** the recorded volume matches what the speaker reports
- **AND** the recorded mute state matches what the speaker reports

#### Scenario: State request fails

- **WHEN** a poll attempt or state refresh returns an HTTP error or times out
- **THEN** the previously recorded playback state remains unchanged
- **AND** the device logs the error to serial
- **AND** the device does not crash or reboot

#### Scenario: Eventing healthy — steady-state polling suspended

- **WHEN** push notifications are healthy and playback continues without user interaction
- **THEN** the device does not issue periodic full-state polls
- **AND** the elapsed position is still resynchronized against the speaker at a slow cadence

#### Scenario: Eventing unhealthy — polling resumes

- **WHEN** push notifications become unhealthy
- **THEN** the device polls the full playback state periodically as it did before eventing existed
- **AND** the recorded state continues to track the speaker

### Requirement: Track Position Is Interpolated Between Polls

Between network polls, the device SHALL advance the reported elapsed position forward in time at a rate of one second per real second when the transport state is Playing. When the transport state is Paused, Stopped, or Unknown, the reported position SHALL remain fixed. On each poll, the interpolation base SHALL be reset to the freshly polled position value.

#### Scenario: Progress advances between polls while playing

- **WHEN** the transport state is Playing and 3 seconds elapse since the last poll
- **THEN** the reported position has advanced by approximately 3 seconds relative to the polled position
- **AND** the reported position does not exceed the track duration

#### Scenario: Progress does not advance while paused

- **WHEN** the transport state is Paused and several seconds elapse
- **THEN** the reported position remains equal to the last polled position

### Requirement: Transport Commands Are Executed Without Blocking the UI Task

When the user issues a transport command (play, pause, next, previous, or mute toggle), the device SHALL execute the corresponding UPnP action on the home room speaker. The command SHALL be executed on the sonos task, not on the UI task. The UI task SHALL be able to enqueue a command without blocking.

#### Scenario: User taps play while paused

- **WHEN** the home room speaker is in a Paused state and a Play command is enqueued
- **THEN** the sonos task issues a Play action to the home room speaker via UPnP
- **AND** the next state poll reflects that the speaker is Playing

#### Scenario: Mute toggle command is enqueued and executed

- **WHEN** a ToggleMute command is enqueued and the sonos task drains it
- **THEN** the home room speaker's mute state is inverted via a `SetMute` call
- **AND** the next poll reflects the updated mute state

#### Scenario: Command issued when no home room resolved

- **WHEN** a transport command is enqueued and no home room speaker is resolved
- **THEN** the command is discarded without issuing a network request
- **AND** the device does not crash or reboot

### Requirement: Playback State Is Accessible to Other Subsystems via a Thread-Safe Interface

The current playback state (transport state, metadata, position, poll timestamp) SHALL be readable from any task without causing data races. Writes happen only on the sonos task; reads MAY happen from any task.

#### Scenario: UI task reads playback state during an active poll

- **WHEN** the UI task reads a playback state snapshot while the sonos task is writing a new poll result
- **THEN** the read returns either the previous complete state or the new complete state
- **AND** no torn or partial read occurs
