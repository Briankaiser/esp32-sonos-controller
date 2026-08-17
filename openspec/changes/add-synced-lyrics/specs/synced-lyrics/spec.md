## Purpose

Fetches, parses, caches, and time-aligns lyrics for the currently playing track from a third-party lyrics source, entirely off the interface task, and degrades silently to no lyrics on any failure or whenever line-level timing cannot be trusted.

## ADDED Requirements

### Requirement: Device Fetches Timed Lyrics for the Current Track

When the current track's identifying metadata (artist, title, album, and track duration) is available, the device SHALL request time-aligned lyrics for that track from the lyrics source over the network, on a task other than the interface task. The device SHALL cache the result keyed on the current track's identity and SHALL NOT re-request lyrics for a track whose identity matches lyrics already held. Lyrics are not part of the Sonos UPnP surface; they are obtained from an external source over the LAN's internet uplink, never from the Sonos cloud/OAuth API and never via Home Assistant.

#### Scenario: New track with resolvable lyrics

- **WHEN** the playing track's identity differs from the track whose lyrics are currently held
- **THEN** the device requests lyrics for the new track on a background task
- **AND** the parsed lyrics become available to the now-playing screen once retrieved

#### Scenario: Same track polled again

- **WHEN** consecutive polls report the same track identity
- **THEN** no new lyrics request is issued
- **AND** the held lyrics remain in use

#### Scenario: Track changes before a request completes

- **WHEN** the track changes while a lyrics request for the previous track is still in flight
- **THEN** the stale result is discarded rather than shown against the new track

### Requirement: Lyrics Degrade Silently On Any Failure

The device SHALL treat the absence of lyrics as the normal, unremarkable case. When lyrics cannot be obtained — no internet uplink, the source returns no match, the source returns an error or times out, or the response cannot be parsed — the device SHALL show no lyric region, no error message, and no placeholder specific to lyrics, and SHALL continue to present the now-playing screen exactly as it appears when no lyrics feature exists. No failure of the lyrics path SHALL crash, reboot, or stall the device or the Sonos control path.

#### Scenario: No internet uplink

- **WHEN** the device cannot reach the lyrics source because the internet uplink is down
- **THEN** no lyric region is shown
- **AND** playback, volume, grouping, and all other now-playing behavior continue unaffected

#### Scenario: Lyrics not found for the track

- **WHEN** the lyrics source reports no match for the current track
- **THEN** no lyric region is shown
- **AND** no error text is presented to the user

#### Scenario: Request fails, times out, or returns unparseable content

- **WHEN** the lyrics request errors, times out, or returns content that cannot be parsed
- **THEN** the device does not crash or reboot
- **AND** no lyric region is shown
- **AND** the Sonos control path is not stalled

### Requirement: Current Lyric Line Advances In Real Time

When time-aligned lyrics are available for the current track and the transport state is Playing, the now-playing screen SHALL highlight the lyric line corresponding to the current playback position and SHALL advance the highlight in real time using the device's on-device interpolated position, resynced on each state poll. When the transport state is not Playing, the highlighted line SHALL hold at the last known position and SHALL NOT advance. All lyric content SHALL be clipped within the safe area on round displays.

#### Scenario: Track playing — highlight advances

- **WHEN** time-aligned lyrics are available, the transport state is Playing, and time passes between polls
- **THEN** the highlighted lyric line advances to match the interpolated playback position
- **AND** no lyric glyph is drawn outside the safe area on a round display

#### Scenario: Position resyncs on poll

- **WHEN** a state poll returns a playback position that differs from the interpolated estimate
- **THEN** the highlighted line is corrected to the polled position rather than continuing to drift

#### Scenario: Paused — highlight frozen

- **WHEN** the transport state is Paused
- **THEN** the highlighted lyric line does not advance

#### Scenario: New track starts

- **WHEN** a new track begins and its lyrics become available
- **THEN** the highlight resets to the line for the new track's current position

### Requirement: Untrustworthy Timing Degrades To Unsynced Or Hidden

The device SHALL highlight a lyric line in real time only when the line-level timing can be trusted. When the retrieved lyrics carry no per-line timestamps, when the lyrics were matched to a track whose duration differs materially from the playing track's duration, or when the playback position is not reliable enough for line-level alignment, the device SHALL NOT show a drifting or misaligned highlight. In those cases it SHALL either present the lyrics as static unsynced text with no moving highlight, or show no lyric region at all — never a highlight that misrepresents the current line.

#### Scenario: Duration fingerprint mismatch

- **WHEN** the best available lyrics were matched to a track whose duration differs materially from the playing track's duration
- **THEN** the device does not show a real-time highlight based on that mismatched timing
- **AND** it either shows the lyrics as static unsynced text or shows no lyric region

#### Scenario: Only plain (unsynced) lyrics available

- **WHEN** the retrieved lyrics contain no per-line timestamps
- **THEN** no moving highlight is shown
- **AND** the lyrics may be presented as static text within the safe area or omitted

#### Scenario: Position not reliable for line alignment

- **WHEN** the current playback position cannot be trusted to line-level accuracy
- **THEN** the device suppresses the real-time highlight rather than advancing a line that may be wrong
