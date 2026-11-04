# DirectX .x integration — xparsermodget98 v0.5

Vendor: `vendor/xparsermodget98_v0_5`

Provider: `DirectX .x / xparsermodget98 v0.5`

## Runtime contract

- `.x` remains native; no universal converted model is stored.
- `txt`, `bin`, `tzip`, and `bzip` stay supported.
- Frames keep stable parser IDs; SkinWeights resolve against those IDs.
- DirectX UV V is exposed to the OpenGL-facing view as `1-V`, without clamping tiled UVs.
- `xp_anim_pose` is captured in caller-owned runtime arena storage: nodes, clips, tracks, and source keys.
- Every clip exposes `first_tick`, `last_tick`, `frame_count`, `source_key_count`, and authored `AnimTicksPerSecond` when present.
- Dense frame `i` means native tick `first_tick + i`. No display FPS is invented.
- `modelrt_animation_frame_info_get()` exposes one dense frame/tick and its Q16.16 time when the source authored a tick rate.
- When all Animation targets resolve, `modelrt_visit()` samples the v0.5 pose database and skins the mesh using `matrixOffset * animatedBoneCombined`. Unresolved/ambiguous tracks remain visible but unbound.
- Matrix keys are held between authored ticks; S/R/T channels use the vendored v0.5 fixed-point sampler.
