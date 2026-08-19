## ADDED Requirements

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
