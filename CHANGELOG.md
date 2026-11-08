# Changelog

## [Unreleased]

### Changed
- BozoEngine moves to its current main (296ac6d), with the other ports' engine work since the release.

### Fixed
- Loading screens no longer wait forever on the newer engine: the game's own vsync interrupt handler is kept.

## [0.1.0] - 2026-11-08

### Added
- First Windows release with the TMB-style launcher, bundled offline build kit and English/French interface.
- Disc verification, resumable local preparation, Play, video settings, maintenance and verified release updates.
- In-game Cheats and Quit Game, including resource toggles, equipped weapon power, ABS multiplier, Gilda, active consumable quantities, movement speed, lighting freeze and safe inventory sizing.

### Fixed
- Sky feedback and brightness boundaries, stale texture bindings after weapon menus, sprite seams, shadow alpha and water refraction.
- Companion-menu load cancellation and expensive normal-play diagnostics.

### Known limits
- Active gameplay testing covers Norune, Divine Beast Cave and Matataki; a complete story playthrough and full visual parity are unverified.
- The release retains the gameplay-qualified engine revision. The newer-engine loading stall remains deferred.
- Linux and widescreen are not qualified.

[0.1.0]: https://github.com/TheBozos/DC_Recomp/releases/tag/v0.1.0
