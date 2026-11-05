## 1. Implementation

- [x] 1.1 Classify upload failures by phase in `post_update` (connect failure and timeout are failures; drop after upload is accepted)
- [x] 1.2 Raise the default upload timeout
- [x] 1.3 Add unit tests: unreachable, timeout, drop-after-upload, success, rejection

## 2. Verification

- [x] 2.1 Both PlatformIO envs build (tooling-only change; firmware untouched)
- [x] 2.2 `openspec validate --all --strict` is clean
- [x] 2.3 Host unit tests pass (`python3 -m unittest scripts/test_fleet_update.py`, 27 tests)
- [x] 2.4 No CHANGELOG entry: internal tooling
