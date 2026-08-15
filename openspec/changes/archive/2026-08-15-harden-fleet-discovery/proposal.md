## Why

Live-testing the fleet updater against two controllers exposed a real gap: a
single discovery scan is a point-in-time snapshot, so a controller that is
briefly unresponsive during that one pass (rebooting, busy, mid-DHCP) is silently
dropped — and the tool then reports and updates only the controllers it happened
to see, giving a false impression that it covered the whole fleet. In the test,
one of two controllers was missed on the first run for exactly this reason.

## What Changes

- Discovery scans the subnet more than once and unions the results, so a
  controller missed on one pass is still found on another. The number of passes
  is configurable (`--passes`, default 2). This makes "update all controllers"
  reliable rather than dependent on every device answering a single scan.

## Capabilities

### Modified Capabilities
- `fleet-update`: The controller-discovery requirement now specifies that the
  scan is repeated and results unioned, so a briefly-unresponsive controller is
  not silently missed.

## Impact

- **Host tooling**: `tools/fleet_update.py` — new `discover()` (multi-pass union
  by IP), a `--passes` option, and a unit test in `tools/test_fleet_update.py`.
- No firmware change.
