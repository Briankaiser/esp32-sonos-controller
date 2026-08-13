# Delta Spec: sonos-playback

## MODIFIED Requirements

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
