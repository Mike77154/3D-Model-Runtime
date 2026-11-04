# ModelRuntime89 Native 75 — IQM quaternion, hierarchy and CPU skinning patch

**Base:** `Modelo_Runtime_Experimental_Nativo_75_IQM_Frames_C89.zip`

**Scope:** Provider `MODELRT_CODEC_NATIVE_IQM89` (ID 101), exclusively. **Registered providers: 75 (unchanged).** Original 14 providers and other vendor sources are retained. GACKT-LANG and cooked artifacts are not used.

## Implemented

- Loads native IQM v2 bind transforms (translation XYZ, quaternion XYZW, scale XYZ) and native pose channels.
- Decodes quaternion components and authored frame scales directly from IEEE754 source bits using integer arithmetic; normalizes quaternions in fixed-point Q16.16.
- Composes local bone matrices into world matrices using the parent hierarchy. Computes inverse bind matrices from inverse local transforms, then produces per-frame skinning matrices `global_frame * inverse_bind`.
- Evaluates weighted CPU linear-blend skinning for triangle positions (up to four IQM influences per vertex), and transforms/normalizes vertex normals approximately.
- Exposes complete local TRS and global world 3x4 matrix through **`include/modelrt_native_iqm_pose89.h`**, keeping the common `modelrt_bone_view` layout ABI-compatible. The common bone visitor still reports local translation only.
- Rejects invalid skeleton parent indices and weighted bone indices, malformed pose channel masks and out-of-range frames; works in C89 without `malloc`, `float`, `double` or `long long` in this implementation. Arena allocations provide fixed capacity for per-bone matrices.

## Results verified in this build

| Test | Result |
| --- | --- |
| `make -j8 libmodel_runtime89.a` | PASS — `-std=c89 -pedantic-errors -Wall -Wextra -O2` |
| Synthetic IQM: 1 bone, 1 triangle, 90° Z rotation | PASS: (1,0,0) -> (4,65534,0) in Q16, expected (0,65536,0) |
| Synthetic quaternion XYZW | PASS: observed Z=46340, W=46341; expected ~46341 each |
| `mrfixit.iqm` | PASS: 2,988 triangles, 75 bones, 101 frames, 24 fps |
| `mrfixit.iqm` frames 0 vs 100 | PASS: skinned vertex changes, 93 quaternion components differ |
| `mrfixit.iqm` parent hierarchy | PASS: 72 joints demonstrate nontrivial global transforms |
| Invalid IQM pose mask | PASS: rejected |
| Assimp `InvalidMeshOffset.iqm` | PASS: rejected |
| Legacy IQM frame regression | PASS |
| FBX `box.fbx`, COLLADA `duck.dae`, 3MF `box.3mf`, MD2 `faerie.md2`, OFF `Wuson.off` | PASS: 12, 4,212, 12, 654, 3,732 triangles respectively |

## Constraints and limitations

1. Frame-based skinning uses **authored discrete frames**; time-based visits snap to a frame. Quaternion nlerp/subframe interpolation is still future work.
2. Normal transformations are an approximation for **nonuniform bone scales** (the code uses the forward skin matrix, not inverse-transpose).
3. Saturated Q16 arithmetic and fixed matrix capacity protect against integer overflow, but extreme geometry magnitudes can lose precision; additional adversarial fuzzing is needed.
4. This is a CPU-side visitor, **not an integrated GPU animation renderer**. Morph targets and attachments are not covered by this patch.
5. These are automated decoding and mathematical checks, not full visual parity certification for all IQM files.

## Reproduce

```sh
make native-iqm-skinning-smoke
# Optional external model/corpus tests, with paths supplied by the user:
make native-iqm-skinning-build
./tests/native_iqm_skinning89_regression \
  /path/to/IQM/mrfixit.iqm /path/to/corrupt_mask.iqm
make native-iqm-frame-build
./tests/native_iqm_frames89_regression \
  /path/to/IQM/mrfixit.iqm /path/to/IQM/invalid/InvalidMeshOffset.iqm
```

The source-only archive intentionally excludes compiled programs, generated fixture files and third-party corpus models.
