# Delta Spec: device-provisioning

## ADDED Requirements

### Requirement: Device Can Re-Enter Provisioning On Demand During Normal Operation

Beyond first-boot detection, the device SHALL provide a way to re-enter provisioning while running normally, so a user whose WiFi has changed can recover without a separate factory-reset gesture. When this path is invoked and confirmed, the device SHALL reset its stored configuration to an unprovisioned state and reboot; on the subsequent boot it SHALL detect the absence of credentials and enter portal mode exactly as it does on first boot. The action SHALL be guarded by an explicit confirmation so a single stray input cannot reset the device. Any on-screen confirmation prompt SHALL be positioned within the round display's inscribed safe-area circle.

#### Scenario: Confirmed re-provisioning resets and reboots to portal

- **WHEN** the re-enter-provisioning action is invoked during normal operation and the user confirms
- **THEN** the device resets its stored configuration to an unprovisioned state
- **AND** the device reboots
- **AND** on the next boot it detects no stored credentials and starts the `OpenSonos-Setup` portal

#### Scenario: Unconfirmed re-provisioning is a no-op

- **WHEN** the re-enter-provisioning action is invoked but not confirmed
- **THEN** stored configuration is left unchanged
- **AND** the device continues normal operation without rebooting

#### Scenario: Confirmation prompt respects the safe area

- **WHEN** a re-provisioning confirmation prompt is shown on a round display
- **THEN** all of its text and controls fall within the inscribed safe-area circle
