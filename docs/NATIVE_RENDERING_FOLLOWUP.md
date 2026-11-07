# Native sky, generated flames and periodic hitches

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

## Qualification status

The final rebuild passes **78/78 normal suites** and **21/21 disc-free suites**,
including generated-texture GPU, balloon GPU and model-lifetime regressions.
Both paired timing runs complete normally. A fresh user-controlled quiet run and
GitHub CI remain follow-up checks. Exact sky texture/colour and full-game visual
parity remain open.

Local evidence is under `build/native-cleanup30/` and the two manual-session
directories referenced by its handoff. Assets, cards, generated code and captures
remain local. No runtime packet interpretation or engine change is introduced.
