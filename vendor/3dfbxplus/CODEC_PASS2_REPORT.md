# 3dfbxplus codec pass 2

## Goal

Continue the engine-oriented FBX refactor while keeping the core FBX library generic:

- replace host zlib with an internal mini zlib/deflate decoder for FBX compressed arrays,
- keep GACKT as a final-stage/export concern instead of the center of the codec,
- add a separate `fbx_codec/` layer for generic FBX decode/write APIs,
- improve animation and morph coverage,
- keep strict C89-ish/no heap/no float/no double rules intact.

## Implemented

### Internal mini zlib

Added:

```txt
fbx_codec/fbx_mini_zlib.h
fbx_codec/fbx_mini_zlib.c
```

Capabilities:

- zlib wrapper validation: CMF/FLG, compression method, header check, no preset dictionary.
- Adler-32 validation.
- DEFLATE decode:
  - stored blocks,
  - fixed Huffman blocks,
  - dynamic Huffman blocks,
  - LZ77 distance/length back references.
- store-only zlib encoder for binary FBX array export.
- no malloc/calloc/realloc/free.
- no float/double.
- no stdio.

`3dfbx/fbx_arrays.c` now uses the internal mini decoder for FBX binary array properties with `encoding == 1`.

### Generic FBX codec layer

Added a dedicated generic folder:

```txt
fbx_codec/
├─ fbx_codec.h
├─ fbx_codec_decode.h
├─ fbx_codec_decode.c
├─ fbx_codec_writer.h
├─ fbx_codec_writer_ascii.c
├─ fbx_binary_writer.h
├─ fbx_binary_writer.c
├─ fbx_mini_zlib.h
├─ fbx_mini_zlib.c
└─ tools/
   ├─ fbx_to_fbxascii.c
   └─ fbx_to_fbxbin.c
```

Decode API:

```c
int fbx_codec_decode_memory(const void *data,
                            fbx_i64 size,
                            int build_scene,
                            FbxCodecDecodeResult *out);
```

Generic export APIs:

```c
int fbx_codec_write_ascii(const FbxNode *root,
                          const FbxCodecWriteStream *stream,
                          const FbxCodecWriteOptions *options);

FBX_Result fbx_binary_write_tree(FbxWriteBuffer *buffer,
                                 const FbxNode *root,
                                 const FbxBinaryWriteOptions *options);
```

Tools:

```txt
fbx_to_fbxbin    input.fbx output_binary.fbx [version] [compress_arrays]
fbx_to_fbxascii  input.fbx output_ascii.fbx [version]
```

### Fixed-point FBX scalar export support

Added:

```c
fbx_u32 fbx_real_to_ieee32_bits(fbx_real v);
fbx_u64 fbx_real_to_ieee64_bits(fbx_real v);
```

This lets the binary FBX writer emit standard FBX scalar/array real values while the internal engine-side representation remains fixed point.

### Animation curve metadata

`FbxKeyframe` now stores:

```c
unsigned int tangent_flags;
fbx_real left_tangent;
fbx_real right_tangent;
fbx_real left_weight;
fbx_real right_weight;
fbx_real left_velocity;
fbx_real right_velocity;
```

Scene build now parses:

```txt
KeyAttrFlags
KeyAttrDataFloat
```

Interpolation modes are mapped into:

```txt
constant
linear
cubic
```

Sampling remains fixed-point and supports clamp + constant/linear/cubic Hermite. Full Autodesk weighted tangent evaluation is still partial.

### Morph targets / blend shapes

Added `FbxMorphTarget` and `mesh->morph_targets`.

Scene build now supports common connection chains:

```txt
Shape -> BlendShapeChannel -> BlendShape -> Geometry
Shape -> Geometry
```

Stored data:

```txt
Shape ID/name
Indexes
Vertices
Normals
DeformPercent as weight
```

The data is raw Shape payload by original control point index. It is not yet expanded to duplicated final render vertices.

### Binary compressed corpus

Added generated binary FBX corpus with compressed arrays:

```txt
tests/generated/binary_compressed_7400.fbx
tests/generated/binary_compressed_7500.fbx
tests/generated/binary_compressed_7700.fbx
```

These use zlib-compressed binary arrays for:

```txt
Vertices
PolygonVertexIndex
Normals
UV
UVIndex
```

### Morph smoke corpus

Added:

```txt
tests/generated/morph_7400_ascii.fbx
tests/morph_smoke.c
```

Expected smoke result:

```txt
morph_targets=1 first_index_count=2
```

## Validation performed

Strict build:

```txt
cmake -DCMAKE_C_FLAGS='-Wall -Wextra -Werror -pedantic' ..
cmake --build . -j1
result: OK
```

No-stdio build:

```txt
cmake -DDFBX_BUILD_TOOLS=OFF -DDFBX_BUILD_TESTS=OFF \
      -DCMAKE_C_FLAGS='-Wall -Wextra -Werror -pedantic -DFBX_NO_STDIO=1' ..
result: OK
```

No-mini-zlib build:

```txt
cmake -DDFBX_BUILD_TOOLS=OFF -DDFBX_BUILD_TESTS=OFF \
      -DDFBX_ENABLE_MINI_ZLIB=OFF \
      -DCMAKE_C_FLAGS='-Wall -Wextra -Werror -pedantic' ..
result: OK
```

No-stdio + no-mini-zlib build:

```txt
cmake -DDFBX_BUILD_TOOLS=OFF -DDFBX_BUILD_TESTS=OFF \
      -DDFBX_ENABLE_MINI_ZLIB=OFF \
      -DCMAKE_C_FLAGS='-Wall -Wextra -Werror -pedantic -DFBX_NO_STDIO=1' ..
result: OK
```

Source scans:

```txt
direct malloc/calloc/realloc/free calls: 0
C float/double type tokens in code:      0
direct math function calls:              0
```

Smoke tests:

```txt
miniz smoke OK: 127 bytes
morph_targets=1 first_index_count=2
```

Regression:

```txt
regression OK
fuzz smoke OK (128 cases)
```

Export round trips:

```txt
ASCII FBX -> binary FBX 7400 with compressed arrays -> fbx_dump OK
binary compressed FBX 7400 -> ASCII FBX -> fbx_dump OK
```

## Still partial / not claimed complete

- Full streaming incremental decode by chunks is not implemented yet.
- Full semantic FBX authoring export is not complete; current export is raw-tree ASCII and binary tree writing.
- Animation weighted tangents/velocity/extrapolation/layer blending are only partially represented.
- Morph targets are parsed as raw Shape payload; not yet baked onto final duplicated render vertices.
- This still needs large real-world corpus validation from Blender/Maya/3ds Max/Mixamo and differential tests against known mature parsers.
- Rendering validation with a real 3D viewport/viewer was not performed in this environment.
