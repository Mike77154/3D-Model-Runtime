# RTCW V19 + 3DGS Fase15 integration in Model Runtime 75

- RTCW V19 is **unchanged**. Its 119 files in `vendor/rtcw_natsuki_v19/` have identical SHA256 hashes to the uploaded V19 archive; provider 105 remains registered once.
- 3DGS Fase14 has been **replaced** by Fase15 in `vendor/3dgs_phase15/`; no second vendor or provider was introduced. Provider 106 still recognizes `MDL3`, `MDL4`, `MDL5`, `MDL7`.
- The new Fase15 implements optional MDL7 matrix file-space skinning (`GS3D_POSE_FILE_SPACE`). Older hierarchy mode remains the default for backward compatibility. This can be controlled through `modelrt_3dgs_set_pose_mode(MODELRT_3DGS_POSE_FILE_SPACE)` before calling `modelrt_visit_animation_frame`. Use `MODELRT_3DGS_POSE_HIERARCHY` to restore legacy behavior; queries use `modelrt_3dgs_get_pose_mode()`.
- The vendor and runtime are non-reentrant because the codec holds a process-global model and pose mode.
- Fase15 file-space mode is **experimental**, not yet frame-by-frame validated against Acknex.
- The runtime retains other providers unchanged. Generated test fixture assets are NOT included in the source ZIP.

## Test commands

```sh
make -j2 libmodel_runtime89.a
make gs3d-phase15-runtime-test
(cd vendor/3dgs_phase15 && sh scripts/check_phase15.sh)
```

Windows MinGW32 has not been validated in this build.
