# Changelog

## [0.5.0] - 2026-11-28

### Added
- Widescreen (Video tab, the in-game Graphics page, `widescreen = on` in `video.ini`): a 16:9 picture with more of the scene at the sides, nothing stretched; the HUD moves to the screen's edges (the town clock, the dungeon's status, floor and attack gauge, the minimap), menus and full-screen pictures stay centred, fades and backgrounds cover the whole width.
- The in-game Graphics page also sets widescreen, anti-aliasing and texture smoothing.
- Travel (Options → Travel, F2 → Travel): warp to any town or area, to any floor of any dungeon (boss floors included), next to the floor's monsters; every georama part, the whole world map, and a story chapter select. A story dungeon's floors ahead of its allies say which chapter to advance to first. See [the cheat reference](docs/CHEATS.md).
- `--cheat-script FILE`: travel, story and cheat commands by frame for scripted test runs, with dialogue read through (`talk 1`), the game's state logged (`where`: map, floor, chapter, party, the active character, the monsters' HP, the game-cleared mark) and a map's own events (`townevent`).
- The whole story by script: [docs/playthrough.cheat](docs/playthrough.cheat) beats every story boss (Dran, Master Utan, La Saia, King's Curse, Minotaur Joe, the Dark Genie) through the game's own fights and defeat scenes, plays the ending and the credits and saves the cleared game, headless. Script commands for it: `event`, `kill`, `party`, `item`, `press`, `until`.

### Changed
- BozoEngine moves to its current master (c27ce41): the widescreen setting and picture shape it needs, among other engine work.

### Known limits
- Gameplay testing covers, headless and by script: every town and area map, every dungeon floor (the Demon Shaft's 100 included), every story boss, the ending and the cleared save; every ally switched in and fighting with real input; the cleared save opening the Demon Shaft through the world map. A complete playthrough by hand, the allies' own join scenes and georama town building are unverified, as is full visual parity.
- A few areas warped to outside their story show an empty or dark view (s11, s12, s14, s38, s39, s96).
- Linux is not qualified.

## [0.4.0] - 2026-11-24

### Added
- Save states: F5 saves the game at any moment into one of ten slots, F7 loads it back (F6 / Shift+F6 pick the slot, F2 lists them); also `--save-state` / `--load-state` on the command line.
- Hidden test runs: `--hidden` runs the game silently without showing its window, and `--capture-frame` then reads the game's offscreen picture. `--mute` runs without a sound device.

### Fixed
- Character shadows no longer vanish with anti-aliasing on (Video tab).
- Town water no longer turns into a mirror of the cliffs after the menu has been opened.

### Changed
- The controller is read natively: the game's pad code takes the PC controller's state directly and sends rumble back to it, with no PS2 pad library in between.
- Sound commands, sound-hardware settings (volumes, reverb) and sample uploads go straight to the game's natively compiled sound drivers, with no PS2 SIF (EE-to-IOP) transfer layer in between.
- Fast forward is the engine's: hold **`** or a gamepad's touchpad (rebindable in the Controls tab) at the Video tab's speed, 2x to 10x or unlimited. The Cheats menu keeps **Fast-forward always on**, at that same speed; its own speed row is gone.
- BozoEngine moves to its current master (d04e9d8): anti-aliasing, texture smoothing, F12 screenshots, save-state pictures, rumble strength, pause in the background and the settings window from a gamepad.
- The game's PS2 cache-maintenance calls are answered natively instead of by the emulated console kernel: kernel calls during play drop to none after boot.
- Leftover PS2 graphics and DMA setup calls at boot, loading screens and mode changes are retired, the renderer no longer reads GS register images, and the vblank wait and video-mode setup are DC's own: the game calls no PS2 graphics library function.

### Known limits
- Gameplay testing covers the new-game opening, Norune, Divine Beast Cave, Matataki, Georama, saving and loading, and save states; a complete story playthrough and full visual parity are unverified.
- Linux and widescreen are not qualified.

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
