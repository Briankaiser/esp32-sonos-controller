## ADDED Requirements

### Requirement: Volume Widget Is Positioned at the Top Edge on Rectangular Boards

On boards whose display profile declares a rectangular shape, the volume widget SHALL occupy a flat strip at the top of the right-column layout region of the now-playing screen. The strip SHALL span the full width of that column and SHALL be short in height (not arc-shaped). On round boards the widget placement is governed by the round archetype and this requirement does not apply.

#### Scenario: Volume strip at top of right column on a rect board

- **WHEN** the now-playing screen is rendered on a board with a rectangular display shape
- **THEN** the volume control area is rendered at the top of the right column
- **AND** the widget is flat — no curved or arc geometry is used
- **AND** the track-info and transport-control areas appear below it in the column
