# RE4 Classic Codec v2.0-WR-GCPS2 — integration note

## Scope and provenance

Independent C89 readers based on study of user-supplied references; there is **no
Capcom/Nintendo/CRI game code** or original copyrighted game data in this package.
The two GameCube source packages share identical decompiled `include/model.h`,
`include/tpl.h`, `src/game/model.cpp` and `src/game/texture.cpp`; the
CubeEvil4 Android port additionally documents GX command-stream interpretation.
The PS2 tool adds a distinct LE BIN/segment description. These references are
not redistributed inside this package.

Reference archive names:

- `CubeEvil4-Android-master.zip`: GameCube BIN part layout, indexed GX
  primitives, vertex packing, GX/TPL texture layout, and source-port examples.
- `re4-master.zip`: GameCube resource/skeleton structure corroboration.
- `RE4-PS2-BIN-TOOL-main.zip`: 0x50 PS2 BIN header, 16-byte material
  records, node/segment traversal, VIF block lengths, alternate 24-byte
  vertex layouts and PS2 winding rules.

Warning: the CubeEvil4 / re4-master CC0 scope excludes reconstructed original
engine code. RE4-PS2-BIN-TOOL is MIT. This implementation was written anew and
is independently licensed under this package's existing LICENSE.

## New files

```
include/re4c3d/re4_gcps2.h        GC and PS2 public interface
model/re4_gc_bin_fifo.c           GC BIN part headers, GX FIFO, unpack vertex attributes
model/re4_ps2_bin_segments.c      PS2 BIN, bones, materials, VIF segments, packed vertices, faces
examples/re4_probe.c             detects GC / PS2 BIN before original UHD probe
tests/re4_gcps2_test.c            synthetic resource streams and malformed input checks
```

### GameCube path

1. `re4_gc_bin_read()` validates version signatures and BE section bounds.
2. `re4_gc_bin_polygon()` exposes a part's FIFO bytes and material/texture IDs.
3. `re4_gc_gx_decode()` handles supported GX VAT0 indexed commands:
   quads 0x80, triangles 0x90, triangle strips 0x98, triangle fans 0xA0,
   lines 0xA8, line strips 0xB0, points 0xB8, zero padding.
   All corners preserve **separate** position/normal/color/UV indices; strips
   alternate winding. Output uses caller-provided buffers and a dry-run mode.
4. `re4_gc_corner_q16()` provides position (s16 / 2^shift), normal (s8 /64
   or s16 /16384), signed UV (s16 /256), and RGBA8 channels. It only
   accepts colored BIN VAT mode (0x80000000), as the alternative mode's
   vertex descriptor is not fully verified. No model transformations/skin applied.
5. For texture pixels, **existing** GameCube TPL decoder remains available:
   `re4_tpl_read_gc()` + `re4_tpl_decode_image_rgba32()` in `re4_texture.h`.

### PlayStation 2 path

1. `re4_ps2_bin_read()` reads 0x30/0x0030 magic, 0x50 header, table offsets,
   bounding Q16 values and bonepair span; strict little-endian.
2. `re4_ps2_bin_bone()`, `re4_ps2_bin_node()`,
   `re4_ps2_node_segment()` traverse 16-byte bones, materials, 16-byte
   aligned node headers, optional weight-maps and VIF submesh chunks.
3. `re4_ps2_segment_vertex()` recognizes **both** 24-byte attribute orders,
   including scenario color mode. Original packed values stay available.
4. `re4_ps2_vertex_q16()` preserves original axis order and returns positions
   multiplied by a per-segment Q16 scale / 100; UV = packed signed / 255.
5. `re4_ps2_segment_weight()` exposes raw bone slot references, amount and
   Q16 weights without guessing skinning/parent transforms.
6. `re4_ps2_segment_triangles()` identifies strip faces with alternating
   winding and restart conditions, using caller-owned output.

## Minimal runtime-facing flow

```c
re4_gc_bin gc;
re4_gc_polygon part;
re4_gc_gx_counts counts;
re4_err err;

err = re4_gc_bin_read(input, &gc);
if (err == RE4_OK && gc.polygon_count != 0U) {
    err = re4_gc_bin_polygon(&gc, 0U, &part);
    if (err == RE4_OK)
        err = re4_gc_gx_decode(part.fifo, RE4_TRUE,
            gc.positions_count, gc.normals_count,
            gc.colors_count, gc.texcoords_count,
            0, 0U, 0, 0U, &counts);
    /* Allocate caller-owned corner/triangle arrays after checking counts. */
}
```

The binary loader owns neither files nor allocations. No `malloc`, `float`,
`double`, `stdin`, and no dependency from the three research ZIPs.

## What is NOT done

- No claims of verified compatibility across retail models: no original RE4
  GameCube/PS2 BIN or matching TPL samples came with these ZIPs.
- No full material shaders, GX TEV combiner, opaque/alpha sorting, UV animation,
  bump-map blending, model morph, skinning application, or animation playback.
- No finished texture-binding implementation that automatically resolves
  BIN part diffuse IDs against a specific model's TPL container.
- No PS2 swizzled-TPL decode; the v1.9 texture codec still reports unsupported
  where applicable.  No geometry/model round-trip writer.
- No Win32 cross-compilation tested; gcc/clang checks here run on Linux.

## Validation

`make check`: original v1.9 tests + new standalone synthetic GC/PS2 tests.
`clang -std=c89 -Wall -Wextra -Werror -pedantic` tested.
`clang` AddressSanitizer / UndefinedBehaviorSanitizer tested for both test executables.
One existing FCV signed-left-shift UB found by UBSan was corrected by replacing
it with mathematically equivalent signed multiplication by 65536.

## Next milestone

Obtain original **legally held** paired `BIN` + `TPL` sample assets and compare
vertex counts, material slots, UV seams, skin weights and color channels
against independent renderers.  Then implement a full indexed runtime bridge
without assuming contiguous vertices or identical PS2/GC layouts.
