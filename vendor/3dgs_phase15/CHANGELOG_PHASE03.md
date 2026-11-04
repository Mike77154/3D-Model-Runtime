# Changes v02 → v03

- Six independent new modules, total 32 C89 library modules.
- New `anim/` subsystem: Q16.16 math, key loading, hierarchy, rigid skinning, clip frame lookup.
- MDL7 key parser extracted from MDL7 frame parser.
- Fix: MDL7 frame vertices no longer overwrite bind bone attachment with vertex index.
- Safety: explicit weighted deformer rejection; matrix and key count bounds; invalid bone IDs and duplicate key rejection; posed-vertex results restored from bind on each call.
- CLI: optional frame argument to export animated rigid pose as OBJ.
- New synthetic two-joint matrix/pivot test and robust pose mutation coverage.
- Limits: no weighted LBS, real MDL7 file variants not tested, matrices assumptions provisional.
