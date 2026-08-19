## Purpose

The primary screen of the device. Displays the currently playing track, provides transport controls, and shows a progress bar that advances in real time via on-device interpolation. All content is positioned through named layout slots and respects the safe area of the active display profile.
## Requirements
### Requirement: Now-Playing Screen Displays Current Track Information

The now-playing screen SHALL display the current track title, artist name, and album name. When no track metadata is available (speaker not yet polled, stopped, or transitioning), the screen SHALL show a placeholder state that makes clear the device is not yet displaying track information. All text content SHALL be positioned within the display's safe area.

#### Scenario: Track is playing with full metadata

- **WHEN** a track is playing and metadata is available
- **THEN** the screen shows the track title in the title layout slot
- **AND** the artist name and album name are visible in the track info area
- **AND** all text is clipped within the safe area on a round display

#### Scenario: Metadata is unavailable

- **WHEN** no track metadata is available
- **THEN** the track info area shows a placeholder (e.g., "—") rather than empty or missing labels

#### Scenario: Track title exceeds available width

- **WHEN** a track title is longer than its slot
- **THEN** the title is truncated or scrolled within the slot
- **AND** no glyph is drawn outside the safe area

### Requirement: Now-Playing Screen Shows a Progress Bar That Advances in Real Time

The now-playing screen SHALL include a progress bar that reflects the current elapsed position as a proportion of the track duration. When the transport state is Playing, the bar SHALL visually advance between polls using on-device interpolation. When the state is not Playing, the bar SHALL remain fixed at the last known position. When the track duration is zero or unknown, the bar SHALL remain at its minimum position.

#### Scenario: Track is playing — bar advances smoothly

- **WHEN** the transport state is Playing and time passes between polls
- **THEN** the progress bar advances in real time
- **AND** the advance rate matches one second per second

#### Scenario: Track is paused — bar is frozen

- **WHEN** the transport state is Paused
- **THEN** the progress bar does not advance

#### Scenario: New track starts — bar resets

- **WHEN** a new track begins (position resets to near zero)
- **THEN** the progress bar position updates to reflect the new elapsed position

### Requirement: Now-Playing Screen Provides Play, Pause, Next, and Previous Controls

The now-playing screen SHALL render visible controls for play/pause, next track, and previous track. Each control SHALL be activatable by touch. Tapping a control SHALL issue the corresponding transport command. The play/pause control SHALL reflect the current transport state (showing a play icon when paused and a pause icon when playing).

#### Scenario: User taps play while paused

- **WHEN** the transport state is Paused and the user taps the play/pause control
- **THEN** a Play command is issued to the home room speaker
- **AND** the play/pause control switches to the pause icon once the state updates to Playing

#### Scenario: User taps next

- **WHEN** the user taps the next control
- **THEN** a Next command is issued to the home room speaker

#### Scenario: User taps previous

- **WHEN** the user taps the previous control
- **THEN** a Previous command is issued to the home room speaker

### Requirement: Now-Playing Screen Shows a Loading Overlay Until First State Is Received

When the now-playing screen is first displayed and no playback state has been received from the home room speaker, the device SHALL overlay the screen with a dimmed layer and a visible connecting indicator. The overlay SHALL cover all screen content. When the first non-unknown transport state is received, the overlay SHALL be removed with a brief animated transition. The overlay SHALL NOT persist indefinitely: when a connectivity fault is diagnosed (no WiFi, no Sonos system found, or home room offline), the fault status layer SHALL take over the screen in place of the connecting overlay, and normal now-playing content SHALL resume once the fault clears and state is received. The overlay and its text SHALL be positioned within the display's safe area on round displays.

#### Scenario: Screen first displayed before any state

- **WHEN** the now-playing screen is shown and `transport_state` is Unknown
- **THEN** a semi-transparent overlay covers the screen contents
- **AND** a connecting indicator text is visible and centred within the safe area

#### Scenario: First state received — overlay dismisses

- **WHEN** a `PlaybackState` with a non-Unknown `transport_state` is passed to the screen for the first time
- **THEN** the overlay begins fading out
- **AND** the overlay is fully removed within 300 ms

#### Scenario: Overlay does not reappear after first dismissal

- **WHEN** the overlay has been dismissed and a subsequent poll returns any transport state
- **THEN** the overlay remains absent

#### Scenario: Connecting overlay yields to a diagnosed fault

- **WHEN** no state has been received and a connectivity fault is diagnosed
- **THEN** the fault status screen is shown instead of an indefinite connecting overlay
- **AND** when the fault clears and state is received, the now-playing screen resumes normally

### Requirement: Now-Playing Screen Respects the Safe Area on Round Displays

All content rendered by the now-playing screen SHALL lie within the safe area declared by the active display profile. No widget boundary, label glyph, or icon SHALL be drawn in the dead corners of the round framebuffer.

#### Scenario: Screen rendered on the round display

- **WHEN** the now-playing screen is rendered on the 480×480 round CrowPanel
- **THEN** all content is within the inscribed disc
- **AND** the corners of the framebuffer contain no visible content

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

### Requirement: Now-Playing Screen Adapts Layout When Lyrics Are Available

The track-metadata region SHALL be anchored at the top of the now-playing screen in every state, so that its top edge does not move when lyrics appear or disappear. When no lyrics are available — the common case, and every lyrics-failure case — the metadata region SHALL show the track title, artist, and album, and no lyric region SHALL be drawn. When time-aligned lyrics are available for the current track, the metadata region SHALL shrink — dropping the album line and keeping title and artist — to free space for a lyric region shown below it presenting the current line and its immediate context. Only the bottom extent of the metadata region changes between the two states; the top edge SHALL remain fixed. The transition SHALL NOT disturb the transport controls, progress bar, volume indicator, or album art, and all lyric and metadata content SHALL remain within the safe area on round displays.

#### Scenario: Lyrics available — metadata shrinks in place, lyric region appears below

- **WHEN** time-aligned lyrics are available for the current track
- **THEN** a lyric region is shown below the metadata with the current line highlighted
- **AND** the metadata region shrinks to title and artist only, dropping the album line
- **AND** the metadata region's top edge stays where it was — nothing shifts up or down
- **AND** all lyric and metadata content is clipped within the safe area on a round display

#### Scenario: No lyrics — full metadata at the top, no lyric region

- **WHEN** no lyrics are available for the current track
- **THEN** no lyric region is drawn
- **AND** the top-anchored metadata region shows title, artist, and album

#### Scenario: Lyrics disappear on track change

- **WHEN** a new track begins for which no lyrics are available after a track that had lyrics
- **THEN** the lyric region is removed
- **AND** the metadata region returns to showing title, artist, and album at the same top position

#### Scenario: Controls and art are undisturbed by the layout change

- **WHEN** the layout switches between the lyrics and no-lyrics arrangements
- **THEN** the play/pause, next, and previous controls, the progress bar, the volume indicator, and the album art remain functional and within the safe area

