# fleet-update Specification

## Purpose
Host-side tooling that discovers OpenSonos controllers on the local network via
their `/status` endpoint and applies a firmware update to every controller
sequentially — with operator confirmation, per-device verification, and
abort-on-failure — so updating a fleet of controllers is one safe command
instead of a manual per-device `curl`.
## Requirements
### Requirement: Tool Discovers Controllers by Scanning the Local Subnet

The tool SHALL scan a local IPv4 subnet for OpenSonos controllers by issuing
`GET /status` to hosts on the subnet and treating a host as a controller only
when the response is valid JSON whose `device` field equals
`"opensonos-controller"`. The tool SHALL determine the subnet to scan
automatically (from the host's own network configuration) and SHALL also accept
an explicitly supplied subnet or CIDR. The scan SHALL probe hosts concurrently
so a typical `/24` completes in seconds, and SHALL use a short per-host timeout
so unreachable addresses do not stall the scan. Hosts that do not respond, that
respond with non-JSON, or whose `device` marker does not match SHALL be silently
ignored.

To avoid silently missing a controller that is only briefly unresponsive (for
example rebooting, busy, or mid-DHCP) during a single scan, the tool SHALL run
the scan more than once by default and SHALL treat the union of all passes as the
discovered set. The number of passes SHALL be configurable.

#### Scenario: Controllers on the subnet are discovered

- **WHEN** the tool scans a subnet on which one or more controllers are reachable
- **THEN** each host returning a `/status` JSON body with
  `device` == `"opensonos-controller"` is collected as a discovered controller
- **AND** its IP, home room name, and firmware version are recorded from the response

#### Scenario: Non-controllers are ignored

- **WHEN** a scanned host does not respond, returns non-JSON, or returns JSON
  whose `device` field is absent or not `"opensonos-controller"`
- **THEN** the tool does not treat that host as a controller
- **AND** the scan continues without error

#### Scenario: A briefly-unresponsive controller is still discovered

- **WHEN** a controller responds to `/status` on at least one discovery pass but
  not on another
- **THEN** the tool includes it in the discovered set via the union of passes
- **AND** it is not silently dropped from the fleet

### Requirement: Tool Lists Discovered Controllers Before Any Update

Before sending any firmware, the tool SHALL present the operator with the list
of discovered controllers, ordered by home room name and then by IP address.
Each listed controller SHALL show at least its home room name, its IP address,
and its current firmware version. When no controllers are found, the tool SHALL
report that none were found and exit without attempting any update.

#### Scenario: Discovered controllers are listed by name then IP

- **WHEN** the scan finds one or more controllers
- **THEN** the tool prints them ordered by home room name, then by IP address
- **AND** each line shows the home room name, IP address, and firmware version

#### Scenario: No controllers found

- **WHEN** the scan finds no controllers
- **THEN** the tool reports that no controllers were found
- **AND** the tool exits without sending any firmware

### Requirement: Tool Confirms Before Sending Updates

The tool SHALL obtain explicit operator confirmation after listing the
controllers and before sending firmware to the first device. Confirmation MAY be
given interactively at a yes/no prompt, or, for unattended runs, non-interactively
by passing an explicit confirmation flag (e.g. `--yes`). Absent confirmation — the
operator declines at the prompt, or the run is non-interactive and the flag was
not given — the tool SHALL exit without updating any device. The tool SHALL NOT
proceed to flash on a bare invocation with no confirmation.

#### Scenario: Operator confirms the update interactively

- **WHEN** the operator confirms at the yes/no prompt after reviewing the list
- **THEN** the tool proceeds to update the controllers

#### Scenario: Operator declines the update

- **WHEN** the operator declines at the confirmation prompt
- **THEN** the tool exits without sending firmware to any device

#### Scenario: Confirmation given non-interactively

- **WHEN** the operator passes the explicit confirmation flag (e.g. `--yes`)
- **THEN** the tool proceeds to update the controllers without prompting

#### Scenario: No confirmation available

- **WHEN** the run is non-interactive (no prompt possible) and no confirmation
  flag was given
- **THEN** the tool exits without sending firmware to any device

### Requirement: Tool Authenticates OTA Uploads Without Exposing the Password

The tool SHALL update each controller by sending the firmware image to that
device's authenticated OTA update endpoint, supplying the OTA password in the
`X-OTA-Password` request header. The tool SHALL read the OTA password from the
`OPENSONOS_OTA_PASSWORD` environment variable, and when that variable is unset
SHALL prompt for it interactively with the input hidden. The tool SHALL NOT
require the password to be passed as a command-line argument.

#### Scenario: Password taken from the environment

- **WHEN** `OPENSONOS_OTA_PASSWORD` is set in the environment
- **THEN** the tool uses its value as the `X-OTA-Password` header for every upload
- **AND** the tool does not prompt for the password

#### Scenario: Password prompted when absent

- **WHEN** `OPENSONOS_OTA_PASSWORD` is not set
- **THEN** the tool prompts the operator for the OTA password with input hidden
- **AND** uses the entered value for every upload

### Requirement: Tool Updates Controllers Sequentially With Verified Completion

The tool SHALL update the discovered controllers one at a time, never
concurrently. For each controller, after the upload completes the tool SHALL
tolerate the device dropping off the network as it reboots — a dropped
connection or reset immediately after a successful upload SHALL be treated as
expected, not as a failure. The tool SHALL then poll the device's `/status`
endpoint until the device responds again reporting the newly uploaded firmware
version, subject to a timeout. Between finishing one controller and beginning
the next, the tool SHALL wait at least 15 seconds as a settle floor.

#### Scenario: Device is verified back on the new version

- **WHEN** a controller has been sent new firmware and reboots
- **THEN** the tool tolerates the connection drop during reboot without erroring
- **AND** the tool polls `/status` until the device reports the new firmware version
- **AND** the tool waits at least 15 seconds before starting the next controller

#### Scenario: Updates never overlap

- **WHEN** the tool has more than one controller to update
- **THEN** it updates them strictly one at a time, beginning the next only after
  the current device is verified and the settle delay has elapsed

### Requirement: Tool Aborts the Run on a Failed Update

If a controller does not come back reporting the new firmware version within the
verification timeout, the tool SHALL stop the run immediately without updating
any further controllers, and SHALL report which controller failed and how many
controllers had already been updated successfully. The tool SHALL exit with a
non-zero status in this case.

#### Scenario: A device fails to return on the new version

- **WHEN** a controller does not report the new firmware version within the
  verification timeout after its update
- **THEN** the tool stops and does not update any remaining controllers
- **AND** the tool reports the failed controller and the count already updated
- **AND** the tool exits with a non-zero status

#### Scenario: A device already on the target version is skipped

- **WHEN** a discovered controller already reports the firmware version being
  rolled out
- **THEN** the tool may skip re-flashing that controller
- **AND** the tool reports it as already up to date

