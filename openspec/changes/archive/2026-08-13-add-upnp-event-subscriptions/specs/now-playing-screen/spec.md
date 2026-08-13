# Delta Spec: now-playing-screen

## MODIFIED Requirements

### Requirement: Screen Enters Idle After 5 Minutes of Non-Playing State

When the home room speaker has not been in the Playing transport state for 5 continuous minutes, the device SHALL enter an idle state. In idle, the display backlight SHALL be off or at minimum brightness. The device SHALL continue tracking speaker state while idle: via pushed notifications while eventing is healthy, or via reduced-frequency polling otherwise.

#### Scenario: Enters idle after 5 minutes paused

- **WHEN** the transport state has been Paused, Stopped, or Unknown for 5 continuous minutes
- **THEN** the display backlight turns off
- **AND** the device continues tracking speaker state in the background

#### Scenario: Idle timer resets while playing

- **WHEN** the transport state is Playing
- **THEN** the idle timer is held at zero and does not fire
- **AND** the display remains active

### Requirement: Background Poll Frequency Is Reduced While Idle

While the device is in the idle state and eventing is unhealthy, the transport poll interval SHALL increase to reduce unnecessary network traffic, and SHALL return to normal as soon as the device wakes. While eventing is healthy, steady-state polling SHALL remain suspended during idle — pushed notifications are the state source — and push subscriptions SHALL continue to be maintained (including renewals) for the entire idle period.

#### Scenario: Reduced polling while idle without eventing

- **WHEN** the device enters idle while eventing is unhealthy
- **THEN** playback state is polled approximately every 10 seconds

#### Scenario: Normal polling resumes on wake without eventing

- **WHEN** the device wakes from idle while eventing is unhealthy
- **THEN** playback state is polled at the normal 2-second interval

#### Scenario: Subscriptions maintained through idle

- **WHEN** the device is idle for longer than a subscription lease period while eventing is healthy
- **THEN** subscriptions are renewed on schedule during idle
- **AND** no steady-state polls are issued
