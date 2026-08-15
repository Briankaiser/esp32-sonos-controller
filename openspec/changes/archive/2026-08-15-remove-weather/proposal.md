## Why

The captive-portal setup form collects a weather latitude and longitude, and the device persists them to NVS (`prefs::lat()` / `prefs::lon()`), but nothing ever reads them: there is no weather service, no weather UI, and no weather capability spec. It is dead configuration that clutters the setup form, adds fields a user must fill to get through provisioning, and forces any "reset just the WiFi" logic to preserve values that don't matter. Removing it simplifies provisioning and makes a full factory reset lossless (which is what lets `add-error-state-handling` use `factory_reset()` for its re-setup path).

## What Changes

- Remove the **weather latitude** and **weather longitude** fields from the captive-portal setup form.
- Stop persisting weather lat/lon to NVS; drop the `lat`/`lon` keys and the `prefs::lat()` / `prefs::lon()` accessors, and remove the lat/lon parameters from `prefs::store_provisioning()`.
- The setup form's required-field validation and everything else about provisioning is unchanged.

No external services are involved — this only removes unused local configuration; nothing was reading weather data from Home Assistant, the Sonos cloud, or any weather API in the first place.

## Capabilities

### New Capabilities
(none)

### Modified Capabilities
- `device-provisioning`: the setup form no longer offers or persists weather latitude/longitude; it collects only WiFi SSID, WiFi password, and OTA password.

## Impact

- **Affected code**: `src/services/prefs.h` / `src/services/prefs.cpp` (remove `kLat`/`kLon`, `lat()`, `lon()`, and the lat/lon params of `store_provisioning()`); `src/services/portal.cpp` (remove the two form inputs and the submit-handler wiring). No new dependencies.
- **No functional regression**: no code path reads weather values today, so removal changes no runtime behavior beyond the setup form's field set.
- **NVS**: previously stored `lat`/`lon` keys become orphaned on already-provisioned devices; harmless, and cleared on the next factory reset. No migration required.
