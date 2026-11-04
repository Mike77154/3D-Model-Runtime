# ModelRuntime89 Experimental — IQM Fractional Sampling + Normals

Date: 2026-10-12
Baseline: `Modelo_Runtime_Experimental_Nativo_75_IQM_Skinning_C89.zip`.

## Implemented

- **IQM v2 fractional time sampling** through the existing `modelrt_visit(handle, animation_index, time_q16_seconds, ...)` API. Sampling interpolates adjacent dense frames instead of snapping to whole frames. Clips clamp at the last frame. Explicit `modelrt_visit_animation_frame` continues to use exact integer frames.
- Local translation and scale use Q16.16 linear interpolation. Quaternion XYZW uses shortest-arc normalized linear interpolation (NLERP), not SLERP. Bone hierarchy and CPU linear-blend skinning are recomputed at each sampled time.
- New optional native accessor `modelrt_native_iqm_pose_time89()` returns interpolated local TRS and world matrix; original frame accessor is unchanged.
- Per-bone affine normal transformation uses the cofactor matrix (proportional to inverse-transpose) before weighted blending and renormalization. Supports positive, nonsingular scales permitted by the IQM parser.
- No new codec IDs; 75 providers remain registered. No GACKT-LANG or persistent cooked intermediate.

## Test commands

From the project directory:

```sh
make -j4 libmodel_runtime89.a
make native-iqm-interpolation-smoke
make native-iqm-frame-build native-iqm-skinning-build
./tests/native_iqm_frames89_regression valid_mrfixit.iqm invalid_offsets.iqm
./tests/native_iqm_skinning89_regression valid_mrfixit.iqm malformed_pose_mask.iqm
make native-iqm-interpolation-corpus-build
./tests/native_iqm_interpolation_corpus89 box.fbx duck.dae box.3mf faerie.md2 Wuson.off
```

The ZIP is source-only: external Assimp samples are NOT redistributed.

## Observed regression results

- Synthetic 24 FPS one-bone IQM: midpoint of a 0→90° rotation produces approximately (0.707,0.707,0) from the input vertex (1,0,0). Native midpoint pose quaternion verified; time past the clip end clamps to the final pose.
- Synthetic non-uniform scale (X2, Y1, Z1): normal (0.707,0.707,0) becomes approximately (0.447,0.894,0); skinned vertex (1,0,0) becomes (2,0,0).
- Assimp `mrfixit.iqm`: 2988 triangles, 75 bones, 101 frames; prior skeletal and malformed-input regressions pass.
- Cross-format regressions: FBX 12 triangles; COLLADA 4212; 3MF 12; MD2 654; OFF 3732.
- Build: `-std=c89 -pedantic-errors -Wall -Wextra -O2`, native runtime library.

## Known limitations

- Quaternion interpolation is NLERP rather than exact SLERP; very different keyframe orientations may exhibit non-constant angular speed.
- Frame rates in the current time-based sampler must be whole-number FPS (inherited from provider behavior).
- Numeric Q16.16 saturation bounds extremely large/scaled scenes. Nonuniform normal transforms use cofactor arithmetic; precision can degrade with extreme scaling.
- IQM GPU skinning, IK, crossfades, animation retargeting and texture/render parity remain outside this patch.
- Passing numeric geometry/pose tests does not demonstrate visually identical rendering against original engines.
