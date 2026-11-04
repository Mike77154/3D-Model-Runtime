# DTS89 v3.7 — pose evaluator and skinning

## What changed

Using the user's Torque3D source as a **behavioral reference**, these modules now implement a single active DTS animation thread, local bone pose evaluation, parent-to-child global transforms, a per-mesh skin palette and CPU linear blend skinning of positions and normals. All arithmetic in the shipped C sources is Q16.16 and 32-bit integers. Bit patterns of source binary32 values remain as `dts89_u32` solely to decode the DTS binary format.

### Module map

- `dts89_animer.c` — keyframe cursor and public single-thread animation evaluation entry point.
- `dts89_posefeamw.c` — Quat16 unpack, shortest-arc quaternion *normalized linear* interpolation, global affine transforms, parent composition.
- `dts89_mesher.c` — bone palette `global[node] * inverseBind[bone]`, linear blend vertex and normal CPU skinning. Normal skinning assumes no non-uniform scale/shear.
- `dts89_uv_texturer.c` — UV per model vertex (unchanged).
- `dts89_texturer_opener.c` — DDS/BMP/PNG **header probe only**, not complete pixel decoders (unchanged).
- `dts89_pose37_export.c` — CLI demonstration exporting OBJ with vertices, texture UVs, normals, triangulated faces and material references. Uses explicit files, never standard input.

### Process used as behavior reference

- `Torque3D-development/Engine/source/ts/tsAnimate.cpp`, `TSShapeInstance::animateNodes`: default pose, keyframe samples, parent propagation.
- `Torque3D-development/Engine/source/ts/tsTransform.cpp`, `Quat16::getQuatF`: signed 16-bit quaternion mapping to [-1,+1].
- `Torque3D-development/Engine/source/math/mMath_C.cpp`, `m_quatF_set_matF_C`: **Torque's quaternion matrix convention**, which differs in off-diagonal signs from many textbook examples.
- `Torque3D-development/Engine/source/ts/tsMesh.cpp`, `TSSkinMesh::updateSkinBuffer`: transforms[nodeIndex[bone]] multiplied by inverseBindMatrix, then weight accumulation over influences.

This is a clean implementation of that algorithm; it does not contain copied Torque3D source.

### Test command

```
make clean
make CFLAGS='-std=c89 -O2 -Wall -Wextra -Werror -pedantic-errors -Iinclude'
./tools/dts89_selftest
./tools/dts89_pose37_selftest
./tools/dts89_pose37_export Zombie.dts reference.obj -1 0
./tools/dts89_pose37_export Zombie.dts attack.obj 0 32768
```

`sequence_index=-1` means default pose, position Q16 in [0,65536]. Example `sequence_index=0` corresponds to the first embedded sequence, `Attack1` in the tested `Zombie.dts`.

### What was tested

ZombiePack T3D1.1: all six DTS files, default pose and all eleven embedded sequences per model (66 half-time evaluations). The LOD sampler retained the expected 1461 triangles on Zombie, 4872 on MegaZombie. Geometry movement between poses confirmed in the generated OBJ files. Additional deterministic synthetic matrix and skin tests passed. UBSan sampled one model and the pose test.

### Explicit limitations — NOT Torque 1:1 yet

1. Interpolates quaternions by shortest-arc **nlerp**, not the reference engine's trigonometric slerp. Fixed-point results will differ for larger angular differences.
2. Implements only **one normal, non-blended sequence** at a time; multilayer animation, blended animations, additive threads, masks, transitions and multiple sequence priority handling remain future work.
3. Animated **scale** tracks (uniform/aligned/arbitrary) are rejected with `DTS89_ERR_UNSUPPORTED`, rather than silently producing corrupt poses.
4. Normals assume rigid rotation; scaling and shearing would require an inverse-transpose normal matrix.
5. External DSQ decoding and import *planning* exist in the prior library, but the phase-3.7 pose evaluator currently samples **embedded sequences** already resident in the DTS shape; fully joining standalone DSQ tracks still needs implementation.
6. Demonstration OBJ exports skinned vertices and normals at a sampled instant, not GPU vertex buffers, shaders, tangent-space normal mapping or a continuously running animation.
7. DDS/BMP/PNG decoder module still **probes headers only**. Texture PNGs in the separate demo were converted offline from the original DDS assets by the external test harness; the C89 codec does not yet decode their pixels.
8. The accompanying four-pose PNG/GIF is an **external Python test-renderer** using per-pixel UV texture sampling and depth buffering from the actual exported OBJ data. It is **not** the C89 codec rendering by itself; it does not reproduce Torque shaders, lightmaps, vertex effects or a complete engine runtime.
9. The 32-bit fixed-point format bounds the dynamic range and introduces quantization. Saturation, supported sequence types and caller buffer capacities must be checked by the host.

The correct next stage is to implement scale tracks and slerp-compatible integer interpolation, plus an actual textured renderer and bone-animated runtime bridge.
