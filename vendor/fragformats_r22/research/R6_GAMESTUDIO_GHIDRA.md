# FragFormats R6 — GameStudio investigation

## Primary evidence from user's supplied artifacts

Source RAR: `fragMOTION 1.3.2.rar`. Files inspected, but **not redistributed**:

- `Plugins/FileTypeMDL_GS.dll`: PE32 x86, 245760 bytes; binary embedded
  importer/exporter messages for GameStudio MDL3/4/5/7. It labels MDL7 as
  `GameStudio A8 MDL7 Files` in some embedded UI data.
- `SDK/Samples/FileTypeMDL7/MDL7.H`: packed definitions for MDL7
  headers/groups/bones/skins/skinpoints/triangles/main vertices/frame
  vertices/transforms, plus legacy MDL3/MDL6 headers and MDL3/4 frames.
- `SDK/Samples/FileTypeMDL7/FileTypeMDL7.cpp`: reference calling sequence:
  Head -> Bone -> Group -> Skin -> SkinPoint -> Triangle -> Vertex -> Frame.

## Ghidra 12.0.4 results

The attached Ghidra distribution imported `FileTypeMDL_GS.dll` with image base
`0x10000000`. `ScanGSLight.java` was executed as a headless post-script without
full autoanalysis. The mapped `.data` range was `0x10032000..0x100384cb`.
Selected ASCII hits (these are addresses, **not decoded function entries**):

- `MDL3` 0x10033259, 0x10033280, 0x10033288
- `MDL4` 0x10033205, 0x1003322c, 0x10033234
- `MDL5` 0x100331b1, 0x100331d8, 0x100331e0
- `MDL7` 0x100333c9, 0x100333ed, 0x100334b6
- `GameStudio A8` 0x100334a8

Ghidra full autoanalysis was attempted with a time budget but timed out; the
file had only partial analysis. **No claim is made that decompiled importer
functions or their control-flow paths were recovered in this phase.** The
portable C89 parsing work is based on file-format structure documentation and
independent tests, rather than decompiler-equivalent reconstruction.

## Structural checks on authentic test data from GitHub Assimp

Examined through GitHub's file connector using a separate in-memory
base64-decoding and format-layout checker. These tests did not directly run
against the built C89 binary; original test assets are not redistributed.

- `test/models/MDL/MDL5 (3DGS A5)/PhosphoricAcid_MDl5.mdl`
  SHA `fde56fd779f5c424dd7e40e9401f1af60773f82e`;
  15612 bytes, 496 vertices, 960 triangles, 1 frame, 1 UV vertex,
  zero skins, 16-bit packed frame type 2; all triangle vertex indices in range.
- `test/models/MDL/MDL7 (3DGS A7)/Sphere_DiffPinkBlueSpec_Alpha90.mdl`
  SHA `51ffc6749edcc6b63a138899ef876547257ad4fc`;
  3720 bytes, 1 group, 62 vertices, 120 triangles, zero UVs,
  a material-only skin type 16, and MDL7 vertex stride 26/triangle stride 16.
  Group byte count and face indices checked.
- `test/models/MDL/MDL7 (3DGS A7)/PhosphoricAcid_MDl7.mdl`
  SHA `0027b3554504788c5ef806d9b4ee1ec456db7b96`;
  28944 bytes, 8 groups, 496 vertices and 960 triangles total, 0 UVs,
  3 material-only skins in the first group; group sizes and indices checked.

## Format mapping / caveats

A3/older uses `MDL2` in some references, not shown as an import option in the
supplied fragMOTION DLL. A4 uses MDL3, A5 uses MDL4/MDL5, A6/A7/A8 may use
MDL7. A `MDL6_HEADER` struct appears in the SDK, but that **does not prove**
any importer for MDL6 is exported by this version of fragMOTION.

Reference sources:
- https://www.fragmosoft.com/fragMOTION/index.php
- https://help.nchsoftware.com/3dconverter/es/kb/1917.html
- https://www.cnblogs.com/billwillman/articles/1037380.html
- https://github.com/assimp/assimp/tree/master/test/models/MDL
