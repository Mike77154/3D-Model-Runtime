# 3dfbxplus codec pass 3 report

Focus: keep the library as a generic FBX codec first; leave GACKT/export-to-engine as a downstream consumer.

## Implemented in this pass

### 1. Real streaming surfaces

- `fbx_open_stream_read()` / `fbx_codec_decode_stream()` can decode from a forward-only read callback.
- `fbx_stream_scan_binary()` is a lightweight binary-FBX event scanner that does not materialize the raw tree.
- The scanner reports nodes/properties/arrays/compressed arrays/depth and validates 32-bit vs 64-bit binary node records by version.
- The scanner is forward-only and skips payloads with bounded scratch buffers.

Current scope: binary FBX preflight/indexing/diagnostics. ASCII full decode goes through the normal parser path using streaming prefix replay.

Additional streaming fix in this pass:

- Forward-only ASCII decode no longer requires seek after header sniffing; `FbxContext` replays a small prefix buffer so the ASCII parser sees the bytes consumed by detection.
- Stream ASCII version is recovered from the parsed raw tree when possible.

### 2. Morph bake to final mesh vertex buffer

- `FbxMorphTarget` now stores baked target arrays aligned to `FbxMesh.positions` / `FbxMesh.normals`.
- `fbx_mesh_bake_morph_targets()` expands shape data to final duplicated render vertices.
- `fbx_mesh_apply_morph()` blends baked targets into caller-provided final vertex buffers.
- Scene build invokes morph bake after target attachment.

### 3. Animation evaluator pass

- Added `FbxAnimEvalOptions`.
- Added constant, linear, repeat, mirror extrapolation paths.
- Added cubic Hermite evaluation using parsed tangent metadata.
- Added weighted tangent approximation using FBX key attribute flags/weights.
- Added `PreExtrapolation` / `PostExtrapolation` parsing when present.

Current scope: practical evaluator for fixed-point runtime/tooling. It is not claimed bit-exact against Autodesk for every rare tangent/extrapolation flag yet.

### 4. Semantic FBX writer API

- Added/kept `fbx_semantic_writer.h/.c` as a scene-to-FBX API.
- Supports semantic mesh input: positions, normals, UVs, colors, material name, diffuse texture.
- Emits raw-tree FBX, ASCII FBX, and binary FBX using the generic codec writers.
- Tested binary semantic output by decoding it again with the local decoder.

### 5. Version-by-version test battery

Generated deterministic fixtures under `tests/version_matrix/`:

- FBX 5-ish: 5000
- FBX 6-ish: 6000
- FBX 2010-ish: 6100
- FBX 7-ish: 7000
- FBX 2011: 7100
- FBX 2012: 7200
- FBX 2013: 7300
- FBX 2014: 7400
- FBX 2015: 7400
- FBX 2016: 7500
- FBX 2016x: 7600
- modern sanity: 7700

For each label:

- ASCII FBX
- binary raw arrays
- binary zlib-compressed arrays

Total: 36 source fixtures.

The runner performs:

- decode/dump
- streaming binary scan for binary cases
- FBX -> ASCII roundtrip -> decode
- FBX -> binary compressed roundtrip -> decode
- preview PPM smoke for one case

### 6. Differential hooks

- `tests/run_differential_external.sh`: generic mature-parser differential harness.
- `tests/diff_against_ufbx.py`: optional ufbx-style dump differential hook.
- `tests/corpus/README.md` and `tests/corpus/SOURCES.md`: external corpus drop-zone and suggested sources.

No third-party assets are bundled because free does not mean redistributable.

### 7. Automatic preview/render harness

- `fbx_preview_ppm`: C tool that decodes FBX and writes a tiny dependency-free wireframe PPM.
- `fbx_to_obj`: OBJ export smoke tool for geometry inspection.
- `tests/render_harness.py`: converts a small FBX set to OBJ, renders deterministic PGM wireframes, and writes SHA-256 hashes.

## Validation commands run

```sh
cmake -DCMAKE_C_FLAGS="-Wall -Wextra -Werror -pedantic" ..
cmake --build . -j1
```

Result: OK.

```txt
miniz smoke OK: 127 bytes
morph_targets=1 first_index_count=2 baked_vertices=6
stream OK: binary=0 version=7400 models=1 scan=skip nodes=0 props=0 arrays=0 compressed=0 max_depth=0
stream OK: binary=1 version=7400 models=1 scan=yes nodes=21 props=24 arrays=5 compressed=5 max_depth=3
stream scan OK: version=7400 nodes=21 props=24 arrays=5 compressed=5 bytes=1072
anim_eval OK: mid=327680 pre=-655360 repeat=327680 mirror=327680
semantic OK: ascii_bytes=2112
semantic writer OK: bytes=2935
regression OK
version matrix OK: 36 source fixtures, decode-stream + scan-stream + ascii roundtrip + binary roundtrip
render harness OK: 5 images
fuzz smoke OK (256 cases)
real corpus skipped: no FBX files found; see tests/corpus/README.md
differential skipped: set REF_DUMP to a mature parser dump tool, e.g. a local ufbx-based dumper
```

Additional build variants:

```txt
no_stdio_OK
no_zlib_core_OK
no_stdio_nozlib_core_OK
```

## Standard/conformance scan

Direct heap calls in core library:

```txt
malloc/calloc/realloc/free direct calls: 0
```

C floating point type tokens in core library:

```txt
float/double tokens: 0
```

Direct math dependency in core library:

```txt
math.h / sin / cos / sqrt / tan / fabs: 0
```

`fbx_importer.h` includes `<stdio.h>` only when `FBX_NO_STDIO` is not defined. The no-stdio library build was validated.

## Still not honestly final

- The streaming event scanner is binary-first. Full ASCII decode now works from a forward-only read callback through prefix replay, but ASCII event scanning is still not separate.
- Animation is much stronger, but not yet a bit-exact Autodesk clone for every weighted tangent/extrapolation corner.
- Real Blender/Maya/3ds Max/Mixamo assets are not bundled; harnesses are ready for local asset drops and differential testing.
- Export is now semantic enough for mesh/material/texture/color smoke tests, but not yet a full authoring exporter for skeletons, animation stacks, constraints, takes, poses, and morph animation.
