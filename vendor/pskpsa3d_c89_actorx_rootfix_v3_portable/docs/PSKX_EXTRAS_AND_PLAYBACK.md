# PSK/PSA v4 extras, bone maps, playback cursor

This package keeps the same hard constraints as the earlier versions:
C89, no heap allocation, no malloc/free/realloc, and no C float/double values in the decoder core.
Binary IEEE-754 float32 payloads from PSK/PSA chunks are decoded into Q16.16 integers.

## PSKX / extended chunks

The parser now recognizes common extended ActorX/PSKX chunk names used by tooling around UE Viewer and Blender importers:

- `EXTRAUVS0`, `EXTRAUVS1`, ...: extra UV sets, two float32 fields per wedge, stored as Q16.16.
- `VERTEXCOLOR`: four bytes per wedge, stored as RGBA.
- `VTXNORMS`: three float32 normal components, stored as normalized Q16.16 vectors.
- Alternate legacy chunk IDs observed in import scripts: `VTXW3200`, `FACE3200`, `REFSKEL0`, `RAWW0000`, and `RAWWEIGH` prefix.

The extension arrays are static and bounded:

```c
model.extra_uv_set_count;
model.extra_uv_counts[set];
model.extra_uvs[set][wedge_index];
model.vertex_color_count;
model.vertex_colors[wedge_index];
model.normal_count;
model.normals[index];
```

`VTXNORMS` may be emitted by different tools as point-style or wedge-style data. The normal skinning helper accepts both: if `normal_count == point_count`, normal index maps directly to the point index; otherwise it treats normals as wedge-indexed and resolves `wedges[n].point_index`.

## Bone map for PSA -> PSK compatibility

Some pipelines import PSAs against a mesh whose root bone name or case differs. v4 adds a static bone map:

```c
psk3d_bone_map map;
psk3d_build_bone_map(&model, &animset,
    PSK3D_BONEMAP_CASE_INSENSITIVE |
    PSK3D_BONEMAP_ROOT_FALLBACK |
    PSK3D_BONEMAP_ALLOW_MISSING,
    &map);
```

Then sample directly in model bone order:

```c
psk3d_sample_sequence_mapped_ex(&model, &animset, &map,
    seq, frame, t_q16, PSK3D_ROT_SLERP, &local_pose);
```

Missing bones fall back to the PSK bind pose only when `PSK3D_BONEMAP_ALLOW_MISSING` is enabled.

## Playback cursor

The cursor advances animation frames using `AnimInfoBinary.AnimRate` stored as Q16.16:

```c
psk3d_anim_cursor cur;
psk3d_cursor_reset(&cur, seq);
psk3d_cursor_advance(&animset, &cur, delta_seconds_q16, 1u);
psk3d_sample_sequence_slerp(&animset, cur.sequence_index,
    cur.frame_index, cur.frame_t_q16, &local_pose);
```

If `AnimRate` is zero or invalid, the cursor falls back to 30 FPS. Negative deltas are rejected because this runtime path is meant for simple forward playback on small targets.

## Normal skinning

For renderers that want lit skinned meshes without building matrices, v4 adds:

```c
psk3d_skin_normals_lbs_cached(&model, &cache, &global_pose, &skinned_normals);
```

This applies bind inverse rotation and current rotation, without translation, then weighted-normalizes the result. It shares the same `psk3d_skin_cache` used by point skinning.
