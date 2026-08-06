## ADDED Requirements

### Requirement: Device Displays Setup Instructions on Screen During Portal Mode

While in portal mode, the device SHALL display a setup screen showing a title and a connection instruction that names the access point. The screen text SHALL be positioned within the round display's inscribed safe-area circle so it remains fully visible on the physically circular panel. The screen is static and requires no user interaction on the device itself; all input happens on the connecting phone or computer.

#### Scenario: Portal screen appears on first boot

- **WHEN** the device boots with no WiFi credentials and enters portal mode
- **THEN** the device display shows a title reading "Initial Setup"
- **AND** the device display shows an instruction reading "Please connect to OpenSonos-Setup to start setup."
- **AND** both lines of text are positioned within the round display's safe-area circle

#### Scenario: Screen remains visible throughout portal mode

- **WHEN** the device is in portal mode and a user is interacting with the captive portal on a connected device
- **THEN** the setup screen remains visible on the device display until the device reboots after provisioning
