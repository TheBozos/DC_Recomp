# BozoEngine migration

Implemented on 2026-11-06. The active build uses only the `BozoEngine` submodule. The previous dirty PS2Recomp checkout is retained locally as an ignored recovery directory; it is not a submodule, source input, include path or linked dependency. Local recovery patches and the previous runner are in `build/migration-baseline`.

## Ownership boundary

| BozoEngine: reusable services | DC_Recomp: game policy and data |
|---|---|
| Symbol prefixes and resident-identity guarded native tables | Retail code spans, TITLE/DUN resident range and identity |
| ISO9660 lookup and native SDK file functions | User-owned disc and archive paths |
| Memory-card directories, pagination, command completion, configurable slots | Slot alias policy and existing save files |
| Thread status, cache-call aliases, shared reentrant allocator | DC native hook addresses and soft-float ABI exclusions |
| Native RPC/transfer/audio provider interfaces | Selected sound libraries and their offline layout |
| Strict IRX relocation/translation and compiled-library SDK host | Embedding the player's relocated images and registering them |
| Render targets, frame history, LOD sampling, blend modes | Game draw translation, shadows, work surfaces, texture formats |
| Hot-reloaded native input scripts | Test scenarios and copied cards |

Engine sources contain no DC addresses, filenames, game-ID checks or port-selection branches. Defaults preserve the existing TMB path; DC enables the generic capabilities through its own adapter.

## Native execution boundary

EE and overlay code are compiled ahead of time. Overlay dispatch selects linked native functions only when resident bytes match the registered identity; an absent image cannot execute stale code from the other overlay.

The offline sound pipeline compiles LIBSD, SDRDRV, MODMIDI, MODHSYN and EZMIDI. Native libsd handles sound API operations; the compiled library retains its pure note/pitch arithmetic. The native sound host provides bounded data buffers, linking, shared allocation, cooperative callbacks and SDK RPC/timer/thread/file services. It never fetches or decodes an instruction. Unsupported targets/imports/device addresses throw; unsafe translation fails. SIO2MAN, PADMAN, MCMAN and MCSERV are replaced at their native SDK interfaces, not loaded into a machine model.

The host retains logical EE addresses and native SDK scheduling/completion contracts. No console graphics packets are decoded, no VU1 program executes, and the build contains no interpreter/device backend. Native movie decoding is mandatory at configure time. Missing native functionality is a porting task, never a reason to activate an emulated path.

## Validation

- Built both native executables against BozoEngine alone, including a newly generated texture pack. Its SHA-256 exactly matches the previous pack: `0a2fb4ae59f00e11061d24ee9c7237902c031beebd867883560348b01a77f10b` (382,592,124 bytes).
- Engine tests cover instruction semantics, all DC code words decoding, guarded overlapping image dispatch, allocator coherence/overflow, ISO reads crossing sectors, save-directory resolution and pagination, input updates/partial writes, native kernel status, native audio, unsupported IRX paths, isolated host instances, SDK clock/status ABI, callback/task stack separation, and real OpenGL target/history/blend readback.
- Final local CTest: 9/9 with the GPU fixture; standalone headless CTest: 8/8 including the retail decode census. A configuration without FFmpeg was deliberately rejected. The linked executable imports only Windows system/UCRT DLLs.
- A 4,100-frame window run completed normally at about 60 fps, including save load, Norune and pause/unpause. Additional interactive checks with a copied existing save covered Toan/HUD, movement and the main game menu. Native captures are in local `build/migration-evidence`; original saves were not overwritten.
- Rebuilt the separate TMB port against the edited engine and ran its 1,000-frame boot/menu regression: same final screen 8, EE cycle 5,144,574,984, 1,044 vblanks, 803,141 override calls, 216,132 native VU0 calls, and 1,219,508 I/O compatibility accesses with zero unknown addresses. Library/kernel call totals change because additional SDK symbols now resolve directly to native bindings. This is a boot/menu regression, not a new full-match qualification.

Local logs and WAVs live under `build/bozo-*`; game data, generated sources, logs and captures stay untracked. No full-game playability claim is made. Earlier visual defects and unqualified dungeons/combat remain follow-up work after this migration.
