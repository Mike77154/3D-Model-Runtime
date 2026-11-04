# Source facial motion precision - hl2modelib 1.0.8

## Root cause in v1.0.7

The MDL bone parser converts binary32 `rotscale` to Q16.16 ahead of RLE
sampling. T-103 face values near 0.000012 rad/sample round to ~1 Q16 tick
or zero. In `a89_sample`, multiplying an RLE sample by an already-rounded scale
silenced many native face channels. This is independent from IK or PHY.

## SDK basis

`source-sdk-2013-master/src/public/bone_setup.cpp`:
`CalcBoneQuaternion` uses `ExtractAnimValue` with native `baseRotScale`, adds
reference Euler rotation for non-DELTA channels, and applies AngleQuaternion.
`CalcBonePosition` similarly multiplies each sample by the original
`baseBoneScale` before adding reference position. For RAWROT2 it reads
`pQuat64()` rather than using RLE rotation scales.

## Implementation

Parser in `vendors/source/formats/mdl_codec89.inc` stores both converted
metadata and raw bits `rotscale_ieee32[3]` / `posscale_ieee32[3]`.
The native animator in `vendors/source/animator/hl2m_native_anim89.c` uses
`hl2m_native_scale_rle89` for nonzero raw scales, computing signed RLE sample
multiplied by IEEE binary32 scale, then rounded once to Q16.16. Magnitude is
represented as two unsigned 32-bit words; there are no 64-bit types, floating
point operations, allocations or dependencies. Scale-zero cases retain a
fallback for manually constructed MDL structures in existing tests.

The RAWROT2 path stays quaternion-based in `hl2m_native_sample_anim_matrices89`.
`hl2m_native_sample_anim_frame89` remains Euler-only and may return
`HL2M_ERR_UNSUPPORTED` for raw-quaternion motions. Use the matrix API for them.

## Verification

Run:

```sh
make CFLAGS='-std=c89 -Wall -Wextra -Werror -pedantic -Iinclude' check
make CFLAGS='-std=c89 -Wall -Wextra -Werror -pedantic -Iinclude' hl2m_facial_probe108
./hl2m_facial_probe108 /path/to/tyrantphase1.mdl
```

Real corpus tested: T-103 phase 1 `Idle`, `Rage`, `Attack_Stand_LH`,
`Attack_Stand_RH`, `Attack_Stand_Back`, `Attack_Stand_2H`, `Attack_Run`.
Prior version returned an identical local jaw matrix to reference for 6 of
these 7 sample frames; v1.0.8 returns nonzero jaw matrix deltas in all 7.
`Attack_Stand_2H` already exercised RAWROT2; it remains supported.

Image comparison renders the `tyrant_head` triangles extracted via the
versioned OBJ exporter, with the same camera, sampled frames, and material UVs.
No changes to original T-103 assets were made. Rasterization used an external
Python visualization script, not a dependency of the C89 library.

## Remaining scope

Does NOT implement Source sequence 9-way blending, time interpolation, complete
IK runtime, GPU shader equivalence or morph/flex animation for MDLs with flexes.
All users embedding `hl2m_mdl` or `hl2m_mdl_bone` must recompile because of
struct layout changes. The old source-level functions remain available.
