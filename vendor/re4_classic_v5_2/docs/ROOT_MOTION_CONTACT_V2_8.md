# RE4 Classic Codec v2.8 — FCV root-position telemetry and gated experimental IK

## What changed

- New `animation/re4_root_motion.c` + `include/re4c3d/re4_root_motion.h`:
  - Detect unique primary FCV `bone_type == 1`, `bone_no == 0` root-position tracks.
  - Hermite sample any frame with the existing C89 fixed-point FCV reader.
  - Export signed Q16.16 displacement relative to a chosen frame without heap allocation.
  - Optional translation of *every world-space bone matrix*, checked before mutation for 32-bit overflow.
  - Applying this on the master before `re4_character_retarget()` propagates movement to the hair, hands and head.
- `re4_character_clip` now always writes root-position observations to its `poses_index.csv`.
- New opt-in `--root-apply`: translates the character by `root(frame)-root(first_exported_frame)`.
  - This is an **experimental interpretation**, not a proven clone of native RE4 locomotion/space conversion. Default OFF.
- New opt-in `--ik-window=START:END`: locks candidate effectors only within the inclusive range. The contact target is sampled at `START` and held in world space for that window.
  - Fixes the prior diagnostic behavior where `--ik-lock` anchored feet/hands across an entire animation.
  - `--ik-lock` is retained for backward compatibility; both IK variants remain optional and NON-NATIVE.
  - Window boundaries are evaluated independently from the export stride: `--ik-window=3:4` can start at 3 even if exported frames are 0,2,4,6.
- New `re4_root_catalog` CLI: scans original nested DAT without writing individual OBJ and creates root telemetry for all clips (samples start, midpoint and end). This is a coarse motion probe, not a full kinematic analysis.
- New `tests/re4_root_motion_test.c` checks actual FCV interpolation/endpoints, negative offsets, missing/duplicate tracks, atomic out-of-range behavior and world-space translation.

## Example commands

```sh
make -j4 all
make check
./re4_root_catalog pl00.dat leon_root.csv
./re4_root_catalog em10.dat ganado_root.csv
./re4_character_clip pl0000.pmd clip_0037.fcv 0 40 2 out_leon pl0001.pmd lh00.pmd rh00.pmd --root-apply --ik-window=10:22
./re4_character_clip em1000.pmd clip_0176.fcv 0 86 2 out_ganado em1001.pmd em10lh00.pmd em10rh00.pmd --root-apply --ik-window=20:38
```

The CSV `poses_index.csv` includes `root_raw_*_q16`, `root_delta_*_q16`, `root_applied` and `ik_contact_active`; the values are signed integers (divide by 65,536 to express original model coordinates).

## Real-data measurements

Source: user-supplied original RE4 PC 2007 `em.dat`, nested `em\\pl00.dat` and `em\\em10.dat` (not shipped with library).

| Archive | Structurally valid FCV | Unique primary root track | Nonzero at midpoint or end | Failed |
| :-- | --: | --: | --: | --: |
| Leon `pl00` | 65 | 58 | 51 | 0 |
| Ganado `em10` | 277 | 255 | 245 | 0 |

There is also one empty FCV marker in `pl00`. `nonzero` only considers 2 sample points; a clip might move in between and return to zero.

**Multipart smoke test:**

- Leon `pl0000` plus hair and two hands: `clip_0037.fcv`, 4 frames 0/2/4/6, 14,751 vertices per frame; optional contact active 2 and 4 only.
- Ganado `em1000` plus head and two hands: `clip_0176.fcv`, same four frames, 3,729 vertices per frame; contact active 2 and 4 only.
- Ganado root-enabled vs disabled: frame 0 has identical mesh; at frame 6 the user-source FCV root delta is approximately `(9.0137, 0, 157.2734)` model units. All 3,729 vertex positions change by this delta with maximum pointwise deviation under 0.0025 model units, allowing for fixed-point and retargeting rounding.
- Linux GCC `-std=c89 -Wall -Wextra -pedantic` full build passes. All 10 test binaries pass. ASan/UBSan pass on the 10 suites plus a real Ganado 3-frame root+contact export.

## Explicit limitations

1. Native root-motion coordinate space, orientation and units are **not yet established**, including interaction with `bone_type == 64` root rotation or gameplay camera facing.
2. CCD IK remains an experimental geometry solver, not native IK e.g. ankle/toe, bend restrictions, IK controller targets, transition blending.
3. Collision-aware foot planting, individual contact auto-detection and IK events still require research; `--ik-window` is a **manual diagnostic override**.
4. The code does not add morph targets or facial animation and is not yet linked/tested in the user's particular Win32 Model Runtime.
5. Original game assets and exported models are intentionally not bundled.
