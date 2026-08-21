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

For each discovered controller the tool SHALL record, from its `/status`
response, at least its IP address, home room name, firmware version, and board
identifier (the `board` field). When a controller's response omits the `board`
field — a legacy controller predating board reporting — the tool SHALL record
its board identifier as unknown rather than dropping the controller from the
discovered set.

To avoid silently missing a controller that is only briefly unresponsive (for
example rebooting, busy, or mid-DHCP) during a single scan, the tool SHALL run
the scan more than once by default and SHALL treat the union of all passes as the
discovered set. The number of passes SHALL be configurable.

#### Scenario: Controllers on the subnet are discovered

- **WHEN** the tool scans a subnet on which one or more controllers are reachable
- **THEN** each host returning a `/status` JSON body with
  `device` == `"opensonos-controller"` is collected as a discovered controller
- **AND** its IP, home room name, firmware version, and board identifier are
  recorded from the response

#### Scenario: A controller that does not report a board is recorded as unknown

- **WHEN** a discovered controller's `/status` response has no `board` field
- **THEN** the tool still collects it as a discovered controller
- **AND** records its board identifier as unknown

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

### Requirement: Tool Updates Only Controllers Matching the Firmware's Target Board

The tool SHALL determine the target board of the firmware image being rolled out
by reading a board identifier embedded in the image itself, so the image is
self-describing and no companion file is required. When the image carries a board
identifier, the tool SHALL flash only those discovered controllers whose reported
board identifier equals the image's target board. A controller whose board does
not match, and a controller that does not report a board at all, SHALL be
excluded from the set of controllers to update. Excluded controllers SHALL be
reported to the operator with the reason for exclusion — never silently dropped —
so the operator can see that a device was intentionally skipped rather than
missed.

The tool SHALL provide an explicit override flag (e.g. `--force`) that disables
board matching and updates every discovered controller regardless of board, for
the deliberate case where the operator accepts responsibility for a cross-board
flash.

When the firmware image carries no board identifier — a legacy image built before
board embedding — the tool SHALL fall back to its prior behavior of not filtering
by board, and SHALL warn the operator that board matching is unavailable for this
image. This fallback SHALL NOT make the tool less safe than a board-unaware
version: version-based selection and all confirmation, verification, and
abort-on-failure behavior continue to apply.

Board matching SHALL be applied when selecting which controllers to update, in
addition to (not instead of) skipping controllers already on the target firmware
version.

#### Scenario: Matching-board controllers are updated

- **WHEN** the firmware image declares a target board and one or more discovered
  controllers report the same board
- **THEN** those controllers are eligible for update (subject to version and
  confirmation)

#### Scenario: Mismatched-board controllers are excluded and reported

- **WHEN** a discovered controller reports a board different from the image's
  target board
- **THEN** the tool excludes it from the controllers to update
- **AND** reports it as skipped, stating that its board does not match the image

#### Scenario: Unknown-board controllers are excluded and reported

- **WHEN** a discovered controller does not report a board and the image declares
  a target board
- **THEN** the tool excludes it from the controllers to update
- **AND** reports it as skipped, stating that its board could not be determined

#### Scenario: Force overrides board matching

- **WHEN** the operator passes the override flag (e.g. `--force`)
- **THEN** the tool disables board matching
- **AND** every discovered controller is eligible for update regardless of board

#### Scenario: Legacy image without a board identifier falls back safely

- **WHEN** the firmware image carries no embedded board identifier
- **THEN** the tool does not filter controllers by board
- **AND** warns the operator that board matching is unavailable for this image
- **AND** continues to apply version selection, confirmation, verification, and
  abort-on-failure exactly as before

