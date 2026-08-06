## Why

The current factory reset requires a 30-second button hold (10 s silent, then 20 s countdown). In practice this is too long — users testing or re-provisioning the device find the wait unnecessarily tedious. 15 seconds total (5 s silent, 10 s countdown) is still long enough to prevent accidental resets while being meaningfully faster.

## What Changes

- `kResetCountdownStartMs`: 10 000 ms → 5 000 ms (countdown overlay appears after 5 s hold)
- `kResetTotalMs`: 30 000 ms → 15 000 ms (reset fires at 15 s total hold)

## Capabilities

### New Capabilities

### Modified Capabilities

- `device-provisioning`: The factory reset gesture timing changes — countdown appears at 5 s, reset fires at 15 s total.

## Impact

- Two `constexpr` values in `src/services/prefs.h`. No logic changes anywhere.
