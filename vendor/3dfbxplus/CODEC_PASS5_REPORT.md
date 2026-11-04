# 3dfbxplus Codec Pass 5 Report

Focus of this pass:

- tighten behavior against `ufbx`/FBX SDK-style semantics without copying code
- improve weighted tangents and animation layer composition
- fix legacy `Takes` mapping by FBX version family
- improve embedded texture decoding, including split ASCII `Content` payloads
- stabilize advanced texture graph counting, including `LayeredTexture`
- add a first public NURBS tessellation/evaluation API
- improve material-colored preview rendering
- preserve the project constraints: C89-ish, fixed-point, no direct heap calls, no runtime stdio build path

## Highlights

### Differential against ufbx corpus

Pass 5 final matrix over the expected-valid, non-fuzz `ufbx/data` corpus:

```txt
expected_valid_total=693
dfbx_ok=693
ufbx_ok=693
mismatching_files=12
```

The remaining manifest is saved at:

```txt
tests/ufbx_matrix_pass5_final/manifest.tsv
```

This pass reduced the active mismatch set from the mid-pass 49 down to 12 by fixing ASCII `Content` chunking, duplicate embedded media lookup, legacy take counting, texture graph object filtering, and future ASCII header version detection.

### Embedded textures / advanced texture graphs

Fixed cases where ASCII FBX writes embedded media as many quoted base64 chunks:

- `Content: , "chunk", "chunk", ...`
- giant single-line base64 payloads
- duplicated `Video` objects that share the same filename/path and rely on another `Video` object for content
- invalid tiny split chunks that should not be reassembled if each chunk is not a valid base64 payload, matching ufbx behavior on synthetic split tests

Representative fixed comparisons:

```txt
marvelous_quad_7700_ascii.fbx:
  embedded_textures=3
  embedded_bytes=37729

synthetic_texture_split_7500_ascii.fbx:
  embedded_textures=4
  embedded_bytes=8163

synthetic_embedded_base64_7700_ascii.fbx:
  embedded_textures=1
  embedded_bytes=2487
```

Texture graph counting now filters out scalar child nodes named `Texture` and correctly counts object-level `Texture`/`LayeredTexture` nodes.

### Legacy Takes / animation stacks

`Takes` parsing is now version-aware:

- FBX 3.x/5.0-style files can expose `Current` as a real placeholder stack.
- FBX 5.8/6.1-style files usually use `Current` as an active-take selector, so it is not double-counted when a real `Take` node exists.
- Modern 7.x files do not synthesize stacks from legacy `Takes` when no `AnimationStack` exists.

This fixed broad `anim_clips`/`anim_layers` over-counting against ufbx.

### Weighted tangents / layer blending

The evaluator now treats neutral weighted Bezier handles closer to FBX behavior by scaling tangent weight around the `1/3` Bezier handle convention. Additive layers now apply weighted deltas relative to the layer base value instead of adding absolute values directly.

Validation smoke:

```txt
anim_eval OK: mid=327680 pre=-655360 repeat=327680 mirror=327680 weighted=327679 layer=819200
```

This is improved, but not claimed bit-exact against every Autodesk/ufbx tangent edge case yet.

### NURBS tessellation/evaluation API

Added:

```txt
3dfbx/fbx_nurbs.h
3dfbx/fbx_nurbs.c
```

Public API:

```c
int fbx_nurbs_curve_tessellate(const FbxCurveGeometry *curve,
                               int segments,
                               fbx_real **out_positions,
                               fbx_u32 *out_vertex_count);

int fbx_nurbs_surface_tessellate(const FbxSurfaceGeometry *surface,
                                 int segments_u,
                                 int segments_v,
                                 fbx_real **out_positions,
                                 fbx_u32 **out_indices,
                                 fbx_u32 *out_vertex_count,
                                 fbx_u32 *out_index_count);
```

Smoke result:

```txt
nurbs smoke OK: curve_vertices=9 surface_vertices=9 indices=24
```

This is a real fixed-point tessellation/evaluation entry point. Surface evaluation is still a conservative grid/fallback path, not a complete rational NURBS renderer.

### Material-colored preview rendering

`fbx_preview_ppm` now fills triangles using resolved material diffuse colors before drawing wire overlay. This turns the old geometry-only preview into a material-colored render smoke harness.

Smoke result:

```txt
render harness OK: 5 images
```

## Validation

Stored in:

```txt
tests/validation_pass5.txt
```

Key results:

```txt
strict_build=OK
no_stdio_build=OK
no_mini_zlib_build=OK
no_stdio_no_mini_zlib_build=OK
regression=OK
version_matrix=OK fixtures=36
render_harness=OK images=5
fuzz_smoke=OK cases=256
heap_lines=0
float_double_lines=0
math_call_lines=0
```

## Remaining known mismatches vs ufbx

Pass 5 leaves 12 mismatch files, mostly synthetic or edge-case assets:

```txt
blender_293_barbarian_7400_binary.fbx        meshes 33!=34
max_quote_6100_ascii.fbx                    meshes 2!=3
synthetic_cube_nan_*                         NaN/Inf numeric semantics + material/anim placeholders
synthetic_duplicate_id_*                     duplicate object ID semantics
synthetic_empty_elements_7500_ascii.fbx      empty element layer placeholder
synthetic_inf_nan_*                          Inf/NaN numeric semantics + placeholders
synthetic_negative_fbx_id_7700_*             negative ID duplicate geometry semantics
synthetic_unicode_7500_binary.fbx            intentionally hostile unicode/name/object handling
```

These are not ignored; they are the next obvious attack surface if the goal is to chase ufbx-style behavior down to the last synthetic edge case.

## Honest status

Improved substantially:

- differential compatibility
- embedded textures
- legacy takes/layers
- texture graph object counting
- weighted tangent behavior
- additive layer blending
- NURBS API surface
- material preview rendering

Still partial:

- fully bit-exact weighted tangents
- every Autodesk/ufbx layer blending mode and override behavior
- complete OpenPBR/Arnold layered material graph semantics
- full rational NURBS tessellation/evaluation
- pathological duplicate IDs / negative IDs / NaN/Inf synthetic behavior
- production viewport rendering with real lighting and texture sampling
