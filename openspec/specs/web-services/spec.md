## Purpose

Exposes an HTTP server on the device's home WiFi network for two purposes: capturing the current display as a BMP image for visual review, and uploading new firmware over the air without a USB connection.

## Requirements

### Requirement: Device Connects to Home WiFi on Boot

After confirming provisioned credentials are present, the device SHALL connect to the home WiFi network using the SSID and password stored in NVS. The device SHALL log its assigned IP address to serial once the connection is established, so the operator can locate the HTTP endpoints without a network scan.

#### Scenario: Successful WiFi connection logs IP

- **WHEN** the device boots with valid WiFi credentials in NVS
- **THEN** the device connects to the home WiFi network in station mode
- **AND** the device prints its assigned IP address to the serial console

#### Scenario: WiFi connection failure is reported

- **WHEN** the device attempts to connect to the home network and does not receive an IP within 15 seconds
- **THEN** the device prints a connection-failed message to the serial console
- **AND** the device continues booting without the HTTP server

### Requirement: Screenshot Endpoint Returns Current Display as BMP

The device SHALL expose `GET /screenshot` on port 80. A successful response SHALL contain the full contents of the active LVGL display rendered as a valid 24-bit BMP image. The endpoint SHALL require no authentication.

#### Scenario: Screenshot returns BMP with correct content type

- **WHEN** a client on the home network issues `GET /screenshot`
- **THEN** the device responds with HTTP 200
- **AND** the `Content-Type` header is `image/bmp`
- **AND** the response body is a valid BMP file representing the current display contents

#### Scenario: Concurrent screenshot request is serialized

- **WHEN** a second `GET /screenshot` request arrives while a screenshot is already being captured
- **THEN** the second request waits for the first to complete before the capture begins
- **AND** both requests eventually receive a valid BMP response

### Requirement: OTA Update Endpoint Accepts Authenticated Firmware Upload

The device SHALL expose `POST /update` on port 80. The endpoint SHALL require the correct OTA password, supplied in the `X-OTA-Password` request header, before accepting a firmware binary. On a valid upload the device SHALL apply the firmware and reboot. On an incorrect password the device SHALL respond with HTTP 401 and reject the upload without writing any data.

#### Scenario: Authenticated upload applies firmware and reboots

- **WHEN** a client sends a `POST /update` with the correct `X-OTA-Password` header and a valid firmware binary body
- **THEN** the device writes the firmware
- **AND** the device responds with HTTP 200 and a confirmation message
- **AND** the device reboots into the new firmware

#### Scenario: Wrong password is rejected before any write

- **WHEN** a client sends a `POST /update` with an incorrect `X-OTA-Password` header
- **THEN** the device responds with HTTP 401
- **AND** no firmware data is written to flash

#### Scenario: Malformed upload is rejected with 400

- **WHEN** a client sends a `POST /update` with the correct password but a body that is not a valid firmware image
- **THEN** the device responds with HTTP 400
- **AND** the device remains on the current firmware
