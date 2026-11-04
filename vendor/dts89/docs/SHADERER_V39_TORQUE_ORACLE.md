# DTS89 v3.9: Shaderer material state + fixed-point texture pipeline

## Scope

This is a from-scratch, C89 renderer-neutral model of the material-loading path
in the user-supplied Torque3D source. It does not embed Torque3D implementation.
It preserves v3.8 DTS/DSQ, animation, skinning, UV and runtime modules.
The library never opens a file and never owns a heap allocation; the command-line
tools use host-owned static input, scratch and output buffers.

### Source oracle locations (Torque3D-development.zip)

- `Engine/source/ts/tsMaterialList.h` (S_Wrap, T_Wrap, Translucent,
  Additive, Subtractive, SelfIlluminating, NeverEnvMap, NoMipMap,
  MipMap_ZeroBorder)
- `Engine/source/ts/tsMaterialList.cpp` (material flags and index maps,
  legacy detail/reflection values and version-dependent fields)
- `Engine/source/materials/materialDefinition.h` (Material::MAX_STAGES=4,
  diffuse/normal/specular/detail map properties, BlendOp)
- `Engine/source/materials/materialDefinition.cpp` (initial render-state defaults,
  translucentBlendOp default PreMul, alphaRef default 1, detail scale 2)
- Original `art/shapes/actors/GG_Zombie/materials.cs` from user-uploaded
  GarageGames ZombiePack T3D1.1 (diffuse/normal/specular maps and mapTo)

## File layout

```
include/dts89_shaderer.h    Public material layout, UV addressing, pixel sampler,
                           RGBA8 blend and normal unpacker
src/dts89_shaderer.c       State builder and TorqueScript mapping
src/dts89_texturer_opener.c  Existing probe plus BC1, BC3, BMP24/32 decoders
include/dts89_modules.h   Texture codec API additions
/tools/dts89_shaderer_audit.c  CLI to inspect DTS + materials.cs per model
/tools/dts89_shaderer_selftest.c  Wrap, clamp, blend and mapTo tests
/tools/dts89_texture39_selftest.c BC1, BC3 and BMP semantic tests
/tools/dts89_texture_decode.c DDS/BMP -> raw RGBA8 output
/tools/dts89_normal_decode.c  DDS normal -> reconstructed RGB normal vectors
```

## Material rules

The DTS material table supplies base flags and legacy links. A sidecar
TorqueScript file provides named `Material` definitions. Binding first uses the
exact `mapTo` name (ASCII case-insensitive), then the material object name.
`Shaderer` exposes an explicit `script_matched` flag; no asset name is invented.
String slices point into caller-owned decoded shape/script data.

Per DTS flag, wrap/clamp works independently for S and T. UVs are preserved
as read; shaderer_address_uv() applies modulo 1 for negative/extended UVs
and clamps when wrapping is disabled. Sampling assumes top-left-origin RGBA8
pixels and top-left-origin native Torque UVs. OBJ conversion is a separate,
explicit step in `UV Texturer`; double-flipping V is incorrect.

Script stages [0..3] carry diffuse, normal, specular, detail, detail-normal,
overlay, glow, AO, roughness, metalness and ORM map references. Shaderer records
blend mode, alpha-test state, reference, double-sided and self-illumination.
`unsupported_features` reports effects whose actual lighting/runtime logic is
not yet implemented in this module: tangents/normal lighting, specular BRDF,
detail compositing, cubic environment mapping, UV animation, colorspace,
parallax, multistage composition, etc.

Translucent blend operations understood from the Torque enum are None, Mul,
PreMul, Add, AddAlpha, Sub, LerpAlpha and ToneMap. A limited reference RGBA8
compositor supports the first seven; ToneMap correctly reports unsupported.
The reference compositor is not a substitute for GPU blend/depth state.
When a modern TorqueScript material is translucent but does not specify a
blend mode, the default is PreMul (matching Material.cpp).

## Texture decoding

Implemented in independent read-only routines:

- DDS DXT1/BC1 -> RGBA8, with correct 3-color alpha variant.
- DDS DXT5/BC3 -> RGBA8, including 8-level interpolated alpha.
- BMP BI_RGB 24/32 -> RGBA8, top-down and bottom-up rows.
- DDS mip count and first-level byte size inspected. **Level zero only** decoded.
- PNG header probe preserved, **PNG pixel decompression not implemented**.
- Unhandled DDS FourCC/DX10, BC5, uncompressed DDS, additional mipmaps,
  sRGB transfer, anisotropic filtering, bilinear/trilinear/AF remain to do.
- All output buffers and sizes are passed by caller; no intermediate allocation.

### DXT5nm / AG normal packing

The 16 `*_n.dds` images of ZombiePack 1.1 have R=255, B=0 and nonconstant
alpha and green. The normal's X is stored in A, Y in G, and Z reconstructed
with bounded integer square root. Shaderer contains both mode detection and
an explicit unpack routine. It does not silently rewrite the raw DDS bits.
For arbitrary normal maps, callers should inspect/override the mode rather than
assume all DXT5 images use AG packing. The integer reconstruction is an
approximation, not the original engine's floating-point shader implementation.

## Audits on the provided ZombiePack 1.1 corpus

- 6 DTS files decoded, the bundled `materials.cs` parsed (17 script Material objects).
- 47 DTS material table entries, **41 bound by `mapTo`**; 6 unbound
  `ShapeBounds` entries are nontextured shape-bounds material names.
- All 65 DDS images decoded, 49 BC1 and 16 BC3.
- 1,885 sampled channel comparisons against Pillow's DDS reader, none with
  any channel difference above 3 (maximum difference 2; interpolation rounding).
- All 16 `*_n.dds` normal maps identified as AG-packed DXT5nm.
- C89 strict compile + selftests + sanitized reference selftest.

## Example build and usage

```
make clean
make CFLAGS='-std=c89 -Wall -Wextra -Werror -pedantic-errors -Iinclude'
./tools/dts89_shaderer_selftest
./tools/dts89_texture39_selftest
./tools/dts89_shaderer_audit Zombie.dts materials.cs
./tools/dts89_texture_decode zombie_tshirt_torn_d.dds diffuse.rgba
./tools/dts89_normal_decode zombie_tshirt_torn_n.dds normal_rgb.rgba
```

## Remaining for Torque 1:1

Full per-material custom shader selection/instancing, tangent generation,
normal/specular PBR lighting, gloss map decode semantics, multiple texture
stages, cubemaps, detail blending, GPU sampler/sRGB handling, explicit
backface/depth/alpha test in renderer and animation of UV coordinates.
No such operations should be marked 'rendered' merely because their inputs
are recognized. The v3.9 milestone is metadata + state planning + robust
image decoding + reference color mixing; the complete lighting pipeline is
still future work.
