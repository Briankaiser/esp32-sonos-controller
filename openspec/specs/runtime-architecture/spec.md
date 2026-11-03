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

Interface draw buffers, cover-art buffers, and lyric buffers SHALL be allocated from external PSRAM. Memory used by transport-security (TLS) sessions SHALL also be allocated from external PSRAM, so that a secure fetch does not consume the internal RAM reserved for wireless networking and display scan-out. If external RAM is exhausted, a TLS allocation MAY fall back to internal RAM rather than fail. Memory reserved for wireless networking SHALL remain in internal RAM.

#### Scenario: Draw buffers placed in external RAM

- **WHEN** the device finishes starting up
- **THEN** the interface draw buffers reside in external PSRAM
- **AND** sufficient internal RAM remains available for wireless networking

#### Scenario: Lyric buffers placed in external RAM

- **WHEN** lyrics are fetched and parsed for a track
- **THEN** the buffers holding the retrieved lyrics and parsed lines reside in external PSRAM
- **AND** internal RAM remains available for wireless networking

#### Scenario: Secure fetch does not drain internal RAM

- **WHEN** the device fetches album art or lyrics over TLS
- **THEN** the TLS session's memory is placed in external PSRAM
- **AND** internal RAM free space does not fall to the point where allocations fail or the display flickers

#### Scenario: External RAM exhausted during a secure fetch

- **WHEN** external RAM cannot satisfy a TLS allocation
- **THEN** the allocation is satisfied from internal RAM if available
- **AND** the fallback is counted in the status output

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

### Requirement: Lyric Fetch and Parse Run Off the Interface Task

The lyric fetch (network request to the lyrics source) and the parsing of the retrieved lyrics into timestamped lines SHALL execute on a task other than the interface task, and SHALL deliver the parsed result to the interface task by message passing. The interface task SHALL only render lyric lines that have already been parsed; it SHALL NOT perform the lyric network request or parse on its render or input path. The lyric path SHALL NOT block the sonos task's command-drain or poll cycle. The sustained frame-rate budget SHALL hold while lyrics are being fetched, parsed, and displayed.

#### Scenario: Lyric fetch during playback

- **WHEN** a lyric request for the current track is in flight
- **THEN** the interface continues to render and respond to input
- **AND** the sustained frame rate does not drop below the stated budget

#### Scenario: Lyric parse does not block the interface

- **WHEN** retrieved lyrics are being parsed into timestamped lines
- **THEN** the parse runs off the interface task
- **AND** the parsed result is delivered to the interface task by message passing

#### Scenario: Lyric path does not delay commands

- **WHEN** the user issues a transport or volume command while a lyric fetch or parse is in progress
- **THEN** the command is dispatched within the normal bounded time
- **AND** the lyric work continues in the background without affecting command latency

### Requirement: Steady-State Frame Rate Holds With Lyrics Active

The device SHALL sustain at least 55 frames per second on the now-playing screen while time-aligned lyrics are displayed and their current-line highlight is advancing, under normal operation including background lyric fetch and parse.

#### Scenario: Highlight advancing at the frame-rate budget

- **WHEN** the now-playing screen is displaying time-aligned lyrics with the highlight advancing in real time
- **THEN** the sustained frame rate on the now-playing screen is at least 55 frames per second

### Requirement: Secure Fetches Do Not Overlap

The device SHALL run at most one TLS session at a time across album-art and lyrics fetches. A fetch that cannot start because another TLS session is in flight SHALL wait for it, up to a bounded time, and SHALL then give up for that attempt without affecting playback or the rest of the interface. Lyrics remain best-effort: giving up degrades silently to the normal now-playing screen.

#### Scenario: Art and lyrics requested together

- **WHEN** a track change requires both an album-art fetch and a lyrics fetch over TLS
- **THEN** the two fetches run one after the other, not concurrently
- **AND** both complete if each finishes within the bounded wait

#### Scenario: Secure session stuck for too long

- **WHEN** another TLS session has been in flight longer than the bounded wait
- **THEN** the waiting fetch gives up for that attempt
- **AND** the interface and playback control remain responsive

