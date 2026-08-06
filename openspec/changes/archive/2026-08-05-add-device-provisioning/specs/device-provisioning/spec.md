## Purpose

Enables a factory-fresh device to self-configure over a captive portal without any credentials baked into firmware, and persists all device settings durably across reboots and power cycles.

## ADDED Requirements

### Requirement: Device Detects First Boot and Enters Portal Mode

On first power-up (or after a factory reset), the device SHALL detect that no WiFi credentials are stored and enter provisioning mode automatically. It SHALL NOT attempt to connect to a WiFi network when no credentials are present.

#### Scenario: First boot starts portal

- **WHEN** the device boots with no WiFi credentials in NVS
- **THEN** the device starts a WiFi access point named `OpenSonos-Setup`
- **AND** the device starts an HTTP server on 192.168.4.1
- **AND** no connection to any external WiFi network is attempted

#### Scenario: Previously provisioned device skips portal

- **WHEN** the device boots with valid WiFi credentials already stored in NVS
- **THEN** the device does not start a WiFi access point
- **AND** the device proceeds directly to WiFi station mode using stored credentials

### Requirement: Captive Portal Redirects to Setup Form

While in portal mode, the device SHALL redirect any HTTP request to its own setup page, regardless of the hostname or path requested. A device connecting to the `OpenSonos-Setup` AP SHALL reach the setup form without manually typing an IP address.

#### Scenario: DNS intercept redirects arbitrary hostname

- **WHEN** a client connected to `OpenSonos-Setup` sends a DNS query for any hostname
- **THEN** the device resolves that hostname to 192.168.4.1
- **AND** the client is directed to the device's HTTP server

#### Scenario: HTTP root returns setup form

- **WHEN** a browser on the `OpenSonos-Setup` AP issues an HTTP GET to any URL
- **THEN** the device responds with a setup form containing fields for: WiFi SSID, WiFi password, home room UUID, weather latitude, weather longitude, and OTA password

### Requirement: Setup Form Persists Credentials and Reboots

On submission of the setup form, the device SHALL validate that required fields are non-empty, persist all values to NVS, and reboot into normal operating mode. No credential SHALL be logged over serial or exposed in any HTTP response after submission.

#### Scenario: Valid form submission stores values and reboots

- **WHEN** a user submits the setup form with all required fields filled
- **THEN** the device stores WiFi SSID, WiFi password, home room UUID, latitude, longitude, and OTA password in NVS
- **AND** the device responds with a confirmation page stating that setup is complete
- **AND** the device reboots within 3 seconds of confirming

#### Scenario: Incomplete form is rejected

- **WHEN** a user submits the setup form with SSID or WiFi password left blank
- **THEN** the device responds with an error indicating which fields are missing
- **AND** no values are written to NVS
- **AND** the device remains in portal mode

### Requirement: Settings Are Persisted Across Reboots

All values stored during provisioning SHALL survive a normal power cycle. The device SHALL read these values from NVS on every subsequent boot and make them available to all subsystems before any network operations begin.

#### Scenario: Stored credentials survive reboot

- **WHEN** a device that was provisioned is rebooted
- **THEN** the device reads WiFi SSID, password, home room UUID, latitude, longitude, and OTA password from NVS
- **AND** the device connects to WiFi using the stored credentials without re-entering the portal

#### Scenario: Individual setting can be retrieved by key

- **WHEN** any subsystem requests the home room UUID, OTA password, or weather coordinates
- **THEN** the value previously stored during provisioning is returned
- **AND** if a value was never stored, an empty/default sentinel is returned so the caller can handle it explicitly

### Requirement: Factory Reset Is Triggered by a 30-Second Encoder Button Hold

The device SHALL initiate a factory reset when the rotary encoder button is held continuously for 30 seconds during normal operation. The device SHALL display a countdown overlay on screen once the button has been held for 10 seconds, so the user has visible confirmation of the pending reset and a chance to abort by releasing the button. Releasing the button at any point before 30 seconds SHALL cancel the reset with no side effects. This path SHALL be accessible without a WiFi connection or any external tooling.

#### Scenario: Short press does not trigger reset

- **WHEN** the encoder button is pressed and released in under 10 seconds
- **THEN** no countdown overlay appears
- **AND** no NVS values are modified

#### Scenario: 10-second hold shows countdown overlay

- **WHEN** the encoder button has been held continuously for 10 seconds
- **THEN** a countdown overlay appears on screen showing the remaining seconds until reset
- **AND** the overlay updates each second as the hold continues

#### Scenario: Releasing during countdown cancels reset

- **WHEN** the encoder button is released after the countdown overlay has appeared but before 30 seconds have elapsed
- **THEN** the countdown overlay is dismissed
- **AND** no NVS values are modified
- **AND** the device continues normal operation

#### Scenario: Full 30-second hold erases NVS and reboots to portal

- **WHEN** the encoder button is held continuously for 30 seconds
- **THEN** all provisioning values are erased from NVS
- **AND** the device reboots
- **AND** on the next boot the device enters portal mode as if factory-fresh
