## Why

After boot, the now-playing screen loads immediately but the device hasn't heard from the Sonos speaker yet. The result is an empty layout — dashes where the title should be, a dark square where the art will go, and a green arc sitting at 50% with no data behind it. This looks unfinished and gives the user no signal that the device is doing anything.

Dimming the screen with a centred "Connecting…" label is the standard pattern for a loading state: it communicates that content is coming, prevents the user from misreading the empty layout as a bug, and disappears automatically once real state arrives. The fade-out transition makes the moment of connection feel intentional rather than abrupt.

## What Changes

- Add `s_loading_overlay` to `now_playing.cpp`: a full-screen semi-transparent overlay created at the end of `now_playing_create()`, sitting above all screen content.
- The overlay has a black background at 80% opacity and a centred "Connecting…" caption label.
- In `now_playing_update()`, when the first `PlaybackState` with a non-`Unknown` transport state arrives, start a 300 ms opacity fade-out animation on the overlay and delete it on completion.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `now-playing-screen` (DELTA): When no playback state has been received yet, the screen is covered by a semi-transparent loading overlay with centred connecting text. The overlay fades out and is removed the first time a real transport state is received.

## Impact

- `src/ui/screens/now_playing.cpp` only: one new static, a few lines in `now_playing_create()`, and a guard in `now_playing_update()`.
- No changes to layout, fonts, architecture, or any other file.
