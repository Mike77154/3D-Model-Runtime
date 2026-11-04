# Giffany MS3D Codec C89 v1.7 — SLERP + Crossfade + Transparency Passes

**Additive continuation of v1.6**; no legacy importer, normals, UV, materials, animation or OBJ exporters removed.

### New API

```c
#include "gif_ms3d_player.h"
#include "gif_ms3d_render.h"

/* After ms3d_player_prepare(...): */
ms3d_player_rotation_mode(&player, MS3D_ROTATION_QUAT_SLERP);
ms3d_player_range(&player, 0UL, 1000UL, MS3D_PLAYER_LOOP);
ms3d_player_crossfade_to_range(&player,1000UL,2000UL,400UL);
ms3d_player_advance(&player,16UL);
ms3d_player_emit(&player,&sink); /* skinned positions + normals + UV + materials */

/* Renderer translates these hints into actual GPU state: */
ms3d_render_material_pass(&model,0,&material_pass);
ms3d_render_build_queue(&model,&bindings,&vertices,camera_forward,&queue);
```

A clip here is a **time range** assigned by the host; the MS3D binary does not store named clip records. `ms3d_player_blend(player, other_time_ms, weight_Q16)` mixes any two poses of the same rig manually. `ms3d_player_blend_clear(player)` disables the blend.

### CLI with new SLERP mode

```sh
make CC=gcc CFLAGS='-std=c89 -O2 -Wall -Wextra -Werror -pedantic -Iinclude' check
./examples/ms3d_player_stream_obj data/zmb.ms3d preview/zmb 6 350 \
    --slerp --smooth-weld --texture-root data
./examples/ms3d_v17_corpus_probe data/zmb.ms3d
```

### Tests and documentation

GCC and Clang strict C89 tests passed, plus independent 157/157 model probe: 3 SLERP poses and an optional blend pose per model. **Visual parity with native MilkShape** and **native MinGW32 builds** are not yet demonstrated. Rendered previews are produced by a separate software QA rasterizer consuming the export outputs. See [`docs/MilkShape3D_185_Research_v1_7.md`](docs/MilkShape3D_185_Research_v1_7.md).

**Transparency**: C89 module generates alpha-test / alpha-blend flags and a depth-based draw order when requested; actual decoding, alpha composition and drawing remain renderer responsibilities.

---

# Giffany MS3D Codec C89 v1.6 — Animation rotation update

**Additive release based on v1.5.** All previous imports, UV/material callbacks, skinning, smooth shading and OBJ/MTL tools remain available. The new `gif_ms3d_quat` module adds configurable quaternion **NLERP** for articulated animation. No malloc/free, floating-point types, or 64-bit integer dependency in the C89 runtime.

**Main API:** `ms3d_player_prepare(...)` (now defaults to quaternion NLERP), `ms3d_player_rotation_mode(&player, MS3D_ROTATION_EULER)` for legacy behavior, `ms3d_player_rotation_mode(&player, MS3D_ROTATION_QUAT_NLERP)` to restore new behavior; `ms3d_player_advance`, `ms3d_player_emit`. `ms3d_skin_evaluate` retains original Euler semantics; `ms3d_skin_evaluate_mode` allows mode selection.

**Audit:** `./examples/ms3d_spec_audit path/to/model.ms3d` outputs extension subversions, parsed size and keyframe anomalies. See [research notes](docs/MilkShape3D_185_Research_v1_6.md).

**Build:** `make CFLAGS="-std=c89 -O2 -Wall -Wextra -Werror -pedantic -Iinclude" check` in an environment with GNU Make/GCC. For Windows see `build_msys2_mingw32.sh` (native Windows execution not yet tested).

**Textured animation sequence (new NLERP default):**

```bash
./examples/ms3d_player_stream_obj model.ms3d output/zombie 4 350 \
  --texture-root textures --smooth-weld --nlerp
```

`--euler` reproduces the old v1.5 angular interpolation. Blender/MilkShape comparison is still required for animation parity.

---

# GIFFANY MS3D Codec v1.5 — Runtime Animation + UV + Materials

**NEW:** `ANIMATION_PLAYER_v1_5.md` documents the real-time player API with per-frame texture material and UV events, playback control, integration sample and tests.

# Giffany MS3D Codec C89 v1.4 — UV + Material Binding

MilkShape 3D `.ms3d` importer, normals, rigid inverse-bind skinning and textured geometry bridge in **strict ANSI C89**. Designed for vendor integration in Win32 / Linux engines.

## Constraints

- `-std=c89 -Wall -Wextra -Werror -pedantic`; signed Q16.16 data; no `float`, `double`, 64-bit integers or dynamic heap allocation in the codec.
- All large model, vertex, normal and material buffers are statically provided by the host.
- No dependency on graphics APIs; the shader / image decoding stays with the host.
- Parser, skinning, normal generation and original UV coordinates from v1.3 remain intact.

## What v1.4 fixes

1. Original corner `S/T` coordinates are preserved in callbacks, without rounding to vertices, clamping or an extra flip.
2. `ms3d_uv_obj()` emits OBJ `vt(S,1-T)`: transform once at the OBJ boundary; the runtime uses native MS3D UV.
3. `ms3d_uv_build_bindings()` resolves each triangle through the authoritative group triangle-index list (not just the possibly stale `triangle.group_index` field).
4. The previously **untextured animated OBJ exporter** now writes `mtllib` / `usemtl`, `map_Kd` images, optional `map_d`, normals, and UV per corner for every face.
5. Textures are copied into the destination OBJ folder, with model-specific names to prevent same-name collisions across exports or material subfolders.
6. Both static and animated output are material-correct. Missing images are reported, rather than silently remapped to a different image.
7. Renderer callbacks now support `ms3d_emit_uv_corners` and `ms3d_emit_skinned_uv_corners` to return corrected material membership with each emitted corner.

## Build

```sh
make clean
make check CC=gcc CFLAGS='-std=c89 -O2 -Wall -Wextra -Werror -pedantic -Iinclude'
```

MSYS2 MinGW32:

```sh
./build_msys2_mingw32.sh
```

The MinGW32 build script is included, but was **not executed on Windows** during this release.

## Textured static OBJ / MTL

```sh
./examples/ms3d_obj_export data/zmb.ms3d preview/zmb.obj \
  --smooth-weld --texture-root data
```

## Textured animated OBJ / MTL

```sh
./examples/ms3d_anim_obj data/zmb.ms3d preview/zmb_500.obj 500 \
  --smooth-weld --texture-root data
```

Both commands write `.obj`, `.mtl` and uniquely named copies of the texture image(s) alongside the OBJ. Other normal modes (`--source`, `--smooth`, `--flat`) continue to work. The OBJ file contains a single **pose snapshot** and cannot preserve a skeletal animation timeline.

To resolve images with `--texture-root`, supply a folder containing the referenced image files. If not found there, the model's own directory is checked. For a full game asset tree, keep the original `data/` / `weapons/<id>/` layout with each model and textures together.

## Direct renderer / engine

```c
#include "gif_ms3d_uv.h"
static ms3d_model model;
static ms3d_corner_normals normals;
static ms3d_uv_bindings bindings;
static ms3d_skin_pose pose;
static ms3d_skin_vertices vertices;

/* After parse / normal construction / pose evaluation: */
ms3d_uv_build_bindings(&model, &bindings);
ms3d_emit_skinned_uv_corners(&model, &pose, &vertices,
                             &normals, &bindings, &sink);
```

`ms3d_draw_corner.uv` contains native MilkShape `S/T`: `S` moves right, `T` moves downward in a top-row-first bitmap. Your texture loader should set the image origin accordingly, or invert V once if the backend expects bottom-left UVs. **Never invert V both when uploading the image and when sampling the mesh.**

Use `corner.material_index` to select the material `model.materials[index]` and its `texture` path. Use `corner.uv` per corner rather than recreating UV per shared vertex. The callback supplies per-corner normals, skinned vertex position, and original source vertex index. The host decides image decoding, filtering, alpha blend / alpha test / lighting.

## Regression and corpus

`make check` tests all legacy functionality and the new UV material lookup, including a triangle where the header group index is intentionally wrong.

The 157-model **Night Of The Zombies** corpus was exported through both static and animated OBJ pipelines: **314/314 cases passed**, with original triangle count and three UV/normal records per face. The game material references include 534 texture slots; 533 resolve to original archived image names, and one (`pinet2_mask.tga`) was not found in the supplied LHA. Visual QA: `zmb` with `ZOMBIE1.JPG`, `zomb` with `corpse1.bmp`, and `zmb_hs` with two texture materials.

The comparative PNG comes from a Python software rasterizer, which is a QA tool only; it reads the C89-exported OBJ/MTL and textures. This is **not** proof of pixel-identical rendering compared with MilkShape itself or the original game engine.

## Known limits

- No automatic hole filling, retopology, texture repainting or UV unwrapping. Open geometry remains an independent issue.
- No texture decoder in the C89 codec: images are referenced/copied; the engine handles BMP/JPEG/TGA/PNG decoding.
- No full animation clip retargeting, quaternion SLERP, transparency sorting or advanced GPU shaders in v1.4.
- The C89 runtime API keeps all native UV; image-space origin is handled by the host renderer.

## Modules

- `include/gif_ms3d_codec.h`, `src/gif_ms3d_codec.c`: parser, fixed point.
- `include/gif_ms3d_normals.h`, `src/gif_ms3d_normals.c`: normals / smooth shading.
- `include/gif_ms3d_skin.h`, `src/gif_ms3d_skin.c`: animation skeleton and skinned positions.
- **`include/gif_ms3d_uv.h`, `src/gif_ms3d_uv.c`**: per-triangle material lookup, UV convention, corrected runtime emission.
- **`include/gif_ms3d_mtl.h`, `src/gif_ms3d_mtl.c`**: optional OBJ MTL writer / texture filename collision-proof copy.
- `examples/ms3d_obj_export.c`, `examples/ms3d_anim_obj.c`: standalone CLI export examples.
- `tests/test_ms3d_uv.c`, `tools/regress_uv_corpus.py`: new tests.

See `CHANGELOG.md`, `SPEC_NOTES.md` and `UV_MAPPING_REPORT_v1_4.md` for details.
