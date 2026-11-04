# studiohalflife extensions

Extension pack layered on top of `hlfp_goldsrc_steroids`.

Goal: keep the original GoldSrc MDL v10 opener C89/static-buffer friendly while adding practical support for Xash/extended StudioMDL-style features.

## New modules

- `hlfp_studiohalflife.h/.c`
  - `studiohdr2` scanner through the GoldSrc/Xash-compatible header slot at offset 220.
  - Pose parameter table scanner.
  - Extended boneinfo scanner (`STUDIO_HAS_BONEINFO`): `poseToBone`, alignment quat, procedural type/index, stored quat.
  - Extended boneweight scanner (`STUDIO_HAS_BONEWEIGHTS`): up to 4 influences per vertex/normal using the Xash-style `mstudioboneweight_t` layout.
  - Weighted fixed-point skinning path.
  - Autolayer table parser from extended sequence fields.
  - IK chain / IK link / IK lock parser.
  - Manual IK rule record decoder and lightweight IK target application.
  - Procedural-bone-lite runtime: pose-to-bone concatenation, billboard/screen-align bones, simple jiggle offset.
  - Better `.dol`/companion classifier and header reader.

- `hlfp_softmdl.h/.c`
  - Tiny software renderer for baked MDL meshes.
  - RGBA framebuffer output, optional zbuffer, fixed-point orthographic camera.
  - Filled triangle or wireframe mode.
  - PPM writer.
  - Render path can use classic GoldSrc single-bone skinning or the new weighted-bone path.

- `test_studiohalflife.c`
  - Smoke test for extension scanning, `.dol` header classification, autolayer weights, IK target application, procedural bones, weighted fallback bake, and the software renderer.

- `demo_render_studiohalflife.c`
  - Generates `studiohalflife_preview.ppm` using the C software renderer.

## Build

```sh
make clean all test_output.txt studiohalflife_preview.ppm
```

Tested with:

```sh
gcc -std=c89 -Wall -Wextra -pedantic -O2
```

## Important honesty notes

This is an opener/runtime support layer, not a complete replacement for StudioMDL or HLMV yet.

Implemented strongly:

- extension table discovery
- fixed-buffer parse helpers
- boneweight decoding and weighted bake path
- sequence autolayer ramp math
- IK chain/lock scan and target application
- procedural bone helpers
- `.dol` companion classification/header validation
- software render preview path

Still partial / deliberately conservative:

- complex procedural rules such as full axis-interp/quat-interp/aim-at are parsed as metadata but not fully evaluated.
- full StudioMDL IK solving is not fully reproduced; this pack adds IK metadata parsing plus a lightweight target mover.
- `.dol` is handled as a Dreamcast-style companion/header/container variant, not as a full platform-specific renderer or decompressor.

## No heap policy

Core library additions avoid:

- `malloc`
- `calloc`
- `realloc`
- `free`
- runtime `float`
- runtime `double`

All scratch memory is caller-owned or fixed-size in structs.
