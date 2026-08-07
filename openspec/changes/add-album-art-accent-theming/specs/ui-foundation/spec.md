## Purpose

Delta spec for `ui-foundation`. Implements the accent colour token as a live value updated from album art. Satisfies the animated-transition requirement that the bootstrap left as a hard-coded constant.

## MODIFIED Requirements

### Requirement: A Single Accent Colour Token Drives Interface Emphasis

(Existing requirement — implementing the animated update mechanism.)

The accent colour token SHALL be updated when album art dominant colour is available. The update SHALL be animated: affected elements SHALL transition from their previous colour to the new colour over approximately 400 milliseconds rather than switching instantaneously. If the derived colour is not sufficiently distinguishable from the interface background, the device SHALL apply a legibility adjustment before using it as the accent.

#### Scenario: Accent updated from album art

- **WHEN** a new album art dominant colour is available
- **THEN** the accent-derived elements begin transitioning toward the new colour
- **AND** the transition completes within approximately 400 ms

#### Scenario: Accent contrast is legible after update

- **WHEN** the derived dominant colour has low contrast against the dark background
- **THEN** the applied accent colour is adjusted to remain distinguishable
- **AND** the animated transition still occurs
