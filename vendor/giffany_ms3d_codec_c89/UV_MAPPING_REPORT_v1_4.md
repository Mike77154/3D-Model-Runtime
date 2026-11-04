# MilkShape 3D v1.4 — UV/material inspection report

## Diagnosis

The core MS3D parser already read three `s[]`, three `t[]` and three vertex normals per triangle correctly. The v1.3 **animated** OBJ example wrote geometry/UV/normals, but omitted material identifiers and the MTL file. A viewer could consequently display untextured models even with valid UV.

The new paths implement:

```
MS3D triangle UV (S, T), native image top-left
   ├─ engine runtime: callback per corner (S, T), material lookup from group face lists
   │    └─ host texture loader/renderer handles image orientation and alpha
   └─ OBJ/MTL export: vt (S, 1-T) + usemtl + map_Kd (+ map_d when present)
        └─ original image copied under model/material specific name
```

Never flip `T` twice; never use one texture for every group. Do not merge UV at vertex indices, because a shared MS3D vertex can have different UV in different faces. Preserve UV values outside [0,1] for repeat textures; the renderer decides wrapping.

## Models visually verified

| Model | Triangles | Materials | Image(s) | Results |
|---|---:|---:|---|---|
| `zmb.ms3d` | 735 | 1 | `ZOMBIE1.JPG` | Facial features, vest, jeans and wounds in corresponding islands at t=500 ms |
| `zomb.ms3d` | 1505 | 1 | `corpse1.bmp` | Flame/flesh atlas applied to animated mesh at t=500 ms |
| `zmb_hs.ms3d` | 685 | 2 | `ZOMBIE23.jpg`, `bl.jpeg` | Two authored materials survive group face assignment |

A diagnostic scene intentionally simulated the *wrong* image-space V flip; that image is not the old importer output. The correct render uses original authored UV and one explicit conversion at OBJ output.

## Tests performed

- GCC strict ANSI C89 build, `make check`: parser + skeleton + normals + UV/material lookup tests passed.
- All 157 original MS3D corpus files exported to static OBJ and 500 ms animated OBJ: 314/314 cases passed, all original face counts preserved, 3 `vt` and 3 `vn` per face.
- Source asset scan: 534 referenced texture fields across 157 models, 533 resolvable by original archive basename, 1 missing `./pinet2_mask.tga`; 131 selected original asset entries were recovered with correct byte sizes.
- Windows MSYS2 MinGW32 native build not run in this Linux environment.

## Scope / pending

This changes material/UV assignment, **not** triangulation. The geometrical openings noted in zmb in v1.3 remain for separate inspection. Texture preview renderer is a Python QA rasterizer reading C89 OBJ/MTL; runtime C89 contains no image decoder or GPU renderer. Transparency sorting, SLERP and all-sequence playback remain follow-up work.

### External specifications

- https://paulbourke.org/dataformats/ms3d/index.html
- https://sappersblog.blogspot.com/2014/08/milkshape-3d-185-ms3d-file-format.html
