# DTS89 v4.1 -- projected LOD / polygon seam correction

This release fixes the reference renderer's **hard-coded 64px detail
selection**. See `docs/PHASE41_POLYGON_LOD_DIAGNOSTIC.md` for investigation,
before/after captures and reproducible regression results. The reusable
C89 no-heap module is `dts89_lod41`.

# DTS89 v4.0 — Torque DTS/DSQ, fixed-point lighting bridge

**New:** `include/dts89_lighting40.h`, `src/dts89_lighting40.c` and
`tools/dts89_lit40_render.c` connect posed/skinned meshes, native Torque UV,
DDS DXT1/DXT5 materials, normal and specular maps into a C89 integer CPU
reference renderer. See `docs/PHASE40_LIT_SKIN_RUNTIME.md` for exact support,
limitations, usage and memory ownership.

## v3.9 SHADERER (new)

`include/dts89_shaderer.h` and `src/dts89_shaderer.c` add renderer-neutral
Torque material state, UV wrap/clamp, script `mapTo` matching, RGBA8 reference
blending and DXT5nm normal unpacking. `Texturer Opener` now decodes DDS
BC1/BC3 and BMP24/32 to caller-owned RGBA8 (PNG remains probe-only).
See [docs/SHADERER_V39_TORQUE_ORACLE.md](docs/SHADERER_V39_TORQUE_ORACLE.md).

# dts89 v3.5-giffany-oracle-deep-push

`dts89` is a **C89**, **caller-arena**, **no internal heap** reader/writer/prober for Torque native shape assets.

It targets Torque/Torque3D **DTS** shape files, **DSQ** animation files, and common TorqueScript sidecars such as `materials.cs` and `TSShapeConstructor` scripts. The implementation stays fixed-point friendly: engine real-number payloads are preserved as raw IEEE-754 binary32 bit patterns and converted to Q16.16 only through integer-only helpers.

## Hard rules this tree follows

- C89-compatible public API and implementation.
- No internal heap ownership.
- Caller supplies arenas and output buffers.
- No host C real-number arithmetic in the library.
- No `stdint.h` dependency.
- No `long long` dependency.
- No C++ line comments in source files.
- Little-endian decoding is explicit for 16-bit and 32-bit payloads.

## What v3.4/v3.5 adds

- `dts89_probe(...)` for quick DTS / DSQ / TorqueScript classification without a caller arena.
- `tools/dts89_probe` command-line detector.
- DTS split-pool hardening: arena-owned, native-endian pool copies are built from explicit little-endian reads.
- Corrected DTS `Quat16` placement: rotations are read from and written to the 16-bit pool, matching Torque's format layout.
- DSQ reader hardening: one-word Torque DSQ version header, checked counts, checked count multiplications, length-safe strings, no alias-cast reads.
- DSQ v24-v26 acceptance, matching the documented compatibility range.
- TorqueScript parser no longer depends on `ctype.h`; it uses local ASCII helpers.
- Stronger self-test coverage for DTS, DSQ, TorqueScript emission, probe classification, runtime planning, and DSQ import planning.
- White-room Torque runtime planning layer: detail lookup, pixel-size LOD selection, translucent/non-translucent object splitting, alpha-crossfade spans, and draw-command emission.
- White-room DSQ import layer: node-name remapping, ignored-node accounting, sequence-name append, transform-track append, ground-frame append, and trigger append.
- Optional DTS v29 pool XOR support through caller-provided key material, plus a key generator that accepts an explicit caller seed. No Torque application password is hard-coded.
- Legacy DTS v19-v23 are now detected as unsupported instead of being misread as modern v24+ layouts.
- Structural validators for decoded DTS and DSQ data: counts, references, subshape/detail ranges, mesh primitive/material ranges, sequence track ranges, ground ranges, and trigger ranges.
- White-room animation planning helpers for Torque-style keyframe selection, node/object track index resolution, ground frame index resolution, and trigger scanning.
- New command-line helpers: `tools/dts89_validate` and `tools/dts89_anim_plan`.
- v3.5 LOD/runtime refinements from the Torque oracle: smallest visible detail selection uses the minimum non-negative detail size, detail lookup follows Torque's `pixelSize > detail.size` process, intra-detail blend uses the previous-detail size boundary, and screen-error mode uses the legacy average-error path.
- v3.5 render crossfade spans now follow Torque's alpha-in/alpha-out bands: next detail fade-in, current detail full span, and current fade-out are emitted in renderer-neutral order.
- v3.5 DSQ node matching is ASCII case-insensitive while still accepting the Torque-style space/underscore looseness.
- v3.5 triangle expansion/count helpers expose primitive-to-triangle planning for `Triangles`, `Strip`, and `Fan` primitive runs without owning heap memory.
- New command-line helper: `tools/dts89_tri_dump`.

## Current scope

### Binary DTS

Supported in library form:

- shape header and split data pools
- nodes / objects / details / triggers / object states
- names table
- embedded sequences
- material-list tail decoding/writing
- packed vertex blob preservation for v27+
- mesh payloads for:
  - `StandardMesh`
  - `SkinMesh`
  - `SortedMesh`
  - `DecalMesh` compatibility format
  - `NullMesh`
- encoder for current format and legacy `save_old_format=1` output

### Binary DSQ

Supported in library form:

- node name table
- sequence table
- bitsets
- triggers
- node rotation / translation / scale tracks
- ground transforms
- round-trip encode/decode
- v24 through v26 decode/probe compatibility

### Probe API

```c
#include "dts89.h"

int rc;
dts89_probe_result pr;

rc = dts89_probe(bytes, size, &pr);
if (rc == DTS89_OK && pr.kind == DTS89_PROBE_KIND_DTS) {
    /* DTS shape candidate */
}
```

The probe is intentionally conservative. DTS probing is shallow and validates the header/pool shape plus major count fields. DSQ probing performs a structural scan of the whole DSQ file. TorqueScript probing is textual and intended for generated/normal sidecars, not adversarial source-code classification.

### Runtime planning API

```c
dts89_runtime_options opt;
dts89_runtime_shape rt;
dts89_detail_choice choice;
dts89_render_span spans[4];
dts89_u32 span_count;

dts89_runtime_options_default(&opt);
if (dts89_runtime_build(&shape, &opt, &arena, &rt) == DTS89_OK) {
    dts89_runtime_select_detail_from_pixel_size(&shape, &rt, dts89_fx_from_int(128), &choice);
    dts89_runtime_make_render_spans(&shape, &rt, &choice, 0u, spans, 4u, &span_count);
}
```

This layer mirrors Torque's loader/runtime process at the planning level: it does not call a graphics API. It emits detail choices, render spans, and draw commands that an engine can submit through its own renderer.

### Validation and animation planning API

```c
dts89_validate_report report;
dts89_sequence_cursor cursor;
dts89_node_anim_indices node_tracks;

if (dts89_validate_shape(&shape, &report) == DTS89_OK) {
    dts89_sequence_select_keyframes(&shape.sequences[0], DTS89_FX_ONE / 2, &cursor);
    dts89_sequence_get_node_tracks(&shape, 0, 0, &cursor, &node_tracks);
}
```

The animation helpers follow Torque's process for cyclic vs one-shot keyframes, ground-frame indexing, trigger activation order, and per-node/per-object track lookup. They do not force a math backend; the host can interpolate with its own fixed-point transform layer.

### DSQ import API

```c
dts89_dsq_import_plan plan;
dts89_shape shape_with_anim;

dts89_dsq_build_import_plan(&shape, &dsq, &arena, &plan);
dts89_shape_merge_dsq(&shape, &dsq, &arena, &shape_with_anim);
```

The importer follows Torque's process of mapping DSQ node names onto existing DTS nodes, ignoring unknown nodes, remapping matters bitsets, and appending imported transform/ground/trigger data.

### Sidecars

Torque content often resolves real material behavior through script sidecars. This repo includes:

- small TorqueScript singleton parser/writer
- `materials.cs` emitter from decoded DTS material lists
- `TSShapeConstructor` emitter that wires DSQ sequences with `addSequence(...)`

## Limitations

- This is still a pragmatic engineering library, not a byte-perfect clone of the whole Torque runtime/GFX stack.
- Runtime support emits a renderer-neutral plan. It intentionally does not recreate Torque's material-instance system, scene state, skinning backend, or GPU submission.
- DTS guard checkpoint validation is stronger than before, but legacy DTS v19-v23 are treated as unsupported rather than decoded through a compatibility shim.
- Texture image files are not embedded here; Torque usually resolves those through sidecars and runtime material mapping.
- The serializer preserves engine-native binary32 payloads instead of quantizing the whole asset into Q16.16. Fixed-point helpers are provided for inspection/edit layers that want Q16.16 views.
- Packed vertex data for v27+ is preserved well for import/export, but semantic editing of every packed layout variant is still incomplete.

## Build

```sh
make CFLAGS="-std=c89 -Wall -Wextra -Werror -pedantic-errors -Iinclude"
```

This builds:

- `tools/dts89_roundtrip`
- `tools/dts89_inspect`
- `tools/dts89_selftest`
- `tools/dts89_tscript_dump`
- `tools/dts89_dsq_inspect`
- `tools/dts89_probe`
- `tools/dts89_emit_materials`
- `tools/dts89_emit_constructor`
- `tools/dts89_runtime_plan`
- `tools/dts89_validate`
- `tools/dts89_anim_plan`
- `tools/dts89_tri_dump`

## Quick example

```c
#include "dts89.h"

static unsigned char workspace[16 * 1024 * 1024];
static unsigned char output_blob[48 * 1024 * 1024];

void example(const void *file_data, dts89_u32 file_size)
{
    dts89_arena arena;
    dts89_shape shape;
    dts89_writer w;

    dts89_arena_init(&arena, workspace, sizeof(workspace));
    if (dts89_decode_shape(file_data, file_size, &arena, &shape) != DTS89_OK)
        return;

    dts89_writer_init(&w, output_blob, sizeof(output_blob));
    if (dts89_encode_shape(&shape, &w, 0) != DTS89_OK)
        return;

    /* output_blob[0..w.pos) now contains a DTS file */
}
```

## Files

```text
include/dts89.h             public API
src/dts89_core.c            streams, arenas, fixed helpers
src/dts89_probe.c           DTS / DSQ / TorqueScript probe
src/dts89_decode.c          DTS decoder + material-list decode
src/dts89_encode.c          DTS encoder + material-list write
src/dts89_detach.c          borrowed-payload detacher
src/dts89_tscript.c         TorqueScript singleton parser/writer
src/dts89_dsq.c             DSQ codec + script emitters
src/dts89_runtime.c         Torque white-room runtime/DSQ import planning
src/dts89_validate.c        structural validators
src/dts89_anim.c            sequence keyframe/track/trigger planning
examples/example_roundtrip.c
tools/dts89_probe.c
tools/dts89_inspect.c
tools/dts89_selftest.c
tools/dts89_tscript_dump.c
tools/dts89_dsq_inspect.c
tools/dts89_probe.c
tools/dts89_emit_materials.c
tools/dts89_emit_constructor.c
tools/dts89_runtime_plan.c
tools/dts89_validate.c
tools/dts89_anim_plan.c
tools/dts89_tri_dump.c
```


## Probe example

```c
dts89_probe_result pr;
int rc;

rc = dts89_probe(file_data, file_size, &pr);
if (rc == DTS89_OK && pr.kind == DTS89_PROBE_KIND_DTS) {
    /* safe candidate for dts89_decode_shape */
}
```

CLI:

```sh
tools/dts89_probe player.dts
tools/dts89_probe run.dsq
tools/dts89_probe player.cs
```

## Suggested layering

```text
DTS / DSQ / materials.cs / constructor.cs
                |
              dts89
                |
           your editable IR
                |
     dts89 encoder + script writers
```

That keeps Torque-format weirdness isolated from higher-level import/export code.

## Phase 3.7: Fixed-point pose and CPU skinning (ZombiePack T3D1.1)

See [docs/POSE37_SKINNING_IMPLEMENTATION.md](docs/POSE37_SKINNING_IMPLEMENTATION.md).
New public interface `include/dts89_pose37.h`; single DTS sequence sampler
`dts89_animer_evaluate`, rigid parent-child pose matrices, skin palette,
linear blend skinning for vertices and normals. See `tools/dts89_pose37_export.c`
for a source-only, C89, caller-owned-buffer OBJ demonstration.

## v3.8 UV convention correction

See `docs/UV_ORIGIN_V38.md`. The image-row convention is now explicit in
`dts89_uv_texturer_get` and the pose OBJ export. Do not flip native DTS UVs
in-place. Test with `tools/dts89_uv38_selftest`.
