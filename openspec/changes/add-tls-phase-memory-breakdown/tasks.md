## 1. Implementation

- [x] 1.1 Add `diag::FetchProbe` (kinds art_tls/art_plain/lyrics; phases connect/first_read/done) with static storage
- [x] 1.2 Mark the phases in `src/app/sonos/art.cpp` and `src/app/lyrics/service.cpp`
- [x] 1.3 Serve `fetch_mem` on `/status`
- [x] 1.4 Log `fetch_mem` in `scripts/heap_log.py` and show it in the report
- [ ] 1.5 Update the Kitchen investigation doc with the findings

## 2. Verification

- [x] 2.1 Both PlatformIO envs build (`crowpanel_21_round`, `ref_rect_stub`)
- [x] 2.2 `openspec validate --all --strict` is clean
- [x] 2.3 Hardware check: `/status` shows plausible `fetch_mem` after art and lyrics fetches
- [x] 2.4 Add a `CHANGELOG.md` entry under `## [Unreleased]` only if the field is user-visible
