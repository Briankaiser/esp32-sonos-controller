## 1. Multi-pass discovery

- [x] 1.1 Add `discover(subnets, passes)` to `tools/fleet_update.py` — scans all subnets `passes` times and unions results by IP (freshest record wins).
- [x] 1.2 Use `discover()` in `main()`; add a `--passes` option (default 2).

## 2. Tests & verification

- [x] 2.1 Add a unit test (`test_discover_unions_across_passes`) proving a controller seen on only one pass is still discovered.
- [x] 2.2 `python3 tools/test_fleet_update.py` passes.
- [x] 2.3 `openspec validate --all --strict` is clean.
- [x] 2.4 Live-verified on two controllers: the first run missed one controller on a single-pass scan; with multi-pass discovery a re-run found both, updated the pending one, and skipped the already-updated one (both ended on the target build).
- [x] 2.5 Add a `CHANGELOG.md` note under `## [Unreleased]`.
