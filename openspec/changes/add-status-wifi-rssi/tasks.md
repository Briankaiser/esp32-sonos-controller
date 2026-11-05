## 1. Implementation

- [ ] 1.1 Serve `wifi_rssi` on `/status` (integer dBm, `null` when not connected)
- [ ] 1.2 Log `wifi_rssi` in `scripts/heap_log.py` and show it in the report
- [ ] 1.3 Add a line to the Kitchen investigation doc noting RSSI is now available

## 2. Verification

- [ ] 2.1 Both PlatformIO envs build (`crowpanel_21_round`, `ref_rect_stub`)
- [ ] 2.2 `openspec validate --all --strict` is clean
- [ ] 2.3 Hardware check: `/status` on both controllers returns a plausible negative `wifi_rssi`
- [ ] 2.4 Add a `CHANGELOG.md` entry under `## [Unreleased]`
