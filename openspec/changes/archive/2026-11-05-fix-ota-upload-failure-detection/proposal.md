## Why

The fleet updater treated *any* connection error during the upload as "the device
is rebooting", including failing to connect at all. With the controller unreachable
(`No route to host`) it printed an upload success and only failed later at the
version check, which misleads the operator about what happened.

## What Changes

- An upload that never connected, or that timed out, is reported as a failed
  upload immediately, with the reason. Only a drop after the image was fully
  sent is treated as the expected reboot.
- The upload timeout default is raised so a slow WiFi link is not misreported as a failure.
- Tooling only; scope unchanged (no Home Assistant, no Sonos cloud/OAuth API).

## Capabilities

### New Capabilities

### Modified Capabilities
- `fleet-update`: upload failure classification.

## Impact

- `scripts/fleet_update.py` (`post_update`) and `scripts/test_fleet_update.py`.
- Internal tooling: no CHANGELOG entry.
