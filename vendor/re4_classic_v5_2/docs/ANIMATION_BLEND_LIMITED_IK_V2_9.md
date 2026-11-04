# RE4 Classic Codec v2.9 — Local FCV blending and bounded experimental IK

## Implemented

- `re4_skel_evaluate_blend` samples TWO independently decoded FCV curves on the same PMD skeleton and blends LOCAL rotation deltas on the shorter Euler angular arc before calculating the FK hierarchy. This keeps the original multipart retargeting path intact. It is a practical crossfade, **not** verified native Capcom animation blending. Missing rotation tracks contribute zero bind-pose delta.
- `re4_root_blend_relative` mixes displacement **relative to each clip's sampled origin**, so differences between clip root starting positions do not cause an instant jump. It rejects signed overflow, and writes output atomically on success.
- `re4_skel_ik_ccd_axis_limited`: opt-in WORLD-axis 2-pivot CCD correction with caller-specified signed angular correction budgets around the incoming pose. These limits are NOT extracted from the game; they only keep the experimental solver inside configured correction ranges. Zero budgets prohibit all correction.
- `re4_character_clip`: `--blend-file=OTHER.fcv --blend-percent=0..100`. Primary and secondary frames are matched by fraction of their max_time; both FCVs remain in their original units. The source file is NOT modified. The old `--root-apply`, `--ik-window`, and multipart loading options are compatible. The diagnostic CSV gains `blend_percent` and `blend_frame_b`.
- `tests/re4_blend_test.c` tests both endpoints, the middle pose, invalid weights, and blended root motion. Existing 10 suites pass unchanged.

## Usage

```sh
make -j4 all
make check
./re4_character_clip pl0000.pmd clip_A.fcv 0 6 3 output_dir pl0001.pmd lh00.pmd rh00.pmd --root-apply --blend-file=clip_B.fcv --blend-percent=50
```

The final 2 flags are a pair and must both be present. The percent is constant for the requested export; time-varying transition weights are a future extension. The IK limits are an API option, not a CLI default.

## Verified with the provided RE4 PC 2007 files

- Leon: PMD `pl0000` + hair + two hands, clips 0037 and 0041, frames 0/3/6. All three exports contain **14,751 vertices**. 0% blender output equals the unblended original OBJ byte-for-byte at frame 6; 100% differs as expected.
- Ganado `em10`: body + head + two hands, clips 0176 and 0001, frames 0/3/6. Each export contains **3,729 vertices**.
- VTK 9.6.2: real generated OBJ/MTL/TGA, 55 actors for Leon and 14 for Ganado; sample frame 3 images and 3-frame GIFs generated. No generative imagery.
- Linux GCC: `-std=c89 -Wall -Wextra -pedantic` full build and 11 test suites. ASan/UBSan tested separately (11 suites and real Leon 2-frame export); no issues in those runs.

## Limits / what this is not

1. This is a **fixed-weight local Euler crossfade**, not proven native RE4 quaternion or Hermite blend. Rotation order and interpolation policy are inherited from our earlier engine.
2. IK joint correction limits are imposed around the current pose in world-space, not inferred anatomical bone constraints. Heel, toe, hip balancing and exact native IK control targets remain unresolved.
3. Root motion coordinate basis remains an approximation; large offsets in some FCV samples should not be interpreted as verified game movement.
4. No collision system, morph/facial target solver, or Win32 runtime verification is included.
5. Images are computed from the user's supplied game files for private diagnostic testing; original proprietary assets are not included with the full-source ZIP.
