# RE1MV coordinate + EDD/EMR animation fix pass

## Bug that made EM1111 look like a textured slab

The previous RE1 mesh loader read TMD vertices as raw signed 16-bit `(X,Y,Z)` values. RE1MV does **not** display RE1 EMD TMD vertices that way.

RE1MV `tmd.pas` reads each vertex word with `Calculate_Pos()` and `main.pas` renders vertices as:

```pascal
glVertex3f(Vr[ii, Pr[ii, i, n], 2],
           Vr[ii, Pr[ii, i, n], 1],
           Vr[ii, Pr[ii, i, n], 0]);
```

So the runtime display axis is `(Z,Y,X)`, and each coordinate is sign-flipped/scaled by `Calculate_Pos()`.

The bad loader mixed huge raw TMD coordinates with small RE1MV EMR pivots. That made animation look broken even when EDD/EMR frame lookup was landing on valid pose bytes.

## Fix applied

`src/giffy_recodec.c` now uses a RE1-specific TMD vertex path:

```text
file X,Y,Z words -> RE1MV Calculate_Pos() -> display X=Z, Y=Y, Z=X
```

Normals use the same RE1MV axis order for consistency.

## Animation table check

A no-heap/no-float diagnostic probe was added:

```sh
./build/emd_anim_table EM1111.EMD
./build/emd_anim_table EM1111.EMD --frames
```

For `EM1111(6).EMD`, it resolves:

```text
Bank A: EMR_A + EDD_A, EMD_Type=0x3C0006, poses=215, clips=12
Bank B: EMR_B + EDD_B, EMD_Type=0x410000, poses=645, clips=31
```

The frame stream is not guessed. It follows RE1MV `edd.pas`:

```text
first_stream = EDD[2]
clip_count   = first_stream / 4
clip entry   = { frame_count:u16, frame_start:u16 }
track record = { pose_index:u16, step:u16 }
pose         = EMR.animation_offset + pose_index * EMR.frame_size
```

## EM1111 validation result

```text
verts=352 normals=900 uvs=1944 faces=648 pieces=15
tris=648 quads=0 textured=648 invalid=0 degenerate=0 unused_vertices=0
status=ok
```

Bounds are now in RE1MV-sized coordinates instead of raw PSX word space:

```text
bounds_min: -5.160 -8.760 -3.550
bounds_max:  5.150  7.490  3.570
```

## Industrial note

`tools/emd_anim_table.c` is intentionally static-buffer based and contains no runtime heap allocation and no floating-point math.

The existing public `grc_model` and software textured viewer still use legacy `float` and heap-based arrays. This pass fixes the RE1 coordinate/animation decode and adds a strict fixed-buffer animation-table verifier, but it does not rewrite the entire renderer/model API into fixed-point arenas yet.

## v12 viewer presentation cleanup

The software BMP renderer now flips only the final bitmap presentation Y axis.
This does **not** change model coordinates or pose matrices; it just avoids a
vertically mirrored diagnostic strip because BMP scanlines/pixel coordinates grow
opposite to the OpenGL-style world Y used by RE1MV.

The diagnostic rotation conversion was also tightened to RE1MV's exact angle
normalization:

```text
(angle & 0x0FFF) / 0x0FFF * 360
```

That keeps the generated frame strips closer to the Delphi viewer instead of
using the nearby but slightly different `4096` denominator.

Additional visual artifact:

```text
docs/EM1111_ANIM_CLIP_ATLAS_FIXED.png
```

This atlas renders start/mid/end for every detected clip from both banks, using
the EDD clip table instead of hand-selected boundaries.
