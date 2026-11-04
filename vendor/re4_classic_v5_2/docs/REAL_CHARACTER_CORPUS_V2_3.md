# RE4 2007 real-character PMD validation — v2.3

## Input corpus
Eight user-provided GCAX archives: `xfile.dat`, `em.dat`, `emjp.dat`, `rel.dat`, `etc.dat`, `evd.dat`, `op.dat`, `font.dat`.
Only input indexes and selected data are analyzed; no original game bytes are bundled with this source distribution.

### Locations

- `xfile.dat` -> `xfile\em\pl00\pl0000.pmd`: Leon jacket base with head geometry, 69 skeletons, 23 nodes, 26 meshes/materials, 10,974 vertices / 3,658 triangles.
- `xfile.dat` -> `xfile\em\pl00\pl0001.pmd`: hair accessory; `lh00.pmd` + `rh00.pmd` add hands.
- `xfile.dat` -> `xfile\em\em10\em1000.pmd`: Ganado Don José body, 1,830 vertices / 610 triangles, 5 meshes, 45 skeletons.
- `xfile.dat` -> `xfile\em\em10\em1001.pmd`: head, 1,305 vertices / 435 triangles.
- Companion TGA textures are in matching `xfile\em\pl00\` and `xfile\em\em10\` folders (also other duplicate texture namespaces).
- `em.dat` -> `em\pl00.dat`: indexed little-endian DAT with 133 entries: 25 BIN, 15 TPL, 66 FCV, 23 SEQ, 1 EFF, 3 empty extension slots.
- `em.dat` -> `em\em10.dat`: indexed little-endian DAT with 698 entries: 120 BIN, 75 TPL, 277 FCV, 223 SEQ, 3 EFF.

Note: FCV and SEQ counts are *resource entries*, not independently validated animation clips.

### What was actually verified

The original v2.2 PMD survey traversed the entire Leon PMD with `RE4_OK`. This update reads real PMD 64-byte vertices with Q16.16 attributes, validates indexed triangles, and exports Wavefront OBJ/MTL as a proof of available static data. Unit tests with synthetic corrupted data and the existing v2.2 regression tests pass in strict C89.

A separate *external diagnostic* renderer (Python/VTK) was used to generate textured preview PNGs, and a Python/trimesh exporter to create GLB samples. Neither external tool is part of the C89 codec. Static scenes are not animated and do not preserve bind/rest skinning hierarchies. The main program still needs a runtime adapter to bind textures, assemble parts, and evaluate FCV/SEQ animation.

## Build

```sh
make check
make re4_pmd_to_obj
./re4_pmd_to_obj pl0000.pmd leon_static.obj
```

The CLI emits `leon_static.obj` and `leon_static.obj.mtl`, referring to original TGA texture names; copy the associated textures next to the OBJ file. It uses fixed-size static file/arena buffers, writes OBJ through stdio (only in this CLI), and never uses heap allocation or floating-point types. The library API performs no file I/O.

## Unsupported/remaining

- Animation mapping, interpolation, body-piece attachment/selection and skinning.
- Direct GLB writing from the C89 library (GLB demo generated with an external diagnostic tool).
- Validating every PMD or every visual artifact from all versions (the corpus validated here is RE4 2007).
- Win32/MSYS2 cross-build not yet tested.
