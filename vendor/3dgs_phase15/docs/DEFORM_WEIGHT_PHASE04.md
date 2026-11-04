# Weight and deformer vendor boundaries

```
           public GS3D API
                 |
    vendor generation router
        / |  |  | \
      A4 A5 A6 A7 A8     (each has its own deformers.c + weights.c)
                 |
      on-disk MDL signature
        /  /  \  \
      MDL3 MDL4 MDL5 MDL7 (each has deformers.c + weights.c)
        \  |   /    |
       legacy frame    MDL7 deformer parser + bone hierarchy
             \       /
          shared Q16.16 weight store/pose engine
```

## MDL3
- Entire-vertex animation frame (byte-packed)
- `GS3D_DEFORM_VERTEX_FRAME` with implicit frame weight 65536 and bone sentinel 65535.
- No explicit weight list is read from file.

## MDL4
- Entire-vertex animation frame (byte/word packed).
- Historical documentation describes packed vertex bone tags and bones as **future expansion**.
- No invented weighted bone transform is performed.

## MDL5
- Same legacy morph layout in this importer, separate skin payload formats.
- No explicit bone influence decoding is claimed.

## MDL7
- Group header bytes 1 and 2 advertise deformer count and maximum per-vertex weights.
- Experimental layout: after skins, UV, triangles, vertex list and frame blocks, parse
  group-owned deformer records, each: header(16) + elements(28 + weights*8).
- Bone type=0; element index references bone; weight index references group-local vertex.
- Finite weights in [0,1] converted from IEEE754 bits to Q16.16 with no FP operations.
- Per-vertex sum > 1.015625 rejected; positive sums normalized to 65536 with last-bit
  correction to the largest contributor. Bone indices validated against global skeleton.
- Pose: `worldAnimated * inverse(restBind)` from Phase 03 then weighted LBS.
- **Not yet confirmed with genuine MDL7 exporters**: layout order, bind convention and
  influence semantics. The 3DGS community thread describes uncertainty about the format.

## Source separation

- `src/formats/mdlN/` is the binary-format specialization point.
- `src/vendors/aN/` is the engine-generation specialization point.
- `src/weights/` provides shared bounded fixed-capacity stores and blend calculations.
- `src/legacy/` and `src/mdl7/` are independently linked reader paths.

## Validation status

- All format/vendor hooks compiled with GCC/Clang C89.
- MDL7 controlled fixture: vertex `(2,0,0)` with 0.5/0.5 bone influences returns
  `(4.5,4.5,5)` at the test pose.
- Truncation and mutation smoke tests checked 6,231 inputs without detected sanitizer failures.
- The fixture is synthetic and does NOT establish compatibility with every MDL7 file.
