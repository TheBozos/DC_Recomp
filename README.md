<div align="center">
<img src="docs/branding/dc_banner.png" alt="Dark Cloud native PC port by The Bozos" width="100%">

# DC_Recomp

A native Windows port of **Dark Cloud**, NTSC-U `SCUS-97111`, statically recompiled with [BozoEngine](https://github.com/TheBozos/BozoEngine).
</div>

No game files or generated game code are distributed. Build from your own North American disc, version 1.02.

| File | SHA-1 |
|---|---|
| `SCUS_971.11` | `d9965d6908a334a5dfc982f3384428ad42048d54` |
| `TITLE.BIN` | `8da27dd6a4f8d2090216b17d5d64a0c142cfd6d5` |
| `DUN.BIN` | `ab7e56a4eeb69e178638088de6e57857d48f1d98` |

## Status

Bring-up, with the pre-migration Norune baseline restored on BozoEngine: boot, title/attract sequence, existing save detection and loading, Toan and the village HUD, movement, pause and the main game menu. The native window runs at approximately 60 fps. This is not a claim that the full game is playable or that every earlier rendering defect is fixed. Dungeons, combat and a complete playthrough remain unqualified.

PS2Recomp is no longer a dependency. The main executable and both overlays are statically compiled to C++; five sound libraries are relocated and compiled offline. Pad and memory-card calls use native SDK services. Rendering comes only from the game's own model, texture, camera and drawing data through the native OpenGL adapter. Sound uses compiled drivers, native libsd and the native mixer. Movies use FFmpeg.

There is no interpreter, dynamic recompiler, firmware bootstrap, GS/VIF/VU1 packet renderer, SPU2/SIO2 device backend or emulation fallback. Unknown native calls, unsupported module code and device-address accesses fail explicitly. SDK thread/timer/file contracts, logical address spaces and transfer-completion acknowledgements still exist; they are native compatibility services, not a console machine backend.

Known pre-migration gaps include the sky colour, occasional green geometry, some Georama pictures and incomplete effects/sprites. See [migration details](docs/BOZO_MIGRATION.md). Earlier investigation notes are retained in [RECOMP_NOTES](docs/RECOMP_NOTES.md) as historical evidence.

## Build on Windows

Use MSYS2 UCRT64 with GCC, CMake, Ninja, Bash and Python 3. SDL3 and ImGui are fetched at pinned revisions by BozoEngine. FFmpeg's libavcodec/libavutil are required; use installed pkg-config packages or a compatible static build with `include/` and `lib/` directories. The texture converter uses only the Python standard library.

```bash
git submodule update --init BozoEngine
scripts/extract_iso.sh "/path/to/Dark Cloud (USA).iso"
scripts/build_runtime.sh Release -DBOZO_FFMPEG_ROOT="D:/path/to/ffmpeg" \
    -DDC_ISO="D:/path/to/Dark Cloud (USA).iso"
scripts/run.sh
```

Alternatively, CMake directly from a UCRT64 shell:

```bash
cmake -S . -B build/bozo -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DBOZO_FFMPEG_ROOT="D:/path/to/ffmpeg" -DDC_ISO="D:/path/to/disc.iso"
cmake --build build/bozo --parallel 6
build/bozo/dc.exe
```

The build prepares `build/bozo/dc_gen`, compiles the sound libraries in `dc_audio_gen`, and converts the disc textures into `build/bozo/textures/dc_textures.bin`. A fresh build needs no artifacts from the previous runtime. No game-derived output belongs in Git. `DC_WINDOW=OFF` builds only `dc-headless`. The binaries remember the configured data paths; use `--elf` and `--iso` to override them, and `DC_TEXTURES` for a relocated texture pack.

The window uses BozoEngine's native keyboard/gamepad input and per-user card directory. Existing `game/mc0` saves are migrated through its normal application-data service. For an isolated copied card, pass `--card-dir <directory containing mc0>`. Headless runs use an in-memory card unless a directory is supplied.

```bash
build/bozo/dc-headless.exe --frames 1400 --wav build/boot.wav
scripts/boot_test.sh title 30 5
```

`--pad-script <file>` accepts entries such as `1200:START/6,1500:CROSS/6`. `--live-pad <file>` reloads changed script text and starts it at the current game frame, useful for native automation. `--frames 0` runs until the window closes; the window's default is unlimited. `--capture-dir`, `--capture-frame` and `--wav` provide native capture. There is no fallback switch.

## Checks

```bash
cmake -S . -B build/bozo -DDC_ENGINE_TESTS=ON
cmake --build build/bozo --parallel 6
ctest --test-dir build/bozo --output-on-failure
```

This runs the instruction, runtime, ELF decode, library, kernel, audio, strict compiled-library and GPU render-target suites. GPU tests require a working OpenGL 3.3 context. `scripts/engine_tests.sh` runs the standalone headless engine suites; `scripts/fork_tests.sh` remains an alias for existing contributors.

CI checks the pinned engine with synthetic fixtures and validates the offline tools/scripts. Retail gameplay checks run locally using the player-owned disc and copied saves.

Game-specific addresses, formats, hook selection and audio-module layout stay in this repository. Engine APIs and fixes must remain reusable by other recompiles.
