## Purpose

Delta spec for `now-playing-screen`. Adds idle-state behaviour so the display powers down after a period of inactivity and wakes when playback resumes or the user touches the screen.

## ADDED Requirements

### Requirement: Screen Enters Idle After 5 Minutes of Non-Playing State

When the home room speaker has not been in the Playing transport state for 5 continuous minutes, the device SHALL enter an idle state. In idle, the display backlight SHALL be off or at minimum brightness. The device SHALL continue polling the speaker at a reduced frequency while idle.

#### Scenario: Enters idle after 5 minutes paused

- **WHEN** the transport state has been Paused, Stopped, or Unknown for 5 continuous minutes
- **THEN** the display backlight turns off
- **AND** the device continues background polling at a reduced interval

#### Scenario: Idle timer resets while playing

- **WHEN** the transport state is Playing
- **THEN** the idle timer is held at zero and does not fire
- **AND** the display remains active

### Requirement: Screen Wakes Immediately on Playback Resumption or Touch

When playback resumes (transport state changes to Playing) while the device is idle, the device SHALL wake the display and return to normal operation. Any touch event while idle SHALL also wake the display. The first touch while idle SHALL be treated as a wake-only event and SHALL NOT trigger any button action.

#### Scenario: Playback resumes while idle

- **WHEN** the device is idle and the transport state changes to Playing
- **THEN** the display wakes immediately
- **AND** the now-playing screen shows the current track
- **AND** the poll interval returns to normal

#### Scenario: Touch while idle

- **WHEN** the device is idle and the user touches the screen
- **THEN** the display wakes
- **AND** the idle timer resets
- **AND** the touch does not activate any button or overlay

### Requirement: Background Poll Frequency Is Reduced While Idle

While the device is in the idle state, the transport poll interval SHALL increase to reduce unnecessary network traffic. The poll interval SHALL return to normal as soon as the device wakes.

#### Scenario: Reduced polling while idle

- **WHEN** the device enters idle
- **THEN** playback state is polled approximately every 10 seconds

#### Scenario: Normal polling resumes on wake

- **WHEN** the device wakes from idle
- **THEN** playback state is polled at the normal 2-second interval
