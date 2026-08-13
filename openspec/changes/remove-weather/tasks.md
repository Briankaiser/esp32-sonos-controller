# Tasks: remove-weather

## 1. Preferences

- [ ] 1.1 Remove the `kLat` / `kLon` NVS key constants from `prefs.cpp`.
- [ ] 1.2 Remove `prefs::lat()` and `prefs::lon()` (declarations in `prefs.h`, definitions in `prefs.cpp`).
- [ ] 1.3 Drop the `lat_val` / `lon_val` parameters from `prefs::store_provisioning()` (signature in `prefs.h`, body in `prefs.cpp` — remove the `putString(kLat/kLon, …)` writes).

## 2. Captive portal

- [ ] 2.1 Remove the weather latitude and longitude `<input>` fields from the setup form HTML in `portal.cpp`.
- [ ] 2.2 Remove lat/lon parsing from the submit handler and drop them from the `store_provisioning()` call.
- [ ] 2.3 Confirm required-field validation (SSID + password) and the confirmation/reboot flow are otherwise unchanged.

## 3. Verification & documentation

- [ ] 3.1 `grep -rin "weather\|kLat\|kLon\|::lat(\|::lon(" src/` returns nothing (no weather references remain).
- [ ] 3.2 Both PlatformIO envs build (`crowpanel_21_round`, `ref_rect_stub`).
- [ ] 3.3 **[hardware]** Fresh provisioning via the captive portal succeeds with only SSID / password / OTA fields; device connects and operates normally.
- [ ] 3.4 `openspec validate --all --strict` clean; CHANGELOG.md entry under `[Unreleased]`.
