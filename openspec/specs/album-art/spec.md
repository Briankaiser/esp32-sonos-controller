## Purpose

Fetches, decodes, and displays the album art for the currently playing track. Extracts a dominant colour from each fetched image for use as the interface accent. All network and decode work runs off the UI task and does not block the Sonos control path.

## Requirements

### Requirement: Device Fetches and Displays Album Art for the Current Track

When a track's album art URL is available, the device SHALL fetch the JPEG from that URL, decode it, and display it in the album art area of the now-playing screen. When no art URL is available or the fetch fails, the device SHALL display a placeholder. The device SHALL not re-fetch art whose URL matches the art already displayed.

#### Scenario: New track with art URL

- **WHEN** the polled track has an album art URL that differs from the last displayed URL
- **THEN** the device fetches the JPEG from that URL on a background task
- **AND** the decoded image appears in the album art area of the now-playing screen

#### Scenario: Art fetch fails

- **WHEN** the art fetch returns an HTTP error or times out
- **THEN** the album art area continues to show the previously displayed art or the placeholder
- **AND** the device does not crash or reboot

#### Scenario: Same track polled again

- **WHEN** consecutive polls return the same album art URL
- **THEN** no new fetch is issued
- **AND** the displayed art remains unchanged

### Requirement: Album Art Decode Runs Off the UI Task and Does Not Block Sonos Controls

The JPEG decode operation SHALL execute on a task other than the UI task. The UI task SHALL only render pixels that have already been decoded. The decode SHALL NOT block the sonos task's command-drain or poll cycle. A dedicated art task SHALL handle fetch and decode independently; the sonos task SHALL only signal when a new art URI is available.

#### Scenario: UI task is not blocked during decode

- **WHEN** a new JPEG is being decoded
- **THEN** the UI task continues to handle LVGL timer events and input without stalling

#### Scenario: Encoder turn during art decode

- **WHEN** the user rotates the encoder while a new JPEG is being fetched and decoded
- **THEN** the volume command is enqueued and executed within the next normal poll cycle
- **AND** the art decode continues in the background without affecting command latency

#### Scenario: Transport control during art decode

- **WHEN** the user taps play/pause while album art is being fetched or decoded
- **THEN** the command is drained and issued to the speaker within the normal 2 s poll window
- **AND** the art decode continues to completion independently

### Requirement: Device Extracts a Dominant Colour from Each Fetched Image

After decoding each album art image, the device SHALL extract a representative dominant colour from the image. The extraction SHALL operate on a downsampled version of the image, not the full-resolution decode.

#### Scenario: Dominant colour extracted

- **WHEN** a new album art image is decoded
- **THEN** a dominant colour is derived from the image
- **AND** the colour is made available to the accent-theming subsystem
