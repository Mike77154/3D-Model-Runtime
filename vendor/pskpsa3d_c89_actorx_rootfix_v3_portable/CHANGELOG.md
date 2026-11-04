# Changelog

## ActorX root-index regression fix (2026-10-08)

- Correct PSK/PSA quaternion import: child orientations are conjugated, but bone index 0 is preserved, even if `Bip01.parent_index == 0` (self-parent root).
- The previous patch incorrectly used `parent_index >= 0` to classify children. This conjugated the root and corrupted all descendants during LBS skinning.
- Read bone index explicitly for both PSK REFSKELT and PSA BONENAMES; correct per-key treatment for PSA ANIMKEYS by `key_index % bone_count`.
- Add a synthetic regression test with non-identity root and child quaternions in PSK and PSA, plus a bind-pose skinning check.
- Add `pskpsa_skin_dump` diagnostic tool, which outputs Q16 skinned points for external renderers.
- Validated on FemaleHobo.psk and MalePunk.psk with ZombieStandard.psa WalkF sequence (48 frames, 18 fps) using C89 skinning and VTK display (external, not part of codec).

# Changelog

## v6 - PSA blend stack + final triangle stream

- Added `psk3d_anim_stack` for static animation layering.
- Added override and additive blend modes.
- Added `psk3d_pose_blend()` with alias-safe output.
- Added `psk3d_pose_apply_additive()` for weighted PSA deltas against a base pose.
- Added public `psk3d_quat_conjugate()`.
- Added `psk3d_triangle_stream` / `psk3d_final_vertex` / `psk3d_build_triangle_stream()`.
- Extended tests for PSA stack sampling, pose blending, and final triangle stream output.

## v5 - PSKX shape keys + PSA scale arrays + blend modules

- Added PSKX morph parsing:
  - `MRPHINFO` target metadata.
  - `MRPHDATA` point deltas.
  - Prefix fallback for `MORPHINF*` / `MORPHDAT*`.
- Added static morph arrays in `psk3d_model`:
  - `morph_targets[]`.
  - `morph_deltas[]`.
- Added separate blend modules:
  - `psk3d_morph_weights`.
  - `psk3d_morph_stack`.
  - `psk3d_apply_shape_keys_points()`.
  - `psk3d_apply_shape_key_stack_points()`.
- Added morph-to-skinning bridge:
  - `psk3d_skin_points_lbs_cached_from_points()`.
  - `psk3d_skin_points_lbs_cached_morphed()`.
- Added optional PSA scale key support from `SCALE*` chunks.
- Added `psk3d_pose.scales[]`, identity scale defaults, scale-aware global pose propagation, and scale-aware point skinning.
- Added `psk3d_extract_sequence_track_range()`.
- Extended `pskpsa_info` output for morph and scale counts.
- Extended tests for shape keys, blend stack, scale keys, mapped sample, scale-aware LBS, and morphed cached LBS.

## v4 - PSKX extras + bone map + playback cursor

- Added PSKX/extended chunk parsing:
  - `EXTRAUVS0`, `EXTRAUVS1`, ... stored as Q16.16 UVs.
  - `VERTEXCOLOR` stored as wedge RGBA bytes.
  - `VTXNORMS` stored as normalized Q16.16 normals.
- Added alternate chunk ID acceptance for compatibility with older scripts/tools:
  - `VTXW3200`, `FACE3200`, `REFSKEL0`, `RAWW0000`, and `RAWWEIGH` prefix.
- Added `psk3d_bone_map` and `psk3d_build_bone_map()`.
- Added mapped PSA sampling into PSK/model bone order:
  - `psk3d_sample_sequence_mapped_ex()`.
- Added forward fixed-point animation cursor:
  - `psk3d_cursor_reset()`.
  - `psk3d_cursor_advance()`.
- Added cached normal skinning:
  - `psk3d_skin_normals_lbs_cached()`.
- Added `psk3d_vec3_normalize()`.
- Updated `pskpsa_info` to print extension counts.
- Extended test coverage for UV extras, vertex colors, normals, bone maps, playback cursor, point skinning, and normal skinning.

## v3

- Added optional fixed-point SLERP.
- Added cached point skinning.

## v2

- Added fixed-point quaternion operations.
- Added LBS point skinning.

## v1

- Initial heapless PSK/PSA parser.

## v3 portable ABI hardening (2026-10-08)

- `include/pskpsa3d.h`: C89 `<limits.h>` compile-time checks now demand exact 8-bit bytes, 16-bit shorts and a 32-bit signed/unsigned integer type. Preserve Win32 ABI (`long`/`unsigned long`) where those types are exactly 32-bit; otherwise select 32-bit `int`/`unsigned int` (LP64). Unsupported targets fail at compile time instead of silently parsing data with a wrong width.
- `src/pskpsa3d.c`: runtime type checks require exact lengths. The core's integer literals no longer force unintended LP64 `long` arithmetic. Fixed the upper-midpoint arithmetic in fixed-point square root at the Q16.16 signed maximum to prevent signed overflow on 32-bit types.
- `Makefile`: include both regression suites, include the skin dump utility, support `EXEEXT` for MSYS2/MinGW and remove `.exe` and extensionless binaries on clean.
- `tests/test_minimal.c`: added public ABI/float32-bits conversion sanity check alongside the ActorX root-vs-child regression.
- `tests/test_numeric_limits.c`: added saturation, fixed-point sqrt, and extreme IEEE-754 bit-pattern regression tests.
- `build_msys2_mingw32.sh`: build and run the numerical regression program too.

All V2 ActorX quaternion conventions and parsing/animation APIs are retained. This is **not** a claim of verified visual correctness on unprovided real-world PSK/PSA assets. No old Linux ELF binaries are included in the source archive.
- Sanitizer QA revealed and fixed a pre-existing test-fixture string padding overread in `tests/test_minimal.c` (`put_name` now stops dereferencing at the first NUL and pads the remaining bytes). ASan/UBSan tests pass after this correction.
