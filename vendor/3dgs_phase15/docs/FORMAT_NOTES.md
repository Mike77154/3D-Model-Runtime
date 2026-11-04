# Formats and current scope

| On-disk signature | Reader | Known historical association | v04 status |
| --- | --- | --- | --- |
| MDL3 | `legacy/*` | A4 | vertices, UV, skin and simple frame |
| MDL4 | `legacy/*` | A5+ | vertices, UV, skin and simple frames |
| MDL5 | `legacy/*` | A5+ | MDL5 skin sizing, simple frames |
| MDL7 | `mdl7/*` | A7/A8 | groups, dual UV, materials IDs, bone hierarchy, frame overrides, raw animation keys and experimental rigid and weighted bone poses |

A4-A8 are **engine generations**, not 1:1 unique binary signatures.
Unsupported formats include MDL2 (older 3DGS), fully validated commercial skeletal weighted
deformers, custom material shaders, implicit external DDS decoding, terrain HMP
and level WMP. General FBX/3DS/X/OBJ are unrelated native import formats
and are not implemented by this codec.

`GS3D_BoneKey` preserves 16 elements in on-disk order, no floating-point
math; the math model and weighted deformer mapping require real model tests.

## Safety notes

The source reader checks file boundaries, count/capacity limits and
triangle/UV indices, and rejects parent bone cycles. It avoids a dynamic
heap and stores numeric vertex values as signed Q16.16. The MDL7 security
issues in the Assimp tracker motivated extra bounds validation. This is
**not** a claim of a security audit or a fuzz-hardened parser.

## External research (studied, not copied)

- https://github.com/assimp/assimp/blob/master/code/AssetLib/MDL/MDLFileData.h
- https://github.com/assimp/assimp/blob/master/code/AssetLib/MDL/MDLLoader.cpp
- https://github.com/assimp/assimp/blob/master/code/AssetLib/MDL/MDLMaterialLoader.cpp
- https://help.nchsoftware.com/3dconverter/es/kb/1917.html
- https://github.com/assimp/assimp/issues/6632
- https://github.com/assimp/assimp/issues/6630
- https://github.com/assimp/assimp/issues/6633
