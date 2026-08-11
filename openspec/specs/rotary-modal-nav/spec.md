## Purpose

Defines the encoder-driven navigation mode that activates while a modal overlay is open, allowing the user to move a selection indicator through the modal's list items and activate them by pressing the encoder.

## Requirements

### Requirement: Encoder Navigation Mode Activates When a Modal Overlay Is Open

While any modal overlay (settings, favorites, grouping) is visible, the encoder SHALL operate in navigation mode. In navigation mode, encoder rotation SHALL move a selection indicator through the list items of the modal's primary navigable list, and an encoder press SHALL activate the currently selected item. The encoder SHALL NOT produce a volume-change or transport-toggle intent while navigation mode is active.

#### Scenario: Modal opens — encoder switches to navigation mode

- **WHEN** a modal overlay becomes visible
- **THEN** the encoder no longer produces volume or transport intents
- **AND** subsequent rotation events are interpreted as navigation intents

#### Scenario: Modal closes — encoder reverts to normal

- **WHEN** the active modal overlay is dismissed
- **THEN** the encoder immediately resumes producing volume-change and transport intents
- **AND** no navigation mode artifact remains active

#### Scenario: No modal is open

- **WHEN** no modal overlay is visible
- **THEN** the encoder operates normally (volume and transport)
- **AND** no selection indicator is shown

### Requirement: First Encoder Turn in Navigation Mode Activates the Selection Indicator

On the first encoder rotation event after navigation mode becomes active, the device SHALL display a selection indicator around the first item in the modal's primary navigable list. The selection indicator SHALL be visible and clearly distinguish the selected item from its neighbours. The indicator SHALL be positioned within the display's safe area on round displays.

#### Scenario: First turn after modal opens

- **WHEN** a modal overlay is open and the encoder is rotated for the first time
- **THEN** a selection indicator appears around the first list item
- **AND** the indicator is within the safe area on a round display

#### Scenario: No indicator before first turn

- **WHEN** a modal has just opened and the encoder has not yet been rotated
- **THEN** no selection indicator is shown

### Requirement: Encoder Rotation Moves the Selection Indicator Through List Items

After the selection indicator is active, each encoder step SHALL move the indicator one position forward (clockwise rotation) or one position backward (counter-clockwise rotation) through the list items. The selected item SHALL be scrolled into view if it is not fully visible. When the selection reaches the last item and the encoder is turned forward, the selection SHALL remain on the last item. When the selection is on the first item and the encoder is turned backward, the selection SHALL remain on the first item.

#### Scenario: Turn forward moves selection down

- **WHEN** the selection indicator is on item N and the encoder is turned clockwise
- **THEN** the selection indicator moves to item N+1
- **AND** item N loses the selection indicator

#### Scenario: Turn backward moves selection up

- **WHEN** the selection indicator is on item N (N > 1) and the encoder is turned counter-clockwise
- **THEN** the selection indicator moves to item N-1

#### Scenario: Selected item scrolls into view

- **WHEN** the selection indicator moves to an item that is not fully visible in the list viewport
- **THEN** the list scrolls so that item is fully visible

#### Scenario: Selection at last item — turn forward is clamped

- **WHEN** the selection indicator is on the last list item and the encoder is turned clockwise
- **THEN** the selection indicator remains on the last item

#### Scenario: Selection at first item — turn backward is clamped

- **WHEN** the selection indicator is on the first list item and the encoder is turned counter-clockwise
- **THEN** the selection indicator remains on the first item

### Requirement: Encoder Press Activates the Selected Item

While the selection indicator is active, an encoder button press SHALL trigger the same action as a touch tap on the selected list item. If the selection indicator has not yet been shown (no encoder rotation since the modal opened), an encoder press SHALL have no effect.

#### Scenario: Press activates selected item

- **WHEN** the selection indicator is on a list item and the encoder button is pressed
- **THEN** the item is activated identically to a touch tap on that item

#### Scenario: Press with no selection has no effect

- **WHEN** a modal is open but the encoder has not yet been rotated
- **THEN** an encoder button press produces no action

### Requirement: Selection Indicator Is Destroyed When the Modal Closes

When a modal overlay is dismissed, the selection indicator SHALL be removed immediately. It SHALL NOT persist across modal open/close cycles. The next time a modal opens and the encoder is first turned, the selection indicator SHALL begin again at the first list item.

#### Scenario: Indicator removed on modal close

- **WHEN** the active modal overlay closes while a selection indicator is visible
- **THEN** the selection indicator is removed with the modal

#### Scenario: New modal open starts fresh

- **WHEN** a second modal is opened after a previous one was closed
- **THEN** no selection indicator is present until the encoder is first turned
- **AND** the first turn places the indicator on the first item of the new modal's list
