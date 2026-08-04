# Changelog

All notable changes to this project are documented here.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and
this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

Each entry describes observable device behavior in one sentence and links to the
archived OpenSpec change that carries the rationale. Design discussion belongs in
`openspec/changes/archive/`, not here.

## [Unreleased]

### Added

- Firmware project scaffolding: PlatformIO + Arduino framework + LVGL 9.x for two board
  environments (`crowpanel_21_round` and `ref_rect_stub`), with a board-abstraction layer that
  ensures screen code never references hardware directly and a CI pipeline that enforces this.
  ([bootstrap-firmware-skeleton](openspec/changes/bootstrap-firmware-skeleton/proposal.md))

[Unreleased]: https://github.com/brian-kaiser/esp32-sonos-controller/compare/HEAD
