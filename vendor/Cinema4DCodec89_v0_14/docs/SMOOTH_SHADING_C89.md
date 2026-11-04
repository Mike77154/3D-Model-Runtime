# Crease-aware smooth normals (C89)

- `src/c4d89_smooth.c` is a native mesh-adjacency/normal generator.
- No C99, `malloc`, `free`, `float`, `double`, 64-bit arithmetic or SDK.
- Vertex position input is Q16.16; normals output signed Q10.
- Default `cosine_q10=587`, approximately 55° hard-edge threshold.
- Only adjacent faces with compatible orientation and matching native
  polygon-selection region are blended.
- `c4d89_emit_smooth_faces()` streams per-face, per-corner normals into
  a user-provided game-engine callback. It may be used directly by a presenter.
- `c4d89_export_obj_smooth()` creates diagnostic OBJ with `vn`, `vt`, `g` and
  `f v/vt/vn`. OBJ is NOT an intermediary required by the runtime.
- The 55° threshold is a new generic option, not a native readout of the
  angle from Cinema 4D Phong tag. Next version may parse native Tphong angles.
- Scratch is **static**; this helper is not reentrant/thread-safe. Use a
  user-owned decoder state in a future multi-threaded integration.

Normal limitations: hard edges and UV seams require examining C4D Phong break
and normal tags to perfectly match Maxon's look. Current result is a
stable, credible smoothing implementation for game-engine integration,
not pixel-identical reproduction of Cinema 4D.
