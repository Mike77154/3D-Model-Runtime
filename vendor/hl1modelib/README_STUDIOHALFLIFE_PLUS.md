# StudioHalfLife Plus

Extra GoldSrc/Xash helpers layered over `studiohalflife_extensions`.

This package keeps the same project policy:

- C89 build target.
- No `malloc`, `calloc`, `realloc`, or `free` calls.
- Runtime math remains fixed-point.
- All working memory is caller-owned, static, stack, or explicit scratch buffers.

## New modules

### `hlfp_studiohalflife_plus.h/.c`

Adds decompiler/runtime helpers that sit above the previous `hlfp_studiohalflife` layer:

- Xash-style model extra analyzer:
  - treats `mstudiomodel_t` legacy fields as `blendvertinfoindex` / `blendnorminfoindex` when `STUDIO_HAS_BONEWEIGHTS` is set.
  - validates raw weight table ranges against the MDL main buffer.
- Raw Xash boneweight table reader:
  - reads `uint8 weight[4] + int8 bone[4]` records.
  - converts to 16.16 weights.
  - compensates rounding by normalizing to one.
- Autolayer weight batch evaluator.
- Root-motion extraction from sequence `linearmovement`.
- World-space hitbox AABB builder.
- Two-bone IK lite target mover.
- Simple LOD selector for external runtime policy.
- Model audit report.
- Richer QC scaffold writer:
  - `$boneweights`
  - `$externaltextures`
  - `$bodygroup`
  - `$texturegroup`
  - `$hbox`
  - `$attachment`
  - `$sequence`
  - poseparam/autolayer comments when detected.

### `hlfp_softmdl_plus.h/.c`

Adds a second tiny software render path:

- Affine textured triangle rasterizer.
- Palette sampling from GoldSrc indexed textures.
- Masked texture alpha support.
- Optional flat normal shade.
- Wire overlay.
- Skeleton overlay.
- 32-bit uncompressed TGA writer.

## New test/demo files

- `test_studiohalflife_plus.c`
- `demo_render_studiohalflife_plus.c`

Build:

```sh
make clean all test_output.txt studiohalflife_plus_preview.tga
```

Expected final lines include:

```txt
studiohalflife_plus_ok=1
studiohalflife_plus_checks=xash_model_offsets normalized_weights root_motion hitbox_world_aabb two_bone_ik_lite lod_select qc_decompile_plus audit_report affine_textured_tga
```

## Honest limits

This improves runtime/decompiler coverage, but is still not a bit-perfect replacement for all StudioMDL/HLMV/Xash behavior. The two-bone IK is intentionally a fixed-point lite target mover, not a full trigonometric limb solver. The renderer is affine and intentionally small; it is meant for previews, debug, and tests.
