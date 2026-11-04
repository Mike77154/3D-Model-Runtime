# 3dfbxplus Codec Pass 6 Report

Focus of this pass:

- chase the remaining differential mismatches from pass 5
- preserve the project constraints: C89-ish, no direct heap calls, no C float/double numeric core, fixed-point, arena-backed allocation, no-stdio build path
- align mesh asset counting with ufbx semantics for pathological FBX assets

## Differential result

Expected-valid, non-fuzz `ufbx/data` corpus:

```txt
total=693
dfbx_ok=693
ufbx_ok=693
mismatching_files=0
```

Manifest:

```txt
tests/ufbx_matrix_pass6_final/manifest.tsv
```

## Mismatch fixes

### NaN / Inf / Indeterminate ASCII numerics

The ASCII tokenizer now keeps SDK-style numeric spellings intact:

```txt
1.#NAN
-1.#NAN
1.#INF
-1.#INF
inf
-infinity
nan(ind)
-1.#IND
```

The fixed-point parser sanitizes them deterministically:

```txt
NaN / IND -> 0
+Inf      -> FBX_REAL_MAX
-Inf      -> FBX_REAL_MIN
```

This fixed the synthetic NaN/Inf files without introducing `float` or `double` types.

### Empty legacy ASCII array nodes

Legacy ASCII exporters can write empty payloads like:

```txt
Vertices:
PolygonVertexIndex:
Normals:
```

Previously the parser could accidentally consume the next sibling identifier as a string property of the empty node. The parser now treats known array-payload node names as empty when the next token is another identifier. This fixed `blender_279_empty_cube_6100_ascii.fbx` and prevents empty array nodes from eating their siblings.

### Duplicate and negative FBX IDs

The scene now counts object-level `Geometry::Mesh` assets instead of collapsing everything by FBX ID. This preserves:

```txt
orphan mesh assets
empty mesh assets
duplicate object IDs
negative object IDs
INT64_MIN / INT64_MAX IDs
```

Negative IDs are no longer rejected as invalid parents; only ID `0` remains the root/no-parent sentinel.

### Mesh asset semantics

`fbx_scene_count_unique_meshes()` now follows asset semantics closer to ufbx:

```txt
modern FBX:
    return Geometry::Mesh object count

legacy FBX 5/6 direct-geometry models:
    count distinct mesh payloads attached to Model nodes
```

This fixed:

```txt
blender_293_barbarian_7400_binary.fbx
max_quote_6100_ascii.fbx
synthetic_duplicate_id_*.fbx
synthetic_negative_fbx_id_*.fbx
synthetic_empty_elements_7500_ascii.fbx
synthetic_unicode_7500_binary.fbx
```

### Empty animation layers

The scene now tracks object-level `AnimationLayer` placeholders, including empty/orphan layer nodes. The probe reports the max of connected clip layers and raw layer objects, matching ufbx's `scene->anim_layers.count` behavior for empty element edge cases.

## Validation

Stored in:

```txt
tests/validation_pass6.txt
```

Key result:

```txt
strict_build=OK
no_stdio_build=OK
no_mini_zlib_build=OK
no_stdio_no_mini_zlib_build=OK
regression=OK
version_matrix=OK fixtures=36
render_harness=OK images=5
fuzz_smoke=OK cases=256
ufbx_expected_valid_total=693
dfbx_ok=693
ufbx_ok=693
mismatching_files=0
heap_direct_calls=0
float_double_code_tokens=0
direct_math_calls=0
runtime_stdio_unguarded_include=0
```

## Honest status

Pass 6 clears the known pass-5 differential mismatches in the expected-valid ufbx corpus. This is not a proof that every FBX ever produced by every exporter is supported, but the codec is now much better aligned with ufbx-style behavior on the real corpus we have locally.

Remaining long-term hardening areas:

```txt
fuzz corpus expansion beyond smoke cases
visual validation with texture sampling and lighting
bit-exact animation tangent comparisons beyond probe-level counts
full NURBS rational surface evaluator/tessellator hardening
more semantic writer coverage for exotic material/animation graphs
```
