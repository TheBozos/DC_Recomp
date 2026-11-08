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

**Playable Windows port, under active gameplay testing.** Checked routes include Norune,
Divine Beast Cave exploration and combat, the Dran encounter, and region travel to Matataki Village.
Movement, dialogue, party/weapon menus, save/load, native music and effects have been exercised.
A complete story playthrough and full visual parity remain unverified.

| Milestone | |
|---|:---:|
| Map the main ELF and both overlays from the game's own symbol table | ✅ |
| Recompile and link with BozoEngine; remove the PS2Recomp build dependency | ✅ |
| Boot through native SDK services and reads from the owner's disc | ✅ |
| Title screen, attract sequence and native 3D rendering | ✅ |
| Norune: movement, interiors, dialogue, HUD and menus | ✅ |
| Divine Beast Cave: multiple floors, exploration, combat and the Dran encounter | ✅ |
| Travel to Matataki through “Go to Other Area” | ✅ |
| Memory-card saving, loading and restart with saved progress | ✅ |
| Native music and effects | ✅ |
| Controller-operated Cheats and Quit Game inside Options; F2 settings remain available | ✅ |
| New-game opening and Georama coverage on BozoEngine | 🔧 |
| Complete story, all companions, locations and effects | 🔧 |
| Linux build | ⏳ |
| Widescreen | ⏳ |

**Native port.** Models, textures, camera, lights and drawing routines feed native PC draws.
BozoEngine supplies the runtime, native SDK services, renderer and mixer.

| Layer | |
|---|:---:|
| Main EE functions and both overlays statically recompiled to C++ | ✅ |
| Five sound libraries relocated and recompiled offline, without a firmware bootstrap | ✅ |
| Pad and memory-card calls answered through native SDK services | ✅ |
| Compiled sound drivers and native libsd/mixer; no SPU2 emulator | ✅ |
| Frame loop paced by the PC's clock | ✅ |
| OpenGL renderer; no GS memory or GS renderer | ✅ |
| Models, shadows and cloth drawn from game data; no VIF/VU1 execution | ✅ |
| Disc textures converted offline into native images | ✅ |
| HUD, menus, text, frame copies, water refraction and depth of field drawn natively | ✅ |
| Full range of sprites, particles and scene effects checked against the original | 🔧 |

✅ works · 🔧 in progress · ⏳ not reached yet

Native thread/timer/file contracts and transfer-completion acknowledgements remain as SDK compatibility
services. Some compiled routines still build unused console-format buffers; the renderer does not consume
them. There is no runtime GIF packet renderer, IOP interpreter or emulation fallback.

Still being checked: heavier-scene performance (including Dran's house), the full range of effects and
menus, and broader story/Georama coverage. The Dran face sampler correction passes its pixel regression;
the full encounter still needs a player comparison. Matataki's reported sharp sky boundary was corrected
and confirmed in the player's comparison.

[docs/NATIVE_RENDERING_FOLLOWUP.md](docs/NATIVE_RENDERING_FOLLOWUP.md) records recent fixes, playtest
coverage and the retained engine pin. [docs/BOZO_MIGRATION.md](docs/BOZO_MIGRATION.md) retains the migration
and conditional dispatch audit; [docs/RECOMP_NOTES.md](docs/RECOMP_NOTES.md) records earlier bring-up work.

## Screenshots


Historical bring-up captures from before the native renderer and BozoEngine migration. They show the earlier
progress and are not evidence of the current build; current validation is in
[docs/NATIVE_RENDERING_FOLLOWUP.md](docs/NATIVE_RENDERING_FOLLOWUP.md).

| | | |
|---|---|---|
| ![The intro's castle](docs/screenshots/intro-castle.png) | ![The Dark Genie](docs/screenshots/genie.png) | ![The title menu](docs/screenshots/title-logo.png) |
| ![The desk narration](docs/screenshots/opening-desk.png) | ![In a dungeon](docs/screenshots/dungeon-scene.png) | ![Toan and the Fairy King](docs/screenshots/fairy-king.png) |
| ![Toan's house](docs/screenshots/toan-house.png) | ![The forest](docs/screenshots/forest-scene.png) | ![New game: the name entry](docs/screenshots/register-name.png) |
| ![The Dark Shrine](docs/screenshots/shrine-soldiers.png) | ![Colonel Flag](docs/screenshots/colonel-flag.png) | ![The urn](docs/screenshots/shrine-urn.png) |
| ![Opening dialogue](docs/screenshots/opening-dialogue.png) | ![The Dark Genie rises](docs/screenshots/genie-rises.png) | ![Norune at night](docs/screenshots/norune-night.png) |
| ![Toan at home](docs/screenshots/norune-house.png) | ![Norune's festival](docs/screenshots/norune-festival.png) | ![A cutscene dialogue](docs/screenshots/cutscene-balloon.png) |

## Goal

A fully playable *Dark Cloud* as a native PC program, from the opening to the ending, drawing and playing
sound directly from the game's data. Accuracy and complete gameplay coverage come first; Linux,
widescreen and additional presentation options follow.

## How it works

```
disc files (game/) ──► tools/dc_bozo_prepare.py ──► resident ELF + TITLE/DUN overlay ELFs
                                           │
                        BozoEngine's bozo-recomp ──► native C++ in build/bozo/dc_gen/
   five sound IRX ──► offline relocation + strict native translation ──► build/bozo/dc_audio_gen/
   disc textures ──► tools/dc_texconv.py ──► build/bozo/textures/dc_textures.bin
                                           │
                 CMakeLists.txt ──► BozoEngine + generated code + DC's own adapters
                                           │
   dc_native.cpp: models, lighting, shadows, cloth, menus and HUD ──► native OpenGL renderer ─┐
   dc_audio.cpp: compiled drivers + native libsd ──► native mixer                           ├─► dc.exe
   dc_port.cpp: native frame boundary; SDK files, cards and pads ──► native host services ───┘
```

- The retail executable keeps its CodeWarrior symbols. `tools/symtab_map.py` and the retained function maps define
  the resident code and overlays; no Ghidra pass is needed. The offline preparation checks all three disc hashes.
- `TITLE.BIN` and `DUN.BIN` are `MWo3` modules sharing `0x01DABD00`. Each has its own generated symbols and function
  table; native dispatch checks the resident header before selecting it, including resume entries.
- `runtime/dc_native.cpp` translates the game's MDT models, RenderInfo camera/lights, textures and drawing calls
  into native draws. It does not read console graphics packets or run VU1 microcode.
- `LIBSD`, `SDRDRV`, `MODMIDI`, `MODHSYN` and `EZMIDI` are relocated and compiled offline. Their SDK calls use
  reusable native services; libsd uses the native mixer. Pad/card modules are replaced at their SDK interfaces.
- The top-level build generates code and converts textures automatically, then links the windowed `dc` and
  `dc-headless` runners. Game-derived output stays in the ignored build directory.

[BozoEngine](https://github.com/TheBozos/BozoEngine) is the sole engine submodule. Game-specific addresses, formats,
hook choices and module layouts stay in DC_Recomp; engine changes must be generic and reusable by other recompiles.

## For players (Windows)

Download **`DC_Recomp-0.1.0-bozo.zip`** from [Releases](https://github.com/TheBozos/DC_Recomp/releases/latest),
check its `.sha256`, and extract it to a writable folder such as `D:\Games\DC_Recomp_Bozo`.
Open **DC Launcher.exe**, select your own USA disc image (`.iso` or `.bin`/`.cue`), click **Prepare the game**,
then **Play**. Preparation runs locally with the bundled offline tools; no developer software is needed.
It can be cancelled and resumed. The launcher follows TMB_Recomp's layout and supports English and French.

You need Windows 10 version 1903 or newer (or Windows 11), an AVX2/FMA CPU, OpenGL 3.3 and about **10 GB free**.
Only the unmodified `SCUS-97111` version 1.02 disc is supported; the executable and both overlays are verified.
The unsigned launcher may trigger Windows SmartScreen. Game files and generated game code are never distributed.

Use the launcher's **Settings** for display options, logs, maintenance and updates. **F2** opens the game's
settings. Saves, controls, video settings and cheats are stored in `%APPDATA%\DC_Recomp` and survive launcher
updates. Existing development saves are kept separately; copy only the desired card files into that directory.
The in-game pause menu includes **Cheats** and **Quit Game**. Updates offer a rebuild when the source changes.
See [CHANGELOG.md](CHANGELOG.md) for release changes and the checked routes above for current coverage.

## Requirements

- Windows 10/11 with [MSYS2](https://www.msys2.org/) (UCRT64, see below); Linux is not qualified yet
- CMake 3.21+, Ninja, GCC with C++20 support; development builds currently use MSYS2 GCC
- FFmpeg development libraries (`libavcodec`, `libavutil`) through `pkg-config`, or a compatible build supplied with `BOZO_FFMPEG_ROOT`
- `bsdtar` (libarchive) to extract the disc, Python 3; the offline texture converter uses only the standard library
- OpenGL 3.3 for the window; SDL3 and ImGui are fetched at pinned revisions by BozoEngine
- Your own copy of `SCUS-97111` (check the hashes above)

## Build and run

The following is the developer workflow; players can use the launcher above.

```bash
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

### Windows

Run the scripts in an MSYS2 **UCRT64** shell; MSVC is not supported for this port.

```bash
pacman -S --needed mingw-w64-ucrt-x86_64-{gcc,cmake,ninja,pkgconf,ffmpeg,libarchive,python} git
```

### Controls and settings

**F2** opens native settings, including the same Cheats controls. **F11** toggles fullscreen.
`--fullscreen` starts fullscreen; `--no-vsync` disables VSync for comparisons. The default is windowed
with VSync enabled. **F8** captures a frame when a capture directory is configured.

The game's **Options** entry page offers **Game Options** (the original settings), **Cheats**, and
**Quit Game**. Up/Down selects rows, Cross confirms and Circle goes back. Quit Game asks for confirmation,
with Cancel selected initially. Save through the game's normal save menu before quitting to keep progress;
quitting does not autosave.

### Cheats

Open **Options → Cheats** in the game, or **F2 → Cheats** in native settings.
The in-game list scrolls with Up/Down.

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

### Memory cards and data paths

The windowed runner uses BozoEngine's per-user memory-card directory. Existing `game/mc0` saves migrate
through its application-data service. `--card-dir <directory>` selects a separate root containing `mc0`;
copy the card directory before experiments. Headless runs use an in-memory card unless `--card-dir` is
supplied. Inventory edits reach the active card only when the game performs a save.

The build retains configured asset paths. `--elf` and `--iso` override executable/disc locations;
`DC_TEXTURES` selects a relocated native texture pack. The executable still needs the owner's game files
and converted assets.

### Checks and captures

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

### Making a release

`VERSION` declares the release version; `CHANGELOG.md` contains its release notes. The launcher, offline build
kit and release scripts are adapted from [TMB_Recomp](https://github.com/TheBozos/TMB_Recomp) under its MIT
licence, retained in `launcher/LICENSE`. The release includes the pinned BozoEngine source, compiler dependencies,
licences and corresponding toolchain sources, and audits the archives for game data.

```bash
python tools/make_build_kit.py --out D:/dc_kit/kit
scripts/bench_run.sh --alone 20 "release package" python tools/make_release.py \
    --version 0.1.0 --kit D:/dc_kit/kit --out D:/dc_release --with-sources
```

Only committed source is exported. Follow [the release checklist](docs/RELEASE_CHECKLIST.md) before publishing;
the version-tag workflow builds and checks a draft release without a disc.

## Disc facts


| | |
|---|---|
| Boot ELF | `SCUS_971.11`, entry `0x100008`, one segment at `0x100000` (1.6 MB of code and data, BSS up to `0x1DABD00`), symbols kept |
| Code | `0x100000-0x2452B8` (EE code), `0x245300-0x24FB00` (VU microcode), `0x29FE80-0x2A1010` (static initializers) |
| Overlays | `TITLE.BIN` (177 functions) and `DUN.BIN` (62 functions), CodeWarrior `MWo3`, both at `0x01DABD00` |
| Video mode | NTSC |
| IOP modules | `MODULES/*.IRX`: SIO2MAN, PADMAN, MCMAN, MCSERV, LIBSD, SDRDRV, MODMIDI, MODHSYN, EZMIDI (and the unused network stack: INET, AN986, LIBNET, NETCNF, MSIFRPC, USBD) |
| IOP image | `MODULES/IOPRP211.IMG` |
| Data | `DATA.DAT` (1.7 GB archive) with its indexes `DATA.HED` and `DATA.HD2` |

## Sharing the machine with other ports

The team's native ports share a local benchmark lock, now provided by BozoEngine's `tools/bench_lock.sh`. Its
protocol remains compatible with the other ports: a game run takes one slot; a build or timed measurement takes
the bench alone.

```bash
scripts/bench_run.sh 5 "boot test" scripts/run.sh
scripts/bench_run.sh --alone 20 "runner build" scripts/build_runtime.sh
```

## Repository layout

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
