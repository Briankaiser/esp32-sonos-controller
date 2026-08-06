## 1. Timing Constants

- [x] 1.1 In `src/services/prefs.h`, change `kResetCountdownStartMs` from `10000` to `5000`
- [x] 1.2 In `src/services/prefs.h`, change `kResetTotalMs` from `30000` to `15000`

## 2. Verification

- [x] 2.1 `pio run -e crowpanel_21_round` exits 0
- [x] 2.2 `pio run -e ref_rect_stub` exits 0
- [x] 2.3 `openspec validate --all --strict` is clean
- [x] 2.4 On hardware: hold encoder button — confirm countdown overlay appears at ~5 s and device factory-resets at ~15 s
