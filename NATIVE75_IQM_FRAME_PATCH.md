# ModelRuntime89 Native 75 — IQM frame patch

This checkpoint is an additive **native C89** update of provider ID **101**. The number of registered providers stays **75**; no duplicates are added.

## Implementation

- Parses IQM v2 `poses`, `anims`, frame-channel bitmasks, authored frame words and clip ranges, with bounded offsets and stricter hierarchy validation.
- Decodes channel values using **32-bit integer arithmetic** without `long long`, `float`, `double`, heap allocation or intermediate GACKT assets. Preserves very small authored channel scales by multiplying the IEEE-754 mantissa and 16-bit source sample **before** Q16 rounding.
- Enables `modelrt_animation_count`, `modelrt_animation_info_get`, `modelrt_animation_frame_info_get`, `modelrt_visit_animation_frame` and time-based `modelrt_visit` when the native frame rate is a positive integer.
- Emits native IQM **bone-local translations** for an exact authored frame; original geometry and UVs remain visible in bind pose.
- Reports frame count, duration and an authored FPS only when exactly represented by an integer rate. Other rates still permit exact frame-indexed sampling.

## Validated on actual external corpus

- `IQM/mrfixit.iqm` from the user-supplied `assimp-test-models.zip`: **1 clip (`idle`), 101 frames, 24 fps, 75 bones, 2,988 triangles**.
- First root-bone Y position = **1922 Q16**, last = **1811 Q16**; observed delta = **-111 Q16**. The difference proves the two sampled frames are not identical.
- Frame 101 (past end) is rejected; `IQM/invalid/InvalidMeshOffset.iqm` is rejected.
- Full runtime compilation: `-std=c89 -pedantic-errors -Wall -Wextra -O2`.

## Limitations (do not overstate)

**Not full skinned animation yet.** `modelrt_bone_view` only has position, not quaternion or scale. Rotations, scales and original frame channels remain available in original source bytes via `modelrt_native_iqm_bytes89`; applying skin matrices and deformation will require another API/sampler stage. Frame interpolation is also not implemented: nearest authored frame is selected for time-based visits.

## Reproduce

```
make native-iqm-frame-build
./tests/native_iqm_frames89_regression /path/to/IQM/mrfixit.iqm /path/to/IQM/invalid/InvalidMeshOffset.iqm
```

No corpus files or compiled binaries are distributed in this ZIP.
