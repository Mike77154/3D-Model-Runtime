# Phase 05 — C89 Fixed-Point FCurve Evaluator

Version: 2026-11-02. Incremental upgrade from Phase 04 (all prior source retained).

## New feature

```sh
make clean all
./blendc89 --sample model.blend sampled.tsv 5.5
```

Sampled TSV columns:

`action layer strip slot curve rna_path component frame value`

- Evaluates stored **action-local** FCurves at a requested frame, encoded in signed Q16.16.
- Follows both legacy linked `bAction.curves` lists and Blender 4.4+ layered Action path from Phase 04.
- Uses SDNA to discover `FCurve`, `BezTriple`, and `FPoint` layout offsets.
- Supports CONSTANT (0), LINEAR (1), BEZIER (2), and linear interpolation across `FPoint` samples.
- Handles input frames with up to four decimal digits. Frame range must fit signed Q16.16.
- Reads endian-aware binary32 from serialized data via integer bit decoding, no floats.
- All intermediate blending arithmetic avoids `long long` and 64-bit integer types. Fixed-point accumulation is bounded.
- Bézier uses de Casteljau + bounded binary search of X. Handles outside the keyframe X range are **clamped**, making the result approximate compared to Blender's more elaborate handle correction.
- Bézier result Q16.16/TSV is not bit-exact to Blender's native evaluator. Tiny interpolation error is expected.
- Exact keyframe time returns the keyframe's value even for constant interpolation.
- Before or after curve bounds, constant extrapolation holds the endpoint. If serialized FCurve `extend` requests linear extrapolation, an out-of-bounds sample is **rejected as unsupported**. More sophisticated extrapolation is intentionally not faked.
- Unrecognized interpolation modes, drivers, and FCurve modifiers are rejected rather than evaluated incorrectly.

## Important NON-features

- No end-to-end character animation or skinned vertex generation.
- No evaluated pose matrices, quaternion/Euler channel composition, IK or constraints.
- No NLA, Action layer influence/mix, strip time mapping, or drivers; `frame` means stored *Action-local* frame.
- No sampling historical pre-2.5 IPO data with `--sample` (IPO raw export remains `--ipo`).
- No Gzip/Zstandard in-process decompression.
- No real Blender-generated `.blend` corpus confirmed for Phase 05. Synthetic tests alone do not establish compatibility with all Blender releases.

## Safety / portability

Source compiles with `-std=c89 -pedantic -Wall -Wextra -Werror` using 32-bit `unsigned int`. It declares no `float`, `double`, or `long long`; it never directly calls `malloc`, `calloc`, `realloc`, `free`. stdio may allocate internally, so this is a no-explicit-heap C implementation, **not** a claim that all runtime library internals are allocation-free.

`--sample` always removes an output file if parsing/evaluation fails. Binary data is bounded via existing BC_File block/schema limits and input validation. This is not a proof of memory safety.

## Source additions / modifications

- `src/bc_anim_eval.c` — safe Q16 math, decimal frame parser, Bézier inversion and sample exporter.
- `src/bc_anim_fcurve.c` — switches between key extraction and evaluation.
- `src/bc_anim_export.c` — unified `--sample` / `--keys` legacy and modern traversal.
- `src/main.c`, `include/bc.h`, `Makefile`, `build_win32_mingw32.bat` — CLI/build API integration.
- `tests/test_phase05_eval.py` — 17 synthetic cases including 32/64-bit pointers, big-endian data, 17-byte headers, step boundary, extrapolation and unsupported modes.
- `tests/test_phase05_mutations.py` — 80 deterministic mutated synthetic files, no process termination by signal observed.

## Primary implementation references

- Blender `DNA_anim_types.h` and `DNA_curve_types.h` (FCurve and BezTriple layouts).
- Blender `DNA_curve_enums.h` (BEZT_IPO_CONST/BEZT_IPO_LIN/BEZT_IPO_BEZ).
- Blender `DNA_anim_enums.h` (FCurve extrapolation).
- Blender `fcurve.cc` and related animation source for interpolation semantics.

Blender source informed this clean-room code; Blender's GPL implementation was not copied into this CC0 project.
