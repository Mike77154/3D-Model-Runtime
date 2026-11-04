# xparser_model_geter v0.5 - C89 fixed-point DirectX .x decoder

Runtime-oriented edition: strict C89, caller-owned storage, no runtime allocation,
no C `float`/`double`, Q16.16 numeric callbacks, streaming I/O providers, bounded
parsing budgets, and no inferred humanoid anatomy.

## v0.5 at a glance

v0.4 branches from the frozen v0.3 corpus-hardened baseline.  The v0.3 artifact
is not modified.  v0.4 adds runtime integration and format coverage without
changing the proven text/binary/MSZIP transport core.


v0.5 keeps the v0.4 parser/rig contract and adds a caller-owned animation
recovery layer in `xp_anim_pose.h`: arbitrary clip/key capacities, per-clip
frame counts, random-access frame sampling, and streaming reconstruction of the
complete local pose for every Frame node at every integer animation tick.

New in v0.5:

- `xp_anim_capture`: caller-owned collector for Frame bind matrices, AnimationSet
  clips, Animation tracks/targets and normalized source keys.
- count-only first pass: NULL/zero-capacity arrays still report exact required
  node/clip/track/key counts; re-run with caller arena/static arrays of that size.
- no `XP_MAX_CLIPS`, `XP_MAX_FRAMES`, or internal pose heap.
- `xp_anim_clip_record.frame_count`: inclusive integer-tick timeline
  (`last_tick - first_tick + 1`) when a clip has keys.
- `xp_anim_sample_pose_frame()`: random access `clip + frame -> complete local
  skeleton pose`.
- `xp_anim_emit_all_poses()`: streams every clip/frame/node pose through
  callbacks with only one caller-owned pose scratch array.
- position/scale use Q16.16 linear interpolation; quaternion rotation uses
  shortest-path normalized linear interpolation; matrix-key tracks preserve
  exact source matrices and hold the previous matrix between sparse keys.
- source `raw_key_type`, semantic key type, flags and all original key values are
  retained alongside reconstructed poses.

Important timing note: the `.x` format stores positive animation `time` values
as ticks (`TimedFloatKeys`) and optionally an `AnimTicksPerSecond` scale; it does
not store a universal display FPS. v0.5 therefore calls one integer tick one
reconstructed runtime frame. A runtime that wants 24/30/60 FPS may resample the
clip using the retained tick metadata.
New in v0.4:

- `xp_open_io()` provider API: the decoder no longer requires `FILE*` internally.
- `xp_open_file()` remains as a backwards-compatible stdio adapter.
- `xp_io_init_memory()` provides a caller-owned memory source; custom VFS/PAK/ROM
  providers only need a sequential read callback and do not need seek support.
- configurable parse budgets (`xp_limits`) for objects, vertices, faces,
  index-like records, skin weights, animation keys, raw DWORD payloads and
  decoded bytes; zero means unlimited and preserves v0.3 behavior.
- `xp_usage` counters for post-parse telemetry.
- enriched `xp_diagnostic` callback and `xp_get_last_diagnostic()`, including
  original source encoding, decoded byte offset and current object context.
- semantic passthrough callbacks for unknown/custom objects.  Binary packed
  number lists are exposed after v0.3 scalar flattening, not as byte-exact
  transport records.
- explicit `VertexDuplicationIndices` decoding.
- explicit `DeclData` / `VertexElement` decoding while preserving the raw DWORD
  data payload instead of imposing renderer semantics.
- explicit `FVFData` decoding while preserving its raw DWORD payload.
- binary custom template declarations now reach the same unknown-object stream
  used by text template declarations.
- full unsigned text DWORD parsing through `UINT_MAX`, useful for raw vertex
  declaration/FVF payloads.
- stable parse-local Frame node IDs, including anonymous/duplicate Frames, plus
  explicit mesh-to-owner-frame binding.
- runtime skin-binding callbacks that report mesh + bone + inverse-bind matrix
  together, avoiding name/context ambiguity.
- animation semantic normalization: standard key types remain raw, while legacy
  exporter `keyType=4` records with 16 values are additionally reported as
  absolute local matrix keys with a compatibility flag.
- animation-set summaries (track count, raw tick range, optional ticks/second).
- legacy MaterialWrap/MeshFaceWraps callbacks and a DirectX-to-OpenGL UV helper
  that flips V without clamping or destroying texture tiling.

Microsoft template definitions used by the v0.4 parsers:

- VertexDuplicationIndices: https://learn.microsoft.com/windows/win32/direct3d9/id3dxfile--registertemplates
- DeclData: https://learn.microsoft.com/windows/win32/direct3d9/decldata
- VertexElement: https://learn.microsoft.com/windows/win32/direct3d9/vertexelement
- FVFData: https://learn.microsoft.com/windows/win32/direct3d9/fvfdata

## Core format coverage

- `.x` encodings: `txt`, `bin`, `tzip`, `bzip`
- Mesh vertices and faces
- MeshNormals
- MeshTextureCoords
- MeshVertexColors
- MeshMaterialList and Material
- TextureFilename
- Frame and FrameTransformMatrix
- SkinWeights and XSkinMeshHeader
- AnimationSet, Animation, AnimationKey, AnimationOptions
- AnimTicksPerSecond
- object references (`{ Name }`) and animation targets
- VertexDuplicationIndices
- DeclData / VertexElement
- FVFData
- unknown/custom objects through semantic token callbacks
- MaterialWrap / MeshFaceWraps legacy texture-wrap hints
- stable Frame node IDs + mesh owner-frame binding
- explicit mesh/bone/inverse-bind skin binding
- normalized animation-key semantics and AnimationSet clip summaries

## Numeric protocol

All real-valued model data is emitted as signed 32-bit Q16.16:

```c
typedef /* 32-bit signed C89 fundamental type */ xp_fx32;
#define XP_FX_ONE ((xp_fx32)65536L)
```

Text decimals and binary 0032/0064 real payloads are converted without declaring
C real-number types in the implementation.

## Provider I/O

A custom source is just:

```c
typedef xp_result (*xp_io_read_fn)(void* user,
                                   unsigned char* dst,
                                   unsigned int cap,
                                   unsigned int* out_n);

typedef struct xp_io {
    void* user;
    xp_io_read_fn read;
} xp_io;
```

The provider may return fewer than `cap` bytes. `XP_OK` with `out_n == 0` means
clean EOF.  The parser only requires forward sequential reads.

Typical runtime topology:

```text
asset/VFS/PAK/ROM
       |
       v
    xp_io
       |
       v
 xparsermodget98
       |
       +--> mesh callbacks
       +--> frame/skin callbacks
       +--> animation callbacks
       +--> xp_anim_capture / pose sampler
       +--> extension/raw callbacks
```

For old code, `xp_open_file(&ctx, file, &hdr)` still works.

## Rig / skinning runtime contract

The decoder now exposes both the original callbacks and an explicit runtime
contract.  For Frames, prefer `on_frame_node_begin()` / `on_frame_node_transform()`
when building a persistent rig because numeric node IDs preserve anonymous and
duplicate Frame names.

The matrices are emitted in the authored DirectX `.x` convention.  For this
library's helpers:

```text
combined(child) = local(child) * combined(parent)
finalSkin(bone) = inverseBind(matrixOffset) * combined(bone)
```

Use `xp_mat4_mul_dx()` and `xp_skin_final_matrix()` rather than transposing or
reversing the operands in the runtime.  After weighted skinning, do **not**
apply the mesh owner Frame transform a second time.  `on_mesh_frame_node_binding()`
identifies the owner Frame explicitly.

`SkinWeights.matrixOffset` is exposed unchanged by the original callback and
also as `on_skin_inverse_bind_matrix(mesh,bone,matrix)`.  Bone names remain
exactly as authored; no humanoid mapping is invented.

## Integrated animation clips

Every `AnimationSet` is surfaced as a clip boundary.  The existing callbacks
still expose every Animation/target/key verbatim.  v0.4 additionally emits
`on_animation_key_semantic()` and `on_animation_set_summary()`.

Semantic mapping:

```text
raw 0 +  4 values -> rotation
raw 1 +  3 values -> scale
raw 2 +  3 values -> position
raw 3 + 16 values -> matrix
raw 4 + 16 values -> matrix + XP_ANIM_KEY_FLAG_LEGACY_TYPE4
```

Recognized keys carry `XP_ANIM_KEY_FLAG_ABSOLUTE_LOCAL`: they describe the local
pose at that tick, not an extra transform to multiply on top of the static
Frame transform.  A matrix key should therefore replace the current local
matrix for the target Frame at that sample.  Component tracks likewise provide
the authored local component values.

`AnimTicksPerSecond` is reported when present.  If a file omits it, v0.4 does
not invent a rate: the clip summary returns `ticks_per_second_known == 0` and
keeps the raw tick range.

## Texture coordinates and tiling

`on_texcoord()` returns authored UVs unchanged.  Values below 0 or above 1 are
intentional data and remain untouched; the decoder never clamps, wraps, folds
or normalizes them.  `MaterialWrap` and `MeshFaceWraps` are reported separately
as legacy topology hints when present.

For an OpenGL-style UV convention, `xp_uv_dx_to_gl(u,v,...)` performs only:

```text
u' = u
v' = 1 - v
```

This preserves tiling.  Whether to use REPEAT/CLAMP and whether the image loader
already flips the bitmap vertically are renderer decisions; do not flip both
the bitmap and V coordinates unless that is intentional.

## Budgets

Configure after `xp_open_file()` / `xp_open_io()` and before `xp_parse()`:

```c
xp_limits lim;
xp_limits_unlimited(&lim);
lim.max_objects = 4096UL;
lim.max_vertices = 1000000UL;
lim.max_faces = 1000000UL;
lim.max_indices = 4000000UL;
lim.max_skin_weights = 4000000UL;
lim.max_animation_keys = 1000000UL;
lim.max_raw_dwords = 8000000UL;
lim.max_decoded_bytes = 64UL * 1024UL * 1024UL;
xp_set_limits(&ctx, &lim);
```

`max_indices` covers mesh face indices, normal-face indices, material face-index
records and VertexDuplicationIndices.  A zero field disables that budget.

## Diagnostics

`on_error` remains for source compatibility.  `on_diagnostic` additionally gets:

- `xp_result`
- original source encoding (`txt/bin/tzip/bzip`)
- line/column
- decoded stream byte offset
- current object type/name
- message

The most recent failure remains available through `xp_get_last_diagnostic()`.

## Storage model

`xp_context` and all parser state are caller-owned.  Internal reader, token,
MSZIP dictionary/output and callback scratch buffers are statically bounded by
the context or automatic storage.  The library performs no heap allocation.

Callback strings, face arrays, matrices, key values and unknown token strings
are borrowed and valid only for the callback duration.

## No anatomy inference

Frames, skin weights, references and animation targets are emitted as authored.
The decoder does not invent pelvis/spine/limb relationships and does not coerce a
mesh into a humanoid skeleton.  The runtime decides how to interpret its frame
graph.

## Build

```sh
make clean
make CFLAGS='-std=c89 -Wall -Wextra -Werror -pedantic -O2'
make test-v04
```

## Validation

v0.4 retains the frozen v0.3 corpus result:

- external corpus: **196/199** parse successfully;
- 104/104 text, 3/3 binary, 89/92 bzip;
- the same three bzip files remain rejected as malformed DEFLATE input;
- GCC + Clang: all eight TEXT/BINARY/MSZIP feature-toggle combinations compile
  with strict C89 + `-Werror -pedantic` (**16/16 builds**);
- UBSan: full 199-file corpus, no undefined-behavior finding;
- ASan+UBSan: v0.4 provider/extensions/budget regression test passes;
- implementation scan: no `malloc/calloc/realloc/free`, no C `float/double`, no
  `long long` declarations.

See `CORPUS_REPORT_V0.3.md` for the frozen corpus investigation and
`V0.4_REPORT.md` for this release.
