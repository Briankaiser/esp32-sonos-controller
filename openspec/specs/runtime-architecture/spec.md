# Runtime Architecture Specification

## Purpose

Defines how concurrent work is partitioned across cores so the interface stays responsive while network requests, image decoding, and colour analysis proceed, and fixes the sustained frame-rate budget the device must hold.

## Requirements

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

### Requirement: Sustained Frame Rate Budget

The device SHALL sustain at least 55 frames per second while displaying the primary screen under normal operation, including while background input and output work proceeds. The firmware SHALL provide a diagnostic overlay, enabled by a build flag, reporting current frame rate and free memory.

#### Scenario: Measuring the frame rate

- **WHEN** a firmware image is built with the diagnostic overlay flag enabled
- **THEN** the overlay displays current frame rate and free memory
- **AND** the reported sustained frame rate on the primary screen is at least 55 frames per second

#### Scenario: Overlay absent from release images

- **WHEN** a firmware image is built without the diagnostic overlay flag
- **THEN** no overlay is rendered
- **AND** the image contains no overlay rendering code

### Requirement: Interface Memory Is Allocated From External RAM

Interface draw buffers and cover-art buffers SHALL be allocated from external PSRAM. Memory reserved for wireless networking and transport security SHALL remain in internal RAM.

#### Scenario: Draw buffers placed in external RAM

- **WHEN** the device finishes starting up
- **THEN** the interface draw buffers reside in external PSRAM
- **AND** sufficient internal RAM remains available for wireless networking

#### Scenario: Allocation failure is reported

- **WHEN** an external RAM allocation for an interface buffer fails
- **THEN** the failure is reported over the diagnostic serial output
- **AND** the device does not silently render with a partial buffer

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
