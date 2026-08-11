## Why

The rotary encoder is unused dead weight when a modal overlay (settings, favorites, grouping) is open — users must reach for the touchscreen to navigate lists. Repurposing the encoder as a list navigator when any modal is visible makes one-handed interaction practical on a small round display.

## What Changes

- When a modal overlay (settings, favorites, grouping) is open, encoder rotation no longer adjusts volume and encoder press no longer toggles play/pause
- On the first encoder turn after a modal opens, a selection indicator appears around the first item in the modal's primary list
- Subsequent turns move the selection indicator forward or backward through the list items
- An encoder press activates the currently selected item identically to a touch tap
- When the modal closes, the encoder reverts immediately to normal volume/playback control and the selection indicator is destroyed

## Capabilities

### New Capabilities

- `rotary-modal-nav`: Encoder-driven navigation mode that activates when a modal overlay is open — selection indicator lifecycle, navigation movement, and item activation semantics

### Modified Capabilities

- `ui-foundation`: Encoder input routing must be context-aware. When a modal overlay is visible, VolumeDelta and EncoderButtonPress intents are replaced by NavigateIntent and SelectIntent before reaching the active screen. The existing requirement that "rotation becomes a volume intent" applies only when no modal overlay is present.

## Impact

- `src/main.cpp` — encoder event handler gains overlay-presence check before deciding which intent to emit
- `src/ui/screens/now_playing.cpp` — no changes required; intent routing is upstream
- Overlay screens (settings, favorites, grouping) — must expose a list-navigation interface so the encoder handler can deliver NavigateIntent and SelectIntent to the correct list
- New LVGL overlay helper or overlay base logic to track which overlay (if any) is currently visible and which list within it is the navigation target
- `openspec/specs/ui-foundation/spec.md` — delta to the "Input Is Delivered To Screens As Intents" requirement
