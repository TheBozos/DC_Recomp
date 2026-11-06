# BozoEngine migration

Implemented on 2026-11-06. The active build uses only the `BozoEngine` submodule. The previous dirty PS2Recomp checkout is retained locally as an ignored recovery directory; it is not a submodule, source input, include path or linked dependency. Local recovery patches and the previous runner are in `build/migration-baseline`.

The graphics cleanup includes the DC adapters, regression fixtures, guarded offline adaptations and published
BozoEngine pin `4a8f70a` together. References to local/unpublished work in the milestone sections below describe
the state at those earlier checks. The latest validation is 36/36 suites, both 5,800-frame routes and byte-identical
3,500-frame unchanged/adapted deterministic records. Remaining coverage, terrain and packet-work limits are listed
in the final section.

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
