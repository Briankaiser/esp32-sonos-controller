## MODIFIED Requirements

### Requirement: Interface Rendering Is Never Blocked By Input Or Output Work

All interface rendering and widget mutation SHALL occur on a single dedicated task. That task SHALL NOT perform network requests, image decoding, filesystem access, blocking diagnostic output, or any operation that blocks on external input or output. Work of that kind SHALL run on separate tasks and deliver results to the rendering task by message passing. Diagnostic logging SHALL NOT be performed on the rendering task's input-read or render path in normal operation.

#### Scenario: Network request during playback

- **WHEN** a speaker state request is in flight
- **THEN** the interface continues to render and respond to input
- **AND** the sustained frame rate does not drop below the stated budget

#### Scenario: Image decode during playback

- **WHEN** cover art is being decoded and analysed for its dominant colour
- **THEN** the interface continues to render and respond to input
- **AND** the decoded result is delivered to the rendering task by message passing

#### Scenario: Unreachable speaker

- **WHEN** a request to a speaker times out
- **THEN** the interface remains responsive throughout
- **AND** the failure is surfaced without stalling rendering

#### Scenario: Touch input is not delayed by diagnostic logging

- **WHEN** the user touches the screen repeatedly
- **THEN** no diagnostic logging occurs inside the input-read or render path
- **AND** touch events are sampled and delivered without being dropped

## ADDED Requirements

### Requirement: User Commands Execute Independently of Background Polling

User-initiated commands (transport, volume, mute, grouping, favorites) SHALL execute without waiting for an in-progress background state poll to complete. Commands SHALL be dispatched on a path that can preempt the polling path so that a command issued while a poll's network requests are in flight is still acted on within a short, bounded time.

#### Scenario: Command issued during an active poll

- **WHEN** the user issues a command while a background state poll is mid-flight
- **THEN** the command is executed without waiting for the poll to finish
- **AND** the command reaches the speaker within a short bounded time rather than after the full poll

#### Scenario: Rapid successive commands

- **WHEN** the user issues several commands in quick succession
- **THEN** each command is dispatched promptly
- **AND** none are dropped while a poll is in progress

#### Scenario: State refresh follows a command

- **WHEN** a command has been executed
- **THEN** a background state refresh is triggered promptly so the display reflects the new state
