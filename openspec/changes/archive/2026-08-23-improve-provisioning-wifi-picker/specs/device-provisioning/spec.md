# Delta Spec: device-provisioning

## MODIFIED Requirements

### Requirement: Setup Form Presents a Network Picker for WiFi SSID Selection

The setup form SHALL present a picker of WiFi networks currently visible to the device in place of a free-text SSID field. The picker SHALL reflect a network scan performed when the setup page is loaded, so results represent current radio conditions. Each visible network SHALL be shown once: when the same SSID is broadcast by more than one access point (as in mesh systems), only the entry with the highest signal strength SHALL appear. Each network entry SHALL display the SSID and a signal-strength indicator. A separate fallback text field SHALL be provided for entering an SSID that does not appear in the scan (hidden networks). Selecting a network from the picker SHALL populate the SSID for submission; entering text in the fallback field SHALL override the picker selection. The WiFi password and OTA password fields are unchanged.

#### Scenario: Setup page loads and shows visible networks

- **WHEN** a browser on the `OpenSonos-Setup` AP loads the setup page
- **THEN** the page displays a picker listing the SSIDs of all WiFi networks currently visible to the device
- **AND** each entry shows a signal-strength indicator alongside the SSID
- **AND** the list is ordered so stronger networks appear before weaker ones

#### Scenario: Mesh network appears once

- **WHEN** the device can see multiple access points broadcasting the same SSID
- **THEN** that SSID appears exactly once in the picker
- **AND** the signal-strength indicator reflects the strongest of those access points

#### Scenario: User selects a network from the picker

- **WHEN** the user selects a network from the picker and submits the form
- **THEN** the submitted SSID matches the entry the user selected
- **AND** the device stores it and proceeds as before

#### Scenario: User enters a hidden network name

- **WHEN** the user's network does not appear in the picker
- **THEN** the user can type the SSID in a fallback text field
- **AND** submitting with the fallback field filled stores that typed SSID

#### Scenario: No networks found

- **WHEN** the scan returns no visible networks
- **THEN** the picker is absent or shown as empty
- **AND** the fallback text field is displayed so the user can still enter an SSID manually
- **AND** setup can proceed normally via the fallback field
