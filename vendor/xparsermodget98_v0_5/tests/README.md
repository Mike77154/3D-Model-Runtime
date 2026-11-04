# Regression fixtures

Packaged fixtures:

- `runtime_refs_text.x`: text references, animation target, ticks and skin header.
- `runtime_refs_binary.x`: equivalent binary path.
- `runtime_refs_tzip.x`: synthetic two-block MSZIP text stream with dictionary reuse.
- `runtime_refs_bzip.x`: synthetic MSZIP binary stream.
- `runtime_v03_binary_lists.x`: packed numeric-list flattening + WORD string terminator.
- `runtime_v03_tzip_standard.x`: standard `.x` MSZIP text framing.
- `runtime_v03_bzip_standard.x`: standard `.x` MSZIP binary framing.
- `runtime_v04_extensions_text.x`: VertexDuplicationIndices, DeclData, FVFData and unknown object passthrough.
- `runtime_v04_extensions_binary.x`: same v0.4 extension coverage through binary tokens.
- `runtime_v04_rig_uv_anim_text.x`: stable Frame IDs, mesh/frame binding, skin inverse-bind callbacks, raw tiled UVs, MaterialWrap/MeshFaceWraps, and legacy type-4 matrix animation semantics.

`tests/test_v04.c` additionally verifies:

- `xp_open_io()` over memory;
- a provider that returns at most three bytes per read;
- standard bzip through the memory provider;
- extension callback values;
- unknown/custom object callbacks;
- object budget exhaustion;
- structured diagnostic retention.

`tests/test_v04_semantics.c` additionally verifies stable Frame node IDs, mesh owner-frame IDs, mesh/bone/inverse-bind association, UV tiling preservation, wrap hints, AnimationSet clip summaries, legacy type-4 matrix normalization, DirectX matrix composition helpers and the OpenGL V-flip helper.

- `runtime_v05_multiclip_poses_text.x`: two AnimationSets with sparse position and legacy matrix keys for frame-count, interpolation, matrix-hold, random-access and full pose-stream regression.

`tests/test_v05_poses.c` verifies count-only sizing, caller-owned capture, multiple clips, recovered frame counts, per-node frame poses, sparse S/R/T interpolation, sparse matrix hold behavior, random-access frame sampling and whole-database pose streaming.

Run:

```sh
make clean
make CFLAGS='-std=c89 -Wall -Wextra -Werror -pedantic -O2'
make test-v05
```
