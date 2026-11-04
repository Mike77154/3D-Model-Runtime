# hl2modelib v0.9.0 animation / skin runtime push

This pass focuses on the runtime path a game engine needs after the codec/compiler has produced or imported Source model data.

## New runtime path

```text
SMD animation frames
        ↓
fixed-point pose sample
        ↓
fixed-point local/world 3x4 matrices
        ↓
inverse bind + current pose
        ↓
weighted CPU skinning over hl2m_engine_model
        ↓
sequence events and selected LOD for engine playback
```

## Added APIs

- `hl2m_smd_frame_range()`
- `hl2m_sample_smd_pose_lerp()`
- `hl2m_sample_smd_pose_time()`
- `hl2m_pose_matrices_from_pose()`
- `hl2m_pose_inverse_bind_from_pose()`
- `hl2m_engine_cpu_skin()`
- `hl2m_event_cursor_init()`
- `hl2m_event_cursor_advance()`
- `hl2m_select_lod_from_distance()`

## Probe

`hl2m_animskin_probe` verifies:

- animation frame range detection,
- half-frame pose sampling,
- inverse bind construction,
- fixed-point CPU skinning,
- loop-aware sequence event cursor playback,
- QC-derived runtime LOD selection,
- expanded parity bits.

## Scope note

This is not a byte-exact Source runtime clone. It is deterministic, fixed-capacity engine support intended to let a non-Source engine consume the decoded/compiled data without pulling in Source SDK runtime dependencies.
