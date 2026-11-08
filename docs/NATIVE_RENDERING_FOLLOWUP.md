# Native rendering masks, sprite seams and periodic hitches

The user-controlled November 7 run exposed scene shapes in Norune's sky, rectangular
halos around the dungeon entrance fires, and brief freezes despite a counter near
60 FPS. The input came from the user's PS4 controller, with isolated copied cards.
The first run recorded frame digests and periodic screenshots; the second omitted
both. Performance qualification must distinguish these two configurations.

## Work-image provenance

The model adapter previously guessed that a pixel-less work record at legacy place
`0x1a40` contained the previous rendered scene. This injected scene imagery into
the sky without a corresponding native producer. The guess is removed. Models
can still sample work images established by actual native producers, and retain
their computed geometry and colours when no native image exists. This does not
establish exact original sky texture or colour parity.

## Generated flame alpha

The native `blendTextuer` owner initialized a work rectangle with opaque grey,
then composed A and B with RGB blend operations. The accumulated render-target
coverage remained opaque. Sampling it for an additive fog sprite therefore showed
the rectangle around the flame.

The owner now writes A's and B's own alpha after their respective RGB operations,
using native alpha-only passes. B replaces alpha within its destination, while A's
alpha remains elsewhere. This preserves transparent borders even when A and B have
different destination rectangles. The existing auxiliary-image sampler already
preserves target alpha; the engine is unchanged.

`dc_generated_texture_tests.cpp` executes the actual native model, blend and sprite
owners with invented assets and GPU readback. The initial fixture failed five
checks before the changes. The expanded twelve sprite cases cover fog on/off,
vertex alpha 0/64/128, and full/inset B rectangles. Transparent source texels carry
nonzero RGB to ensure their invisibility comes from alpha. The CPU model-lifetime
suite also checks that an unproduced work material cannot guess previous content.

## Diagnostic output and frame time

The second user-controlled run, without frame recording or automatic screenshots,
still showed brief freezes. Among the first sampled steady-play timing windows,
47 of 48 windows containing the native 300-frame diagnostic report exceeded
100 ms for the worst picture interval. Other windows had a median worst interval
of about 19 ms. Most game-frame work samples were 7–10 ms; the work summary excludes
samples above 100 ms, so its averages cannot rule out these freezes.

The report was written synchronously before publishing a picture, in many stderr
writes every 300 native frames. It is now opt-in with `DC_NATIVE_STATS=1`. Normal
play retains periodic counter resets and frame publication without that report.
The native compatibility/transfer summary at process exit remains available.
Two subsequent 4,200-frame window runs used the same binary, starting cards and
input script, with only `DC_NATIVE_STATS` changed. Both completed with zero
register/unknown accesses, VU0 calls and GIF submissions. In the predefined stable
window (game frames 3,300 through 3,999, before the final capture):

| Mode | Two-second timing windows | Minimum game FPS | Worst picture interval | Windows over 100 ms |
| --- | ---: | ---: | ---: | ---: |
| Normal quiet mode | 6 | 59.95 | 19.74 ms | 0 |
| Diagnostic report enabled | 6 | 47.83 | 135.47 ms | 3 |

These results support the report-linked hitch fix on this route. An earlier
100.65 ms interval remained in the quiet run; qualification covers the recurring
report-linked stalls in the stable window, rather than all possible pauses.

## Outdoor shadow-mask coverage

The subsequent manual run exposed missing-looking outdoor shadows and a darker
scene. The native shadow work image was cleared with an opaque black draw. The
renderer intentionally assigns alpha 1 to opaque draws, so that cleared image
still covered the entire screen. Compositing it at shadow strength darkened all
pixels, including the character, instead of revealing the silhouette where the
shadow geometry marked it. A live pass-enabled/pass-disabled comparison exposed
this global darkening.

The shadow owner now clears alpha separately after clearing RGB, using the same
alpha-only operation already used by the native dialog mask. Shared scene depth,
shadow geometry, projection and per-pass strength are unchanged. Every pass clears
coverage again so same-frame work-target reuse cannot repeat old shadows.

`dc_shadow_mask_tests.cpp` runs actual native begin/draw/end owners with invented
geometry and GPU readback. Seven assertions fail before the correction and pass
after it. They cover untouched background, overlapping draws darkening once,
foreground depth occlusion, and two independent passes reusing the target. All
five GPU suites pass. The corrected Norune replay capture shows a localized
character shadow while preserving scene brightness. Full-game shadow placement
and exact console visual parity remain unqualified.

## Qualification status

The final rebuild passes **80/80 normal suites** and **21/21 disc-free suites**,
including generated-texture GPU, balloon GPU and model-lifetime regressions.
Both paired timing runs complete normally. GitHub CI exercises the disc-free
fixtures; GPU and retail checks described here run locally. The
subsequent user-controlled quiet run completed 48,292 frames with zero register
accesses, unknown accesses, VU0 calls or GIF submissions. The user still reports
continuously uneven motion. The existing counter timestamps precede rendering
and buffer swap, so a 60 FPS reading does not settle presentation smoothness.
A first fullscreen comparison was attempted as the user exited and is inconclusive.
In the next fullscreen run the user reported smoother movement. Fullscreen and
benchmark isolation changed together, so this is subjective improvement without
an established single cause. That run subsequently completed 172,663 frames
normally, again with zero register/unknown accesses, VU0 calls and GIF submissions.
One requested live draw dump caused an approximately 19-second diagnostic pause;
that interval is excluded from game-performance conclusions.

The same run exposed narrow transparent gaps between minimap tiles, and a later
capture shows a vertical seam through a dialog balloon. Both use native rectangle
helpers that shorten the right edge by 1/16 logical pixel. The generated dialog
owner calls the four-colour rectangle helper for adjoining pieces. At the user's
1920 by 1017 window size, the map's boundaries at logical x=460 and 476 can expose
screen columns 1256 and 1290. Native rectangular helpers now use the full right
boundary, preserving the existing nearest-filter UV inset. The GPU regression
executes five real rectangle owners at four picture heights (448, 720, 1017 and
1080), with adjacent translucent tiles and a contrasting neighboring atlas tile.
Ten fractional-scale cases failed before the correction; all twenty pass after
it. In the 1017-high fixture the previous gaps at columns 1256 and 1290 are covered
without overlapping alpha or exposing the neighboring atlas tile. Existing
balloon-mask and generated-flame GPU suites also pass. Fresh in-game map/dialog
captures and continuous-motion qualification remain open.

`--fullscreen` and `--no-vsync` expose presentation comparisons without changing
the default windowed, VSync-enabled behavior. They do not themselves establish a
smoothness fix.

Two later 4,600-frame runs use identical starting cards, a movement script and a
local diagnostic executable that buffers presentation samples until shutdown.
Only VSync differs. In game frames 3300 through 4299, both record 1,000 samples,
zero repeated or skipped submitted serials and no swap-return interval above
25 ms. The 95th-percentile swap-return intervals are 18.02 ms with VSync and
17.99 ms without it. This does not explain the reported continuous unevenness.
Swap return is not a scanout timestamp, and the sampled geometry fingerprint is
not a full rendered-image comparison. The observer is local only; engine source
and the normal executable are restored without it.

## Water refraction and choice cursor (November 8)

`CWater::CreateVUData` still supplies the original height field and ripple
parameters, but the native owner previously submitted only a flat-colour
material. Dungeon `MainDraw` first copies the current 640 x 224 frame with
`MGMoveImage` into `water`; the resident `work` record used by `CWater` shares
that texture place. The existing native copy owner already produces this image.

Water now samples that actual native target with screen coordinates displaced
by the original previous-row/current and current/next-column height differences
times `CWater + 0xA0`. Projection follows near/far clipping; screen coordinates
then interpolate without another perspective divide. Tint, alpha blending,
foreground depth testing and disabled depth writes are retained. Unproduced
work images retain the colour-only fallback. No previous-frame guess is added.

The synthetic owner checks cover source provenance, ripple offsets, projection,
clipping, finite coordinates and guest-state preservation. OpenGL pixel checks
cover scene detail, displacement, foreground occlusion and unchanged pixels
outside the water. Against the previous renderer, these checks report 58 failures,
including four rendered-pixel failures. The first corrected build passed all
83 local suites. The final build, including finite-projection rejection and the
cursor adjustment, also passes all 83 suites (six GPU checks, 188.09 seconds).
These checks do not establish console water parity; the user's
dungeon pool still needs an in-game comparison after rebuilding.

The choice-menu hand occupies a 32 x 32 tile, but `ClsMes::DrawMesWin` places
its left edge at `text_x - 12`. The guarded layout adjustment changes that to
`text_x - 36`, leaving a four-pixel gap. Selection, vertical motion, font drawing
and other sprite helpers are unchanged. This is an intentional layout correction;
its in-game screenshot comparison remains to be checked. The original/adapted
whole-function fixture covers 192 cases including 40 hand draws: only the
hand's horizontal coordinate changes; message state and the other draw inputs
remain equal to the original.

The user also reports that the sky changes brightness strongly during camera
movement. The earlier removal of unproduced scene sampling does not establish
that this remaining issue is fixed. Exact sky texture/colour and full-game
visual parity remain open. An optional `DC_PROBE_SKY=1` observation records the
actual sky clock and camera basis at most 64 times, once per 120 sky calls.
It is disabled in normal launches and performs no RAM hashing or frame dumps.
In the upward camera sweep, scripted rotation ends at frame 4250. The uniform
sky region changes from RGB (24,34,149) at frame 4250 to (16,28,173) at 4850.
A headless replay logs the identical camera basis from frames 4380 through
4860 while the sky clock advances from 9.634967 to 9.901843. This establishes
a time-dependent colour change while stationary; it does not explain or rule
out an additional camera-dependent brightness jump in the user's route.

## Texture lifetime after weapon customization (November 8)

The user returned from weapon customization on dungeon Floor 4 and saw menu
artwork on dungeon surfaces. `ExitWeaponMenuSelect` and `ExitBattleMenu` call
`CTextureManager::CleanUpTextureList` (0x133A60), which compacts the 196 texture
records by copying complete 0x50-byte descriptors into holes. Native model
bindings and image caches previously retained the old table-slot addresses.
The broken run's read-only RAM snapshot is retained locally under
`build/native-texture-return/`; no saves or guest memory were changed by capture.

Model construction now records texture identity and an owned decoded image.
Drawing resolves the same asset after table compaction, without selecting a
newer same-name texture or a menu image that reuses the old slot. Live updates
remain visible while that asset exists; a removed asset retains its most recent
owned image. Remake preserves the original binding. The original CPU cleanup
still runs, and the native cleanup boundary migrates pinned/decoded images,
patched-record ownership and resident-copy references alongside the records.
Unbound live lookups no longer cache reusable slot addresses.

Synthetic lifetime cases cover compaction, old-slot reuse, duplicate asset names,
Remake, live texel updates, deleted records and native pinned images whose source
file buffer has already been reused. The corrected fixture reports seven
failures against the previous renderer and passes with the correction. The other
82 suites passed in the first qualification, followed by the corrected lifetime
fixture passing separately. The combined build, including the expanded cheats,
passes all 83 suites (six GPU checks, 175.28 seconds). The expanded native Cheats
menu was captured with all four toggles, the ATK field and Apply button visible.
The user's in-game menu-return comparison remains pending.

Local evidence is under `build/native-cleanup30/` and the two manual-session
directories referenced by its handoff. Assets, cards, generated code and captures
remain local. No runtime packet interpretation or engine change is introduced.

## Companion menu cancellation crash (November 8)

The human run `build/manual-play-20261108-022006` ended at frame 94201 after
the user selected an empty companion slot. The watchdog found no completed
frame for 600 fields: the main thread was in `sceSifBindRpc` (0x1172D8),
returning to `scmd_prechk` (0x10AF9C). The compiled SDK loop was trying to bind
the CD/DVD command service 0x80000593, which the native disc implementation
does not provide as an RPC server.

`BattleMenuCharaKey` calls the original `BreakReadBG` (0x13EEC0) on companion
load cancellation. Its `sceCdBreak` call (0x10B4F0) previously lacked a native
binding and therefore entered that SDK bind loop. BozoEngine now implements
and exports `sceCdBreak`: it ends the native sector-read completion timer,
reports `SCECdErABRT` (1), and returns success. Already copied sector bytes
remain in RAM; a new read replaces both the error and completion time.
The original game routine still executes `InitReadBG`, clearing the 32 queue
entries and resetting the active-load bookkeeping. No watchdog or menu
selection check has been disabled.

The shared library regression covers busy/idle cancellation and a successful
subsequent read using a synthetic disc. The port regression executes the real
compiled `BreakReadBG` under the dispatcher for busy and idle drive states,
checks its native external declaration, rejects unexpected SDK/RPC calls,
and checks all queue flags and untouched descriptor bytes. Both tests fail
before the implementation and pass afterwards. Evidence is local under
`build/native-companion-cancel/`. The latest saved cards (02:39:56) from the
closed human run were copied and verified before the isolated play check.
The rebuilt port passes all 84 regression suites, including six GPU checks
(172.54 seconds). This cancellation change includes local BozoEngine source
changes; it is not a game-specific bypass of the RPC wait.
The isolated 10,000-frame play check loaded the latest copied save, opened
Allies, selected an empty `???` slot and continued normally to its planned
frame limit. `play/captures/frame_009100.png` shows the selected empty slot;
the final log reports zero device-register accesses/unknown accesses, zero
native VU0 calls and zero GIF submissions. This checks this menu route, not
all companion loading or full-game stability.

## Dran's house frame rate (November 8)

The human fullscreen run `build/manual-play-20261108-030019` sustained roughly
35–37 FPS (about 27 ms per picture) in the reported room, and about 42 FPS in
the later sampling window. A bounded four-second native instruction-pointer
sample found 391 of 460 game-thread samples in `recordTexture`; 386 were in
its serial FNV word-hash loop. The presentation thread was mostly waiting.
The sampler briefly suspended each sampled thread only to read its CPU
context and resumed it in a finally block; it did not attach a debugger or
write game memory. Evidence and symbol mapping are local under
`build/native-dran-house/`.

The decoded-image cache now hashes its existing texel and palette ranges with
the engine's XXH64 implementation, splitting reads at the guest RAM wrap
boundary. Cache checks still run on every lookup, so this does not defer
animation updates or require guessing when game code changed texture memory.
The hash is an internal decoded-cache key; offline texture-store identities
and asset matching do not change.

Under the exclusive bench lock, three runs of 2,000 native model texture
lookups of a 256 KiB image had median cost 54.367 us/lookup before and
21.077 us/lookup afterwards (about 2.6 times faster). Draw-list digesting was
disabled in both measurements, matching normal play. Regression cases check
end-of-image texel edits, palette-only edits, ownership of earlier submitted
images, and writes across the physical RAM wrap boundary. These are texture
lookup measurements, not an assertion of the room's post-change frame rate.
The complete rebuilt port passes all 84 suites, including six GPU checks
(181.64 seconds). A fresh human fullscreen run uses verified copies of the
latest 03:38:12 saved files from the closed session. The room's post-change
frame rate still needs that play comparison.

## Sky camera colour interpolation (November 8)

The user again reports large sky colour changes while rotating the camera.
A read-only capture from PID 28992 is local under `build/native-sky-rotation/`.
The gradient's retained live MDT is 0x9D9E00; its two authored colour levels
are constant around the circumference. The native OpenGL shader currently
uses default perspective interpolation for vertex colours. SCEA's
[Introduction to GS Programming, slide 22](https://www.scribd.com/document/732592976/gsprogramming1)
specifies that Gouraud shading is not perspective correct.

A source correction changes only the 3D colour varying to `noperspective`,
leaving texture coordinates perspective corrected. A GPU regression checks
the same projected red/blue gradient with uniform and varying homogeneous w,
plus a textured control that requires perspective-correct UVs. The local
`dc-sky-replay` diagnostic freezes the sky clock and varies camera yaw using
the captured RAM. Static native-owned model copies are not embedded in that
capture, so its replay must not be presented as complete scene parity.

The corrected GPU fixture fails only the varying-w colour check before the
shader change (RGB 201,0,54 rather than 124,0,131), and passes after it; the
perspective-correct texture control passes in both versions. The initial
texture-control expectation was corrected to the native texture RGB scale
before this baseline recheck. The first full qualification passed 84/84
suites. The yaw replay covers the retained gradient, not the static native-owned
dome/cloud copies, so it does not establish that every reported camera-related
colour change is eliminated. A complete in-game rotation comparison remains
necessary. The latest 13:42:49 save files were copied and hash-verified after
the human run closed, before any rebuild or test.

## Door item-menu vertical seam (November 8)

The user supplied `codex-clipboard-b8d5e176-aa5a-4251-8698-bd357ae74e56.png`
and clarified that the defect is the vertical line between the fourth item
column (the bomb) and the rightmost column. Do not treat the horizontal line
under the heading as a separately confirmed defect.

The owner is `EventItemSelectDraw` (0x224D50), which calls
`DrawEventItemBoard` (0x225420) for five 40-pixel cells spaced by 42 pixels,
and `DrawEventAndFishMenuBoard` (0x224A60) for the background. The latter
uses `DrawEventAndFishMenuBoard_Ver` (0x224910), then `DrawMenu2DSprite`
(0x22CF20), ending in the native 0x15C310 rectangle owner. That owner already
uses full rectangle geometry after the earlier seam correction. The remaining
defect came from shrinking the source UV interval by 1/32 texel at each end.
That also moves interior texels: a narrow opaque separator inside the wide
background strip sampled the transparent column beside it at a covered pixel
centre. Removing the shrink alone still left floating-point boundary rounding.

Nearest-filter sprites now keep the source interval width and use a constant
sampling bias, bounded to the atlas piece after interpolation by the renderer.
This preserves interior separator positions while preventing sampling of a
neighboring atlas piece. Default-disabled `Material::uvBounds` affects only
batches that request it, and active bounds contribute to the draw-list digest.
The other rendering/material defaults are unchanged.

A synthetic wide-strip regression fails the 1080-high case before the change
(at logical x=374) and passes afterwards. The existing twenty neighboring-atlas,
translucent-tiling and fractional-letterbox cases still pass, together with
four new interior-separator scales. The analytic separator cases use integer
viewports so final letterbox resampling is not confused with atlas sampling.

The original board and cell routines were also replayed with the matching
English `eventmnu` atlas from the user's local converted assets. In paired
1440x1080 captures, column 841 was black at y=620,650,740,770 before the change
and orange afterwards. `build/native-door-seam/after/door-1080.png` was checked
visually: the reported vertical join is continuous. These captures reproduce
the background owner and atlas, without the live game's item icons or scene.
Assets and reconstructed diagnostic RAM remain local and untracked.

The final combined build passes 84/84 suites, including six GPU checks
(162.30 seconds). The interpolation pixel regression, twenty earlier atlas and
tiling cases, four internal-separator scales, model/texture lifetime cases and
all existing packet-retirement checks are included. A final scripted full-game
sky rotation uses separate copies of the earlier known Norune test save; the
human restart must use the latest 13:42:49 cards, not that diagnostic route.
That 5,000-frame rotation run completed normally with zero register/unknown
accesses, native VU0 calls and GIF submissions. Its upward sky captures were
inspected for scene smearing and rendering failures. The sky still changes with
the advancing sky clock; the player's reported strong rotation-related change
is not claimed fully resolved by this smoke test.

## Missing sky texture borrowed a generated work image (November 8)

The player still saw strong camera-related colour changes after the interpolation
correction. A full-game trace using copies of the latest 14:09 save files and a
fixed sky clock exposed a separate ownership error. The gradient MDT at 0x9D9E00
names `f01sky12`, which is absent from the texture manager. During native model
construction, `noteMdt` captured the original allocator's record-zero fallback
as an actual texture binding. Record zero was `work`, TEX0 0x628029A40. Once a
native producer wrote that work image, the sky sampled scene-derived pixels
(`fromFrame=1`, source 2), creating an unintended dependency on the scene.
The earlier RAM-only replay lacked the native-owned binding vectors and missed
this path; its interpolation result remains valid but did not resolve the report.

Native model capture now records only a successful named texture lookup. A
missing name keeps its geometry and vertex colours without borrowing another
asset's image. The original record-zero fallback is retained for allocation
size/ABI calculations. Materials explicitly named `work` still resolve and
sample their actual native producer; water and frame-copy rendering remain
enabled. No sky name or MDT address is special-cased by the correction.

The model lifetime regression creates a real native frame-copy producer, then
tests both a missing material and an explicit `work` binding. Before the change,
only the missing-material assertion fails; after it, both pass, together with
the existing lifetime, compaction, palette-update and RAM-wrap cases. Evidence
is local under `build/native-sky-rotation/followup/`. Paired 5,000-frame full-game
runs complete normally with zero register/unknown accesses, native VU0 calls or
GIF submissions. Their trace changes from record-zero `work`, frame 1/source 2,
to a missing binding, frame 0/source 0. The sky no longer samples that image.

The paired upward captures still show colour differences across the route.
`DC_SKY_TIME` fixes only `EdDrawSky`'s input; the game updates the gradient's
vertex colours elsewhere using its advancing clock, so these captures do not
establish that every remaining variation is caused by rotation. An experimental
global clock hold interfered with fades and yielded black captures; it was
removed, and those captures are excluded from visual validation. A temporary
sky-part selector was also removed because its call-scoped observation did not
isolate the complete sky. The corrected binding is regression-tested, but the
player's whole visual report remains subject to a normal-play comparison.

The bounded `DC_PROBE_SKY` and partial-clock `DC_SKY_TIME` diagnostic options
remain opt-in; normal play must unset both. The first full qualification of the
binding correction passes 84/84 suites, including six GPU checks (169.09 seconds).
The final rebuild after removing the experimental probes also passes 84/84,
including six GPU checks (182.54 seconds). The next human session uses verified
copies of the latest 14:09 files from `manual-play-20261108-140324`, with the
normal clock, fullscreen presentation, and all sky probes disabled.

In that normal-play session (`manual-play-20261108-143854`), the player answered
"Looks stable now" when asked whether outdoor camera rotation still caused
strong sky colour changes. This confirms the reported symptom is resolved in
that play comparison; it does not establish full-game visual parity.

## Dran's mirrored face sampler seam (November 8)

The player reported a thin dark vertical line down Dran's forehead and muzzle.
A single read-only RAM capture from the current human run is local under
`build/native-dran-face/`; no process suspension or writes were used. His
`c12a01` 512x512 atlas and live MDT contain face-centre vertices at U=0.
The opposite atlas edge contains darker, unrelated pixels. Linear repeat
sampling blends those pixels into the mirrored centre join.

The compiled original `SetEnv` (0x140DA0), at 0x140E44..0x140E58, supplies
register 8 with value 5: `CLAMP_1`, both axes clamped. Native MDT material
construction previously left both clamp flags false, so the GPU repeated
instead. The prepared correction sets both native sampler flags; it changes
neither the model nor the atlas and does not introduce console register state.

`dc_native_model_sampler` is a new synthetic GPU fixture checking
mirrored U/V joins, upper-edge out-of-range samples, and an interior control
at three presentation sizes. The baseline fails twelve boundary cases while
the three interior controls pass. All fifteen cases pass after the correction.
The paired synthetic pixel captures were inspected: the dark centre line is
present before the change and absent after it. This reproduces the sampling
defect, without asserting a full-game Dran capture comparison.

The baseline/fixed qualification script is local under
`build/native-dran-face/qualify.py`. The rebuilt port passes all 85 suites,
including seven GPU checks (160.76 seconds). The human session closed normally
at frame 57038; its latest 14:57:48 saves were copied and hash-verified before
the build. The correction is in the rebuilt binary, pending a human comparison.

## Matataki sky boundary (November 8)

After the earlier Norune stability confirmation, the player supplied
`codex-clipboard-61a65a5e-cef5-4691-a1a1-7151d408b35f.png` from Matataki.
It shows a sharp polygonal join between a dark upper sky and a lighter lower
sky above the forest backdrop. The Norune camera-colour validation does not
cover this distinct scene defect.

The player deliberately kept Save 2 (`darkcloud1`) in Norune as the reproduction
checkpoint, and confirmed that Matataki is reached through the region menu.
The initial automated selection mistakenly loaded another slot. A dedicated,
hash-verified backup now lives under `build/native-matataki-sky/file2-checkpoint/`.
Its isolated diagnostic card contains Save 2's exact bytes in the first slot;
the original saves and their slot numbers are unchanged. The corrected script
selects that copy, opens "Go to Other Area", and chooses Matataki. Earlier
Norune-route captures are excluded from Matataki validation.

`arrival/captures/frame_006000.png` reproduces the reported polygonal boundary,
even with the model sampler correction. A read-only RAM snapshot and one native
frame trace identify the plain sky-gradient model and textured cloud layers.
The gradient's lit RGB is already a display colour, but the renderer applies
0x80-unit texture-modulation scaling to all vertex colours, nearly doubling
the plain gradient relative to the textured layers. The distinction follows
the fragment colour versus texture modulation operations in SCEA's
[GS User's Manual, sections 3.3 and 3.4.9](https://www.scribd.com/document/784545197/GS-Users-Manual).

An opt-in native `Material::directColour` now preserves display RGB for
untextured model primitives. Alpha retains its existing 0x80 unit, and textured
primitives retain modulation. Other adapters keep their existing default;
the flag is included in active draw-list digests. The DC adapter enables it
only for models actually submitted without texturing. A global shader-only
experiment also affected fades/screen copies and was discarded; its darkened
captures are not validation evidence.

The model GPU fixture now also checks direct RGB (32,64,96) and half-alpha
blending (16,32,48), at three scales, alongside the fifteen sampler cases.
The isolated model-colour correction passes those pixel checks. In
`model-colour/captures/frame_006000.png`, the sharp Matataki boundary is absent;
this full-game arrival capture was visually checked. Final baseline/fixed
qualification fails exactly the six new RGB/alpha cases before the correction,
with all fifteen sampler controls still passing; all twenty-one cases pass
afterwards. The complete rebuilt port passes 85/85 suites, including seven GPU
checks (143.84 seconds).

The final 9,000-frame Save-2 route reaches Matataki, completes the arrival
dialogue and turns the first-person camera left and right while looking up.
It ends normally with zero register/unknown accesses, native VU0 calls or GIF
submissions. The arrival and rotation captures under `final-play/captures/`
were inspected: the reported sharp sky join is absent. This is bounded scene
validation; the player's comparison of the rebuilt normal-play run remains
pending. The new human session preserves Save 2 in its original slot and uses
the normal clock, fullscreen presentation and no diagnostic probes.

The player subsequently confirmed "The boundary is gone" after loading the
rebuilt normal-play run and travelling from Save 2 to Matataki. This confirms
the reported sky join is resolved in that comparison, beyond the scripted
captures; it does not establish full-game visual parity.


## In-game Cheats, inventory capacity and Quit Game (2026-11-08)

Options now opens a native entry page with Game Options, Cheats and Quit Game.
The original settings, loading/fade cadence and exit lifecycle remain intact.
The scrollable cheat page shares F2's atomic settings and one-shot requests,
using an owned copy of the common game font. Its custom pages suppress only
the outer Options help draw; original settings/help remain available.

Alongside health, WHP, thirst, combo stamina and equipped ATK, the page adds
positive combat ABS multiplication, Gilda edits, active consumable quantities,
horizontal movement speed, a shared sky/lighting time hold, and inventory
capacity. The original bag-upgrade path caps capacity at 100; the new control
allows 1–100 while protecting occupied entries and active-item quantities.
These are game-thread edits, never changes to weapon-menu preview copies.
Town resolves the authoritative status through CSaveData's embedded record;
it cannot rely on DUN's cached pointer being initialized.

The ABS test executes the original compiled monster reward tail, including
its maximum and reverse-ABS branch. Other regression cases check unchanged
neighboring bytes, stale item IDs, capacity constraints, movement before
collision and shared time inputs without stopping global timers. Controller
cases cover scrolling, each new setting, original Options transitions and
explicit/cancelled quit requests. The rebuilt port passes **86/86 suites**,
including **seven GPU tests**, in **141.05 seconds**. Results are local under
`build/native-extra-cheats/qualification.log`.

The Save-2 diagnostic card is separate from human cards. A 7,300-frame native
run checks the first and last cheat pages and original Game Options. A second
script reopens Options and confirms the applied Gilda, active quantity and
100-slot capacity values, then checks default Cancel and explicit Quit Game.
The window shuts down normally at frame 9,202 (`end=stopped`), before the
9,600-frame limit, with zero unknown/register accesses, native VU0 calls or
GIF submissions. Captures under `build/native-extra-cheats/quit/captures/`
were inspected, including the quit confirmation and scrolled inventory row.
This does not replace manual coverage of every combat/movement situation.

The README was refreshed across gameplay status/limits, build/run steps,
controls, cheats, quitting, saves, tests and native architecture. Latest human
cards (16:07:50) were separately backed up and copied with all eight file
hashes verified. The original Save-2 Matataki checkpoint remains unchanged.

The player subsequently reported "Everything seems good !" in the rebuilt
fullscreen session. This is positive general playtest feedback for the current
build; it does not extend the bounded coverage above to a full playthrough.


## Main integration and retained engine pin (2026-11-08)

The shared fixes were integrated onto current BozoEngine main in PR #77,
retaining its newer texture functions, mipmap and image-processing paths.
Current main already has sceCdBreak: that implementation was retained, with
the sector-cancellation/reuse regression added. Two engine test-tool paths
were corrected for embedded game builds. Linux and Windows engine CI pass.
The integrated tree passes 124/124 local suites, including eight GPU checks
(190.94 seconds), after aligning the trial port test with main's observable
idle/active cancellation contract.

That synthetic pass does not qualify a full DC upgrade to current engine main.
The scripted Matataki route stops during initial loading at frame 76, waiting
in check_now_loading_vsync_end (0x153f60; RA 0x153f94), with zero unknown/register
accesses, native VU0 calls or GIF submissions. The later cheat replay was not
run against that tree. Logs remain local under build/main-integration/.
The cause of the newer-engine loading stall has not been established.

DC therefore retains its published, player-accepted 2278ba0 engine pin while
the shared fixes land upstream. Its original cancellation regression is kept
with that pin's contract. The newer engine upgrade remains deferred; merging
the gameplay fixes and refreshed README does not enable the trial upgrade.

## Engine main and the loading-screen vsync callback (2026-11-08)

The frame-76 stall on engine main came from one changed binding. `init_now_loading` installs
`VSyncCallBack_Load` through `MGInitVSyncCallBack`, and `wait_now_loading_vsync` then spins on
`now_loading_vsync_end`, which only that vsync callback sets. `MGInit` registers `VSyncCallBack` with
`sceGsSyncVCallback`. The previous pin had no native `sceGsSyncVCallback`, so the game's libgraph code ran:
`AddIntcHandler(2, VSyncCallBack)` and `EnableIntc(2)`, fired by the kernel's field clock. Engine main
answers `sceGsSyncVCallback` natively and queues the callback only in `Dispatcher::frameCompleted`, where a game
frame ends (its `kernel.md` section 2). The loading wait ends no frame, so the callback never ran.

DC now registers its compiled `sceGsSyncVCallback` (`0x113DE8`) in the port registry, which takes precedence over
the engine's native library at that external address. The callback is the vblank interrupt again, as on the
previous pin. No engine change is involved. The CD-cancel regression asserts main's `sceCdBreak` contract: an
active read ends at the current native time, and an idle drive's error is unchanged.

DC moves to BozoEngine main `296ac6d`. Qualification on this PC:

- **125/125** local suites including eight GPU checks, and the 14 release-tool tests.
- Seven scripted routes on the new build and on a pin build carrying the same fix (`2278ba0`), each from identical
  copied cards: Matataki travel (9,000 frames), in-game Cheats ending in Quit Game, the outdoor water route, the
  companion menu, a main-menu route, a save route and a fresh-process reload. All end identically on both, with
  zero register/unknown accesses, native VU0 calls or GIF submissions. VIF1 operation counts differ by under 1%.
- Captures match where the scene is the same, mostly under 1.5 mean levels. A separate save route on copied cards
  shows "Saving completed." on engine main; its written `darkcloud0` is byte-identical to the pin build's, and a
  fresh process lists and loads that save.
- Two kinds of difference were examined, not dismissed. Some transitions finish a few frames later on engine
  main, so fade captures land at another point of the same fade. Windowed runs of the unchanged pin also move
  that fade from run to run. A dungeon floor entered by the save route starts Toan at another position on engine
  main, while two pin runs agree. `buildRandomMap` seeds each new floor from `rand()`, a sequence that other code
  also draws from and reseeds (`DrawFire`, villager placement). A different number of frames before the floor
  therefore gives another valid random floor. The seed itself was not dumped; the walk, menus and fades before
  the floor match.

Not covered by these routes: dungeon combat, the new-game opening, Georama and a human play session on the new
engine. The released v0.1.0 keeps its `2278ba0` engine; this change applies to the next release.

## Texture change checks once per model draw (2026-11-08)

A human fullscreen run on engine main dropped to 36-42 FPS in a house interior after eleven minutes at 60.
A four-second instruction-pointer sample of the running game put 265 of 455 game-thread samples in
`contentHash`: the record-texture cache's change check, which hashes a record's texels and palette (whole
pictures up to 256 KiB) on every lookup. The presentation thread was mostly waiting. This is the cost the
"Dran's house frame rate" section reduced with XXH64, not an engine-main regression.

The model draw looked a record up once per primitive, and a model's primitives mostly share one picture. No game
code runs between the primitives of one native draw, so their textures cannot change in between. `drawModel` now
looks each record up once per draw and reuses the result for its other primitives. Lookups across draws still
check every time: game code runs between them (character faces are rewritten between characters).

`DC_NATIVE_STATS=1` now also reports record-texture lookups and hashed bytes per frame, averaged over 300 frames.
At the Matataki spot of the run's save: 480 lookups and 4.71 MiB hashed per frame before, 208 and 2.00 after; on
the scripted Matataki route 1.84 MiB before, 1.01 after. The build passes 125/125 suites, and the seven scripted
routes match the previous build (faces, HUD, menus, identical cards); the only differences are the known fade
timing and the random dungeon floor. A 42-minute human fullscreen run on this build averaged 60 FPS every
minute, its slowest two-second window 56.6 FPS, with up to 10.8 MiB hashed in a frame. Whether that run
revisited the slow house is not established from the log.
