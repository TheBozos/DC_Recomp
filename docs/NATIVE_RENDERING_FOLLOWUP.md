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

Exact sky texture/colour and full-game visual parity remain open.

Local evidence is under `build/native-cleanup30/` and the two manual-session
directories referenced by its handoff. Assets, cards, generated code and captures
remain local. No runtime packet interpretation or engine change is introduced.
