# RE4 Classic Codec v3.0 - Deterministic time-varying FCV crossfade

## Implemented

- `animation/re4_transition.c` and `include/re4c3d/re4_transition.h`: **Q16.16 envelope**, C89 strict, caller-owned memory, no malloc and no floats/doubles. Curve choices: linear, cubic smoothstep and quintic smootherstep.
- Cubic/quintic weights evaluated with monotone Bezier/de Casteljau interpolation; avoids rounding-induced backwards motion in blend weights.
- `re4_character_clip` accepts two FCV files with `--blend-file=SECOND.fcv`, and **either** a constant `--blend-percent=0..100` (v2.9 compatible) or `--blend-ramp=FIRST:LAST`. Optional `--blend-curve=linear|smooth|smoother` (default smooth) only when ramp is selected.
- Each frame's computed weight is shared between local skeletal blend and optional root displacement blend. New `blend_weight_q16` column in `poses_index.csv`, along with effective `blend_percent` and mapped `blend_frame_b`.
- No change to character retargeting and 4-part Leon / 4-part Ganado export. By default, IK remains disabled and root translation is **not applied** unless `--root-apply` is explicitly set.

## Usage

```sh
make -j4 all
make check
./re4_character_clip pl0000.pmd A.fcv 0 40 2 output_dir \
  pl0001.pmd lh00.pmd rh00.pmd \
  --blend-file=B.fcv --blend-ramp=10:25 --blend-curve=smoother
```

`--blend-percent=50` continues to work with `--blend-file` if you want the v2.9 behavior. Do not combine a constant percent and a ramp. The ramp interval must fit the output frame range, and its end must be strictly greater than its beginning.

## Verification

- 12 C89 suites pass (`make check`), including all v2.9 regression suites and new monotonic envelope tests over 65,536 possible sample frames for every curve.
- Compiled/tested with GCC in Linux under `-std=c89 -Wall -Wextra -pedantic` and separately under AddressSanitizer + UndefinedBehaviorSanitizer.
- Actual RE4 PC 2007 resources (provided by user) from Leon `pl00` and Ganado `em10`, 4 PMD parts each: 3 frames (0,3,6), 14,751 and 3,729 vertices per frame respectively. Sanitized and ordinary exports match byte-for-byte.
- v3.0 crossfade frame 0 exactly matches an unblended clip A export; frame 6 exactly matches 100% clip B for the Leon sample.
- VTK 9.6.2 off-screen renders were generated from source C89 OBJ/MTL, real TGA textures, 55 actors Leon and 14 Ganado. Images are in the separate previews ZIP; original game resources are not redistributed inside the source ZIP.

## Known limits / cautions

1. This is a **new transition envelope**, not native Capcom transition timing. Local rotation mixing is inherited from v2.9; there is no quaternion SLERP or native animation graph yet.
2. The root position tracks in some FCVs, especially Leon, produce large offsets in model units. They are exported for research but **should not be interpreted as verified world-space locomotion**. `--root-apply` remains opt-in; previews disable it.
3. IK/heel/toe and hand constraints are still experimental. This release does not claim native RE4 IK equivalence.
4. A baseline Win32 build and full game-runtime integration have **not** been validated.
5. FCV B frame mapping still uses duration fraction from FCV A; mismatched gait phases are not automatically aligned.
