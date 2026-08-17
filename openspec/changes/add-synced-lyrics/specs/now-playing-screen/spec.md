## ADDED Requirements

### Requirement: Now-Playing Screen Adapts Layout When Lyrics Are Available

When time-aligned lyrics are available for the current track, the now-playing screen SHALL present a lyric region showing the current line and its immediate context, and SHALL shrink the title/artist/album metadata area to free the space the lyric region occupies. When no lyrics are available — the common case, and every lyrics-failure case — the screen SHALL present exactly the layout it presents today, with the metadata area reclaiming the full space and no lyric region drawn. The transition between the two layouts SHALL NOT disturb the transport controls, progress bar, or album art, and all lyric and metadata content SHALL remain within the safe area on round displays.

#### Scenario: Lyrics available — metadata area shrinks

- **WHEN** time-aligned lyrics are available for the current track
- **THEN** a lyric region is shown with the current line highlighted
- **AND** the title/artist/album metadata area shrinks to make room for it
- **AND** all lyric and metadata content is clipped within the safe area on a round display

#### Scenario: No lyrics — layout unchanged from today

- **WHEN** no lyrics are available for the current track
- **THEN** no lyric region is drawn
- **AND** the title/artist/album metadata area occupies its full normal space
- **AND** the screen matches its pre-lyrics appearance

#### Scenario: Lyrics disappear on track change

- **WHEN** a new track begins for which no lyrics are available after a track that had lyrics
- **THEN** the lyric region is removed
- **AND** the metadata area returns to its full normal space

#### Scenario: Controls and art are undisturbed by the layout change

- **WHEN** the layout switches between the lyrics and no-lyrics arrangements
- **THEN** the play/pause, next, and previous controls, the progress bar, and the album art remain functional and within the safe area
