# 3dfbxplus Engine Pass Report

This package is the first hardening/conversion pass toward using 3dfbxplus as an FBX import and GACKT asset-compiler pillar.

## Hard requirements addressed

- C89/C90-style build is enforced by CMake (`CMAKE_C_STANDARD 90`, no extensions requested).
- No direct `malloc`, `calloc`, `realloc`, or `free` calls remain in `3dfbx/` or `examples/`.
- Allocations route through `fbx_alloc.*`, backed by a configurable caller arena or a default static arena.
- Numeric scene data uses `fbx_real` Q16.16 fixed point.
- No `float` or `double` tokens remain in `3dfbx/` or `examples/` source.
- IEEE FBX binary numeric payloads are converted from raw bits into fixed point.
- ASCII numeric payloads are parsed manually into fixed point.
- Direct `math.h`, `sin`, `cos`, `sqrt`, `tan`, and `fabs` usage was removed from source scans.
- The importer can read from caller-owned memory with `fbx_open_memory()`.
- `FBX_NO_STDIO` builds compile for the importer library when tools are disabled.
- `DFBX_ENABLE_ZLIB=OFF` builds compile; compressed FBX arrays then return unsupported instead of linking zlib.
- Runtime-only `gackt_runtime` target avoids FBX importer, zlib, and stdio.

## Architecture work done

- Raw tree read is exposed as `fbx_read_raw_tree(ctx)`.
- Scene building stays separate as `fbx_scene_build(root)`.
- Safety limits are centralized in `FbxLimits`/`fbx_config.h`.
- Parser tracks finer error categories with `FbxErrorCode`.
- Binary/ASCII parsers enforce depth, node, property, children, array element, and array byte limits.
- Array byte arithmetic now has overflow checks.
- Fixed-point helper math uses integer operations only.

## Engine asset compiler work done

- Added `fbx_gackt.h`, `fbx_gackt_export.c`, and `fbx_gackt_runtime.c`.
- Added `examples/fbx_to_gackt.c`.
- GACKT v1 exports fixed-point mesh data from FBX scene builds.
- GACKT v1 supports positions, normals, UV0, tangents, bitangents, vertex colors, indices, and optional transform baking flag.
- GACKT runtime validates memory buffers with no importer dependency.

## Scene feature work done

- Materials are parsed from `Objects/Material`.
- Texture file paths are parsed from `Texture`/`Video` objects.
- Material-to-model connections are resolved.
- Mesh material slots and contiguous submesh ranges are created from material mapping where present.
- Vertex colors are parsed from `LayerElementColor` for common mapping/reference combinations.
- Camera metadata is parsed from camera model `Properties70`.
- Light metadata is parsed from light model `Properties70`.
- Skin weights are sorted per vertex, capped by `FBX_DEFAULT_MAX_WEIGHTS_PER_VERTEX`, and normalized.
- Animation curve sampling supports constant, linear, and cubic Hermite interpolation infrastructure.
- Animation time conversion now uses fixed ratio conversion, not host real values.

## Validation summary

See `tests/validation_report.txt` for the exact command matrix and results.

Validated in this pass:

- Strict C89-ish build with `-Wall -Wextra -Werror -pedantic`.
- No-stdio build.
- No-zlib build.
- No-stdio + no-zlib build.
- Source scan for heap calls, host real tokens, math calls, and runtime stdio.
- Baseline `fbx_dump` diff on the original quad fixture.
- 18 generated ASCII regression fixtures.
- Synthetic FBX version headers: `6000, 6100, 7000, 7100, 7200, 7300, 7400, 7500, 7600, 7700`.
- GACKT export smoke.
- `fbx_open_memory()` smoke.
- Material/texture/submesh smoke.
- Camera/light smoke.
- Mutation fuzz smoke: 128 generated cases, no crash observed.

## Important limitations

This is not a claim of universal FBX coverage yet.

Still incomplete or partial:

- Blend shapes / morph targets are not fully implemented.
- Advanced FBX animation interpolation flags/tangent parsing is not fully mapped; the sampler infrastructure exists.
- GACKT v1 currently exports mesh geometry streams, not a full material/skeleton/animation/morph package.
- Rendering was not visually validated in this environment; validation used parser output, scene probes, and GACKT binary export.
- Synthetic version fixtures are smoke probes, not a real Maya/Blender/3ds Max/Mixamo version corpus.
- zlib-free builds cannot decode compressed FBX arrays by design; they fail cleanly for those arrays.
- The arena allocator is monotonic: `fbx_release()` is a no-op, and memory is reclaimed by arena reset.

## Recommended next pass

1. Add real external corpus runner with downloaded/checked-in FBX fixtures.
2. Compare results against Autodesk FBX SDK and/or ufbx for selected fixtures.
3. Extend GACKT to include materials, texture table, submesh table, skeleton, skin, animations, and morph targets.
4. Add morph parser for `BlendShape`, `BlendShapeChannel`, and `Shape` objects.
5. Parse FBX animation interpolation flags and tangent/weight data fully.
6. Add a headless render/reference-image test once a renderer is wired into the engine toolchain.
