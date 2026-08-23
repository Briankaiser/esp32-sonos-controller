# Tasks: improve-provisioning-wifi-picker

## 1. Network scan endpoint

- [x] 1.1 Add `/scan` HTTP GET handler in `portal.cpp`: switch radio to `WIFI_AP_STA`, call `WiFi.scanNetworks()` (synchronous, bounded to ~3s), build a JSON array of `{ssid, rssi}` objects deduplicated by SSID (keep highest RSSI per SSID), sorted strongest-first, respond with `Content-Type: application/json`
- [x] 1.2 After the scan response is sent, switch back to `WIFI_AP` mode (or leave in `WIFI_AP_STA` — confirm on hardware whether it affects portal reliability)

## 2. Portal page UI

- [x] 2.1 Replace the `<input name=ssid type=text>` in `kSetupPage` with a `<select id=ssid-picker>` placeholder and a hidden `<input name=ssid>` that carries the final value to the POST
- [x] 2.2 Add a collapsible/secondary `<input id=ssid-manual>` labeled "Hidden network — enter SSID manually" that, when filled, overrides the picker selection in the hidden field
- [x] 2.3 Add inline JS in `kSetupPage` to `fetch('/scan')` on page load, populate the `<select>` with options showing `SSID (signal%)` where signal% maps RSSI (−100 dBm → 0%, −50 dBm → 100%), sync the hidden `name=ssid` field on picker `change` and manual-field `input` events
- [x] 2.4 Handle the no-networks case: if scan returns an empty array, hide the picker and show only the manual field

## 3. Verification

- [x] 3.1 Build both PlatformIO envs (`crowpanel_21_round`, `ref_rect_stub`) — `portal.cpp` is compiled into both
- [x] 3.2 Flash and walk through setup on hardware: picker lists visible networks, mesh SSID appears once, selecting and submitting connects successfully
- [x] 3.3 Verify hidden-network path: disconnect from all visible APs (or use a hotspot with SSID broadcast off) and confirm manual field allows provisioning
- [ ] 3.4 `openspec validate --all --strict` clean
