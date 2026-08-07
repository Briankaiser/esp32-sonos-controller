## Purpose

Delta spec for `album-art`. Replaces the synchronous in-task decode with an asynchronous pipeline that does not block transport controls or volume during art loading.

## MODIFIED Requirements

### Requirement: Album Art Decode Runs Off the UI Task

(Existing requirement — extending to also require non-blocking behaviour relative to the Sonos control path.)

The JPEG decode operation SHALL execute on a task other than the UI task AND SHALL NOT block the sonos task's command-drain or poll cycle. A dedicated art task SHALL handle fetch and decode independently; the sonos task SHALL only signal when a new art URI is available.

#### Scenario: Encoder turn during art decode

- **WHEN** the user rotates the encoder while a new JPEG is being fetched and decoded
- **THEN** the volume command is enqueued and executed within the next normal poll cycle
- **AND** the art decode continues in the background without affecting command latency

#### Scenario: Transport control during art decode

- **WHEN** the user taps play/pause while album art is being fetched or decoded
- **THEN** the command is drained and issued to the speaker within the normal 2 s poll window
- **AND** the art decode continues to completion independently
