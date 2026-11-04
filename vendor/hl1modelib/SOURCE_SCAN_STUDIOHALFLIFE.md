# Source scan notes - studiohalflife extensions

Inputs inspected from the uploaded zips:

- `xash3d-fwgs-master`
  - `engine/studio.h`
  - `engine/common/mod_studio.c`
  - `ref/gl/gl_studio.c`
  - `ref/soft/r_studio.c`

- `reGS_hlsdk-master`
  - `engine/studio.h`
  - `utils/studiomdl/*`
  - `utils/mdlviewer/*`

- `HalfLifeModelViewer2-master`
  - Studio model renderer/shared model files.

- `HalfLifeAssetManager-master`
  - Studio model IO/renderer/editing helpers.

Important implementation clues used:

- Xash-style header uses the old GoldSrc header slot at offset 220 as `studiohdr2index`.
- `studiohdr2_t` carries pose parameters, IK autoplay locks, IK chains, keyvalues and hitbox set offsets.
- `STUDIO_HAS_BONEINFO` means an extended `mstudioboneinfo_t` table follows the normal bone table.
- `STUDIO_HAS_BONEWEIGHTS` means model structs can use the last two model offsets for vertex/normal boneweight tables.
- Boneweights use 4 byte weights + 4 signed bone indices.
- Extended sequence descriptions reuse old sequence fields for autolayer and IK lock offsets/counts.
- GoldSrc/Xash draw paths ultimately build bone/world matrices, skin vertices, then emit triangles. The new software renderer mirrors that flow in a tiny fixed-point way.
