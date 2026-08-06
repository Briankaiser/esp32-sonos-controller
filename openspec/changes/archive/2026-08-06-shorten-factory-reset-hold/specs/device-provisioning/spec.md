## MODIFIED Requirements

### Requirement: Factory Reset Is Triggered by a 15-Second Encoder Button Hold

The device SHALL initiate a factory reset when the rotary encoder button is held continuously for 15 seconds during normal operation. The device SHALL display a countdown overlay on screen once the button has been held for 5 seconds, so the user has visible confirmation of the pending reset and a chance to abort by releasing the button. Releasing the button at any point before 15 seconds SHALL cancel the reset with no side effects. This path SHALL be accessible without a WiFi connection or any external tooling. The countdown overlay SHALL be rendered as a centered full-screen layer; its text SHALL be positioned within the round display's inscribed safe-area circle so it remains visible on the physically circular panel.

#### Scenario: Short press does not trigger reset

- **WHEN** the encoder button is pressed and released in under 5 seconds
- **THEN** no countdown overlay appears
- **AND** no NVS values are modified

#### Scenario: 5-second hold shows countdown overlay

- **WHEN** the encoder button has been held continuously for 5 seconds
- **THEN** a countdown overlay appears on screen showing the remaining seconds until reset
- **AND** the overlay updates each second as the hold continues

#### Scenario: Releasing during countdown cancels reset

- **WHEN** the encoder button is released after the countdown overlay has appeared but before 15 seconds have elapsed
- **THEN** the countdown overlay is dismissed
- **AND** no NVS values are modified
- **AND** the device continues normal operation

#### Scenario: Full 15-second hold erases NVS and reboots to portal

- **WHEN** the encoder button is held continuously for 15 seconds
- **THEN** all provisioning values are erased from NVS
- **AND** the device reboots
- **AND** on the next boot the device enters portal mode as if factory-fresh
