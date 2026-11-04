# EMD textured viewer patch notes

This patch adds a real EMD -> textured BMP preview path:

```text
load_emd()
build_piece_transforms()
assemble_world_vertices()
decode_tim()
project_vertices()
rasterize_textured_triangles()
save_framebuffer_to_bmp()
```

## Why the previous output exploded

The first textured attempt treated RE1 body-part start positions like flat absolute offsets and also used the wrong camera axis for the front view.
Classic RE human/zombie EMD meshes are segmented: each mesh piece is local to a limb, while section 0 contains the body-part start/pivot data. Those starts must be accumulated through a humanoid parent chain.

The current RE1 humanoid/zombie parent fallback is:

```text
0 root/reference
1 torso      -> 0
2 head       -> 1
3 R upper arm-> 1
4 R lower arm-> 3
5 R hand     -> 4
6 L upper arm-> 1
7 L lower arm-> 6
8 L hand     -> 7
9 R thigh    -> 0
10 R shin    -> 9
11 R foot    -> 10
12 L thigh   -> 0
13 L shin    -> 12
14 L foot    -> 13
```

## Build

```sh
make clean
make
```

## Run

```sh
build/emd_textured_viewer EM1111.EMD em1111_front.bmp front
build/emd_textured_viewer EM1111.EMD em1111_side.bmp side
build/emd_textured_viewer EM1111.EMD em1111_iso.bmp iso
```

## Current limitations

- RE1 humanoid/zombie bind fallback is implemented.
- RE2/RE3 need their own skeleton/animation section reader before this should be considered complete.
- No PSX fixed-point rotation frames yet; this is bind pose, not animated playback.
- Rasterizer is intentionally simple: affine UVs + z-buffer + BMP output.
