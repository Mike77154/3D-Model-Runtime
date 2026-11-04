# DTS89 Phase 4.0 — Fixed-point lighting bridge / software render

This package extends v3.9. No existing DTS/DSQ reader, exporter, UV or animation
module was removed. Its key addition is a standalone, renderer-neutral lighting
API and a C89 orthographic software reference rasterizer.

## Inputs and ownership

* `dts89_pose37_evaluate` (Animer / Posefeamw) evaluates bone globals in Q16.16.
* `dts89_lit40_pose_mesh` (Mesher bridge) produces posed vertices, skinned
  normals and raw native Torque UVs, all in caller arrays. Its only temporary
  palette is supplied by the caller.
* `dts89_lit40_triangle_basis` reconstructs a tangent frame from current
  triangle positions and native UVs; this is deliberately done AFTER skinning.
* `dts89_lit40_shade` samples caller-owned RGBA images through Shaderer,
  decodes normal packing RGB or BC3/DXT5nm AG, and adds ambient + Lambert
  directional light + bounded fixed-point Blinn-Phong specular.
* `dts89_lit40_address_raw_uv` handles repeat/clamp for IEEE32 UV values
  outside Q16.16 using bitfield calculations only; the file bits remain intact.
* `dts89_lit40_render` is an optional C89 CPU demonstration, producing PPM
  through file output. It decodes DDS BC1/BC3 through Texturer Opener using a
  fixed external scratch buffer and downsamples textures to <=512x512 *only*
  for the preview. PNG output preview images may be made by any image tool.

## Supported in phase 4.0

- Diffuse texture stage 0 with RGBA8 texel sampling and color multiplier.
- Wrap/clamp modes in U and V and alpha-test cutoff.
- Per-fragment interpolated posed normals and tangent-space map.
- DXT5nm AG unpacking, normal reconstruction and tangent frame.
- Stage-zero specular RGB map and approximate gloss from alpha; integer power.
- Depth-buffered opaque orthographic render and four animation phases.
- C89 type declarations, no heap API calls, no floating-point types,
  no 64-bit integers in the shipped source modules/tools.

## Known limits, intentional and important

- This is a *reference lighting model*, not the identical Torque GLSL/HLSL
  material shader. It uses a single directional light and approximation to
  Blinn-Phong with integer exponent. RGB math is not sRGB-aware or PBR.
- Stage 1-3 composites, overlays, detail/light/cube maps, glow, parallax,
  multipass transparent blending, environment and shadowing are not rendered.
  Earlier Material Binder continues to *recognize* them where applicable.
- Opaque raster path does not correctly sort or render translucent surfaces.
- Affine nonuniform scale requires normal inverse-transpose handling; the old
  pose37 evaluator does not implement arbitrary scale tracks.
- The CPU preview uses nearest texture filtering, downsamples to 512 px and
  orthographic projection. For the runtime, the decoded original DDS and
  caller-supplied views may be full resolution.
- The preview camera is fixed and the view vector is a fixed approximation.
- Input data is source-compatible with Torque DTS v26 corpus, but rendering
  equivalence has not been pixel-verified against the original Torque GFX.

## Build

    make clean
    make CFLAGS='-std=c89 -Wall -Wextra -Werror -pedantic-errors -Iinclude'
    ./tools/dts89_lighting40_selftest
    ./tools/dts89_pose37_selftest
    ./tools/dts89_shaderer_selftest

## Demo, run from dts89 directory

Extract the `ZombiePack_T3D1.1_GameAssets.zip` source files to a separate data
folder. Give the utility the DTS model path, `materials.cs`, and its `Textures`
folder. Use Q16 sequence position in the range 0..65536.

    ./tools/dts89_lit40_render Zombie.dts materials.cs Textures bind.ppm -1 0
    ./tools/dts89_lit40_render Zombie.dts materials.cs Textures attack.ppm 0 32768
    ./tools/dts89_lit40_render Zombie.dts materials.cs Textures flat.ppm -1 0 flat

The optional `flat` flag keeps the diffuse map but disables normal and
specular map sampling for an A/B diagnostic.

## Architecture

    DTS/DSQ  ->  Animer / Posefeamw  ->  global bone matrices
                                           |
                           Mesher  ->  skinned vertex/normal/UV
                                           |
    material.cs -> Shaderer ------> UV sampler + tangent basis
              DDS -> Texturer Opener      |
                                           v
                             Shaderer Lighting40 (RGBA8)
                                           |
                             host renderer / PPM test tool

## White-room source references

Torque3D's material mapping, source archive provided by the user, and the
published Material script reference were consulted for the *behavior* of
`mapTo`, normal/specular texture inputs and specular alpha/gloss. The
implementation in `dts89_lighting40.c` is independently written for fixed
point and is not Torque engine code.

