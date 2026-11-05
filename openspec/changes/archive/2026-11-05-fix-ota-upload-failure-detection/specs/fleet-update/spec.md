## ADDED Requirements

### Requirement: Upload Failures Are Reported As Failures

The tool SHALL distinguish a failed upload from the expected reboot. If it cannot
establish a connection to a controller (refused, no route, or unreachable), or the
upload does not complete within the upload timeout, the tool SHALL report the
upload as failed with the reason and SHALL NOT treat it as the device rebooting.
Only a dropped or reset connection after the whole image has been sent SHALL be
treated as the expected reboot. The upload timeout SHALL be long enough for an
image to cross a slow WiFi link.

#### Scenario: Controller cannot be reached

- **WHEN** the tool cannot connect to a controller to upload firmware
- **THEN** it reports the upload as failed and names the connection error
- **AND** it does not report the controller as updated or wait for a reboot

#### Scenario: Upload does not complete in time

- **WHEN** a controller accepts the connection but the upload does not finish within the upload timeout
- **THEN** the tool reports the upload as failed with a timeout reason

#### Scenario: Connection drops after the image is sent

- **WHEN** the connection drops or resets after the full image has been sent
- **THEN** the tool treats it as the expected reboot and proceeds to verify the new version
