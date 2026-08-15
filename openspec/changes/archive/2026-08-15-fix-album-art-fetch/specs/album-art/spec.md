## ADDED Requirements

### Requirement: Album Art Is Fetched From the Correct Source

The device SHALL fetch album art from the host that can actually serve it, which
depends on the form of the art URL:

- When the art URL is a **relative path** (served by a Sonos speaker as a proxy),
  the device SHALL fetch it from the **group coordinator** of the home room's
  group — the speaker actually playing the track. It SHALL NOT fetch relative art
  from an arbitrary or non-coordinator speaker, which does not serve the currently
  playing track's art and returns an error. When the home room is standalone or is
  itself the coordinator, the home room speaker is the source.
- When the art URL is an **absolute URL**, the device SHALL fetch it from that
  URL's own host and port, not from a Sonos speaker. When the absolute URL uses
  `https` (for example a streaming service's image CDN), the device SHALL fetch it
  over a TLS connection.

#### Scenario: Relative art path while grouped

- **WHEN** the current track's art URL is a relative path and the home room is a
  member of a group coordinated by another speaker
- **THEN** the device fetches the art from the group coordinator
- **AND** the decoded art appears on the now-playing screen

#### Scenario: Absolute HTTPS art URL

- **WHEN** the current track's art URL is an absolute `https://` URL on an
  external host (e.g. a streaming service CDN)
- **THEN** the device connects to that URL's own host over TLS and fetches the image
- **AND** the decoded art appears on the now-playing screen

#### Scenario: Relative art path while standalone

- **WHEN** the current track's art URL is a relative path and the home room is
  standalone (its own coordinator)
- **THEN** the device fetches the art from the home room speaker

### Requirement: Non-Image Art Responses Are Rejected Without Decoding

The device SHALL NOT pass a non-image response body to the JPEG decoder. If the
art request returns a non-2xx HTTP status, or returns a body that is not a JPEG
image, the device SHALL discard the response and leave the previously displayed
art or the placeholder in place. The device SHALL NOT crash, reboot, or render
corrupted output as a result.

#### Scenario: Art URL returns an HTTP error

- **WHEN** the art request receives a non-2xx HTTP status (e.g. 404 for missing
  or expired art)
- **THEN** the device does not attempt to decode the response body
- **AND** the previously displayed art or the placeholder remains

#### Scenario: Response body is not a JPEG

- **WHEN** the art request returns a 2xx response whose body is not a JPEG image
- **THEN** the device rejects it without decoding
- **AND** the device does not crash or render corrupted pixels
