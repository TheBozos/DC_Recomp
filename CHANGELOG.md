# Changelog

## [Unreleased]

### Changed
- The controller is read natively: the game's pad code takes the PC controller's state directly and sends rumble back to it, with no PS2 pad library in between.
- Sound commands and sample uploads go straight to the game's natively compiled sound driver, with no PS2 SIF (EE-to-IOP) transfer layer in between.

## [0.3.0] - 2026-11-09

### Added
- Graphics page in the game's Options (controller): display mode, resolution, render scale and frame rate, applied at once and saved to `video.ini`.
- Cheats: weapon level-up, reveal floor map, fish always bite, item spawner, fast-forward (hold ` or the touchpad, or lock it on), damage multiplier, one-hit kills, no damage taken and a key finder on the dungeon map.

### Changed
- More of the game runs natively: the per-frame and loading-screen DMA transfers, the per-frame GS display swap and the startup GS sync-path calls are gone, and model, remake, shadow and cloth builds no longer reserve space for console packet data.
- BozoEngine moves to its current main (8a4ac05), which adds the calls the Graphics page uses.

### Fixed
- The game now reads `video.ini`: the launcher's display settings and the F2 Video tab apply (the game ignored them). `--fullscreen` still opens borderless over the monitor.

### Known limits
- Gameplay testing covers Norune, Divine Beast Cave, Matataki, Georama, saving and loading; a complete story playthrough and full visual parity are unverified.
- Linux and widescreen are not qualified.

## [0.2.0] - 2026-11-09

### Changed
- BozoEngine moves to its current main (296ac6d), with the other ports' engine work since 0.1.0.

### Fixed
- Frame-rate drops in heavy interiors (36-42 FPS in a house interior): a model's texture is checked once per draw instead of once per piece.
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
