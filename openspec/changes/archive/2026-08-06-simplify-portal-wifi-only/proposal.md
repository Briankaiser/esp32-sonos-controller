## Why

The captive portal currently asks users to enter a home room UUID — an opaque internal identifier they would have to look up from a third-party app or router log. With `add-sonos-discovery` in place, the device can find speakers and show friendly names on-screen after first boot. The UUID field in the portal is therefore unnecessary and confusing; removing it makes first-time setup simpler. Home room selection is handled on the device after WiFi connects.

## What Changes

- Remove the home room UUID field from the captive portal HTML form and from the form POST handler in `src/services/portal.cpp`.
- Update `prefs::is_provisioned()` to return `true` when WiFi credentials (SSID + password) are stored, regardless of whether a home room UUID is present.
- Update `prefs::store_provisioning()` to no longer accept a home room UUID parameter (or make it optional with an empty default). The `prefs::home_room()` accessor and the underlying NVS key remain — they will be written later by the home room selection screen.
- Update the portal confirmation page text to not mention home room.
- No change to the OTA password, weather lat/lon, or any other portal field.

## Capabilities

### New Capabilities

### Modified Capabilities

- `device-provisioning`: The setup form no longer collects a home room UUID. Provisioning is complete when WiFi credentials are stored; home room is a post-boot configuration step.

## Impact

- `src/services/portal.cpp` and portal HTML — remove one form field and its POST handler branch.
- `src/services/prefs.h/.cpp` — `is_provisioned()` logic change; `store_provisioning()` signature change (remove `room` parameter).
- Any call site of `store_provisioning()` — only `portal.cpp` calls it today.
- No impact on `src/app/sonos/`, `src/bsp/`, `src/ui/`, or web services.
