# BozoEngine migration

Implemented on 2026-11-06. The active build uses only the `BozoEngine` submodule. The previous dirty PS2Recomp checkout is retained locally as an ignored recovery directory; it is not a submodule, source input, include path or linked dependency. Local recovery patches and the previous runner are in `build/migration-baseline`.

The graphics cleanup includes the DC adapters, regression fixtures, guarded offline adaptations and published
BozoEngine pin `4a8f70a` together. References to local/unpublished work in the milestone sections below describe
the state at those earlier checks. The latest validation is 37/37 suites, both 5,800-frame routes and a controlled
before/after reproduction of Norune's terrain source-lifetime failure. Remaining coverage and packet-work limits
are listed in the final section.

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

Engine sources contain no DC addresses, filenames, game-ID checks or port-selection branches. DC enables the generic capabilities through its own adapter. The subsequent graphics cleanup below changes the shared native graphics boundary; the older TMB comparison is historical evidence, not a qualification of that later change.

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

## Native graphics cleanup

On 2026-11-06, removed the engine's GS register/transfer state and all three native graphics packet consumers:
draw-environment A+D parsing, GIF DMA PRE/PRIM inspection, and readback BITBLTBUF/TRXREG parsing. Deleted
`bozo/libs/gs.h` and the `Libraries::gs` member, transfer history, synchronization counters, and privileged-register
bank. Native graphics completions now acknowledge work without dereferencing any chain or packet. SDK output
builders initialize flags without inspecting previous packet contents and use the SDK's existing scalar video
parameters rather than retaining engine display state.

The DC adapter converts the game's TEST/ZBUF/ALPHA arguments to native material properties immediately. Its
visibility readback takes coordinates directly from MGEndFrame's game-side query array and samples the native GPU
depth cache. It never reads the packet argument. The first asynchronous cache misses, and all headless queries,
use empty depth until samples become available. This changes the old fabricated-zero visibility behavior: native
occlusion and lens flares can now differ from the earlier build. Other readback callers remain explicitly unsupported
and fail; the disc/screenshot StoreImage path is not qualified.

Also replaced `sceDmaReset`'s hardware loop with a native SDK service and made unknown device accesses fault instead
of silently returning zero or discarding writes. Channel-base byte/halfword updates now preserve the other status
bits; obsolete display/CSR acknowledgement stores retain no GS state.

What remains: logical EE/compiled-library memory and register contexts; SDK scheduling, timers, interrupts, DMA
status/completion bookkeeping, and native translations of the game's material/texture encodings. Some original
recompiled drawing code and SDK builders still produce unused console-format output buffers. Removing those
producers and the remaining device-facing synchronization calls requires further game-level porting. No interpreter,
GS rasterizer, GS memory, runtime GIF/VIF graphics decoder, VU1 execution, or SPU2 backend was introduced.

Validation evidence is local under `build/native-cleanup/` (generated code, copied cards, recordings and captures
remain untracked):

- Both executables built; all 10 CTest suites passed, including negative packet-pointer tests, native completion/reset
  checks, and real GPU depth readback of a primitive followed by empty depth. `verified-tests.log` records the run.
- A 4,100-frame headless run completed with zero unknown accesses. A 4,100-frame windowed run also completed,
  loading the copied save into Norune, pausing/unpausing and entering the menu transition. Its log records 13,504 GPU
  depth samples, 192 pending samples, and zero unknown accesses.
- Before/after captures of Toan in Norune and the pause screen were inspected. They show the same scene and HUD;
  they are not pixel-identical. Attract/menu transition timing and the new native visibility result can differ.
- Before/after WAVs contain nonzero 48 kHz stereo audio, with comparable measured RMS (3,368 versus 3,450 on the
  signed-16-bit scale). Audio regression fixtures passed; this is not a new subjective listening qualification.
- A further 5,200-frame windowed run verified movement in Norune from captured changes in position and camera.
  The final 5,800-frame run verified the actual main menu at frame 5,500 and its dismissal at frame 5,799, following
  movement and pause/unpause. It ended normally with 124,619 compatibility register accesses and zero unknown
  accesses. Evidence: `gameplay.log`, `menu.log`, and `menu/frame_005500.png` / `frame_005799.png`.

The tests do not establish full-game playability or PS2 visual parity. Dungeon/combat/story coverage and the earlier
documented rendering defects remain open. Changes are local in DC_Recomp and its BozoEngine checkout.

## Packet producer and native transfer cleanup

The next pass removes calls to five original packet producers: `CVisualVu1::DrawVu1` (`0x135000`), `SetGsReg3`
(`0x129360`), `SetEnv` (`0x140DA0`), `setAlphaFlag` (`0x15BC20`) and `MGSetGsALPHA` (`0x12EF90`). Their native
replacements retain model/lighting/material behavior without serializing DMA/VIF/GIF output or reserving packet
words. They no longer require those generated producer functions to be present. MDT creation/remaking and other
mixed-purpose routines still use original compiled code; they need further separation of asset/bookkeeping work
from packet generation.

BozoEngine now supplies reusable native `sceDmaSend`, `sceDmaSendN`, `sceDmaSendI` and `sceDmaSync` services.
They validate the SDK channel handle, acknowledge the graphics work the port already submitted, and queue native
completion handlers without reading a packet, retaining its address/length, or executing a register-poll loop.
The SDK's unsupported native data producers/blocking waits fail explicitly; the native movie decoder's existing
deferred-completion bookkeeping remains. Game addresses and draw semantics stay in DC_Recomp.

Evidence under local `build/native-cleanup2/`:

- Both runners built and all **11 CTest suites passed** (`final-build-tests.log`). The new game-free DC fixture
  installs the five native hooks without their original producers and checks that guest RAM, including guarded
  packet buffers, is unchanged. Engine tests cover unreadable source addresses, completion status/handler queuing,
  invalid handles, and unsupported producers, in addition to the earlier GPU and audio fixtures.
- A 5,800-frame headless run completed with zero unknown device accesses. Its original-binary comparison used the
  same copied cards and pad script: **117,269 to 76,302 register accesses**, about 35% fewer (`before-headless.log`
  and `headless.log`). That candidate preceded only the additional environment/blend producer removal.
- The final 5,800-frame windowed run completed with **81,148 register accesses and zero unknown accesses**,
  versus 124,619 in the preceding cleanup's matching run. BozoEngine handled **6,000 VIF1, 5 GIF submissions and
  7,198 sync calls** through native SDK services (`gameplay.log`). These are operation counts, not a speedup claim.
- Captures at frames 3,400 / 3,550 show Toan/Norune and pause; frames 5,100 / 5,799 show movement and the resulting
  position; frame 5,500 shows the main menu. The menu closes again by 5,799. The captures were inspected directly.
  Audio regression fixtures passed and the windowed run produced a stereo WAV. No PS2 parity or full-game claim.

Timer/interrupt compatibility, direct register access in remaining frame/loading routines, native movie completion,
and some unused packet builders remain. The change does not remove the register/address contexts required by
statically compiled code. Other ports have not been qualified against these additional native SDK bindings.

## Native field, timing and register retirement

Integrated upstream BozoEngine **5bb4efb** (PR #3) on local branch `codex/dc-native-cleanup`, preserving the earlier
local cleanup. DC reuses its inline native instruction interface and retired-graphics guard. The two vblank
callbacks now read logical parity from the native frame counter, without reading or reconstructing the GS CSR.
The corresponding old bit-extraction instructions are removed during code generation. Native graphics SDK
submissions notify the scheduler directly; they no longer update CHCR, D_STAT or CPCOND0. Native completion flags
preserve delivery of callbacks masked when their SDK job completed. DMA environment setup/query is an opaque SDK
configuration round-trip, not a hardware controller model.

The remaining console Timer0 accesses were CPU/free-time diagnostics, not gameplay timers. Retired their mode
setup, counter reset, samples and percentage prints. Gameplay, sound, scheduler alarms and logical frame time remain
native services. The channel-byte read/modify/writes were console tag-transfer setup; native submissions do not
consume those bits, so their callers no longer perform the accesses.

BozoEngine's new `--patches` input validates an entire batch of expected instruction words before adapting only its
analysis image, before function discovery/emission. It rejects malformed/overflowing input, wrong original words,
duplicates, unaligned/out-of-code addresses and unsupported replacements without partial application. The original
ELF/disc and runtime guest image are unchanged. DC owns the **30 adaptations** in `recomp/dc_native.patches`; shared
engine code contains no DC addresses. Resident native-PC declarations are excluded from TITLE/DUN externals files,
with a game-free regression for this image boundary.

`--profile-io` uses BozoEngine's optional per-PC/address audit. This pass's 5,800-frame headless audit went from
**76,302 accesses** to 7,236 after display/timer/tag-bit retirement, then **zero** after native field/configuration
integration. Native graphics mode rejects fallback into retired graphics registers and DMA completion conditions.
This is a measured route result, not a claim that unvisited game paths cannot require more port work.

Final evidence in local `build/native-cleanup3/`:

- Both runners built; **13/13 suites passed** (`final-tests.log`), including guarded adaptations, native job callbacks
  through masking, native DMA configuration validation/round-trip, field parity/control-flow preservation, image
  external boundaries, and GPU depth/render targets. The existing instruction/audio/compiled-library suites pass.
- The final **5,800-frame windowed route completed with zero register accesses, zero unknown accesses** and zero
  VU0 microprogram calls (`gameplay.log`). Native SDK services handled 6,046 VIF1 and 5 GIF submissions and 7,244
  sync calls. These labels identify SDK entry points; no console packets or DMA device ran.
- Captures at 3,400 (Norune/Toan), 3,550 (pause), 5,100 (movement), 5,500 (main menu) and 5,799 (menu closed) were
  inspected directly. The title/attract/load path also completed. No PS2 visual parity or full-game claim is made.
- The integrated headless route also completed with zero register accesses. WAV recordings are retained for both
  the previous binary and the final headless/windowed runs. Both headless recordings contain 4,680,883 stereo frames
  at 48 kHz; only two of their 9,361,766 signed 16-bit samples differ (maximum difference 79). Both have peak 28,775
  and RMS 3,594.425. The windowed recording is also non-silent. This is a fixture comparison, not a listening test.

The project still uses statically compiled CPU contexts/logical addresses and native SDK scheduling. Model asset
creation/remaking and some mixed-purpose routines still build unused console-format data, and native visibility
readback remains asynchronous. Other screenshot/readback callers, dungeon/combat/full-story coverage, earlier
rendering defects and qualification of other ports remain open. Code changes are local and unpushed. Progress and
validation results were shared on c2c F2F throughout this pass.

## Native asset construction without console serialization

Replaced the original compiled `CreateVUdataFromMDT` (`0x135AA0`), `CreateVUdataFromMDTRemake` (`0x135E50`),
`CreateVUdataShadow` (`0x136530`) and cloth `CreateVUData` (`0x13C5F0`) calls with native asset bookkeeping.
The model hooks validate the MDT primitive/material ranges, retain its source snapshot and construction-time
texture bindings, and update the visual's data identity and allocation extent. Remakes preserve existing texture
bindings. Cloth geometry remains in its native draw hook, read directly from the simulation grid. No original
serializer runs and none of these hooks reads or writes the obsolete output buffer.

The shared game arena still needs the original extent to keep adjacent allocations and double-buffer identities
stable. The native hooks calculate that extent from primitive counts and scalar texture metadata; they do not
generate VIF commands, material packets or expanded vertex buffers. This pass removes serialization, not arena
reservations. Differential validation found two relevant ABI distinctions: texture setup has a variable extent,
and a missing cloth texture uses the default mode while an MDT material uses texture record zero. Both cases,
including zero and nonzero defaults, are covered against the original statically compiled routines.

BozoEngine's `sceGsSetDefLoadImage` and `sceGsSetDefStoreImage` now return zero without reading stack parameters or
touching their output when the port explicitly enables native graphics. Asset uploads and renderer readback already
belong to the port. Unsupported readback continues to fail explicitly. Generic SDK output remains available outside
native graphics mode, and other templates retain scalar fields that callers can still use.

Evidence in local `build/native-cleanup4/`:

- Both runners built and **14/14 suites passed** (`final-tests.log`). The new synthetic fixture compares **1,064
  cases** with the original compiled builders, covering coloured/uncoloured models, lists/strips, batch boundaries,
  multiple primitives, materials/default texture modes, shadows and cloth dimensions. Native packet guards and
  inaccessible output addresses remain untouched; invalid MDT input leaves guest RAM unchanged. The engine fixture
  checks unused image builders with both inaccessible and valid guarded outputs. No game bytes are embedded in tests.
- **5,800-frame headless and windowed replays completed with zero register accesses, zero unknown accesses and zero
  VU0 microprogram calls** (`headless.log`, `gameplay.log`). At the windowed 5,700-frame counter sample, the native
  hooks had handled 1,063 model builds, 11,497 remakes, 2,532 shadow builds and 12 cloth builds. Their cumulative legacy
  output extents totalled 453,906,496 bytes. This counts repeated extents, not exact avoided writes, resident memory
  savings or measured speedup; the original remake also left some bytes within its extent unchanged.
- Inspected the window captures at 3,400 (Norune/Toan), 3,550 (pause), 5,100 (movement), 5,500 (main menu), and 5,799
  (menu closed). The title/attract/load path completed too. These checks do not establish PS2 visual parity.
- Headless 48 kHz stereo output stayed active: RMS 3,592.478 versus 3,594.425 previously; the recording is 67 stereo
  frames (about 1.4 ms) shorter. PCM differs after the load transition; the removed compiled builders no longer add
  their old scheduler checkpoints, so bit-exact audio/timing equivalence is not claimed. The window recording also
  has active audio in its final ten seconds (RMS 2,826.680). Existing audio fixtures pass; this is not a listening test.

Remaining work includes the old arena reservations, other mixed-purpose texture/frame routines and packet helpers,
unused generated original functions, and unqualified screenshot/readback callers. Dungeon/combat/story coverage,
earlier rendering defects and other ports remain unqualified. Changes remain local and unpushed. Implementation,
test findings, replay progress and audio limitations were shared regularly on F2F.

## Native texture selection and load boundaries

`CTextureManager::ReloadTexture` (`0x133070`) now handles group selection, overlap/pending markers and loaded flags
directly. It binds the selected native texture records and invalidates patched texture copies as before, without
calling its original VIF/GIF flush, palette/mipmap upload and packet-reservation builders. The packet argument is
never read. Invalid group numbers still reset the current group to -1; already-loaded selections remain no-ops.

Both `LoadTexture` wrappers (`0x154770`, `0x154950`) now call the existing pure `SetTextureInfo` asset parsers directly,
preserve the record identity fields their callers use and pin the decoded native texture. The original mixed upload
wrappers, image packet setup, cache flush and upload/sync calls are no longer made. The asset parsers themselves
remain statically compiled routines; fixed-texture insertion and other mixed builders remain future work.

Validation in local `build/native-cleanup5/`:

- **16/16 suites passed** (`final-tests2.log`). The new DC fixture compares all manager bytes in **666 group reload
  cases** (all 72 groups plus invalid groups, cached/missing selections and varied overlap markers) and all record
  bytes in **60 texture load cases** (both overloads, five source formats, sizes and identity boundaries) against the
  original compiled routines. Native reload accepts an inaccessible packet pointer and leaves packet buffers intact;
  native load wrappers make no image/cache SDK calls. The existing 1,064 asset-builder comparisons still pass.
- Added BozoEngine's standalone `native_image_builders` test: generic image definition calls still journal their SDK
  output, while explicit native mode leaves valid guarded buffers unchanged and ignores inaccessible outputs.
  The isolated engine source/test patch was exported against **5bb4efb**, checked for clean application, and shared
  with TMB and VP2 on request. Its local path is `build/native-cleanup5/share/native-image-builders.patch`.
- Both **5,800-frame headless and windowed routes completed with zero register accesses, zero unknown accesses and
  zero VU0 microprogram calls** (`headless.log`, `gameplay.log`). At the windowed 5,700-frame sample the new services
  had handled 57,698 reload requests, 47,163 group selections, one TM2 load and two file loads. These are coverage
  counts, not measured performance gains. Other texture insertion routines still submit native SDK completion jobs.
- Directly inspected title/attract (2,000), Norune (3,400), pause (3,550), movement (5,100), main menu (5,500) and menu
  dismissal (5,799) captures. This does not qualify full-game coverage or PS2 visual parity.
- Headless 48 kHz stereo output has RMS 3,592.503 versus 3,592.478 in the previous cleanup; duration is 57 stereo
  frames (about 1.2 ms) shorter. PCM is not identical. Windowed audio remains active in its last ten seconds (RMS
  2,842.120), and audio fixtures pass. No bit-exact timing/audio or subjective listening claim is made.

Changes remain local and unpushed on root branch `codex/native-dungeon` and engine branch `codex/dc-native-cleanup`.
F2F progress included cross-port tags, responses to TMB/VP2 requests and the reusable patch/test package. Both C2C
history and F2F were checked for DC-Recomp, @all and @WindowsRecomp mentions. Other ports must audit their own
texture-manager layouts and consumers before applying the approach. Remaining arena reservations, fixed-texture
builders, frame/effect packet helpers, unsupported readbacks and broader gameplay/rendering coverage remain open.

## Fixed textures and shared engine publication

Replaced `EnterFixTexture` (`0x1320E0`) and `EnterFixTextureZ` (`0x132930`) with native record insertion and texture
binding. They preserve the caller's record fields, allocation extent/defaults and mip/filter metadata without calling
the original GIF builders, cache flush, DMA send or path synchronization. Native decoding still uses the source
asset/palette, then restores the record's old transient-pointer fields as before. Fixed Z insertion accepts the
original 640x224/type-5 picture contract; other picture layouts leave metadata unchanged.

Evidence in `build/native-cleanup6/fixed/`:

- **16/16 suites pass** against engine main **149bf90** plus the separately reviewed cleanup. The texture fixture now
  adds **224 fixed-texture comparisons** against the original compiled routines, including three pixel kinds,
  allocation-only calls, mip/default/filter modes, non-power-of-two widths, order flags and accepted/rejected fixed-Z
  layouts. All manager/record bytes match, native calls avoid SDK upload/cache/sync traffic and the console packet
  buffer stays unchanged. The prior 666 reload / 60 load / 1,064 model-builder cases still pass.
- Both **5,800-frame headless and windowed routes completed with zero register accesses, zero unknown accesses and
  zero VU0 microprogram calls**. Both now report **zero GIF SDK submissions**, versus five in the previous route.
  The windowed 5,700-frame sample confirms three fixed and two fixed-Z insertions; VIF1 SDK completion jobs remain.
- Inspected Norune (3,400), pause (3,550), main menu (5,500) and menu close (5,799) captures directly. Movement/title
  captures are retained too. These checks do not establish full-story coverage or PS2 visual parity.
- Headless audio has 4,680,761 stereo frames at 48 kHz (two more than before), RMS 3,592.502 versus 3,592.503. Windowed
  audio remains active through its last ten seconds (RMS 2,856.674). Audio fixtures pass; no subjective listening or
  bit-exact timing claim is made.

After discussion with TMB and VP2, the first reusable contribution was published and merged as BozoEngine
[PR #4](https://github.com/TheBozos/BozoEngine/pull/4), main **149bf90**: the opt-in image-builder guards and standalone
generic/native/inaccessible-output regression. Clean upstream Windows/GPU tests and Windows/Linux CI passed. TMB
qualified exact 1P/4P draw/sound/binding records and windowed movies/gameplay, then merged its engine pin in TMB PR #11.
TMB's brief audio underruns and full-campaign limits remain explicit; VP2 source review is not a live-port pass.

The broader GS state/GIF reader/native DMA cleanup is isolated in BozoEngine
[PR #5](https://github.com/TheBozos/BozoEngine/pull/5), and the guarded offline instruction-adaptation loader in
[PR #6](https://github.com/TheBozos/BozoEngine/pull/6). Their clean default-tools builds passed 10 GPU-inclusive and
9 headless suites respectively. They are separate reviews, not implied merged by PR #4. No-option code generation
with the adaptation loader matches the previous engine byte-for-byte across six generated files. Cross-port
qualification and final merge state are recorded in the follow-up below.

## Packet-only TEST/ZBUF setters

`MGSetGsTEST` (`0x12EDF0`) and `MGSetGsZBUF` (`0x12EEC0`) now return through native hooks. Their original routines only
serialized console packets; they changed no game metadata. Native draws already receive explicit material/depth
properties. The hooks read no arguments, build no packet and retain no GS state. Their game-free fixture accepts
inaccessible input pointers and verifies that guest RAM remains unchanged. At the 5,700-frame headless sample,
13,868 calls had been retired; this is a coverage count, not a measured speedup.

Evidence in `build/native-cleanup6/fixed/state-*`:

- All **16/16 DC and engine suites passed**, including the original/native asset and texture comparisons.
- Both **5,800-frame headless and windowed routes completed**, with zero register accesses, zero unknown accesses,
  zero VU0 microprogram calls and zero GIF SDK submissions. VIF1 native completion calls remain.
- Directly checked the latest Norune (3,400), pause (3,550), movement (5,100), main menu (5,500) and menu dismissal
  (5,799) captures. Full story, combat, dungeon coverage and PS2 visual parity remain unqualified.
- Headless 48 kHz stereo output contains 4,680,762 frames, RMS 3,592.502 (one frame longer than the preceding fixed
  texture run). Windowed output contains 5,051,093 frames, RMS 3,534.159, with last-ten-second RMS 2,875.268.
  Audio fixtures pass; these measurements establish active output, not subjective quality or bit-exact timing.

Root changes remain local on `codex/native-dungeon`. Engine publication is tracked separately below; no private
game assets or generated game code are included in the shared engine PRs.

## Shared engine final qualification

The offline adaptation loader is merged as [BozoEngine PR #6](https://github.com/TheBozos/BozoEngine/pull/6), main
`9d24c9d`, with generated-and-executed delay-slot, backward-loop/checkpoint and interior-entry fixtures. It adapts only
the offline analysis image; retail ELF/runtime load bytes stay original. The owner still selects the correct image
and audits register liveness/callback ABI. No-option DC code generation remained byte-identical across six files.

Its stricter complete-instruction executable-range guard is merged as
[PR #8](https://github.com/TheBozos/BozoEngine/pull/8), main `4f99963`. A regression rejects words spanning a code-range
hole without mutating any analysis-image bytes. VP2's separately merged [PR #7](https://github.com/TheBozos/BozoEngine/pull/7)
adds explicit offline entries; its combined entries/adaptations fixture executes both contracts together. Our source
review caught a partial-instruction code-range issue that VP2 fixed before merge. This is not live VP2 qualification.
All Windows/Linux checks passed for these merges; their local and remote feature branches were deleted.

The final PR #5 candidate `94b688d` includes those published compiler dependencies, with no local engine edits.
`build/native-cleanup6/final-combined-tests.log` records **18/18 DC+engine suites** and **13/13 standalone default-tools
engine suites**, including GPU depth/readback. TMB independently tested unchanged TMB main `74fd638` against engine
`0c980b29`: 9,500-frame 1P and 8,500-frame 4P records match draw/count, sound/sample, call, clock, scratchpad and thread
diagnostics. RAM differences from retired SDK writes are expected; no whole-RAM claim is made. TMB also completed
movie/menu (4,201), 1P (6,501) and 4P (6,501) window runs and directly inspected movie/combat/four-view arena captures,
including texture bindings/ordinals. TMB then confirmed the final candidate's runtime tree is identical and its
compiler emits all 34 TMB files byte-identically, and recommended merge after CI. Legacy TMB diagnostic flare
packet readers remain unsupported port debt; full campaign and subjective audio quality remain unqualified.

[BozoEngine PR #5](https://github.com/TheBozos/BozoEngine/pull/5) is now **merged**, main
`4a8f70a5021cbb3429800a02259ae7b816581ee0`, after final-head Windows/Linux CI passed. Its merge tree is identical
to the tested `94b688d` candidate. DC's engine checkout is clean published `main` at that commit; earlier local
engine modifications are preserved only as recovery stashes. PR #5's local/remote feature branches and the old
local cleanup branch were deleted. This supersedes the earlier local/unpublished engine status in the historical
sections above. The root DC changes, tests and updated engine pin remain uncommitted locally.

F2F received the final commit, test results and scope limits; TMB/VP2 were tagged for integration. Both C2C history
and F2F were checked for DC-Recomp, @all and @WindowsRecomp mentions, with replies to relevant requests. Remaining
DC work includes mixed frame/effect helpers that still build unused console buffers, old arena reservations,
unsupported readbacks and broader gameplay/rendering coverage. No claim is made that every compatibility service
or unused compiled original has been removed.

## Window/model wrappers and direct visibility

On clean published BozoEngine `4a8f70a`, DC now retires these additional console builders:

- `MGSetGsTEXA` (`0x12F060`) and `openGiftag`/`closeGiftag` (`0x13DEA0`/`0x13DF00`) return without reading packet,
  argument or default-state pointers. Native drawing has no consumer for their output.
- Both `MGSetWindowRect` overloads (`0x12D9A0`/`0x12DAE0`) retain the four full-width logical rectangle fields at
  `0x1C75510` without packing a hardware scissor command. This preserves existing behavior; it does not add a new
  native clipping implementation.
- `CVisualVu1::DrawVu1`'s packet overload (`0x134BC0`) goes straight to the same native draw handler as `0x135000`.
  Its original wrapper only terminated a packet, forwarded the visual/matrix/RenderInfo/light arguments, then
  reserved the callee's packet words. Neither overload now dereferences its packet/output argument.
- `MGEndFrame`'s visibility loop now resolves all sixteen game query records through native depth sampling and
  24-bit minimum reduction. Disabled records stay unchanged; invalid screen coordinates retain the original
  `0xFFFFFFFF` result. The shared arena retains its old `0x800` extent when a valid query is processed, but no
  readback tile is allocated or written. An undersized arena fails explicitly instead of entering the old hang.
  The asynchronous GPU cache still supplies empty/far depth when unavailable, including headless operation.

Two guarded offline adaptations select the visibility batch at `0x12D268` and continue after the old loop at
`0x12D438`, preserving the NOP delay slot and leaving surrounding frame scheduling, counters and screenshot
handling in the compiled game. The adapter preserves the entire CPU context. Resident inline sites are no longer
also listed as callable overrides, and still do not leak into overlapping overlay images. All SDK StoreImage
callers now require their own native implementation and fail explicitly; the retired visibility SDK path cannot
silently reappear.

Validation in `build/native-cleanup7/`:

- **20/20 suites pass** (`visibility-tests2.log`), including **80 original/native window cases** (both overloads,
  source/destination aliasing, signed/full-width origins and extents), inaccessible-pointer/full-RAM guards for the
  retired packet wrappers and both model overloads, plus **144 original/native visibility cases** with six depth
  patterns, disabled/mixed query arrays, screen-edge coordinates, high depth bytes and varied arena capacities.
- The visibility reference fixture is extracted from the owner's hash-checked retail ELF only during the optional
  local test build, into its own output directory. It recompiles the original loop without DC adaptations, with
  synthetic save/return instructions around it. It uses the original compiled allocator and explicit synthetic
  SDK depth samples. Query records and arena extents match; the native path preserves the complete CPU context
  and all other RAM, even when the obsolete tile address is inaccessible. No game code/assets are committed.
- The earlier wrapper-only headless and windowed **5,800-frame** routes passed with zero register/unknown accesses,
  zero VU0 microprogram calls and zero GIF SDK submissions (`headless.log`, `gameplay.log`). Directly checked
  attract (2,000), Norune (3,400), pause (3,550), movement (5,100), menu (5,500) and dismissal (5,799) captures.
  At the 5,700-frame sample, the model packet wrapper was bypassed 1,353 times headless / 2,347 times windowed.
  Window-rectangle and explicit GIF-envelope hooks had no live hits in that route; their coverage is fixture-only.
- Wrapper-only headless audio has 4,680,762 stereo frames at 48 kHz, RMS 3,592.502; windowed audio has 5,091,310,
  RMS 3,565.607 and last-ten-second RMS 2,869.709. Direct-visibility headless has 4,680,761 frames, RMS 3,592.502,
  last-ten-second RMS 3,434.538, and completed the same 5,800-frame route with zero device/unknown/VU0 activity and
  zero GIF submissions (`visibility-headless.log`). PCM timing is not asserted identical.

This batch changes DC layouts/adapters only. BozoEngine's existing native depth/graphics contracts suffice; its
checkout remains clean at the published main pin. TMB and VP2 received the approach and bounded results on F2F.
TMB also independently integrated the shared engine and removed its own remaining legacy flare packet parsers in
TMB PR #13. Root DC work remains local and uncommitted; full campaign, combat/dungeon coverage and console visual
parity remain open. Mixed frame/effect packet construction, unused arena reservations and unsupported readbacks
still require further audits.

The **final direct-visibility window run also completed 5,800 frames**, with zero register/unknown accesses, zero
VU0 microprogram calls and zero GIF SDK submissions (`visibility-gameplay.log`). Directly inspected all six final
captures again (attract, Norune, pause, movement, menu and dismissal). The 5,700-frame sample reports 4,865 native
visibility batches and 15,040 samples from GPU / 192 pending; headless reports 4,472 batches. The final window PCM
contains 5,071,036 stereo frames at 48 kHz, RMS 3,571.959, with last-ten-second RMS 2,852.092 and peak 32,766.
These are coverage/output checks, not performance, full-game, console parity or subjective-listening claims.
F2F received final results and the inline-site/callable-override distinction; both lanes were checked for addressed
messages, and relevant TMB/VP2 reports were acknowledged.

## Water and running-effect packet envelopes

Guarded offline adaptations now bypass two serialization-only spans in each of `CWater::DrawVu1` (`0x160F20`) and
`CRunEffect::Draw` (`0x163480`). The original packet routines remain compiled for other callers, but these two
wrappers no longer invoke them. Water retains CheckClip, GetLWMatrix, alternating native asset-buffer selection,
CreateVUData metadata, its RenderInfo update, native virtual draw and draw-result forwarding. The effect retains
texture lookups, color/clamping, the alternating texture toggle, all eight particle/lifetime/projection decisions
and native sprite inputs. The skipped commands only built unused TEST/ZBUF packet envelopes.

Four guarded words per wrapper validate both jump sites and their delay slots. Water's delay-slot scalar writes
remain caller-volatile and are overwritten or unused before the retained code. The effect's obsolete default-ZBUF
load becomes a NOP in its new jump delay slot. No live game metadata or renderer material state is inferred from
those removed packets. BozoEngine remains clean at published main `4a8f70a`; no new engine API is required here.

Evidence in `build/native-cleanup8/`:

- **22/22 suites pass** (`effect-tests.log`), including **96 water** and **384 effect** original/adapted wrapper
  cases. The optional local fixture generator extracts both original functions from the owner's hash-checked ELF
  into separate fixture directories. Original/adapted C++ comes from identical original bytes, with the adapted
  version applying exactly the relevant production patch subset. No retail bytes or generated game C++ are stored
  in the repository.
- The reference executes the actual compiled packet builders. It makes twelve packet-builder calls when water is
  unclipped, or when both effect textures are present; the adapted versions make zero. Native fixture packet
  pointers can be inaccessible. Water compares clipping, buffer choice, callback order/live arguments, complete
  matrix data and signed draw results across two virtual targets. The effect compares missing/present textures,
  color modes, pulse states, inactive/sparse/full particle arrays, projection results and submitted rectangle,
  corner and color bytes. Callee-saved registers, stack/return, float status and all non-stack RAM remain checked;
  the original ABI stack scratch is explicitly excluded from the RAM comparison. These fixtures check wrapper
  behavior, not the projection/rendering math implemented by the controlled callback boundaries.
- Both **5,800-frame headless and windowed replays completed** with zero register/unknown accesses, zero VU0
  microprogram calls and zero GIF SDK submissions (`headless.log`, `gameplay.log`). Feeder diagnostics confirm both
  wrappers execute: published blocks through the 5,700-frame sample total **1,426 calls each headless / 2,422 each
  windowed**. The native water draw counters total **1,353 headless / 2,349 windowed** in those blocks. These are
  bounded coverage counts, not performance measurements or complete-run totals.
- Directly inspected final attract (2,000), Norune (3,400), pause (3,550), running with a visible foot dust effect
  (4,800), movement (5,100), menu (5,500) and dismissal (5,799) captures. No console parity/full-campaign claim.
- Headless 48 kHz stereo PCM has **4,680,761 frames**, RMS **3,592.502**, last-ten-second RMS **3,434.537** and peak
  **28,775**. Windowed PCM has **5,073,498 frames**, RMS **3,540.663**, last-ten-second RMS **2,858.348** and peak
  **32,766**. Output is active and audio fixtures pass; neither subjective listening nor bit-exact timing is claimed.

F2F received the approach, fixture/route results and scope limits with TMB/VP2 tags. Both lanes were checked for
DC-Recomp, @all and @WindowsRecomp mentions; relevant hierarchy/time-service reports were acknowledged. A reusable
native epoch/resolution boundary was raised with VP2 for potential BozoEngine work, without assuming alarm or
IRQ/deadline support. Root DC edits remain local and uncommitted. MGDraw's generic packet cursor/reservation,
frame setup/submission, other mixed builders, legacy arena extents, unsupported readbacks and broader gameplay
coverage remain separate audits. This batch does not claim those remaining paths are retired.

## Frame-end completion without packet dependencies

The accumulated cleanup above is published in DC [PR #2](https://github.com/TheBozos/DC_Recomp/pull/2), merged main
`6fb454075cafe0112b3c01e5cdacffa0006929d8` after CI passed. Subsequent tested batches are published separately.

`MGEndFrame` now continues directly to native visibility after its diagnostic bookkeeping, skipping packet
End/Terminate and the always-idle graphics-path sync/error branch. Those operations had no native rendering
consumer. Its later native SDK completion call remains in place and still uses the original channel handle;
the obsolete packet-object lookup is removed and its unused source argument is zero. Completion IRQ/callback
behavior is preserved rather than dropping the SDK submission. Frame pacing, counters, display/asset policy,
visibility, screenshots and buffer toggling outside these spans remain unchanged.

Evidence in `build/native-cleanup9/`:

- **24/24 suites pass** (`frame-tests.log`). Eight isolated close-span cases cover alignment and open-tag states:
  the reference runs actual compiled packet End/Terminate and the real engine's always-idle SyncPath; the adapted
  span makes none of those calls and leaves all guest RAM unchanged, with an inaccessible packet pointer.
- Twenty-four submit-span cases cover counter overflow, buffer states and masked/enabled callbacks against the
  actual BozoEngine native SDK send. Both versions submit exactly once on the same channel. Completion state
  matches, enabled callbacks are queued and masked completions remain pending until enabled. Frame counter,
  opaque channel handle and buffer toggle match; all other guest RAM and the retired device bank stay unchanged.
  Both reference/native versions retain the previously published channel-tag adaptations; only the new packet
  dependency changes. These are isolated-span checks, not whole-MGEndFrame CPU/clock equivalence claims.
- Both **5,800-frame headless/windowed routes complete**, with zero register/unknown accesses, zero VU0 calls and
  zero GIF SDK submissions. Headless retains 5,686 VIF1 native completion submissions and 6,884 sync calls;
  windowed reports 6,073 and 7,271 respectively. These remaining SDK jobs are native completion contracts.
- Directly inspected all seven final captures: attract (2,000), Norune (3,400), pause (3,550), running (4,800),
  movement (5,100), menu (5,500) and dismissal (5,799). Headless 48 kHz stereo PCM has 4,680,762 frames, RMS
  3,592.501 and last-ten-second RMS 3,434.538; windowed has 5,074,257 frames, RMS 3,566.111 and last-ten-second
  RMS 2,862.050. Audio remains active; no subjective listening, bit-exact timing or console parity is claimed.

BozoEngine stays clean on published `4a8f70a`; this uses its existing completion API and adds no engine change.
F2F received publication status and the callback-preservation checks. Generic MGDraw cursor/reservation, remaining
frame setup/display builders, legacy extents and broader game coverage remain open; full campaign is unqualified.

## Native frame dispatch and unused MDT copy retirement

`MGDraw` and the `CFrameVu1` packet overload now call the original raw virtual frame method with no console
output buffer. The resident vtable audit found base `CFrame` and `CFrameVu1` methods; unexpected methods reject
explicitly. Their compiled hierarchy, LOD, matrix and lighting calculations remain in place. Raw frame code only
forwards the output pointer to material/visual/child draws, whose native implementations emit zero words; the
wrapper rejects a nonzero completed result. The null `MGDraw` case retains its void-return behavior. Packet
alignment, cursor lookup and reservation are retired from these two outer wrappers.

Two guarded offline adaptations also skip the unused console-buffer `memcpy` in the raw `CVisualMDTVu1` draw
wrapper. Buffer selection, temporary allocation extent, all eight native draw arguments, signed draw result and
the post-draw reload of the current buffer index remain intact. The native renderer reads the retained MDT asset,
not that output buffer. The distinct MDT packet overload was already a metadata-only wrapper and is unchanged.
This uses existing engine APIs; the BozoEngine pin stays `4a8f70a`.

Evidence in `build/native-cleanup10/`:

- **27/27 suites pass** (`tests.log`). Each frame wrapper has **32 original/native cases** using the owner's
  hash-checked original code and actual compiled packet helpers around controlled virtual methods. The native
  dispatcher matches the frame/RenderInfo inputs and logical callback side effect, preserves saved registers and
  return/stack, and leaves every other RAM byte untouched even with inaccessible packet addresses. Null dispatch,
  unknown virtual methods and nonzero output results are checked separately. These tests isolate wrapper behavior;
  they do not independently qualify the retained hierarchy/light arithmetic.
- **386 MDT cases** compare original/adapted code using exactly the two production patch words. These cover both
  buffers, four sizes and arena offsets, copy flags, callback-driven buffer-index changes and signed draw results.
  Allocation extent, draw inputs, logical state and all non-stack RAM match after excluding only the original
  unused output copy. Native buffer addresses can be inaccessible. The retained allocator is the compiled original.
- Both **5,800-frame routes complete** (`headless.log`, `gameplay.log`) with zero register/unknown accesses, zero
  VU0 microprogram calls and zero GIF SDK submissions. The last published counter blocks record **40,869 headless /
  63,453 windowed MGDraw dispatches**. The member overload counter is zero: its qualification is fixture-only in
  this route. No separate live MDT-wrapper coverage count was collected. Remaining native VIF1 completion jobs
  number 5,686 headless / 6,063 windowed; sync calls number 6,884 / 7,261. These are bounded observations, not a
  performance or exact-clock comparison.
- Directly inspected all seven captures: attract, Norune, pause, running with foot dust, movement, menu and
  dismissal. Headless 48 kHz stereo PCM has 4,680,762 frames, RMS 3,592.501, last-ten-second RMS 3,434.538 and peak
  28,775. Windowed PCM has 5,074,790 frames, RMS 3,559.784, tail RMS 2,869.712 and peak 32,766. Output remains active;
  no subjective listening, bit-exact timing, console parity or full-campaign claim is made.

F2F received the scope and test progress; both lanes were checked for DC-Recomp, @all and @WindowsRecomp mentions.
TMB's typed hierarchy/light work remains separate, and VP2's newer generic engine scheduling/CD work is acknowledged
without changing this batch's pin. Remaining frame setup/display builders, mixed console-data builders, legacy
arena extents, unsupported readbacks and broader gameplay still need auditing.

## Message drawing and static program-chain retirement

Six guarded offline words retire three more unused packet spans. `ClsMes::DrawMesWin` skips its packet-only
TEXFLUSH envelope after the retained `MyTextureMake` call. Text layout, glyph/highlight/pulse calculations,
texture lookup/selection, message state and native rendering helper calls remain compiled from the original
function. `EditLoop` and `BattleMenuDraw` skip calls of the static VIF/VU1 program chain at `0x246100`; native
rendering does not upload or execute that program. Scene stores/`EdSetFlag` before and after the edit span,
and menu camera setup/texture reload around the menu span, remain in place. No shared engine change is required;
the BozoEngine pin stays `4a8f70a`. The frame packet reset remains because other mixed producers still use it.

Evidence in `build/native-cleanup11/`:

- **30/30 local suites pass** (`tests.log`). **192 whole-message cases** compare original/adapted code across
  early exits, balloon/background modes, pulse boundaries, valid/invalid glyphs, highlights, suppression and
  layout variants. Texture, layout and rendering helpers are controlled callback boundaries; original scalar
  layout/glyph/message calculations execute in both versions. Ordered callback inputs and rectangle bytes,
  logical message/glyph state, full saved registers, stack/return, float status and non-stack RAM match.
  The reference invokes six actual compiled packet helpers when the flush span is reached; the adapted version
  invokes none, even with an inaccessible packet address. This does not independently qualify helper rendering math.
- **32 edit-span and 32 menu-span cases** execute the original packet call/terminate helpers across cursor alignment,
  open tags and input markers. The adapted versions never call them or read the packet object. Retained scene stores,
  callback arguments/order/results and all RAM effects match apart from unused console output. The isolated-span
  harness uses `s7` for its synthetic return, explicitly excluded from the saved-register assertion. This is not a
  whole-edit/menu timing-equivalence test.
- Both **5,800-frame headless/windowed routes finish** with zero register/unknown accesses, zero VU0 microprogram
  calls and zero GIF SDK submissions. Native VIF1 completion counts are 5,686 / 6,069; sync counts 6,884 / 7,267.
  Published feeder blocks total **2,877 / 2,116 message draws**, **2,520 / 2,737 edit-loop calls**, and
  **1,094 / 317 battle-menu draws**, headless/windowed respectively. These show function coverage, not exact counts
  of the skipped spans: message draws can return early. They are bounded sampled-block totals, not performance data.
- Directly inspected all seven captures through attract, Norune, pause, running with foot dust, movement,
  readable menu/help text and dismissal. Headless 48 kHz stereo PCM: 4,680,761 frames, RMS 3,592.501,
  last-ten-second RMS 3,434.535, peak 28,775. Windowed: 5,078,803 frames, RMS 3,559.437, tail RMS 2,877.821,
  peak 32,766. Audio is active; no subjective listening, exact timing, console parity or full-campaign claim.

F2F received the audit and qualification progress. Both lanes were checked for DC-Recomp, @all and @WindowsRecomp.
DC's explicit sound/system-module provider pattern and the current engine loader's path-only ABI/permissive
fallback were shared with VP2's loader owner; no overlapping implementation was started. Other console-data
builders, setup/display work, arena extents, unsupported readbacks and wider gameplay remain open.

## Loading callback without private graphics packets

Seven guarded offline words retire `VSyncCallBack_Load`'s private packet Reset/End/Terminate, its source-pointer
read before native completion, and console-only half-offset/display-swap/cache setup. Native sprites still use
the original rectangles, textures and alpha. Fade/title/stage/countdown logic, the already-native field query,
buffer-index toggle and completion/cache/sync ordering remain compiled. Countdown callbacks retain their single
final sync; callbacks that submit an image retain both syncs and exactly one native completion. The adapter still
publishes the loading picture when the main loop waits. The main frame packet reset is separate and unchanged.
BozoEngine remains pinned to `4a8f70a`; its existing native completion API is sufficient.

Evidence in `build/native-cleanup12/`:

- **31/31 suites pass** (`tests.log`). **96 whole-callback cases** cover two logical field parities, both buffer
  indices, masked/enabled callbacks, already-ended callbacks, positive/negative countdowns, logo/title stages,
  fade bounds and hold counters. Both versions retain the two published field-query adaptations, and invoke
  DC's actual native parity service. The reference runs the compiled original packet/half-offset/swap/float-convert
  helpers against the real native SDK completion/sync services. Only sprite submission is a controlled boundary;
  its texture/rectangle/alpha inputs and logical effects are compared. Adapted code performs no packet lifecycle
  or half-offset/swap setup, even with inaccessible private buffer pointers. Saved registers/stack/return, float
  status and all non-stack RAM match after retaining logical state and excluding the original unused packet/display
  writes. The adapted private packet/display records stay untouched. Retired channel/status banks also stay untouched.
- Native submission count/channel and completion state match; enabled callbacks queue, and masked completions
  remain pending until enabled. The initial fixture assertion incorrectly expected two syncs for countdown-only
  callbacks; the actual original makes one. The corrected assertion checks that distinction in both versions.
  No production sync was removed to satisfy the test, and no whole-callback clock-equivalence claim is made.
- Both **5,800-frame headless/windowed routes finish**, with zero register/unknown accesses, zero VU0 microprogram
  calls and zero GIF SDK submissions. Native VIF1 completion counts are 5,686 / 6,045; sync counts 6,884 / 7,243.
  Each run records 41 sampled loading-state rows: 28 with the SCE-logo texture and 13 with the map-title texture,
  including fade progression. These establish real callback coverage, not exact callback or draw totals.
- Inspected ten captures: Sony presentation (100), black transition (300), attract (2,000), load confirmation
  (2,800), Norune (3,400), pause (3,550), running (4,800), movement (5,100), menu (5,500) and dismissal (5,799).
  Map-title fade coverage comes from the state probe, not a dedicated picture capture. Headless 48 kHz stereo PCM
  has 4,680,761 frames, RMS 3,592.501, tail RMS 3,434.535 and peak 28,775. Windowed has 5,076,361 frames, RMS
  3,537.832, tail RMS 2,850.981 and peak 32,766. Audio remains active; no subjective listening, exact timing,
  console parity or full-campaign claim is made.

F2F received the scope, conditional-sync correction and qualification progress. Both lanes were checked for
DC-Recomp, @all and @WindowsRecomp; TMB shader and VP2 transfer/loader work remain independently owned.
Other mixed builders, setup/display producers, legacy extents, unsupported readbacks and wider gameplay remain open.

## Texture-animation flush envelope retirement

Four guarded offline words retire the leading and trailing TEXFLUSH packet envelopes in
`CTextureAnime::TexAnime` (`0x167170`). The function's compiled 24-slot passes, dependent-slot Enable calls,
group selection, direct/additional copy inputs, scrolling arithmetic, pause policy, counters and keyframe chains
remain in place. The native `MGMoveImage` adapter still receives the original texture identifiers and rectangles.
Both new jumps validate their original delay slots, which only zero a caller-volatile argument. No runtime
packet parsing or shared engine change is introduced; BozoEngine remains pinned to `4a8f70a`.

Evidence in `build/native-cleanup13/`:

- **32/32 suites pass** (`tests.log`), including **432 whole-function original/adapted cases**. These cover empty,
  sparse and full slot sets, absent entries, matching/mismatched/negative groups, direct/additional copies,
  scrolling at negative/zero/interior/overflow offsets, paused/unpaused updates, indefinite/finite/zero holds,
  counter overflow and chained keyframes. The reference executes the actual compiled packet builders, Enable
  and float conversion. The native-copy helper is a controlled boundary: ordered texture-identifier, rectangle
  and destination inputs and a logical copy side effect are compared. Across both variants the cases reach
  7,872 copy and 2,016 Enable calls; these are fixture counts, not gameplay or performance totals.
- Each reference case invokes twelve packet helpers; each adapted case invokes zero, even with an inaccessible
  packet pointer. Complete saved registers, stack/return, floating-point status, logical animation state and all
  other non-stack RAM match after excluding the original unused packet output. The adapted packet region remains
  untouched. This qualifies the wrapper and retained arithmetic, not the controlled helper's texture-copy math.
- Both **5,800-frame headless/windowed routes complete** (`headless.log`, `gameplay.log`) with zero device-register
  or unknown accesses, zero VU0 microprogram calls and zero GIF SDK submissions. Remaining native VIF1 completion
  counts are 5,686 / 6,034 and sync counts 6,884 / 7,232. Texture-animation probes record 667 / 898 sampled keyframe
  rows, including both direct-copy and scrolling kinds; native-copy probes record 5,792 / 9,744 rows. These confirm
  bounded live coverage, not exact TexAnime call counts or performance.
- Directly inspected ten captures: Sony presentation (100), fading logo (300), attract (2,000), load confirmation
  (2,800), Norune (3,400), pause (3,550), running (4,800), movement (5,100), readable menu/help (5,501) and dismissal
  (5,799). The requested 5,500 menu capture was written at 5,501. No pixel-parity or full-game claim is made.
- Headless 48 kHz stereo PCM contains 4,680,762 frames, RMS 3,592.501, last-ten-second RMS 3,434.535 and peak 28,775.
  Windowed PCM contains 5,076,393 frames, RMS 3,539.924, tail RMS 2,849.656 and peak 32,766. Audio output is active;
  subjective listening and bit-exact timing remain unqualified.

Texture identifiers still use the game's console-format fields at the native-copy boundary; the adapter
translates those identifiers to native texture records without GS memory or packet reads. Other mixed builders,
the main packet reset, console setup/display producers, legacy allocation extents, unsupported readbacks and
broader gameplay remain separate audits. Full campaign and console visual parity remain unqualified.

F2F received the scope, original/native fixture results and live qualification. Both lanes were checked for
DC-Recomp, @all and @WindowsRecomp; TMB's macro/scalar audit and VP2's compiled module-provider work remain
independently owned. No retail assets or generated game code enter the repository.

## Fire sprite state envelope retirement

Six guarded offline words retire the two conditional GS-state envelopes and the final restore envelope in
`CFireOmni::DrawFire` (`0x161ac0`). Its existing native wrapper supplies the additive blend; native fog sprites
receive the retained texture, source rectangle, projected corners, colour/alpha and fog data directly. Texture
initialization, both mode bits, compiled camera/vector math, counter/PRNG updates and projection gates remain.
All three jumps guard their harmless caller-volatile zero delay slot. The main packet reset and unrelated
producers are unchanged. No shared service change is required; BozoEngine remains pinned to `4a8f70a`.
`DC_PROBE_FIRE=1` enables bounded coverage samples without changing default rendering.

Evidence in `build/native-cleanup14/`:

- **33/33 suites pass** (`tests.log`), including **512 whole-function original/adapted cases** across all four
  combinations of the two relevant mode bits, four texture initialization/cache profiles, four counter values
  including wrap, signed seeds, zero/negative/positive scales and eight projection-acceptance masks. The reference
  executes the actual compiled packet builders, camera/vector helpers and `srand`/`rand`. Texture lookup,
  projection and native fog-sprite submission are controlled boundaries comparing ordered semantic inputs and
  logical sprite side effects. Across both variants these cases reach 768 sprite and 128 random calls.
- Each reference executes nine packet helpers per enabled leading envelope plus nine for the final restore.
  The adapted version invokes none, with an inaccessible packet pointer. Logical fire/PRNG/sprite state, complete
  saved registers, stack/return, saved float registers and floating-point status match. All other non-stack RAM
  stays unchanged after excluding original unused packet output; adapted packet storage stays untouched. A strict
  host rejects unexpected device/library/VU0-program accesses. This qualifies the wrapper, not the controlled
  projection or native renderer's math, and does not assert whole-function clock equivalence.
- Both **5,800-frame headless/windowed routes complete**, with zero device-register or unknown accesses, zero VU0
  microprogram calls and zero GIF SDK submissions. Native VIF1 completion counts are 5,686 / 6,074 and sync counts
  6,884 / 7,272. Fire probes produce 38 / 42 sampled rows, both with mode 15 (the two retained low bits enabled),
  reaching uncached textures, initialized textures and counters 0 through 4. These confirm bounded function/state
  coverage, not exact calls, visible fire pixels or performance totals.
- Inspected all ten captures: Sony presentation (100), black transition (300), attract (2,000), load confirmation
  (2,800), Norune (3,400), pause (3,550), running (4,800), movement (5,100), readable menu/help (5,500) and dismissal
  (5,799). No dedicated fire-only picture or pixel-parity qualification is claimed.
- Headless 48 kHz stereo PCM contains 4,680,762 frames, RMS 3,592.501, last-ten-second RMS 3,434.534 and peak 28,775.
  Windowed PCM contains 5,085,629 frames, RMS 3,567.576, tail RMS 2,838.462 and peak 32,766. Audio remains active;
  subjective listening, exact timing and full-campaign/console parity remain unqualified.

F2F received the audit, fixture correction and qualification results. Both lanes were checked for DC-Recomp,
@all and @WindowsRecomp. TMB's separate projection-math work and VP2's native module/semaphore work remain with
those owners. Other mixed console builders, main packet reset, setup/display producers, legacy extents,
console-format native-copy identifiers, unsupported readbacks and broader gameplay remain open. No retail data
or generated game code is committed.

## Texture-copy owner flush retirement

Eight guarded offline words retire four unused TEXFLUSH envelopes in `setTexAnim` (`0x19f9b0`) and
`setItemToReserved` (`0x1b1ef0`). The animation's phase increment/wrap, float conversion, conditional copy and
both texture lookups remain compiled. The item owner retains its texture-identifier bit extraction, 32-by-32
source rectangle and every scalar/stack copy argument. Both helpers still call the existing native copy
adapters. No engine change is required; BozoEngine stays at `4a8f70a`. Console-format identifiers still cross
those native adapters; no GS memory or runtime packet read is introduced.

Evidence in `build/native-cleanup15/`:

- **35/35 suites pass** (`tests.log`). The two new whole-function differential fixtures cover **144 animation**
  and **96 item-copy cases** with real compiled packet builders and float conversion, controlled texture lookup
  and native-copy submission. After adding source/destination texture-alias cases, the four matching state-wrapper
  suites were rebuilt and passed again (`alias-tests.log`); production code did not change after the full run.
- Cases cover absent/present textures, aliased/distinct records and name pointers, phase values below/at/above
  the wrap threshold, negative/zero/positive increments, signed/large rectangle coordinates and varying identifier
  fields/high bits. Original item behavior has no missing-texture guard; its zero-page input is compared as-is,
  without adding a fallback. Ordered lookup/copy data match, including arguments on the stack. Across both variants
  there are 72 animation-copy and 192 item-copy calls. These are fixture totals, not live gameplay counts.
- The original animation emits six leading packet helper calls, with six trailing calls only when both textures
  exist; the item owner emits twelve. Adapted versions emit none, with inaccessible packet pointers. Saved full
  registers/stack/return, floating-point status and logical phase/copy side effects match. All other non-stack RAM
  stays unchanged after excluding original packet writes; adapted packet storage remains untouched. A strict host
  rejects unexpected device/library/VU0-program access. The copy implementations' math and clock timing are not
  qualified by these controlled boundaries.
- Both **5,800-frame headless/windowed routes complete**, now opening the item inventory before returning to the
  menu and gameplay. Both report zero register/unknown accesses, zero VU0 microprogram calls and zero GIF SDK
  submissions. Native VIF1 completion counts are 5,686 / 6,064; sync counts are 6,884 / 7,262. Neither new owner
  appears in the sampled feeder reports, so their execution is **fixture-qualified only**; opening inventory is
  not evidence that `setItemToReserved` ran.
- Inspected twelve captures: presentation (100), black transition (300), attract (2,000), load confirmation (2,800),
  Norune (3,400), pause (3,550), running with foot dust (4,800), movement (5,100), menu (5,400), inventory with item
  icons/Toan preview (5,550), returned menu (5,700) and dismissal (5,799). No pixel-parity claim is made.
- Headless 48 kHz stereo PCM contains 4,686,527 frames, RMS 3,599.338, last-ten-second RMS 3,496.547 and peak 28,775.
  Windowed PCM contains 5,089,517 frames, RMS 3,563.919, tail RMS 3,018.528 and peak 32,766. Audio remains active;
  subjective listening, exact timing and full-campaign/console parity remain unqualified.

The C2C/F2F server went down after the preceding fire batch's qualification post. The user confirmed the server
crashed and will notify when it returns; the fire merge confirmation and this batch's updates are queued locally
in ignored build evidence. No peer engine work is taken over. Remaining mixed builders, main packet reset,
setup/display work, legacy extents, native-copy identifiers and unsupported readbacks remain separate audits.
`FishLineDraw` mixes real projected XYZ/ADC geometry into its AD stream and needs a native drawing boundary,
with pole/model math retained; it cannot be retired as an unused state envelope. No retail assets or generated
game code are committed.

## Language-loop static chain retirement and terrain investigation

Two guarded offline words skip `LangsetLoop`'s (`0x244c70`) append/terminate of the unused static VIF/VU1 chain
at `0x246100`. Its complete fade/input/drawing state machine and selected-language completion remain compiled.
The original chain-address load in the new jump's delay slot becomes a NOP. No shared runtime API or engine-pin
change is involved; BozoEngine remains at `4a8f70a`.

Evidence in `build/native-cleanup16/`:

- **36/36 suites pass** (`tests.log`), including **480 whole-function original/adapted cases** over invalid/valid
  states, fade boundaries, selected-language values and input results. The actual compiled packet and fade helpers
  execute; input/draw helpers are controlled boundaries with compared order, inputs and logical side effects.
  The reference appends/terminates once; adapted code makes no packet calls with an inaccessible packet pointer.
  Saved full registers/stack/return, complete return value, float status and all non-stack logical RAM match.
  Completion retains `selected language + 2`, including unsigned overflow. This does not qualify the controlled
  helpers' drawing math or whole-function clock equivalence.
- The original **5,800-frame headless and windowed routes finish**, with zero register/unknown accesses, zero VU0
  microprogram calls and zero GIF SDK submissions. A further adapted **5,800-frame windowed requalification** also
  completes with those zero counters, 6,057 native VIF1 completion jobs and 7,255 sync calls. Initial headless/window
  counts were 5,686 / 6,071 completions and 6,884 / 7,269 syncs. The language loop is absent from sampled feeder
  reports; this owner's execution remains **fixture-qualified only** on the tested route.
- Unchanged/adapted **3,500-frame headless records are byte-for-byte identical**, including the complete header
  and all 80-byte frame records, with no masks or excluded fields. The recorded RAM digest covers all 32 MiB;
  scratchpad, native draw output/counts, sound/counts, scheduler and clock fields also match. Both 280,240-byte files
  have SHA-256 `0ca305ae3b446d9e04a73466c47cbcec54cda32acfe2363a8ce99d6ae392cc45`
  (`baseline-record.bin`, `current-record.bin`, `record_check.py`). This establishes deterministic-route agreement,
  not real-time window or console pixel parity.
- Directly inspected all eight final requalification captures: Norune (3,400), pause (3,550), running (4,800),
  movement (5,100), menu (5,400), inventory with item icons/Toan preview (5,550), returned menu (5,700) and dismissal
  (5,799). Terrain is present in this run. Earlier adapted captures show missing terrain; an earlier repeat also
  reproduced it, while later adapted runs and initial unchanged-baseline repeats show terrain. A final paired
  3,500-frame window check reproduced **the same missing terrain in the unchanged baseline**, with terrain present
  in the adapted build (`baseline-paired/frame_003400.png`, `current-paired/frame_003400.png`). Both complete with
  zero register/unknown/VU0/GIF counters. The baseline executable was built with these two adaptations removed;
  its preparation/restore scripts and logs are retained locally. This demonstrates that the terrain failure can
  occur without this cleanup. **Its real-time cause remains unresolved**; successful repeats are not a fix or a
  pixel-parity claim. Preserve both failed and successful captures/logs.
- Headless 48 kHz stereo PCM contains 4,686,527 frames, RMS 3,599.338 and last-ten-second RMS 3,496.547. The final
  windowed requalification contains 5,094,421 frames, RMS 3,568.480 and tail RMS 2,988.911, peak 32,766. Audio is active;
  no subjective listening, exact timing or full-campaign qualification is claimed.

This batch does not remove the main packet reset, mixed packet/geometry producers, legacy allocation extents or
console-format native-copy identifiers. `FishLineDraw` still needs a native geometry boundary. Terrain triage,
unsupported readbacks, broader gameplay and console visual parity remain open. Coordination updates remain queued
locally during the user-confirmed C2C/F2F outage; no peer engine work is taken over. No retail assets/generated code
are committed.

## Static model source lifetime and Norune terrain

`modelOf` previously switched a captured static model back to its original MDT address when the first 64 bytes
looked unchanged. That is not an ownership check: the game can reuse the source allocation while leaving its
header intact. Reading a replaced primitive table then drops the terrain's four primitives. A static
`CVisualVu1` retains its built geometry, not an MDT pointer (its original `GetMDTDataAddress` returns zero).

Static visuals now always draw their captured model. Explicit `CVisualMDTVu1` visuals still use their retained
live MDT for vertex animation, and the original Remake boundary refreshes captured geometry. Texture bindings,
allocation extents and game RAM are unchanged. No packets, device backend or engine change is introduced;
BozoEngine remains pinned to `4a8f70a`. Temporary runtime diagnostics were removed before the qualified build.

Evidence in `build/native-cleanup17/`:

- The synthetic native-renderer regression fails **two checks before the fix**, then passes after it
  (`lifetime-before.log`, `tests.log`). It builds one actual native triangle, reuses the source body while keeping
  the complete header, and checks the published draw digest/vertices. Unrelated source vertex changes must not
  change a static visual. Explicit Remake must change its captured geometry, complete source release must preserve
  it, and an explicit MDT visual must still reflect live vertex animation. The fixture contains no retail data.
- **37/37 local suites pass**. A separate disc-free configure/build also runs the lifetime regression successfully
  (`disc-free.log`). CI now configures that small project, retaining the existing engine tests and adding this
  native-renderer regression without requiring a disc, generated game functions or a window.
- A controlled in-game source-reuse probe reproduces the original missing-terrain picture in the saved baseline
  and leaves the terrain visible in the fixed executable (`reproduced-before/frame_003400.png`,
  `reproduced-after/frame_003400.png`). Both runs retain the terrain MDT's original 64-byte header and invalidate
  only its old source primitive count; saved RAM confirms that count is zero in both. Each completes 3,500 frames
  with zero register/unknown accesses, zero VU0 microprogram calls and zero GIF SDK submissions. These are deliberate
  fault-injection runs, not spontaneous bad-state RAM captures. The actual before/after images were inspected.
- Normal **5,800-frame headless/windowed inventory routes complete** with those same zero counters. Native VIF1
  completion counts are 5,686 / 6,078; sync counts are 6,884 / 7,276. Eight window captures are retained through idle,
  pause, running, movement, menu, inventory, returned menu and dismissal. The controlled reproduction, rather than
  a successful normal repeat alone, establishes the source-lifetime correction.
- Headless 48 kHz stereo PCM contains 4,686,527 frames, RMS 3,599.338, tail RMS 3,496.547 and peak 28,775. Windowed
  PCM contains 5,087,603 frames, RMS 3,582.394, tail RMS 3,008.184 and peak 32,766. Audio remains active; subjective
  listening, exact timing, console visual parity and a full campaign remain unqualified.

The earlier failed captures remain useful evidence; this fixes the demonstrated static-source lifetime path,
not every possible rendering defect. Other mixed builders, main packet reset, legacy extents, native-copy
identifiers, unsupported readbacks and FishLineDraw geometry remain separate work. Coordination updates remain
queued locally during the C2C/F2F outage. No retail assets, RAM captures or generated code are committed.

## Native fishing-line geometry

`FishLineDraw` (`0x1ab0f0`) previously placed its actual projected line geometry into an unused console output
stream. Nine guarded offline entries now bypass its two packet getters, leading state envelope, coordinate
packing/append and trailing state/close calls. The inline native boundary at `0x1ab290` receives the unpacked
stack point, loop index and already-computed connect/skip decision. All 24 compiled projections, threshold/mode
branches, previous-projection acceptance updates, vector math, texture selection and pole/model calls remain.
No packet output or console state is read by the native boundary; no engine change or pin update is required.

Native segments follow the existing `DrawLine` convention: one logical-pixel-wide untextured quads, depth-tested
without depth writes, using the current native alpha blend. Each call starts a fresh strip; skipped connections
still advance its endpoint, and zero-length segments emit nothing. Invalid indices/decisions or nonsequential
entries fail explicitly. This geometry translation does **not** establish console line rasterization/material
parity or qualify fishing gameplay.

Evidence in `build/native-cleanup18/`:

- The final complete native suite passes **39/39** (`final-tests.log`), including both fishing suites. The disc-free project
  also passes **14/14**, including the new boundary suite, and CI builds this same game-free project (`disc-free.log`).
- **144 original/adapted whole-function cases** cover both modes, all pole/model enable combinations, rejected,
  accepted and alternating projection masks, threshold boundaries, repeated points and edge coordinates. The
  original packet helpers and retained vector math execute their compiled implementations. Texture/model and
  projection services are controlled boundaries; their ordered semantic inputs and logical effects match.
  All 24 unpacked points and connection decisions match the reference's emitted coordinates. The adapted loop
  executes no packet helper/getter with an inaccessible global packet pointer and untouched packet guards.
- The comparison checks **292 native segments** in the actual published renderer frames: connection counts,
  endpoint centers/depths, six vertices per segment and one-pixel width. Saved full registers, stack/return,
  floating-point status and all non-stack logical RAM match. The inline draw preserves the complete register
  context. The separate game-free suite also checks signed coordinates, depth truncation, color, skipped and
  degenerate segments, fresh-call resets, explicit invalid-entry failures and no native RAM writes.

- Both **5,800-frame headless/windowed inventory routes complete** with zero register/unknown accesses, zero
  VU0 microprogram calls and zero GIF SDK submissions. Native VIF1 completion counts are 5,686 / 6,051 and sync
  counts 6,884 / 7,249 (`headless.log`, `gameplay.log`). Neither sampled feeder report includes `FishLineDraw`;
  these routes do not qualify its gameplay execution. Eight window captures are retained but were not visually
  inspected in this batch; no new visual-parity claim is made.
- Headless 48 kHz stereo PCM contains 4,686,527 frames, RMS 3,599.338, tail RMS 3,496.547 and peak 28,775.
  Windowed PCM contains 5,093,752 frames, RMS 3,568.074, tail RMS 2,992.469 and peak 32,766. Audio is active;
  subjective listening and exact audio timing remain unchecked (`evidence.py`).

Fishing itself remains fixture-qualified until a live fishing route and its captures are checked. The main packet reset, other mixed
builders, legacy allocation extents, console-format native-copy identifiers and unsupported readbacks remain
separate work. No retail assets or generated code are committed.

## Shadow wrapper state and static-chain retirement

Twelve guarded offline words bypass the leading TEST/ZBUF construction and prebuilt program-chain calls in
`MGDrawShadowFast` (`0x1303b0`), `MGDrawShadowFast2` (`0x1304d0`) and `MGDrawShadow` (`0x1305f0`), plus their trailing
default-chain calls. Their null-frame exits, two ordered vector copies, mode 1/2 selection, native `MGDraw`, mode
reset and (for the non-fast owner) both camera-matrix saves/restores remain compiled. The native draw already
consumes the retained game data; it never consumed these program chains. `MGDrawShade`, the native blend-preserving
wrapper and the engine pin (`4a8f70a`) are unchanged. No new runtime hook, device path or completion mechanism is added.

Evidence in `build/native-cleanup19/`:

- **42/42 local suites pass**, including native GPU readback (`tests.log`).
- Three whole-function original/adapted suites pass **384 comparisons** (128 per owner), using the exact production
  patch subset. Original vector/matrix-copy and packet-call helpers execute their compiled implementations. The
  two published native packet-only setters are controlled no-ops; native model drawing is a controlled boundary
  with compared mode, frame, vector/matrix inputs and logical effects. This does not qualify that boundary's
  rendering math or console visual parity.
- Cases cover null/present frames, four vector-alias arrangements (including sequential overwrite), varied 128-bit
  vector/matrix patterns, default TEST/ZBUF values and packet alignment/open state. The draw deliberately changes
  both matrices; the non-fast owner restores both, while the fast owners retain their original effects. Reference
  owners call two state setters and two static chains only for a non-null frame; adapted owners call none, with an
  inaccessible packet pointer and untouched guards. Both paths make 384 controlled model draws across the fixtures.
- Ordered retained helper inputs, all non-stack logical RAM, full saved registers/stack/return and floating-point
  status match. No unexpected library, device or VU0 microprogram call is accepted by the fixtures.
- Both **5,800-frame headless/windowed inventory routes complete**, with zero register/unknown accesses, zero VU0
  microprogram calls and zero GIF submissions (`headless.log`, `gameplay.log`). Native VIF1 completion counts are
  5,686 / 6,003 and sync counts 6,884 / 7,201. Neither sampled feeder report includes these three owners, so their
  execution remains fixture-qualified. Eight window captures are retained but were not visually inspected here.
- Headless 48 kHz stereo PCM contains 4,686,527 frames, RMS 3,599.338, tail RMS 3,496.547 and peak 28,775. Windowed
  PCM contains 5,058,493 frames, RMS 3,569.191, tail RMS 2,918.091 and peak 32,766 (`evidence.py`). Audio is active;
  subjective listening, exact timing and a complete playthrough remain unchecked.

The known shadow-plane/rendering defect remains open: retiring these unused state chains is not a visual fix.
The main packet reset, remaining mixed producers, legacy extents, native-copy identifiers and unsupported readbacks
remain separate audits. No retail files, generated code or RAM captures are committed.

## Native debug-font picture composition and reset audit

`CDebugFont::Draw` (`0x13df40`) clears a destination picture, copies 8-by-16 glyphs from `ankfnt24`, then displays
that picture. Four resident instruction boundaries now capture the looked-up atlas, initialize the destination,
copy a glyph and publish the completed native texture. Fourteen guarded offline words bypass the old transfer,
state, descriptor-packing and flush/close blocks. All three original texture lookups, compiled space/newline/cursor
logic, count reset and final native sprite remain. The frame-texture query also remains: its native bookkeeping
must survive even though this owner's later packet-only descriptor packing is unused. The query's compatibility
descriptor output is still written to scratch storage; native-copy identifiers remain a separate audit.

Compositions own their decoded source/pixels and are keyed by runtime and guest stack across scheduler unwinds.
Completed pictures are immutable. Atlas/destination identity is captured at lookup, so a record rebound before
completion cannot acquire the previous asset's picture under its new identity. A valid native picture also takes
priority over a stale frame-target association when the retail record has no CPU pixel buffer.

The supported source/destination pictures are true-color CT32/CT24. RGB copies retain destination alpha for CT32;
CT24 uses the retained TA0/AEM rule. Extents and glyph bounds are checked. Indexed pictures, atlas/destination
place aliases and missing composition setup fail explicitly; no graphics-memory interpreter or
fallback is added. Newly allocated native destination storage starts blank outside the explicitly cleared/copied
region. This is not a claim of console rasterization or shared-VRAM alias parity.

Evidence in `build/native-cleanup20/`:

- **44/44 local suites** and **15/15 game-free suites** pass (`qualified-tests.log`, `qualified-disc-free.log`),
  including GPU readback and both new font suites. The game-free font regression is included in CI.
- **192 whole-function original/adapted comparisons** cover CT32/CT24 source/destination combinations, three alpha
  values, empty/mixed/extended-byte text, spaces, newlines, clipping and partial clears. The real compiled append
  helper executes in the reference; its **1,032 glyph-copy rectangles** form an independent texel oracle. Texture
  lookup, frame query and sprite calls are controlled boundaries with compared semantic inputs/order. The adapted
  final sprite also executes its actual native hook, exposing the composed texture in the published draw list;
  every texel matches. This does not qualify those controlled services' own implementation or console GPU output.
- Count/TEXA bookkeeping and all other non-stack logical RAM match; packet guards remain untouched with an
  inaccessible packet pointer. Saved full registers/stack/return and outgoing float status match. Every inline
  pixel boundary preserves the complete CPU context. The original append/envelope calls are absent from adapted
  execution, while all three lookups and the native frame query remain.
- The separate synthetic suite interleaves two calls and resumes copied CPU contexts outside any outer draw scope.
  It checks independent images, immutable previous frames, rebinding a record before completion, a generated
  picture with no CPU buffer sharing an old frame identifier, no pixel/packet RAM writes, and explicit invalid
  setup/extent/format/alias failures. All fixture assets are synthetic.
- The final **5,800-frame headless/windowed inventory routes complete**, with zero register/unknown accesses, zero
  VU0 microprogram calls and zero GIF submissions (`qualified-headless.log`, `qualified-gameplay.log`). Native VIF1
  completion counts are 5,686 / 6,084 and sync counts 6,884 / 7,282. The debug-font owner is absent from the sampled
  feeder reports, so its execution remains fixture-qualified. Eight final window captures are retained without
  a new visual-parity claim; they were not visually inspected in this batch.
- Headless 48 kHz stereo PCM contains 4,686,527 frames, RMS 3,599.338, tail RMS 3,496.547 and peak 28,775. Final
  windowed PCM contains 5,080,769 frames, RMS 3,579.984, tail RMS 2,985.728 and peak 32,766. Audio is active;
  subjective listening, exact timing and a full playthrough remain unqualified (`qualified-evidence.py`).

The local instruction/call audit (`packet_audit.py`, `reachable-packet-callers.txt`) also examines both overlay
images. It finds statically reachable packet calls in resident `main`, `MGInitVif1Packet`, `MGBeginFrame` and
`init_now_loading`, plus TITLE draw/scroll/face-animation/title-loop owners and DUN `MainDraw`/`LoaderLoop`.
The audit accounts for the current guarded resident jumps; it is not a whole-program proof through indirect
calls or all runtime conditions. Existing native replacements still cover many other listed retail producers.
These remaining paths are why the global reset is retained. No retail assets or generated code are committed.

## Image-scoped TITLE and DUN packet retirement

The next batch removes 18 packet-only envelopes in 13 overlay owners. `recomp/title_native.patches`
contains 32 guarded words; `recomp/dun_native.patches` contains four. CMake applies each list only while
compiling its owning ELF. The raw overlay identities and identity-selected module dispatch remain intact.
No runtime override is installed at an overlay address: TITLE and DUN share the same virtual address slot.
BozoEngine stays pinned at `4a8f70a5021cbb3429800a02259ae7b816581ee0`.

| Image | Owner | Retired work |
| --- | --- | --- |
| TITLE | `DrawProcess` `0x01db0780`, `0x01dc9e70` | Static VIF chain call and terminate |
| TITLE | `setTexScroll` `0x01db82d0`, `setCloudTexScroll` `0x01db8550` | Leading texture-flush envelope |
| TITLE | `FaceChange` `0x01db87d0` | Leading flush/terminate and trailing flush |
| TITLE | `setTexAnime` `0x01dbb8a0` | Leading texture-flush envelope |
| TITLE | `FaceChange` `0x01dbba90`, `0x01dc3c90`, `0x01dc8930` | Leading and trailing texture-flush envelopes |
| TITLE | `setTexAnim` `0x01dc3ad0` | Leading and trailing texture-flush envelopes |
| TITLE | `TitleLoop` `0x01dd2220` | Static VIF chain call |
| DUN | `MainDraw` `0x01dae2a0` | Static VIF chain call |
| DUN | `LoaderLoop` `0x01dc1510` | Static VIF chain call and terminate |

Every envelope becomes a guarded jump with an empty delay slot. The surrounding original code retains
texture lookup, TEX0 identifier decoding, rectangle construction, native `MoveImageTest`/`MGMoveImage`
copies, phase arithmetic, RNG, writable face tables and animation flags. Camera/draw helpers, texture
reload and debug-font composition remain at their original call sites. `MoveImageTest` and texture reload
already ignore their compatibility packet arguments. Unused static tag copies to local stack scratch
are left in place; they do not publish packet output.

Qualification uses original/adapted code generated from the same retail bytes and the exact production
patch subset (`tests/make_overlay_packet_fixture.py`, `tests/overlay_packet_cases.py`). Raw-image SHA-1
checks reject another overlay or revision. No retail code or generated fixture is committed.

- Eight small texture/face owners execute as complete original functions. Five larger draw/loop owners
  use exact mid-function spans around each retired envelope and adjacent retained calls, with a synthetic
  save/return wrapper. These spans do not exercise the complete draw/loader state machines.
- **1,904 original/adapted cases pass** across the 13 owners. They exercise absent/distinct/aliased
  textures, raw and synthetic populated face tables, selectors, phase boundaries and animation flags.
  Both versions together make 1,100 controlled native copies and 28 real compiled RNG calls. The original
  packet builders and float conversion execute compiled resident implementations. Native copy/lookup and
  adjacent camera/draw helpers are controlled boundaries whose ordered consumed inputs are compared.
- All non-stack logical RAM, native-copy rectangles/identifiers/stack arguments, saved full registers, return
  state and outgoing float status match. The adapted versions make zero packet-helper calls with an
  inaccessible packet pointer and untouched packet guards. The synthetic span wrapper reserves `s7`
  for its return address; that register is excluded from those spans' saved-register assertion.
- **57/57 local suites pass** (`build/native-cleanup21/qualified-tests.log`). Negative codegen checks
  reject DUN patches on TITLE and TITLE patches on DUN with expected-word mismatches, and fixture
  extraction rejects the other raw overlay's hash.
- The final **5,800-frame headless/windowed inventory routes complete**, with zero register/unknown
  accesses, VU0 microprogram calls or GIF submissions. Native VIF1 completion counts are 5,686 / 6,055;
  sync counts are 6,884 / 7,253 (`qualified-headless.log`, `qualified-gameplay.log`). Eight window captures
  are retained without visual inspection. The feeder probe covers resident owners, so these runs do not
  establish execution of each changed overlay owner or dungeon/combat coverage. Targeted coverage remains
  the fixtures above; no console/visual-parity claim is made.
- Final headless 48 kHz stereo PCM contains 4,686,527 frames, RMS 3,599.338, tail RMS 3,496.547, peak 28,775.
  Windowed PCM contains 5,074,570 frames, RMS 3,576.803, tail RMS 2,985.746, peak 32,766. Audio is active;
  subjective listening and exact timing remain unqualified (`qualified-evidence.py`).

The updated local CFG audit applies all three image patch lists and finds no remaining direct statically
reachable calls to the packet SDK in either overlay (`build/native-cleanup21/reachable-packet-callers.txt`).
It does not prove indirect-call or all-runtime-path coverage. Resident startup/frame/loading producers
remain, including `MGInitVif1Packet`, `MGBeginFrame` and `init_now_loading`; the global packet reset stays.

## Resident packet owners and global reset retirement

This follow-up removes the remaining admitted resident VIF1 packet initialization/reset/static-chain work.
Eight guarded words in `dc_native.patches` adapt four owners:

| Owner | Adaptation | Retained behavior |
| --- | --- | --- |
| `MGInitVif1Packet` `0x12c720` | Skip two `Init` and two `Reset` calls, `0x12c7a8` to `0x12c7f0` | Original input alignment and global buffer metadata |
| `MGBeginFrame` `0x12cd90` | Remove the `Reset` call at `0x12ce90` | Frame-dependent handle selection, colors, matrix/vector initialization, clear/depth calls |
| `main` `0x1410b0` | Skip static chain call, `0x1416bc` to `0x1416d4` | Native `SetEnv` and following main-loop bookkeeping |
| `init_now_loading` `0x153fc0` | Skip packet `Init`, `0x1542b0` to `0x1542c8` | Asset-load/failure paths, setup metadata, loading state and callback registration |

`sceVif1PkReset` only reloads the current cursor from the stored base and clears the open-tag pointer.
Neither operation has a remaining admitted consumer after these changes and the preceding producer
retirements. The per-frame global reset and startup resets are therefore removed. The SDK implementations
remain compiled for explicit original-code fixtures; the production port rejects all 27 GIF/VIF1 packet
builder entries before reading guest pointers or changing guest CPU/RAM. Unexpected direct or indirect
entries now report the missing native producer instead of executing a packet builder.

The audit also separates three previously obscured cases:

- `StoreImage` `0x12c800`, the console screenshot path, read a main packet object's stored base before
  opening files, submitting DMA and eventually reaching unsupported readback. It now rejects at its owner,
  before those effects. This does **not** implement screenshots. It no longer supplies a reason to retain
  packet initialization/reset. Native renderer captures used for qualification are a separate host facility.
- `DrawMaru` `0x150bb0` and `MakeFukidashi_sub` `0x150f50` still contain retail packet geometry. Their seven
  direct incoming edges are confined to the old balloon subtree: the latter helper and `MakeFukidashi`
  `0x151ad0`. The installed native `balloonHook` already owns the complete shape/tail and does not call
  these helpers. Their legacy entries now reject explicitly, preserving that admission boundary without
  pretending to implement their standalone calling conventions.
- Packet-handle selection, `GetVif1Packet`, buffer address/alignment globals, legacy allocations and
  compatibility arguments remain. Native drawing/copy hooks ignore the packet argument. Native SDK DMA
  completion/sync services in the pinned engine do not read VIF1/GIF source bytes or retain packet lengths/
  chains. These handles are bookkeeping, not a need to rewind a writable packet. Display/setup metadata,
  native-copy identifiers and allocation extents remain separate cleanup work.

`tools/dc_packet_audit.py` validates all production patch guards, walks direct calls/tail calls/branches in
the three prepared images and compares packet callers with the **actual installed registry** exported by
`dc-packet-audit-tests`. It also checks that resident/overlay codegen declares every rejection entry external,
so a direct generated C++ call cannot bypass it. It reports **zero active direct-CFG packet callers**, 51 owners covered by native
replacement or explicit rejection, and the seven balloon edges above. It also reports 341 owners with
unresolved indirect sites. This is not a whole-program indirect-call or inline-cursor dataflow proof.
The production rejection boundaries close the packet SDK fallback; additional game routes still require
qualification and can expose unsupported paths.

Qualification:

- **62/62 local suites pass**, including 288 original/adapted comparisons: complete packet-init,
  begin-frame and loading-init functions, and one exact mid-function span in `main`. Retained float/vector/
  matrix math and callback registration execute compiled originals. Native clear/setup/asset/file calls
  are controlled boundaries comparing consumed arguments and logical effects. Coverage includes both
  frame handles, clear modes, alignment/sign edges, loading selectors, early exit and load failure.
- Adapted owners make zero packet-builder calls with poisoned packet objects/output and an inaccessible
  global handle where selection is not itself retained. All other non-stack RAM, ordered native inputs,
  saved full registers/FPU state and return behavior match. The main span reserves `s7` and legitimately
  sets `s4`; those two registers are excluded from its saved-register assertion. Retired packet storage
  and caller-volatile SDK results are intentional comparison differences; the main loop overwrites `v0`
  at its following diagnostic sample.
- Thirty production rejection entries (27 SDK builders, screenshot owner and two legacy balloon helpers)
  reject inaccessible pointers with no CPU/RAM mutation. Their installation also wins over the feeder
  probe, which must not resurrect compiled originals. This guard test runs without retail assets in CI.
- Making SDK entries explicit externals exposes nested `Terminate` calls in five existing references.
  Their exact expected call counts are updated; native calls remain zero and the RAM/semantic assertions
  are unchanged. Local evidence is under `build/native-cleanup22/qualified-tests.log`,
  `qualified-fixtures.log` and `packet-audit.json`; no retail/generated data are committed.
- **16/16 game-free suites pass** (`qualified-disc-free.log`). A local fault injection restoring only
  the removed begin-frame reset call is detected by the audit walker (`reset-fault-injection.txt`).
- Final **5,800-frame headless/windowed inventory routes complete** with the production rejection guards
  enabled, zero register/unknown accesses, zero VU0 calls and zero GIF submissions. Native VIF1 completion
  counts are 5,686 / 6,072 and sync counts 6,884 / 7,270 (`qualified-headless.log`, `qualified-gameplay.log`).
  These remain bounded route checks, not dungeon/combat or full-game coverage. Eight captures are retained
  without visual inspection or a parity claim. The main-loop reset is exercised by these routes; fixture
  coverage is still the specific evidence for loading failure/selectors and other branches.
- Headless 48 kHz stereo PCM contains 4,686,527 frames, RMS 3,599.338, tail RMS 3,496.547, peak 28,775.
  Windowed PCM contains 5,100,894 frames, RMS 3,572.501, tail RMS 3,005.292, peak 32,766. Audio is active;
  subjective listening and exact timing remain unqualified (`qualified-evidence.py`).

BozoEngine remains pinned at `4a8f70a5021cbb3429800a02259ae7b816581ee0`.

## Dedicated packet storage and indirect-call audit

Ten guarded words remove the remaining main packet-buffer reservations and handle lifecycle:

- `SetPacketReadBuffer` `0x125ae0` keeps its first, real read-buffer allocation. Its two VIF1 reservations,
  `MGInitVif1Packet` call and obsolete visibility-tile allocation are skipped at `0x125b08`. The query
  descriptor keeps capacity `0x800` and cursor bookkeeping, with a null backing pointer.
- `BufferAllClear` `0x125bc0` keeps its complete arena clear and the first seven allocations. The two
  50,000-qword VIF1 reservations and the 4,096-qword visibility backing reserve are removed. This recovers
  **104,096 qwords / 1,665,536 bytes of usable capacity inside the shared arena**. The fixed 27,040,000-byte
  arena and its clear loop remain; this is not a reduction of the process's reserved memory/RSS.
- `MGInit` no longer writes a packet-object handle at `0x12c248`; `MGBeginFrame` skips selection of the
  alternating objects at `0x12ce74`. `GetVif1Packet` returns a null compatibility argument, both in guarded
  compiled code and at the installed native entry, without reading GP or a packet object. The obsolete
  buffer initializer now rejects unexpected entry; there are 31 tested rejection boundaries.

Native visibility reads only the descriptor's capacity/cursor, not its old tile backing. Its existing
original/native comparison now includes both null and inaccessible backing pointers. The updated CFG
audit finds no admitted GP-relative writes to the three packet-handle/base globals and no admitted initializer
calls. This is not an alias proof for arbitrary indirect stores. There are still 373 reachable GP-relative
compatibility loads of the zero/BSS handle word in legacy calling sequences. This batch does not rewrite
every legacy parameter or prove every indirect consumer's target set.

The allocation audit deliberately retains these different owners:

| Storage/extent | Current consumer and reason to retain it |
| --- | --- |
| Shared primary arena and read/data subarenas | Real files, models, game objects and allocator cursor/bounds behavior |
| Visibility descriptor/capacity/cursor | Native depth queries and per-frame reset of their logical request extent; its backing reserve is removed |
| Visual MDT data address and size | `CreateVisual` `0x126770` passes the native builder's visual size to `CDataAlloc2::Alloc`, then advances game bookkeeping; the data address keys `g_mdtCopies` and visual fields |
| Model/shadow/cloth allocation extents | The shared game allocation ABI still advances by these sizes. `modelAllocationQwords` and `clothCreateHook` retain legacy VIF-size formulas without serializing packets. Shrinking them requires migrating address identity, reuse/lifetime and live-animation/remake contracts together |

The earlier snapshot/lifetime and original/native builder tests still pass. No generic allocator is replaced,
and the model/cloth size ABI is not collapsed to zero or a shared dummy identity.

`dc-indirect-facts` exports the **pinned compiler's own** function ranges, register transfers and recognized
switch targets after applying the production guards. `dc_packet_audit.py` now follows those switch cases,
retaining their fallback as unresolved. The former 713-site direct-only walk becomes 773 reachable sites:
58 additional register calls were hidden behind switch arms, and two `CActionSeq::Play` calls must remain
because its installed hook forwards to the generated body. Forwarding hooks are explicitly retained in all
audit checks; registry ownership alone does not retire their originals. The complete admitted compiler
inventory contains **775 sites**, including two exception transfers outside the walk's reached blocks:

| Classification | Sites | What is established |
| --- | ---: | --- |
| Compiler-recognized switch | 104 | Known compiled cases are followed; mutable tables/default fallback are not proved closed |
| Function pointer loaded from memory | 649 | Local load/slot origin hint; callback/vtable target sets remain open |
| Other register target | 18 | No locally sufficient target definition |
| Register-copy call | 1 | Local copy hint, not a target-set proof |
| Exception-runtime jump | 3 | `__sjthrow`/`__throw`/`__rethrow` at `0x10d8c0`, `0x10e298`, `0x10e480`; landing targets remain unresolved |

An optional **`DC_AUDIT_INDIRECT=ON` build** adds probes to all 778 non-return transfers, including those in
overridden originals. The transformer requires an exact ordered register-transfer match with compiler facts
and checks total coverage. A probe returns the same target and never writes guest CPU/RAM or decides dispatch.
Its image tag is generated with the owning code; target overlay identity comes from the runtime's actually
selected resume-table entry, including resident-to-overlay calls. A regression switches TITLE/DUN identities
at the same address and checks that both are distinguished. Register snapshots are taken **before link/delay
slot execution** and must not be treated as the callee's consumed arguments.

With `DC_INDIRECT_TRACE` naming an output file, completed audit routes record observed site/target counts.
Two 5,800-frame inventory routes produce 3,416,012 transfer observations: **158 distinct sites and 378 unique
edges** (104 switch-case edges, 269 compiled-entry edges and five native-override edges). Every observed target
has one of these dispositions; none reaches a retired packet entry/interior or an unclassified address.
`CFrameVu1::DrawVu1` `0x129400`, for example, remains compiled hierarchy/light work even though codegen declares
it external; an external declaration alone is not an installed native replacement. TITLE and resident paths
are observed; DUN is not exercised. **617 sites remain unobserved**, enumerated in `indirect-audit.json`, including
all three exception jumps. These traces do not establish an exhaustive callback/vtable target set.

Qualification (`build/native-cleanup23/`):

- **67/67 production suites and 17/17 game-free suites pass.** Twenty-five new original/adapted storage cases
  execute the real arena allocator: complete buffer owners, an exact initialization span and the getter.
  The retained allocation prefix has identical inputs/addresses; only the dedicated reserves disappear.
  All other non-stack RAM, the full arena clear/content, logical descriptors and saved registers match.
  Existing window-setter references explicitly restore their original getter, while native setters still
  obtain no handle. Model/cloth extent and lifetime suites remain intact.
- Synthetic tests cover compiler switch/call facts, patch-guard failure, instrumentation mismatch, probe CPU
  preservation, duplicate-edge counting and source/target image identity. Audit builds and normal builds both
  pass their relevant suites. Traced binaries, input/trace SHA-256 hashes and raw observations remain local
  (`traced-binaries`, `audit-manifest.json`, `audit-headless.json`, `audit-gameplay.json`).
- Final production builds restore **`DC_AUDIT_INDIRECT=OFF`**. Both normal game executables have no probe
  symbols; no tracing branch or collector is shipped in its game path. Its headless PCM is byte-identical
  to the traced route (SHA-256 `21184606f8682af2902a519b98cf882de03b676ce69351f827658816c7176a9a`).
- Final headless/windowed routes each complete 5,800 frames with zero register/unknown accesses, VU0 calls or
  GIF submissions. Native VIF1 completion counts are 5,686 / 6,047 and sync counts 6,884 / 7,245. Final 48 kHz
  stereo PCM: headless 4,686,527 frames, RMS 3,599.338, tail RMS 3,496.547, peak 28,775; window 5,075,496 frames,
  RMS 3,553.549, tail RMS 3,015.392, peak 32,766. Audio is active; listening/exact timing remain unqualified.
- Production captures 3,400 (character/platform) and 5,400 (menu) were inspected. The menu was also compared
  with the previous batch and traced build; the expected scene/UI remain visible. This is bounded visual
  checking, not console parity, dungeon/combat coverage or a full playthrough.
- Negative trace checks reject incomplete runs, mismatched source sites, retired SDK targets and unresolved
  observed targets. Static/unobserved target sets still remain explicitly open.

Remaining work is explicit: the 617 unobserved indirect sites and their mutable target writers, legacy ABI
parameter loads, model/cloth identity-and-size migration, display/setup metadata and unsupported readbacks.
The engine pin remains `4a8f70a5021cbb3429800a02259ae7b816581ee0`.

## Null packet arguments and DUN dispatch follow-up

The 373 remaining admitted `LW gp-0x742c` compatibility loads now supply a literal zero in the same
argument register: **163 resident, 134 TITLE and 76 DUN** guarded substitutions. This makes the callers
follow the null-handle ABI already supplied by the native `GetVif1Packet`, without reading the retired
global. The instruction stays in its original position, including delay slots. Real model/cloth data
addresses, allocation sizes, query capacity/cursor and the shared arena are unchanged.

Original/adapted owner fixtures include the new substitutions and still require their complete earlier
guarded adaptations. Their native runs retain poisoned handle words and packet buffers while comparing
ordered consumed inputs, other non-stack RAM, saved registers and relevant floating-point state.
The packet-only instrumented build passes **67/67 suites**. With the balloon regression and fixes below,
the final normal build passes **68/68 suites**, and the game-free build passes **17/17**. The final normal
executables use `DC_AUDIT_INDIRECT=OFF` and have no trace-collector symbols.

The audit now rejects any admitted GP-relative read or write of the three retired handle/base globals.
It also inventories the raw remaining loads rather than hiding them: **287 remain**, with 115 in replaced
resident bodies and 172 outside the current CFG walk (95 resident, 74 TITLE, three DUN). CFG-unreached
does **not** prove safety against an indirect entry into a function's interior. Trace review now rejects
targets inside production guarded skip spans, using the actually selected target image. A target merely
inside a compiled function, without an established entry or switch case, remains unresolved and fails
review. Local negative traces exercise both cases. Arbitrary pointer aliases and mutable target writers
remain outside this direct-global audit.

DUN has **43 non-return register transfers: 11 compiler switches and 32 adjacent table-load chains**.
The latter now carry the exact object-register/field/slot origin hint:

| Object field containing table pointer | Table slot | DUN sites |
| --- | --- | ---: |
| `+0xa0` | `+0xc4` | 13 |
| `+0xa0` | `+0xac` | 2 |
| `+0xa0` | `+0xa0` | 7 |
| `+0xa0` | `+0x08` | 3 |
| `+0xa0` | `+0x18` / `+0x30` | 2 / 2 |
| `+0x2b8` | `+0x08` | 3 |

These are instruction-chain facts, not immutable vtable/target-set proofs. Coverage is now reported
separately for resident, TITLE and DUN, including distinct observed sites/edges and transfer counts.
Synthetic tests prevent source-module coverage from being confused at reused overlay addresses.

A completed **43,946-frame manual first visit** now supplies real DUN coverage for the packet cleanup.
Its trace predates the rendering/save fixes below and remains separately identified with its preserved
instrumented binary. It records **27,353,266 transfers, 278 distinct source sites and 709 distinct edges**:

| Source image | Sites observed / inventory | Distinct edges | Transfers |
| --- | ---: | ---: | ---: |
| Resident | 226 / 601 | 637 | 27,232,756 |
| TITLE | 19 / 131 | 34 | 9,593 |
| DUN | **33 / 43** | **38** | **110,917** |

Every observed target is a compiler switch case, compiled entry or installed native override. No observed
edge enters a retired packet SDK entry/interior, guarded skipped span or unclassified address. The run ends
normally with zero register/unknown accesses, VU0 calls or GIF submissions; native VIF1 completions/syncs are
44,546 / 46,530. Captures of the first-visit tutorial and floor-one gameplay were inspected. This establishes
observed execution in DUN, not full dungeon/story coverage or closure of mutable callback target sets.

**497 sites remain unobserved in this trace**, including these ten DUN sites:

| DUN owner | Unobserved sites |
| --- | --- |
| `MainDraw` | `0x1dae898` (table call), `0x1daf5bc` (switch) |
| `MoveChara` | `0x1db200c`, `0x1db53b8` (switches), `0x1db6848` (table call) |
| `motionDrive` | `0x1db77d8`, `0x1db79a8` (table calls) |
| `EquipReAttach` | `0x1db9604`, `0x1db9628` (table calls) |
| `Run_TrapCircle` | `0x1dbfb44` (switch) |

### Dialogue mask and save completion found during DUN qualification

The supplied PCSX2 dialogue reference has an irregular paper outline; the native capture had a rectangle.
The existing native polygon/tail was present, but OpenGL disabled depth writes along with a disabled depth
test. The generic engine fix enables an unconditional test when writes are requested, then restores the
requested comparison for tested batches. Main/auxiliary mask and inverted-reuse GPU readback fail before
the fix and pass afterwards. The upstream integration also preserves strict GREATER, stencil and newer
texture-function/mipmap tests.

The port's mask clear now uses the exact finite native near depth (`screen().depthMax`); its old `1e30`
wrapped in the depth shader. It clears coverage separately because an opaque draw deliberately publishes
full coverage. The new game-free GPU fixture invokes the actual native balloon owner with synthetic
diamond/tail records, an inaccessible packet argument and four target-reuse profiles. It checks transparent
corners/old mask locations, filled body/tail, unchanged guest RAM/CPU and a finite clear. Its capture was
inspected. This verifies the masking correction; the supplied PCSX2 scene has not been rerun for exact
outline/antialiasing or scene parity.

The first visit also exposed a save error after files had already been written: the pinned engine lacked
native `sceMcFlush`, so the original SDK client path rejected the flush and the game's save state machine
reported failure. Native Flush already exists upstream in `63caf16`; it is backported for the qualified DC
base. It publishes command `0xA` with result 0 for an open file or -4 for a missing/closed descriptor, retaining
the cursor and descriptor. Tests cover repeated flush, completion consumption, writes after flush, persisted
bytes before close and reload in a fresh runtime. No unsupported-operation success value is fabricated.

The final normal **8,800-frame save route** records both Flush completions with result 0 and displays
**“Saving completed.”** A fresh **4,200-frame process** reloads the resulting save and shows the retained
13:18 playtime / one dungeon visit, then gameplay. Both routes have zero register/unknown accesses, VU0 calls
or GIF submissions. User progress and all evidence remain in copied local cards under `build/native-cleanup24/`.

DC's qualified engine pin is **`e78883f12f6362cb5570bed55aaa2aa7169d9589`**, containing these focused fixes on
the preceding verified base. Engine PR [#73](https://github.com/TheBozos/BozoEngine/pull/73) integrates the
generic rendering fix and additional Flush regressions with current main; **47/47 suites including GPU
readback pass** on that integration. Retail/generated assets, raw traces, cards and captures stay local.

## Remaining DUN dispatch and residual-load audit

The ten DUN sites absent from the manual first-visit trace now have **74 controlled dispatch cases**.
The fixture generator checks the original DUN fingerprint, production patch guards and compiler switch/slot
facts, then recompiles the exact dispatch spans at their original addresses. Switch landing bodies and
callees are synthetic. `EquipReAttach` is compiled in full, with controlled lookup and attachment boundaries.
The harness follows the production dispatcher's register-jump unwind through selected generated resume
entries; it does not run the scheduler or gameplay.

| Owner / sites | Checked dispatch contract |
| --- | --- |
| `MainDraw` `0x1dae898` | Null packet argument to texture reload, followed by object `+0xa0` / slot `+0xac`, retaining object and link address |
| `MainDraw` `0x1daf5bc` | Six indices and default; actual table and range check |
| `MoveChara` `0x1db200c`, `0x1db53b8` | Selectors 195–206 (12 entries, two distinct targets) and 0–5; aliases and defaults |
| `MoveChara` `0x1db6848` | Camera table at object `+0x2b8`, slot `+0x08`, scalar argument 1 |
| `motionDrive` `0x1db77d8`, `0x1db79a8` | Object `+0xa0`, slot `+0x08`; both real `s0 == 1` gates, with 0/1/2 inputs |
| `EquipReAttach` `0x1db9604`, `0x1db9628` | Both lookup branches; zero float arguments for slots `+0x18` and `+0x30`; table reloaded after a mutating first callback; final parent attachment |
| `Run_TrapCircle` `0x1dbfb44` | Ten indices and default; actual table and range check |

Every switch also runs with its first table entry redirected to another known case and to a controlled
resident callback. Call spans select two different objects/callbacks. Selected-image checks use different
function tables at the same address and reject an unknown identity. Saved full registers and relevant RAM
are checked; only synthetic RA-save register `s7` is excluded for extracted spans, and the complete owner's
stack writes are excluded from its RAM comparison. These checks preserve dynamic dispatch; they do not
close target sets or establish the callers' earlier state-selection paths or the callees' full behavior.

The audit also reads four named resident tables and checks the exact constructor sequences that install
them. Their matching slots supply **candidates**, separately from observed targets:

| Table / constructor store | Relevant initialized slots |
| --- | --- |
| `CCharacter` `0x2a12b0` / `0x143550` | `+0x08` Step `0x138530`; `+0x18` SetPosition `0x138fb0`; `+0x30` SetRotation `0x139150`; `+0xac` Draw `0x139310` |
| `CNPCharacter` `0x2a13c0` / `0x156b2c` | Step `0x155fb0`; same position/rotation slots; Draw `0x156540` |
| `CCamera` `0x2a10b0` / `0x124710` | `+0x08` Step `0x123f30` |
| `CCameraFollow` `0x2a1090` / `0x124c2c` | `+0x08` Step `0x1247d0` |

Constructor evidence does not prove the object type at any particular DUN site, exclude other derived
tables, or bound later table/object writers. These methods remain compiled game behavior; this batch
adds no runtime override or packet substitution.

The compiler facts now export all **eight known interior entry points** (resident eight, TITLE/DUN zero).
Starting the CFG from those entries as well as function starts reaches all **775 inventory sites**, including
the two exception-runtime jumps previously missed by the 773-site walk. The audit rejects entries inside
guarded spans, entries bypassing replaced bodies, and admitted direct tail jumps/branches that would
bypass native owners. This last check matters because the pinned emitter routes `override` calls through
the runtime, while a direct tail transfer can invoke the original compiled body. No such bypass is found;
all installed owners also have an external dispatch declaration in resident and overlay codegen.

All **287 residual direct packet-handle loads** now have an explicit disposition:

| Image | Replaced native body | Guarded skipped span |
| --- | ---: | ---: |
| Resident | 115 | 95 |
| TITLE | 0 | 74 |
| DUN | 0 | 3 |

The three DUN loads are `0x1dae324` behind guard `0x1dae31c` (resume `0x1dae334`), and
`0x1dc1728` / `0x1dc1738` behind guard `0x1dc1720` (resume `0x1dc1744`).

The report records the guard/resume addresses for each skipped load and rejects an unaccounted load.
All known entries are checked; none reopens these spans. The 374 null-handle constants remain, with zero
admitted GP-relative handle/base reads or writes and zero active packet-SDK callers. Arbitrary dynamic
interior targets, pointer aliases and mutable target writers remain open. The global reset stays retired;
model/cloth allocation extents still require their own identity/lifetime migration.

The normal build passes **69/69 suites**, and the game-free build passes **17/17**. Local negative audits
check constructor, external-dispatch, interior-entry and direct-tail rejection. Re-auditing the preserved
manual trace leaves coverage unchanged: **DUN 33/43**, resident 226/601, TITLE 19/131, with 497 sites still
unobserved. Controlled cases are not added to those counts. The engine pin remains
`e78883f12f6362cb5570bed55aaa2aa7169d9589`; runtime and production patches are unchanged. Evidence stays
local under `build/native-cleanup25/`; no new visual, audio or gameplay qualification is claimed.

## Pointer aliases and DUN table writers

The pointer used by the remaining weapon calls is **`NowWeapon`**, at `gp-0x6300` (`0x2a34f0`),
not a generic player-character root. `NowCamera` is at `gp-0x6358` (`0x2a3498`). The original ELF
contains several symbols named `NowCamera` or `MainCamera`; this audit selects exact addresses and
the DUN symbol section rather than combining equal names across overlays.

`dc_alias_audit.py` adds bounded low-word flow through constants, GP address formation, register
copies/additions, stack spills/reloads, control-flow joins and delay slots. It recognizes the runtime's
KSEG and `0x20000000` / `0x30000000` RAM aliases and enclosing ranges for partial/wide stores.
It retains up to four known values alongside an unknown path, then widens explicitly. It checks
46 byte ranges: three retired packet globals, two roots, seven named DUN object vptrs, all 23 named
resident vtables and the 11 DUN compiler-switch tables. Aligned pointer words in allocated,
non-executable file-backed data sections are inventoried separately. Mutable image contents are
never folded into constant pointer values.

The recognized root writers are:

| Root | Source image / stores | Established source |
| --- | --- | --- |
| `NowWeapon` | DUN `0x1db966c`, `EquipWeaponFrame` | Its incoming `a0` |
| `NowWeapon` | DUN `0x1dba54c`, `LoadChara2` | Null |
| `NowCamera` | DUN `0x1dac590`, `GameInit` | `MainCamera` `0x1dc45e0` |
| `NowCamera` | Resident `0x1bb398`, `0x1d1c20`, `0x1d2710`, `0x1d2f84`, `0x1d38e8` | `SubCamera` `0x1dc48e0` |
| `NowCamera` | Resident `0x1bb8c8`, `0x1d1ef0`, `0x1d2a28`, `0x1d31ec`, `0x1d3a50` | `MainCamera` |

Direct DUN calls to `EquipWeaponFrame` supply `MainWeapon` (`0x1ea4080`) at `0x1db9a64`,
`CrashWeapon` (`0x1ea63e0`) at `0x1db9d1c`, and `DefaultWeapon` (`0x1ea5230`) at `0x1db9d88`.
Resident transfers at `0x2041ac`, `0x20453c` and `0x2100d0` supply `DefaultWeapon` to that address;
its DUN binding requires DUN to be selected. The three direct DUN calls to `EquipReAttach`
(`0x1db45f0`, `0x1db6554`, `0x1db68c4`) reload `NowWeapon`, with branch argument 1/0/0.
These constrain known direct paths; they do not close indirect entries into either setter or caller.

The DUN static initializer installs `CCharacter` table `0x2a12b0` into the three weapon objects
using **absolute-address stores**, which an immediate-offset-only search misses:

| Store | Destination |
| --- | --- |
| `0x1dc3ad0` | `MainWeapon + 0xa0` (`0x1ea4120`) |
| `0x1dc3b94` | `DefaultWeapon + 0xa0` (`0x1ea52d0`) |
| `0x1dc3c58` | `CrashWeapon + 0xa0` (`0x1ea6480`) |

The same initializer installs `CharaMain` / `CharaHand` vptrs at `0x1dc3948` / `0x1dc3a0c`.
Their base constructor first installs the temporary `CObject` table at `0x157028`; later inline
construction replaces it with `CCharacter`. CameraFollow construction is called for MainCamera
and SubCamera at `0x1dc35ac` / `0x1dc35f4`, with destructors registered afterwards. Destruction
also writes table pointers: `CCamera` at `0x124790` reinstalls `0x2a10b0`, and `CameraFollow`
at `0x124ca8` reinstalls `0x2a1090` before calling the base destructor. This is a remaining
writer, not evidence that the table contents themselves are modified.

Assignment is checked with the **complete compiled owners and their real helper**, not mocked
copies: `CCharacter::operator=` (`0x142da0`), `CObject::operator=` (`0x1431e0`) and
`CameraFollow::operator=` (`0x124aa0`). All **24 cases** pass, including distinct inaccessible
source/destination vptrs, self-assignment, copied fields, full saved registers, source preservation
and RAM changes confined to destination/stack. The first two preserve the destination's
`+0xa0..0xaf`; CameraFollow copies only its six scalar fields at `+0x2c0..0x2d7`, preserving
`+0x2b8`. Its three known direct callers copy MainCamera into SubCamera before selecting it.
Conversely, CCharacter copies its ordinary data at numeric offset `+0x2b8`. An offset match
without object provenance must therefore remain **untyped**, rather than become a camera-vptr claim.

The recognized flow finds no additional admitted alias access to the retired packet globals, and
no recognized store into the watched resident vtable or DUN switch-table contents. It records
pointer publication, root writes and matching untyped field writes separately. New negative audits
on private ELF copies reject a GP address copied through another register, an uncached absolute
quadword overlap, and a pointer recovered from a stack spill. The production audit rejects these
recognized packet aliases as well as its earlier direct GP forms.

These are bounded static findings. Unknown stores and bulk-copy lengths, affine/value-set widening,
unknown upper register words, arbitrary heap/data aliases and computed transfers remain open.
Calls retain only ABI-preserved registers; their memory and return effects are unmodeled. Native
instruction hooks and system effects (`SYSCALL`, `BREAK`, COP0 control operations) stop the affected
flow path explicitly. Unknown-store statistics count analysis visits, not distinct writers or
runtime executions. The native ELF/disc/file-loading paths can write guest RAM; they are outside
this instruction-flow audit. Overlay address coincidences, especially from TITLE, do not establish
DUN runtime writes. Consequently no table is declared immutable and no dynamic dispatch is replaced.

The final normal build passes **71/71 suites**, with **18/18 game-free suites**, including 19 adversarial
alias tests. The preserved gameplay trace still reports **DUN 33/43**, with **497 unobserved sites**
globally; none of these fixtures is added to live coverage. Runtime, production adaptations and the
engine pin `e78883f12f6362cb5570bed55aaa2aa7169d9589` are unchanged. The global packet reset stays
retired; model/cloth identity/extent migration remains separate. Local evidence is retained under
`build/native-cleanup26/`, including source hashes and the writer ledger. No retail assets, generated
sources, saves or captures are committed, and no new gameplay or visual parity is claimed.

## Bulk copies and caller-instantiated callee writes

The next pass retains **all recognized direct bulk-call sites**, including unknown arguments, rather
than filtering them out when they do not point into the original 46 watched ranges. The current flow
records 369 value-set variants at **270 distinct source sites**, including seven DUN `memcpy` sites.
These are static candidates, not execution counts. Native `memcpy` / `memset` use explicit unsigned
32-bit byte-range and destination-return contracts from the pinned handlers. The compiled `memmove`
uses 64-bit size comparisons, so low-word flow alone does **not** establish its extent, including
when the low word is zero. It remains a compiled callee with explicit unknowns.

There are genuine writes to DUN dispatch data in the load lifecycle. Under the declared DUN base,
correct file/header and a successful **complete** read, the guarded loader contract gives:

| Phase | Destination byte interval | Watched dispatch data affected |
| --- | --- | --- |
| Read `DUN.BIN` | `[0x1dabd00, 0x1dc4480)` | All 11 DUN switch-table arrays |
| Clear module BSS | `[0x1dc4480, 0x1f06b6c)` | Seven root-object vptrs and 45 embedded effect vptrs |
| Constructor table | `0x1dc4410..0x1dc4414` → `0x1dc3580` | Static object initialization, followed by later game initialization |

`LoadOverlay` (`0x140aa0`) obtains a mutable destination from the overlay-group table; the file reader
reaches native `sceRead` at `0x122ef4`. `mwOverlayInit` calls `memset` at `0x122e2c`, then constructors
at `0x122e44`. Relevant argument, header-field and call instructions are guarded. Artifact header
ranges instantiate this contract separately from generic instruction flow: an initial destination
word is not silently promoted to an invariant. Short/failed reads, mutated destinations/headers and
other native disc/file/card outputs remain open. These are module-loading writes; the retired
global packet reset is unchanged.

The original seven-root-vptr inventory missed dispatch pointers embedded in `MasekiEffect`, a DUN
array of five `CSHOT_EFFECT` objects at `0x1e5b380`, stride `0xa160`. The guarded static initializer
and complete constructor establish nine embedded `CCharacter` vptr offsets per element:
`0xb0`, and `0x1260 + i*0x11b0` for `i=0..7`. The watch set now contains **91 ranges**, adding all
**45 embedded vptrs**. This is a typed constructor/layout fact, not an offset-only inference.

Four full-object copies in `LoadData` propagate the first element into the other four:

| DUN call PC | Destination | Source / length | Embedded vptr words copied |
| --- | --- | --- | ---: |
| `0x1dbaed8` | `0x1e654e0` | `0x1e5b380` / `0xa160` | 9 |
| `0x1dbaf14` | `0x1e6f640` | Same | 9 |
| `0x1dbaf50` | `0x1e797a0` | Same | 9 |
| `0x1dbaf8c` | `0x1e83900` | Same | 9 |

The intervals stop at `0x1e8da60`; they are disjoint from the earlier 46 ranges while carrying
**36 embedded vptrs**. The report records source-byte origins for each affected field. Their pointer
values remain mutable; raw copying propagates the current prototype bytes. The typed-assignment
preservation result from the preceding batch does not apply to these raw copies.

`dc_callee_audit.py` instantiates compiled callees with established caller arguments and records
their watched and symbolic writes. It preserves the active overlay through resident callees, keeps
caller-stack pointers distinct from the callee's frame, and stops at native/SDK boundaries rather
than analyzing a replaced original. Known `__construct_array` bindings supply guarded normal-loop
constructor candidates after proving the count's 64-bit value from a dominating local immediate
within the compiler-known CFG; unproved counts remain opaque. The complete helper body is
fingerprinted. Contexts recover the 45 embedded constructor vptr stores and the pack
lookup's size-output stores. Callee records are possible effects, not an interleaved trace or a
complete memory/return summary; they are not fed back as assumed values into callers.

This pass reaches its declared limits: **1,536 contexts**, depth **4**, and at most **16** expanded
array elements. The report retains all count/depth/element boundaries, computed callbacks, opaque
native effects, unknown stores and widening. Empty effect lists do not establish absence of writes.
Overlay selection held through a context remains conditional across opaque loader effects.

Local forwarding now uses a function's own observed store→load flow, with alias invalidation and
joins over all compatible low-word definitions; mutable ELF data is still not constant-folded.
This establishes `InitializeDataBuffer`'s local zero counter before `memset` at `0x1259c4`, yielding
`[0x2ab080, 0x1c74980)`, disjoint from all 91 watched ranges on that path.

Three DUN copies remain unresolved: `0x1dba73c`, `0x1dba7e0` and `0x1dba860` in `LoadChara2`.
Their lengths come through stack outputs of **`GetPackFile` (`0x13f720`)**. On a match it returns a
node-relative payload and writes the raw node size at `0x13f7b8`; on absent/null input it returns
zero without initializing the size output. Its comparison helper is the compiled **`strcasecmp`
at `0x105238`**. The complete lookup/helper pass 24 synthetic cases covering names/paths, null and
missing nodes, optional output, zero/raw/maximal size metadata and source/stack write boundaries.
Those cases validate the lookup contract, not payload-size validity.

The destination comes from `CharaModelBuffer` (`0x1f06660`) and its qword cursor. Game initialization
sets a `0x33450`-qword capacity, but the three child-allocation checks occur **after** their copies
(`0x1dba768`, `0x1dba80c`, `0x1dba88c`). They are not preceding bounds checks for pack-derived sizes.
Mapped-RAM validation in native copying also does not establish this logical arena contract.
Pack metadata, failed lookup/stale outputs, cursor arithmetic and opaque native writes therefore
remain unresolved; no disjointness or complete target-set proof is claimed for these copies.

Validation: **73/73 normal suites**, **19/19 game-free suites**, including 24 alias tests and 17
callee/range tests. Another **94 cases** check the production copy/fill handlers and complete compiled
`memmove` for canonical sizes, mirror addresses, overlaps, field boundaries and exact byte effects.
Five private-ELF negative audits reject packet overlap through a bulk call or a callee, shifted
constructor `this`, changed static-array binding and altered loader BSS field. No additional
admitted packet-global overlap is established by the bounded positive pass. Runtime, production
patches and engine pin remain unchanged. Evidence stays under `build/native-cleanup27/`; no retail
data, generated code, cards or captures are committed. Live DUN coverage remains **33/43** and
497 sites remain unobserved globally; these tests add no gameplay, visual or full-game qualification.

## LoadChara2 pack extents and cursor boundaries

The three unresolved copies now have an explicit conditional contract in `dc_loadchara_audit.py`.
Instruction fingerprints cover their selector predicates, lookup/copy/reservation sequences,
complete `GetPackFile`, complete child allocator, global allocation/alignment helpers and the
`GameInit` child-capacity binding. Installed replacements or native instruction hooks reject these
contracts. They document the compiled path without resolving its mutable inputs in generic alias flow.

`dc_pack_inventory.py` reads the initial character table, both archive indexes and the selected
outer files. It checks HED/HD2 names/byte offsets/sizes/sectors, bounded headers and payloads,
terminating node traversal and source hashes. The six initial selector packs contain 79 nodes;
all their payload extents fit their outer files. These are validator checks, **not checks performed
by the game's lookup**. No payload bytes are emitted or committed.

| Copy PC | Initial selector / outer pack | Requested member | Artifact bytes | Reservation bytes |
| --- | --- | --- | ---: | ---: |
| `0x1dba73c` | 5 / `c18a.chr` | `c05w_h.img` | 132,224 | 132,288 |
| `0x1dba7e0` | 5 / `c18a.chr` | `mgan01.chr` | 25,360 | 25,408 |
| `0x1dba860` | 3 / `c05a.chr` | `c05_f03.chr` | 53,552 | 53,568 |

The ordinary resident paths at `0x1d1230` and `0x20e98c` pass the selected pack base as `a2`.
The latter uses background file loading; completion and preserved globals remain conditions.
Normal DUN `LoadData` sets selector zero and loads `c01d.chr`, so that ordinary path does not take
these three copies. Computed callers, changed paths/tables, short/failed reads and changed source
headers are not closed. Earlier `LoadPackData* → ReadInfo` calls also use `CharaModelBuffer`;
resetting its cursor at function entry does **not** prove a zero cursor at these copies.

For an explicit snapshot, destination formation is
`u32(base + u32(usedQwords << 4))`. The nominal child capacity is `0x33450` qwords.
Assume its complete arena lies in global payload `[0x2ab080, 0x1c74980)` and
`0 <= usedQwords <= capacityQwords`. Any destination then lies between the global start and
one-past-end. With the largest verified member size, the write ends no later than `0x1c94e00`,
below the first DUN switch array at `0x1dc2e70`. The envelope is disjoint from all 91 watched
dispatch/packet ranges, including the resident vtables below the global payload. This establishes
**conditional direct-write disjointness**, even allowing a child overflow; it does not establish
the preconditions, preservation of pack metadata or capacity safety. Following allocations and the
global allocator counter at `0x1c74980` may still be corrupted by a child overflow, with unresolved
effects on later calls. This result is not fed back as an unconditional alias-flow fact.

`memcpy` consumes the raw unsigned 32-bit pack size. Only **after** copying does the caller execute
`SRA 6; ADDIU 1; SLL 6; SRA 4` and call the child allocator. For ordinary nonnegative sizes below
the sign transition this reserves the next multiple of 64 bytes, including an extra block for
an exact multiple and 64 bytes for zero. At `0x7fffffc0` it instead yields `-0x8000000` qwords;
`0xffffffc0..0xffffffff` yields zero. The allocator adds in 32 bits and compares the signed result
against signed capacity. Negative inputs and wrapped sums can pass. These arithmetic facts do
not make a huge native copy succeed: full mapped source/destination ranges are checked first.
Conversely a mapped copy can change a synthetic dispatch-table byte **before** a subsequent
capacity rejection. The allocator prediction requires its metadata and the stack size to survive
the copy; arbitrary copies can overwrite those inputs themselves.

A missing/null pack lookup returns zero and leaves the output size untouched. Source zero is
mapped guest RAM, so a missing member need not fault in native copying. The first and third size
locals have no established initialization on their missing-member path. The second reuses the
first local after it has been transformed to a qword reservation: with the first artifact size,
that stale value is **8,268**, then interpreted as **bytes**, unless intervening texture calls alter
it. Their complete source/arena effects remain open. No gameplay overflow or malformed-pack bug
has been observed by this pass, and runtime behavior is unchanged.

Reproduce the artifact envelope after producing a current packet-audit report:

```bash
python -I tools/dc_pack_inventory.py --root . --audit-report build/player-audit.json
```

Synthetic validation executes the generated resume spans through real compiled
`GetPackFile`/`strcasecmp`, production `memcpy` and the complete child allocator. Stops are explicit
at allocator completion or before its rejection diagnostic/infinite loop. Arithmetic-only cases
start after copying and do not pretend to execute invalid huge copies. Cases cover actual-sized
synthetic payloads, mapped source zero/stale lengths, RAM mirrors, child overflows, dispatch-byte
mutation before rejection, negative cursors and signed wrapping. All fixture RAM is synthetic;
these are controlled effects, not gameplay or observed source-pack mutation. Local evidence and
source hashes remain under `build/native-cleanup28/`. The retired global packet reset, runtime,
production patches, engine pin, player saves and live DUN coverage **33/43** are unchanged.

Validation comprises **75 normal suites**, **20 disc-free suites**, **79 compiled boundary cases**
and **14 Python unit methods**. Nine private-ELF negative audits include changed pack-size fields,
signed rounding, capacity checks and selector predicates, in addition to the previous bulk/callee
overlap, constructor and loader guards. Source-local qualification records distinguish the full
suite run from the final targeted rebuild of strengthened fixture assertions.

## Intervening LoadPackData / ReadInfo effects

`dc_readinfo_audit.py` records a guarded local contract for the earlier `LoadChara2`
parser calls. It fingerprints the complete DUN caller, the three resident wrappers,
ReadInfo, the nineteen initial command bodies, token/comment parsers, pack lookup and
child allocator. Native instruction hooks or replaced owners reject this contract.
The report keeps initial callback words separate from runtime target values; no
effect is promoted into generic alias-flow facts.

| DUN call PC | Wrapper | CFG | Arena roles passed by this caller |
| --- | --- | --- | --- |
| `0x1dba544` | `LoadPackData2` at `0x139850` | `base.cfg` | All four roles use `CharaModelBuffer` |
| `0x1dba5ac` | `LoadPackData` at `0x139820` | `hand_up.cfg`, selector 1 | Model/motion share the buffer; shadow/IMG are null |
| `0x1dba61c` | Same | `hand_up.cfg`, selector 3 | Same |

The separate wrapper at `0x1397f0` uses the character's mutable virtual slot `+0xb8`;
it is not an unconditional direct edge to ReadInfo. Compiled synthetic tests execute
the three DUN argument spans through their actual complete wrappers and ReadInfo,
then stop immediately at the wrapper return before subsequent character calls.

ReadInfo resets parser globals, allocation-name-list heads and four IMG slots, but
does **not** reset the child arena's used counter. Its nested loaders and command
callbacks can advance that same cursor before the later three pack copies. Direct
child allocations occur in MODEL (`0x13a13c`), MOTION_END (`0x13a740`) and IMG
(`0x13aac0`); the initial callback inventory also retains direct-call candidates and
compiler-known computed sites. MODEL calls the mutable character virtual slot
`+0xc4` before lookup and `LoadMDSFile`, while motion loading reaches `AnimeDataInit`.
Their complete nested writes, allocations and source relocations remain open.

IMG checks its optional arena and slot, then looks up the member. A missing member
returns without allocating or copying. A match reserves `(s32(size) >> 4) + 1` qwords
**before** copying the raw unsigned byte count, including an extra qword for zero or
an exact multiple of sixteen. This differs from the three later LoadChara2 copies,
whose checks follow their copies. It still inherits the signed/wrapping allocator's
invalid-state cases. Synthetic complete ReadInfo cases demonstrate preserved
nonzero cursors across successive calls, capacity rejection before copying, and
negative cursors accepted by the signed check. A separately seeded complete
MOTION_END callback advances the motion arena; this is not execution of the preceding
MOTION loader.

Pack headers are **not generally preserved by ReadInfo**. `PreProcess` at `0x13b270`
replaces comments in the source CFG with spaces. Its closing block-comment store at
`0x13b340` can write at `payload + size`: a declared `/*x*` followed by `/` just outside
the payload writes five spaces. A complete compiled ReadInfo fixture places that
slash at the next valid header's first byte and checks the exact resulting outer
pack bytes. The header changes without a callback or cursor change. Bounded payload
extents alone therefore do not prove preservation of the surrounding headers.

The read-only pack inventory now records this leaf's planned writes and lookahead
reads for all eight initial `base.cfg` / `hand_up.cfg` snapshots. It fingerprints
the current boot artifact's PreProcess body before using the offline walk, rather
than relying on a prior report's identity. None of their
planned stores leaves its CFG payload or touches an outer header. Optional local
qualification also invokes the complete compiled PreProcess leaf on those eight
outer snapshots, checks every RAM byte outside the declared CFG and every complete
node header, and compares changed-byte counts with the static walk. These checks
require the snapshot's stable pointer/size and disjoint input metadata. They do not
execute complete retail ReadInfo or its callbacks, or establish preservation after
earlier aliases, failed reads or nested loader effects.

GetArg writes parsed strings to the caller's output slots rather than inserting
terminators in the source CFG. SearchCommand and GetArg nevertheless copy tokens
to local 256-byte buffers without a token-length bound; the descriptor type scan
has no explicit sixteen-argument cap. Stack/pack/global/arena nonaliasing and valid
metadata remain preconditions. ReadInfo reloads its callback from mutable
`CommandExe` (`0x251470..0x2514bb`) at `0x139f30`, then calls it at `0x139f34`.
Synthetic IMG destinations can replace outer header bytes or a DUN switch-table
word. Another synthetic IMG copy changes the next callback-table word, and the
actual compiled loop calls that replacement on its next command. These controlled
effects do not establish a gameplay corruption or a complete runtime target set.

Validation for this batch is recorded under `build/native-cleanup29/`: 38 synthetic
compiled cases, eight compiled artifact leaf checks, thirteen disc-free Python unit
methods and six changed-instruction guard rejections. **77/77 normal suites** and
**21/21 disc-free suites** pass; the final artifact-guard unit also passes separately.
Runtime, production patches,
the engine pin and saves are unchanged. Live DUN coverage remains **33/43** with
497 unobserved sites globally; fixtures add no gameplay or visual qualification.

## Per-frame transfer retirement (2026-11-09)

`MGEndFrame`'s tail no longer waits, flushes or sends. The earlier frame-end pass kept one native SDK send per frame,
an empty VIF1 transfer, to preserve its completion interrupt and callback. That completion has no consumer: the game's
only `AddDmacHandler` call is in `sceSifInitCmd`, which BozoEngine answers natively, so no DMA-completion handler is
ever installed. The `sceDmaSync` before it waits on the GIF channel (`gp-0x7434`, from `MGInit`), which never carries a
transfer in this port. Three guarded patches retire `sceDmaSync` (`0x12d61c`), `FlushCache` (`0x12d678`) and
`sceDmaSend` (`0x12d68c`); every delay slot was already a `nop`. The frame counter and its first-frame flag, the
`sceDmaGetChan(1)` lookup stored at `gp-0x7438` and the double-buffer toggle stay.

- The `frame_submit` reference/adapted span now starts at `0x12d610`, covering the wait. 24 cases (counter overflow,
  buffer states, first frame): the published reference waits, flushes and sends once; the adapted tail makes no
  transfer, flush or wait and queues no completion. Frame counter, flag, channel handle and toggle match; all other
  guest RAM is unchanged.
- **126/126** suites. The seven scripted routes end as before with identical cards and zero register/unknown accesses,
  VU0 calls or GIF submissions. VIF1 SDK submissions per route fall from 5,338–10,010 to **1,190** (1,571 on the
  Matataki route), and sync calls from 6,536–10,986 to 2,388–3,162. Captures match the previous build within 2 mean
  levels outside the two known fade frames.

Remaining transfer calls, about 1,190 per run, sit in the boot and loading paths: `main`'s four sends at mode changes
(two upload VU1 microcode from `0x24f960`/`0x296680`, which nothing executes), `VSyncCallBack_Load`'s send and syncs,
`EnterFixTexture`/`EnterFixTextureZ` and `StoreImage`.

### Boot and loading transfers (2026-11-09)

The rest follow the same reasoning:

- `VSyncCallBack_Load`, every vblank while loading: `sceDmaSync` of VIF1 (`0x154380`), `iFlushCache` (`0x15468c`),
  the empty VIF1 `sceDmaSend` (`0x1546a0`) and `sceDmaSync` of GIF (`0x15474c`) are retired. The `loading`
  reference/adapted span (96 cases) now requires the adapted callback to make no cache or transfer call and queue no
  completion, while its fade sprites match the reference's calls in order and all non-stack RAM matches.
- `main`, at mode changes: four `FlushCache` / `sceDmaSend` / `sceGsSyncPath` triples (`0x141388`..`0x141638`) are
  retired, two of them the VU1 microcode uploads. `v0` is reloaded after each removed call.
- `EnterFixTexture` / `EnterFixTextureZ` are replaced whole by native hooks, so their sends never run. `StoreImage` is
  reached only from `MGEndFrame`'s screenshot branch (`0x12d4e4`).

**126/126** suites. All seven routes now report **0 VIF0, 0 VIF1, 0 GIF submissions and 0 sync calls**, with the same
ends, identical cards and zero register/unknown accesses and VU0 calls. A loading transition finishes slightly earlier
(the Matataki route's frame 6,000 arrives where the previous build is still fading); later captures show the same views
within about 3 mean levels.

### Per-frame display-buffer swap (2026-11-09)

`MGEndFrame` no longer calls `sceGsSetHalfOffset` (`0x12d5e8`), `FlushCache` (`0x12d5f4`) or `sceGsSwapDBuff`
(`0x12d608`) every frame. `sceGsSetHalfOffset` only rewrites the draw record's XYOFFSET image, which the native
renderer never reads; `sceGsSwapDBuff` writes nothing and only hands the records to `sceGsPutDispEnv` /
`sceGsPutDrawEnv`, both no-ops in BozoEngine. The per-frame `sceDmaGetChan(1)` (`0x12d64c`) and its store
(`0x12d654`) go too: `MGInit` already stored that VIF1 handle, and without the call the store would have written the
frame counter left in `v0`. The buffer index toggle stays.

- The `frame_submit` span now starts at `0x12d5c0` (24 cases): the reference sets the half offset, swaps, waits,
  flushes twice, looks the channel up and sends; the adapted span does none of these, and all guest RAM matches, the
  channel handle included.
- 126/126 suites; seven routes with the same ends, identical cards, and zero transfers or syncs. Matataki frames differ
  only by cloud drift after the earlier load (up to 3.8 mean levels).
- A temporary per-name library-call count on the 9,000-frame Matataki route (not committed) shows
  `sceGsPutDispEnv` / `sceGsPutDrawEnv` going from 7,869 / 7,868 calls to none, and `sceDmaGetChan` from 7,874 to 6.
  The PS2 library calls left every frame are the controller (`scePad*`) and the sound driver's SIF commands. The
  GS-library calls left run at startup or mode changes: `sceGsSyncV` (86, in `CGamePad::Init`), `sceGsSyncPath` (9)
  and the `sceGsSetDef*` record setup.
