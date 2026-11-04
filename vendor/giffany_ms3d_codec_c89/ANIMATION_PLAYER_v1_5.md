# Giffany MS3D v1.5 — streaming animation with UV and materials

## Release status (2026-10-10)

Native player implemented in `gif_ms3d_player.c/.h` while retaining the v1.4 parser, normals, skinning, UV and MTL exporter. The new player consumes caller-owned, statically allocated buffers. ANSI C89, Q16.16, no malloc/free, no floating point or int64 in the runtime code.

### New live data flow

```text
MS3D parser
  -> group triangle/material binding (+ original corner UV)
  -> bind / inverse bind matrices, joint parents
  -> frame clock (seek / advance / loop / once)
  -> evaluate local keyframes and global transforms
  -> skinned positions and normals
  -> begin_frame(time, model)
  -> material(id, ms3d_material with texture path and alpha path)
  -> begin_triangle(group, material)
  -> corner(position, normal, native UV, vertex index, smooth group)
  -> end_frame()
```

**UV convention:** callbacks return original MilkShape `S/T`, origin at the top left of the image. The host may invert T once ONLY when needed by its image/upload convention. OBJ exports `V = 1 - T`.

`ms3d_player_prepare()` computes the duration as the maximum of the declared `total_frames/animation_fps` and the latest joint rotation/translation key time (some files include keys beyond the nominal duration). It computes normals, UV bindings and bind matrices once. `ms3d_player_advance()` takes elapsed **milliseconds**; `ms3d_player_seek()` takes absolute milliseconds on the source timeline. Set an interval with `ms3d_player_range(first_ms,last_ms,MS3D_PLAYER_LOOP | MS3D_PLAYER_ONCE)`. The interval must have `start < end` and lie within the model's total duration. ONCE clamps to the end, LOOP wraps to the start. Animation time uses Q16.16 seconds internally.

Use `ms3d_player_emit()` each render tick. It evaluates the hierarchy and weighted mesh, then streams triangle corners and their material associations into renderer callbacks. The renderer or host must decode images and perform draw calls. Texture bytes are not decoded inside the runtime.

### Minimal integration

```c
#include "gif_ms3d_player.h"

/* The game already parsed ms3d_model g_model. */
static ms3d_skin_pose g_pose;
static ms3d_skin_vertices g_vertices;
static ms3d_corner_normals g_normals;
static ms3d_uv_bindings g_binding;
static ms3d_player g_player;
static ms3d_player_sink g_sink;

/* Set g_sink.corner and g_sink.material to callbacks in your engine. */
int init_game_model(void)
{
    return ms3d_player_prepare(&g_player, &g_model, &g_pose,
            &g_vertices, &g_normals, &g_binding,
            MS3D_NORMAL_SMOOTH_WELDED);
}
int update_and_draw(unsigned long elapsed_ms)
{
    int rc=ms3d_player_advance(&g_player,elapsed_ms);
    if(rc!=MS3D_OK)return rc;
    return ms3d_player_emit(&g_player,&g_sink);
}
```

`g_model` is provided by your host application; the snippet is for integration, not a self-contained program. Hook `material(id,pointer)` to texture loading/caching and `begin_triangle/corner` to your graphics API. The callbacks are synchronous: do not retain pointers to transient corner structs after a callback returns.

### Build and test

```sh
make clean
make check CC=gcc CFLAGS='-std=c89 -O2 -Wall -Wextra -Werror -pedantic -Iinclude'
```

Sample playback + material/UV/mesh frame trace:

```sh
./examples/ms3d_player_probe zmb_hs.ms3d 350 6
```

Textured sequence via **v1.5 live player callbacks**:

```sh
./examples/ms3d_player_stream_obj zmb_hs.ms3d output/zombie 6 350 \
    --texture-root original_textures --smooth-weld
```

Creates `output/zombie_000.obj` ... `zombie_005.obj`, one MTL, and two copied, uniquely named texture files. Each OBJ stores one posed frame (the player itself retains time/rig/controls in memory). The output OBJ deliberately duplicates vertices **per triangle corner** to preserve split UVs and per-corner normals, so number of OBJ `v` rows is 3x triangle count. This does NOT add polygons or change any UVs.

### Corpus regression and QA

- GCC strict C89 compilation + six unit tests (legacy codec, extra sections, normals, skinning, UV, new player).
- 157 / 157 Night Of The Zombies models, 3 successive frames at 0/500/1000 ms: same triangles, exactly 3 corners per triangle, stable UV and material IDs for the first sampled corner. This is a **structural regression**, not animation-fidelity certification for all clips.
- `zmb_hs.ms3d`: 685 triangles / 2055 animated corners / 2 original materials and diffuse maps. 6 OBJ frames with valid MTL bindings, original images, and software rasterized GIF/MP4 previews.
- `zmb.ms3d`: 735 triangles / 2205 corners, 1 material, 6 OBJ frames.

### Known limitations

- Keyframe rotation is the pre-existing shortest-angle Euler interpolation from v1.4; quaternion SLERP and crossfade are not implemented. Some source animations may require a different interpolation order for parity.
- The optional runtime render backend still needs to load BMP/JPEG/etc and handle alpha/transparency, sampling, shader lighting and culling.
- Closed-bone matrix skinning and sample tests do not prove pixel-for-pixel parity with MilkShape. No clip event tags or auto clip splitting: use custom playback ranges.
- The game mesh openings and UV seams are preserved, never auto-filled.
- The code compiled and tests ran on this Linux environment. **Win32 MinGW32 native build not tested here** (32-bit libc headers unavailable for `gcc -m32`).
- Q16.16 seconds limits timeline representation to about 32767 seconds. The public API uses millisecond clocks, but sub-millisecond precision is not supported.

### Reference

Paul Bourke, MilkShape 3D format: https://paulbourke.org/dataformats/ms3d/index.html
MilkShape 3D 1.8.5 keyframe data specifies local Euler rotation keys in radians, XYZ order, and local translation keys in seconds: https://sappersblog.blogspot.com/2014/08/milkshape-3d-185-ms3d-file-format.html
