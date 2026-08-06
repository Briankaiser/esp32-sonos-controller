## 1. Prefs Layer

- [x] 1.1 In `src/services/prefs.h`, remove the `room` parameter from `store_provisioning()`; updated signature: `void store_provisioning(const String& ssid, const String& pass, const String& lat_val, const String& lon_val, const String& ota)`
- [x] 1.2 In `src/services/prefs.cpp`, remove the NVS write for the home room UUID from `store_provisioning()`; update `is_provisioned()` to return `!wifi_ssid().isEmpty() && !wifi_pass().isEmpty()` (no home room check)

## 2. Portal

- [x] 2.1 In `src/services/portal.cpp`, remove the home room UUID `<input>` field from the portal HTML form string
- [x] 2.2 In the portal form POST handler, remove extraction of the `room` field from the POST body and remove it from the `store_provisioning()` call; update the confirmation page text to omit mention of home room

## 3. Verification

- [x] 3.1 `pio run -e crowpanel_21_round` exits 0
- [x] 3.2 `pio run -e ref_rect_stub` exits 0
- [x] 3.3 `openspec validate --all --strict` is clean
- [x] 3.4 On hardware: factory reset, connect to `OpenSonos-Setup`, confirm the setup form has no home room field; submit with only WiFi credentials + OTA password + coordinates; confirm device reboots into normal operating mode
- [x] 3.5 Confirm `[boot] provisioned — starting normal init` appears in serial after provisioning without home room UUID
