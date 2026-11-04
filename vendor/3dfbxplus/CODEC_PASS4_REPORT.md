# 3dfbxplus Codec Pass 4 Report

## Scope

This pass was applied on top of `3dfbxplus_codec_pass3(1).zip`.

Reference material reviewed locally:

- `ufbx-main`: loader architecture, public test corpus layout, curve/shape/material/embedded-texture edge cases, animation curve metadata conventions, differential testing style.
- `FBXSDK-master`: sample-level behavior for importing scenes, cameras, lights, materials, textures, shapes, NURBS, animation layers and transforms.

No FBX SDK source was copied into the library. The SDK was used as behavioral/API reference only.

## Main changes

### Core importer/runtime

- Added `child_capacity` to `FbxNode` and changed node child array growth from one-by-one `fbx_resize()` to capacity doubling.
  - This greatly reduces arena pressure on files with thousands of child nodes.
  - Fixed large unicode/id-collision test cases that previously exhausted the fixed arena.
- Preserved original binary read errors instead of replacing them with generic child-node failures.
- Added tolerance for binary null-record padding before parent end offsets.
- Improved binary endian handling for big-endian FBX headers and scalar/array payloads.
- Kept the no-stdio/no-heap architecture: all growth still goes through arena-backed `fbx_alloc/fbx_resize`.

### ASCII/base64 and embedded textures

- Improved ASCII parsing for large `Content` strings such as embedded base64 texture payloads.
- Added base64 decode path for ASCII `Content` blobs.
- Added texture/video path alias recovery: if one `Video` node references an empty `Content` but shares the same path/relative path as another `Video`, the blob is reused.
- Extended texture slot metadata: relative path, display name, texture/video IDs, embedded flag, embedded data pointer/size.

Validated parity examples against ufbx:

| File | Our embedded textures / bytes | ufbx embedded textures / bytes |
|---|---:|---:|
| `marvelous_quad_7200_ascii.fbx` | 3 / 37729 | 3 / 37729 |
| `marvelous_quad_7700_ascii.fbx` | 3 / 37729 | 3 / 37729 |
| `marvelous_quad_7700_binary.fbx` | 3 / 37729 | 3 / 37729 |
| `blender_293_embedded_textures_7400_binary.fbx` | 5 / 9921 | 5 / 9921 |

### Legacy FBX 5/6 support

- Added legacy string-ID handling for `Geometry` nodes, not just `Model` nodes.
- Added legacy scalar-array decoding for NURBS/curve/surface payloads.
- Added minimal legacy `Takes` support so FBX 5/6 files expose animation clips/layers even when they do not use modern `AnimationStack/AnimationLayer` objects.
- Fixed legacy ASCII scalar array decoding for mesh data.

### Curves/NURBS/surfaces

- Added/expanded storage for:
  - Line geometry
  - NURBS curves
  - NURBS surfaces
  - Orders, dimensions, forms, points, point indices, knot vectors
- Legacy 6100 NURBS curve/surface fixtures now attach geometry payloads instead of just model type counts.

### Animation

- Added support for empty-but-valid modern `AnimationStack`/`AnimationLayer` clips.
- Added legacy `Takes` clip/layer extraction.
- Expanded animation curve parsing for compact `KeyAttrRefCount`-style metadata.
- Preserved and exposed weighted tangent metadata fields.
- Added layered-channel sampling hooks and morph-weight sampling API.

Current evaluator status: constant/linear/cubic Hermite plus layer blending approximation are implemented, but Autodesk/ufbx bit-exact weighted tangent evaluation is not finished.

### Morphs

- Morph targets can be baked to the final triangulated vertex buffer.
- Morph animation tracks for `DeformPercent` are parsed in modern BlendShapeChannel paths.

Current morph status: modern morph bake + sampling is present; legacy FBX 5/6/ZBrush/Maya shape variants still need more compatibility work.

### Semantic writer

Extended semantic FBX writer API to emit:

- Meshes
- Materials
- Textures
- Cameras
- Lights
- Bones
- Skin clusters
- Animation stacks/layers/curve nodes/curves

Validated by `semantic_extended_smoke`:

```txt
semantic extended OK: bytes=5420 cameras=1 lights=1 bones=2 anims=1
```

### Differential testing

Added/updated:

- `tests/ufbx_probe.c`
- `tests/build_ufbx_probe.sh`
- `tests/run_ufbx_data_matrix.py`
- `examples/fbx_probe.c`
- `tests/ufbx_matrix_valid/manifest.tsv`
- `tests/ufbx_matrix_all/manifest.tsv`

The matrix runner supports:

- `WORKERS=N` for parallel differential runs.
- `INCLUDE_EXPECTED_FAILS=1` to include ufbx `_fail_` test cases.
- `INCLUDE_FUZZ=1` to include fuzz folders.

## Validation run

Core:

```txt
miniz smoke OK: 127 bytes
anim_eval OK: mid=327680 pre=-655360 repeat=327680 mirror=327680
semantic OK: ascii_bytes=2112
semantic writer OK: bytes=2935
semantic extended OK: bytes=5420 cameras=1 lights=1 bones=2 anims=1
morph_targets=1 first_index_count=2 baked_vertices=6
regression OK
version matrix OK: 36 source fixtures, decode-stream + scan-stream + ascii roundtrip + binary roundtrip
fuzz smoke OK (256 cases)
render harness OK: 5 images
```

Build variants:

```txt
strict C89-ish build: OK
FBX_NO_STDIO build: OK
DFBX_ENABLE_MINI_ZLIB=OFF build: OK
FBX_NO_STDIO + DFBX_ENABLE_MINI_ZLIB=OFF build: OK
```

Standards scan:

```txt
direct malloc/calloc/realloc/free calls outside allocator: 0
direct math calls in runtime/codec/examples: 0
C float/double numeric storage in runtime codec: not used; FBX text property names may still contain the word "double" as FBX metadata text.
```

Differential corpus against local ufbx data:

```txt
Expected-valid non-fuzz corpus:
  total=693
  dfbx_ok=693
  ufbx_ok=693
  mismatching_files=80

Full non-fuzz corpus including expected-fail files:
  total=703
  dfbx_ok=701
  ufbx_ok=694
  mismatching_files=80
```

The two full-corpus failures are expected failure/unsupported-version style cases:

```txt
synthetic_node_depth_fail_7400_binary.fbx
synthetic_unsupported_cube_fail_2000_binary.fbx
```

## Remaining compatibility gaps

The parser now successfully opens the full expected-valid ufbx corpus, but 80 files still differ in semantic counts. Main categories:

- Instancing semantics: our probe currently counts expanded model/mesh attachments differently than ufbx instance sharing.
- Advanced texture graphs: OpenPBR/Physical/Arnold/layered texture graphs are under- or over-counted depending on connection topology.
- Legacy 5.x/5.8 material/camera/light/embedded texture variants still need deeper handling.
- Some shape/morph variants from Maya/ZBrush/legacy files are not mapped yet.
- NURBS/curves/surfaces are decoded/stored, but no tessellator/evaluator is implemented.
- Weighted tangent evaluation is not bit-exact yet.
- Animation layer blending is useful and structured, but not full Autodesk/ufbx equivalence.

## Suggested next pass

1. Texture graph normalizer: count and bind texture layers like ufbx, including layered textures, procedural/video indirections and PBR property maps.
2. Instance-sharing model: separate geometry assets from model instances in the public probe/API.
3. Legacy 5.x/5.8 object extraction: materials/cameras/lights/textures and old-style blend shapes.
4. Weighted tangent bit-exact evaluator: packed tangent flags/data, weights, velocities and pre/post extrapolation parity tests.
5. Morph compatibility pass: Maya/ZBrush/legacy shape target chains + morph animation.
6. NURBS tessellation/evaluation module: optional, separate from core decode.
7. Full visual harness: deterministic thumbnail/image diff after tessellation and material preview.
