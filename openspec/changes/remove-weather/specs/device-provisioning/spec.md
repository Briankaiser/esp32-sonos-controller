# Delta Spec: device-provisioning

## MODIFIED Requirements

### Requirement: Captive Portal Redirects to Setup Form

While in portal mode, the device SHALL redirect any HTTP request to its own setup page, regardless of the hostname or path requested. A device connecting to the `OpenSonos-Setup` AP SHALL reach the setup form without manually typing an IP address.

#### Scenario: DNS intercept redirects arbitrary hostname

- **WHEN** a client connected to `OpenSonos-Setup` sends a DNS query for any hostname
- **THEN** the device resolves that hostname to 192.168.4.1
- **AND** the client is directed to the device's HTTP server

#### Scenario: HTTP root returns setup form

- **WHEN** a browser on the `OpenSonos-Setup` AP issues an HTTP GET to any URL
- **THEN** the device responds with a setup form containing fields for: WiFi SSID, WiFi password, and OTA password

### Requirement: Setup Form Persists Credentials and Reboots

On submission of the setup form, the device SHALL validate that required fields are non-empty, persist all values to NVS, and reboot into normal operating mode. No credential SHALL be logged over serial or exposed in any HTTP response after submission.

#### Scenario: Valid form submission stores values and reboots

- **WHEN** a user submits the setup form with all required fields filled
- **THEN** the device stores WiFi SSID, WiFi password, and OTA password in NVS
- **AND** the device responds with a confirmation page stating that setup is complete
- **AND** the device reboots within 3 seconds of confirming

#### Scenario: Incomplete form is rejected

- **WHEN** a user submits the setup form with SSID or WiFi password left blank
- **THEN** the device responds with an error indicating which fields are missing
- **AND** no values are written to NVS
- **AND** the device remains in portal mode
