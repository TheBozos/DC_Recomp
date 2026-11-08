# Changelog

## [0.2.0] - 2026-11-08

### Changed
- BozoEngine moves to its current main (296ac6d), with the other ports' engine work since 0.1.0.

### Fixed
- Frame-rate drops in heavy interiors (36-42 FPS in a Matataki house): a model's texture is checked once per draw instead of once per piece.
- Loading screens no longer wait forever on the newer engine: the game's own vsync interrupt handler is kept.

### Known limits
- Gameplay testing covers Norune, Divine Beast Cave, Matataki, Georama, saving and loading; a complete story playthrough and full visual parity are unverified.
- Linux and widescreen are not qualified.

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

[0.2.0]: https://github.com/TheBozos/DC_Recomp/releases/tag/v0.2.0
[0.1.0]: https://github.com/TheBozos/DC_Recomp/releases/tag/v0.1.0
