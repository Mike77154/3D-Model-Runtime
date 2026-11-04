# 3dfbxplus / GACKT FBX importer

Small C89/C90-style FBX importer, scene builder, fixed-point asset compiler, and runtime GACKT reader.

The project now has two intended build paths:

1. **Tool/import path**: reads FBX from file or caller-owned memory, builds the raw node tree, then builds a scene.
2. **Runtime path**: consumes the exported GACKT binary stream. This path is fixed-point, caller-buffer driven, and can be built without `stdio`.

> FBX needs 64-bit IDs and offsets. ISO C90 has no native 64-bit integer type, so the project uses compiler extensions: MSVC `__int64`, GCC/Clang `long long` wrapped with `__extension__`.

## Current support

- FBX binary and ASCII parsing.
- Raw object tree import: `FbxNode` + `FbxProperty`.
- Connection graph: `Connections/C` with `OO` and `OP` relations.
- Scene build separated from raw tree read.
- Mesh extraction from `Geometry` objects.
- `PolygonVertexIndex` triangulation.
- Common UV/normal mapping cases:
  - `ByPolygonVertex`
  - `ByVertice` / `ByVertex` / `ByControlPoint`
  - `Direct` and `IndexToDirect`
- Multiple UV sets.
- Normal/tangent/bitangent generation when source data is missing.
- Basic skeleton/skin extraction.
- Basic animation stack/layer/curve extraction and bake helpers.
- Version profile diagnostics.
- Legacy FBX 6000 adapter path.

## Strictness pass

This branch removes direct standard heap calls from built source files and routes allocations through `fbx_alloc.*` arena helpers.

The numeric path uses `fbx_real` Q16.16 fixed point. Binary FBX numeric payloads are converted from IEEE bit patterns to fixed point without using host real-number types. ASCII numeric tokens are parsed manually into fixed point.

## Runtime/no-stdio options

The importer now supports:

```c
FbxContext *fbx_open_memory(const void *data, fbx_i64 size);
```

For a no-stdio build:

```bash
cmake .. -DCMAKE_C_FLAGS="-DFBX_NO_STDIO -Wall -Wextra -Werror -pedantic"
cmake --build .
```

The CMake target `gackt_runtime` only builds the fixed GACKT memory validator/reader support and does not include the FBX importer.

## GACKT export

The tool path can export a fixed-point engine binary:

```bash
./fbx_to_gackt input.fbx output.gackt
```

The exported stream starts with the magic `GACKT3D\0` and contains a header, mesh records, name table, packed fixed-point attributes, and indices.

## Build

```bash
mkdir build
cd build
cmake .. -DCMAKE_C_FLAGS="-Wall -Wextra -Werror -pedantic"
cmake --build .
```

## Smoke tests used in this pass

Generated ASCII fixtures were built for FBX version headers:

```txt
6000, 6100, 7000, 7100, 7200, 7300, 7400, 7500, 7600, 7700
```

For each fixture, `fbx_dump` output was compared against the pre-refactor baseline. The outputs matched exactly for the smoke corpus.

## Added smoke tests

Useful commands after building:

```bash
BUILD_DIR=$PWD/build ./tests/run_regression.sh
DUMP=$PWD/build/fbx_dump CASES=128 ./tests/fuzz_smoke.py
```

The detailed pass report is in `ENGINE_PASS_REPORT.md`; command results are in `tests/validation_report.txt`.
