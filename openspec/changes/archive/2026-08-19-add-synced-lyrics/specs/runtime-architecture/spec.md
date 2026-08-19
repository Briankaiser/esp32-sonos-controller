## ADDED Requirements

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

## MODIFIED Requirements

### Requirement: Interface Memory Is Allocated From External RAM

Interface draw buffers, cover-art buffers, and lyric buffers SHALL be allocated from external PSRAM. Memory reserved for wireless networking and transport security SHALL remain in internal RAM.

#### Scenario: Draw buffers placed in external RAM

- **WHEN** the device finishes starting up
- **THEN** the interface draw buffers reside in external PSRAM
- **AND** sufficient internal RAM remains available for wireless networking

#### Scenario: Lyric buffers placed in external RAM

- **WHEN** lyrics are fetched and parsed for a track
- **THEN** the buffers holding the retrieved lyrics and parsed lines reside in external PSRAM
- **AND** internal RAM remains available for wireless networking and transport security

#### Scenario: Allocation failure is reported

- **WHEN** an external RAM allocation for an interface buffer fails
- **THEN** the failure is reported over the diagnostic serial output
- **AND** the device does not silently render with a partial buffer
