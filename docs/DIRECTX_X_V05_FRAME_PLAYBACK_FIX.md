# DirectX .x v0.5 dense-frame playback fix

The v0.5 parser can recover a dense integer-tick frame timeline even when an
asset omits `AnimTicksPerSecond`. The original runtime exposed frame metadata
but `modelrt_visit()` accepted seconds only, so unknown-rate clips remained on
frame 0 during viewer playback.

This revision adds:

```c
int modelrt_visit_animation_frame(modelrt_handle *handle,
                                  int animation_index,
                                  modelrt_u32 frame_index,
                                  const modelrt_visit_sink *sink,
                                  void *user);
```

`provider_x89` implements the call directly through
`xp_anim_sample_pose_frame()`. No FPS is invented by the runtime.

The Win32 viewer uses exact dense-frame visits for native clips that advertise
`MODELRT_ANIM_POSE_SAMPLER`. If `AnimTicksPerSecond` is authored, it drives the
playback cadence. If it is absent, the viewer uses a 30-fps *preview cadence*
only; source timing remains unknown. Left/Right steps exact frames.

Regression: `tests/directx_x_v05_unknown_rate_skin.x` has three recovered
frames and no `AnimTicksPerSecond`; visiting frame 1 produces the interpolated
middle pose.
