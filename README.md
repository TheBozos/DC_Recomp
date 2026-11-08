<div align="center">

<img src="docs/branding/dc_banner.png" alt="Dark Cloud, native PC port by The Bozos" width="100%">

# DC_Recomp

**A native Windows port of *Dark Cloud* (NTSC-U, `SCUS-97111`), made by statically recompiling the game's PlayStation 2 code to C++ on BozoEngine.**<br>
The game's own MIPS R5900 code runs as a PC program, drawing and playing sound through native PC services.
BozoEngine replaces the PS2Recomp fork completely; there is no emulation fallback. A Linux build is planned.

![Platform](https://img.shields.io/badge/platform-Windows-1b8cff)
![Compiler](https://img.shields.io/badge/compiler-GCC%20%7C%20Clang-1b8cff)
![Based on](https://img.shields.io/badge/based%20on-BozoEngine-17a35b)
![Status](https://img.shields.io/badge/status-gameplay%20testing-e6a700)

</div>

> [!IMPORTANT]
> No game files are included: no ELF, overlays, IRX modules, data archives, or code generated from them.
> You need **your own copy** of the North American release, PS2 ID **`SCUS-97111`** (disc version 1.02). The port is
> built against that disc's files:
>
> | File | SHA-1 |
> |---|---|
> | `SCUS_971.11` | `d9965d6908a334a5dfc982f3384428ad42048d54` |
> | `TITLE.BIN` | `8da27dd6a4f8d2090216b17d5d64a0c142cfd6d5` |
> | `DUN.BIN` | `ab7e56a4eeb69e178638088de6e57857d48f1d98` |

## Status

**Playable Windows development build, under active gameplay testing.** Manual runs have reached Norune,
Divine Beast Cave exploration and combat, the Dran encounter, and region travel to Matataki Village.
Save/load, party and weapon menus, dialogue, native music and effects have been exercised.
A complete story playthrough and visual parity have not been established.

| Area | Current coverage |
|---|---|
| Boot, title and attract sequence | Checked on the native runtime |
| Norune | Loaded saves, movement, interiors, menus, dialogue and saving |
| Divine Beast Cave | Multiple floors, exploration, combat and Dran encounter |
| Matataki | Arrival through “Go to Other Area”; sky/camera comparisons |
| Rendering | Models, terrain, HUD, text, shadows, water and frame-copy effects draw natively |
| Menus | Original options plus controller-operated Cheats and Quit Game pages |
| Complete story, all companions, locations and effects | Still needs testing |
| Linux and widescreen | Not qualified |

Recent fixes address texture bindings after weapon customization, menu seams, shadow alpha, water
refraction, sky feedback and Matataki's sharp sky boundary. The player confirmed that boundary was gone
in the rebuilt run. A model sampler correction also targets Dran's mirrored face seam; its pixel regression
passed, but the full encounter still needs a player comparison.

The latest fullscreen build received positive player feedback after the cheat/menu additions.
Fullscreen improved presentation in the player's comparison. Performance in heavier scenes, including
Dran's house, still needs broader measurements. New-game/Georama routes, the full range of effects and
visual parity need further checks. See [the rendering follow-up](docs/NATIVE_RENDERING_FOLLOWUP.md)
for bounded reproductions and remaining questions.

## Requirements

- Windows 10/11 with [MSYS2](https://www.msys2.org/) (UCRT64, see below); Linux is not qualified yet
- CMake 3.21+, Ninja, GCC with C++20 support; development builds currently use MSYS2 GCC
- FFmpeg development libraries (`libavcodec`, `libavutil`) through `pkg-config`, or a compatible build supplied with `BOZO_FFMPEG_ROOT`
- `bsdtar` (libarchive) to extract the disc, Python 3; the offline texture converter uses only the standard library
- OpenGL 3.3 for the window; SDL3 and ImGui are fetched at pinned revisions by BozoEngine
- Your own copy of `SCUS-97111` (check the hashes above)

## Build and run

Use an MSYS2 **UCRT64** shell on Windows; MSVC is not supported for this port.

```bash
pacman -S --needed mingw-w64-ucrt-x86_64-{gcc,cmake,ninja,pkgconf,ffmpeg,libarchive,python} git
git clone --recurse-submodules https://github.com/TheBozos/DC_Recomp.git
cd DC_Recomp
scripts/extract_iso.sh "/path/to/Dark Cloud (USA).iso"
scripts/bench_run.sh --alone 20 "native build" scripts/build_runtime.sh Release \
    -DDC_ISO="D:/path/to/Dark Cloud (USA).iso"
scripts/bench_run.sh 120 "play" scripts/run.sh --fullscreen
```

Game runs take one shared benchmark slot; builds and timed measurements take the bench alone.
Use `scripts/bench_run.sh` when sharing the machine with other recomp projects. Close the game before
replacing its executable. A separate FFmpeg build can be selected with
`-DBOZO_FFMPEG_ROOT="D:/path/to/ffmpeg"` (containing `include/` and `lib/`).

Code generation, sound compilation and texture conversion are included in the full build.
`DC_WINDOW=OFF` builds only the headless runner. The validated static build imports Windows system/UCRT
DLLs; for dynamic builds, `scripts/stage_dlls.sh` stages required MSYS2 DLLs next to the executable.

## Playing, cheats and quitting

Open the game's main menu and choose **Options**. The entry page offers **Game Options** (the original
settings), **Cheats**, and **Quit Game**. Up/Down selects rows and scrolls the cheat list; Cross confirms
and Circle goes back. Quit Game asks for confirmation, with Cancel selected initially.
Save through the game's normal save menu first if you want to retain progress; quitting does not autosave.

| Cheat | Behaviour |
|---|---|
| Infinite health | Keeps party HP full |
| Infinite weapon durability | Keeps equipped weapons at full WHP and prevents wear/breakage |
| Infinite thirst | Keeps party thirst full and prevents dehydration |
| Combo stamina | Keeps the bottom-left dungeon attack/combo gauge full |
| Equipped weapon ATK | One-time edit, 0–999; select Apply weapon ATK |
| Weapon ABS multiplier | 1–16× positive combat rewards; normal full-ABS cap remains |
| Gilda | Set 0–65,535; select Apply Gilda |
| Consumable quantity | Set a quantity for one of the three active HUD slots; limited by bag capacity |
| Movement speed | 50–200% horizontal movement, retaining wall collision |
| Freeze time of day | Holds sky and outdoor lighting; gameplay/story timers continue |
| Inventory size | Set 1–100 slots; occupied entries cannot be hidden or discarded |

Cross toggles on/off rows. Numeric rows use Left/Right for one, L1/R1 for ten and L2/R2 for one hundred,
unless the row's hint says otherwise. Gilda uses 1/100/1000 steps; movement uses 5% steps.
Select the corresponding **Apply** row for ATK, Gilda, consumable quantity or inventory size.
Those edits apply when Options closes or gameplay resumes, and become part of a game save if you save
afterward. Toggle/multiplier settings are session-only: each launch starts with toggles off,
ABS at 1× and movement at 100%. See [the cheat reference](docs/CHEATS.md) for limits and validation.

**F2** opens native settings, including the same Cheats options. **F11** toggles fullscreen.
`--fullscreen` starts fullscreen; `--no-vsync` disables VSync for comparisons. The default is windowed
with VSync enabled. **F8** captures a frame when a capture directory is configured.

## Saves and data paths

The windowed runner uses BozoEngine's per-user memory-card directory. Existing `game/mc0` saves migrate
through its application-data service. `--card-dir <directory>` selects a separate root containing `mc0`;
copy the card directory before experiments. Headless runs use an in-memory card unless `--card-dir` is
supplied. Inventory edits reach the active card only when the game performs a save.

The build retains configured asset paths. `--elf` and `--iso` override executable/disc locations;
`DC_TEXTURES` selects a relocated native texture pack. The executable still needs the owner's game files
and converted assets.

## Checks and captures

```bash
scripts/bench_run.sh --alone 20 "build regression suites" \
    scripts/build_runtime.sh Release -DDC_ENGINE_TESTS=ON
scripts/bench_run.sh --alone 10 "regression suites" \
    ctest --test-dir build/bozo --output-on-failure
scripts/bench_run.sh --alone 5 "bounded native capture" \
    scripts/run.sh --frames 1400 --capture-dir build/captures --capture-frame 1200 --profile-io
```

Suites cover instructions, runtime/kernel services, ELF/overlay decoding, native libraries/audio,
compiled game boundaries, cheats, menu lifecycle and GPU readback. GPU checks require OpenGL 3.3.
CI uses game-free engine fixtures and offline-tool checks; retail-route tests require the owner's disc.
`scripts/engine_tests.sh` runs standalone headless engine checks.

`--frames 0` runs until shutdown; `--frames N` bounds a run. `--wav <file>` records sound.
`--pad-script <file>` accepts entries such as `1200:START/6,1500:CROSS/6`; `--live-pad <file>` reloads
changed input text relative to the current frame. `--capture-dir` and repeatable `--capture-frame`
select native captures. `--profile-io` reports device-register accesses after a bounded run.
Normal play leaves expensive diagnostic recording disabled.

Recent results live in [NATIVE_RENDERING_FOLLOWUP.md](docs/NATIVE_RENDERING_FOLLOWUP.md).
[BOZO_MIGRATION.md](docs/BOZO_MIGRATION.md) retains migration/qualification details and
[RECOMP_NOTES.md](docs/RECOMP_NOTES.md) the earlier bring-up history. Synthetic checks and short
captures do not establish a full-game pass.

## Native architecture

The main R5900 executable and both code overlays are statically recompiled to C++. Native adapters
build draws from the game's models, textures, camera, lights and drawing routines. BozoEngine supplies
OpenGL rendering, native SDK services, input, cards, movie decoding and audio mixing.

There is no GS memory/renderer, runtime GIF packet renderer, VIF/VU1 execution, IOP interpreter,
SPU2/SIO2 device backend or emulation fallback. Some compiled routines still construct unused
console-format buffers; the renderer does not consume them. Logical addresses, thread/timer contracts
and completion acknowledgements remain as SDK compatibility services. Unsupported native calls/module
code and retired device accesses fail explicitly.

`TITLE.BIN` and `DUN.BIN` share a load slot with separate generated function tables selected by resident
image identity. Five sound IRX libraries are relocated and compiled offline; textures are converted
offline. FFmpeg is required for movies. Game-specific formats and hook choices stay in DC_Recomp;
reusable engine services belong in BozoEngine.

The dispatch audit remains conditional: valid arena/cursor preconditions bound known pack copies,
but mutable metadata, shared cursor/header writes and nested loader effects remain under investigation.
Controlled tests do not prove immutable DUN dispatch tables or complete live dispatch coverage.
See the migration notes for the `LoadPackData`/`ReadInfo` findings.

| Path | Contents |
|---|---|
| `BozoEngine/` | Sole engine submodule: native recompiler/runtime |
| `runtime/` | DC render, audio, frame, menu and cheat adapters |
| `recomp/` | Function maps and guarded native adaptations |
| `scripts/` | Extraction, generation, builds, runs, tests and bench lock |
| `tools/` | Offline preparation/conversion and analysis helpers |
| `tests/` | Game-free fixtures and owner-disc compiled-boundary regressions |
| `docs/` | Follow-ups, historical notes, screenshots and branding |

Game-derived code, disc files, generated assets, saves and validation captures stay local and ignored.
No game executable, disc archives or generated game code are distributed.

## Screenshots


Historical bring-up captures from before the native renderer and BozoEngine migration. They show the earlier
progress and are not evidence of the current build; current validation is in
[the rendering follow-up](docs/NATIVE_RENDERING_FOLLOWUP.md).

| | | |
|---|---|---|
| ![The intro's castle](docs/screenshots/intro-castle.png) | ![The Dark Genie](docs/screenshots/genie.png) | ![The title menu](docs/screenshots/title-logo.png) |
| ![The desk narration](docs/screenshots/opening-desk.png) | ![In a dungeon](docs/screenshots/dungeon-scene.png) | ![Toan and the Fairy King](docs/screenshots/fairy-king.png) |
| ![Toan's house](docs/screenshots/toan-house.png) | ![The forest](docs/screenshots/forest-scene.png) | ![New game: the name entry](docs/screenshots/register-name.png) |
| ![The Dark Shrine](docs/screenshots/shrine-soldiers.png) | ![Colonel Flag](docs/screenshots/colonel-flag.png) | ![The urn](docs/screenshots/shrine-urn.png) |
| ![Opening dialogue](docs/screenshots/opening-dialogue.png) | ![The Dark Genie rises](docs/screenshots/genie-rises.png) | ![Norune at night](docs/screenshots/norune-night.png) |
| ![Toan at home](docs/screenshots/norune-house.png) | ![Norune's festival](docs/screenshots/norune-festival.png) | ![A cutscene dialogue](docs/screenshots/cutscene-balloon.png) |

## Credits

- **The Bozos**: boubou666 and Arzaroth, who make this port and BozoEngine
  ([RecompWatcher](https://thebozos.github.io/RecompWatcher/) shows our projects).
- [BozoEngine](https://github.com/TheBozos/BozoEngine), The Bozos' MIT recompiler and native runtime, now the port's only engine.
- [PS2Recomp](https://github.com/ran-j/PS2Recomp) by ran-j and contributors (GPL), on which the earlier port was built;
  it remains credited in the project's history.
- The team's other recompilations, including *Twisted Metal: Black*, *The Lord of the Rings: The Third Age*,
  *Jade Cocoon 2*, *Valkyrie Profile 2: Silmeria* and *Drakengard*, which share native development work.
- *Dark Cloud* © Sony Computer Entertainment, developed by Level-5. This is an unofficial fan project, not affiliated
  with or endorsed by them.
