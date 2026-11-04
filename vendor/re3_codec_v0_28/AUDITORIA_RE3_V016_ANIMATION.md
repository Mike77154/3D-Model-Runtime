# RE3 Codec v0.16 — Animation Audit (2026-10-31)

## Verified work

- Integrates EDD/EMR pose decoding into a C89 integer-only software preview.
- Adds core API `re3c_animated_body_vertex` which uses existing DAT ownership/seam and rigid bindings, transforms from precomputed joint matrices.
- Adds two explicit root modes and a stable frame bounding box across sampled keys of a clip.
- Validates and refuses mismatched joint counts for a skinning bank.
- Compilation and synthetic tests pass with GCC and Clang using `-std=c89 -pedantic -Wall -Wextra -Werror -O2`.
- Audit of 100 EMD files (normal and `EMD08` directories): 100/100 exit-success (three keys per compatible clip, sampled subset of vertices).
- Render smoke test of first frame in first normal clip for 100 EMD/TIM pairs: 100/100 success.
- `EM34` Nemesis Normal A bank: 33 clips, 1511 keys, 1036 physical frames, 16 joints.
- `EM34` Damage bank: 7 clips, 446 keys, 446 physical frames, **15 joints** — not treated as compatible skin binding.
- `EM20` Cerberus Normal A bank: 27 clips; tested running/walking sequences.

## Clip 23 finding

`EM34` Normal A clip 23 is 131 keys long. During its later keys its root Z angle approaches a quarter-turn (~89.5 degrees at final key); the root Y field moves from -2268 to -207. The dramatic horizontal body position is therefore supported by source animation fields and may describe a fall, not a corrupt transformation. The old v0.15 diagnostic viewer suppressed frame/root translation; the new `frame` mode includes it. This does not prove identical timing/ground placement to the original game.

## Important scope limits

- The test does not establish correctness for every frame of every clip.
- Morph tracks, SCD part-state toggles, inverse-bind skeleton remaps, and original engine camera/root physics remain unverified.
- Neither REA nor Ghidra decompilation has been executed in this v0.16 step.
- Clip labels such as 'walk', 'attack', and 'fall' are visual interpretations unless independently verified.
- The preview's rasterizer is a diagnostic, not a PSX GPU/PC Direct3D replica.
- No Capcom EMD/TIM files are included in this source release.
