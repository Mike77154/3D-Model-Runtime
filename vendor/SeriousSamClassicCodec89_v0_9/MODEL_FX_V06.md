# Serious Sam Classic Codec89 v0.6 – model-level effects audit

## What v0.6 actually does

All game parsers and preview tools remain independent ISO C89: `-std=c89 -pedantic-errors -Wall -Wextra -Werror` and no C heap allocation, float/double, int64, GPU or zlib. The CPU preview presents *approximations of some original fixed-function effects*, NOT a replacement for the complete Serious Engine game renderer.

New modules: `include/ss89fxpasses.h`, `src/ss89fxpasses.c`, tests, and new stages in `src/ss89present.c`. Extra CLI `ss89probe` for auditing material flags. `ss89scene` now accepts a single model without AMC, using `--model RELATIVE.mdl --texture RELATIVE.tex`; original `.amc` assembly is still available.

**Critical v0.5 defect fixed:** all scenes previously wrote `ss_part_00.ppm/pam`, overwriting the atlas for other characters in the same directory. The scene exporter now generates `<OBJ_STEM>_part_00.ppm/pam`, preserves one material per source surface, and the presenter loads the matching atlas. It can still read old `ss_part_*` exports for compatibility. Also fixed an uninitialized haze-depth range and auto-framing that clipped large creature models.

## Original code inspected

Croteam `Serious-Engine-master/Sources/Engine/Models/RenderModel_View.cpp`:
- Around `RENDER BUMP LAYER` (lines 2063+): original OpenGL-only dual offset texture lookups along source tangent vectors (`mtv_vU`, `mtv_vV`), controlled by `SRF_BUMP`, model/surface bump colors and light/view direction.
- Around `RENDER DETAIL LAYER` (lines 2388+): uses `mo_toBump` as the detail atlas; the pass is only used when the model's bump rendering has not been executed and `SRF_DETAIL` is set; blend mode is `GFX_DST_COLOR, GFX_SRC_COLOR`.
- Around `RENDER HAZE LAYER`, `RENDER FOG LAYER` (lines 2853+, 2911+): calculated using world/camera fog/haze projection and lookup textures. They are not inherent `.mdl` values.
- `RenderShadow_View` (3060+) and `AddSimpleShadow_View` (3319+) require world light and a receiving plane. `RenderModel.cpp` also propagates shadows to attachments.
- `RenderPatches_View` within model rendering is marked `TODO` in the original code snapshot; world decals, geometry stickers and particles are separate. No `.mdl` polygon material flag magically carries a complete blood/bullet decal.

Also checked both `SeriousSamClassic-main/SamTFE/.../RenderModel_View.cpp` and its SamTSE counterpart. Both expose the corresponding model bump/detail/haze/shadow paths; they are not byte-identical to Croteam's 1.10 reference.

## Implemented preview stages and limits

| Feature | Code status | Limitations |
|---------|-------------|-------------|
| MDL mesh, UV/MEX, materials, animation | Functional from v0.5 | No engine-perfect lighting, sorting or clipping |
| Bump | Executed from a real auxiliary map only when `SRF_BUMP` is set; optional `--demo-bump` override | **Scalar two-lookup height difference approximation**, not the exact per-vertex tangent/light two-pass OpenGL bump of Serious Engine |
| Detail | Executed from auxiliary map on `SRF_DETAIL`; skipped whenever bump stage runs | Approximate `2 * diffuse * detail` channel blend. Source UV/stretch and model/surface tint not fully replicated |
| Haze | `--haze 0..255`, depth-attenuation pass | Demonstration with explicit density; not source world haze tables/params |
| Fog | `--fog 0..255`, camera-image height pass | Demonstration only; not exact world fog height/orientation |
| Shadow | `--shadow 0..255`, smooth contact ellipse below model | Placeholder receiving plane, not polygon projected shadows or original stencil/light falloff |
| Decal | `--decal` or `--decal-face N` selects an animated triangle and draws clipped mark | Screen-space decal center follows animated triangle; mark does not fully conform to 3D polygon or transfer to level surfaces |
| Specular/reflections | source flags are parsed in `.fx89` | Shaders and original environment textures STILL pending |
| Procedural texture effect FXDT | Recognized by texture decoder | FXDT rendering NOT supported. Lava Elemental's animated effect texture cannot yet be rendered authentically |

The v0.6 renderer is a **debug/test presenter** for model effects, not pixel-perfect Serious Engine. It does not promise equivalent level-dependent lighting, deferred effects, soft shadows, particles, full entity scripts, detail tint or tangents.

## Real original TFE tests

`SE1_10.gro` contains 390 `.mdl` entries including `ModelsMP`. `ss89probe` audited all 390; 12 have surfaces with a BUMP or DETAIL flag. Of these, 5 are in the original `Models/` TFE folder (remainder in `ModelsMP/`).

- `Models/Items/Health/Pill/Pill.mdl`: **1 BUMP surface**, with original `Pill.tex` and `PillBump.tex` (converted to auxiliary PPM for CPU test).
- `Models/ages/egypt/traps/rollingstone/stone.mdl`: **3 DETAIL surfaces**, with `stone.tex` and `detail.tex`.
- `Models/Enemies/ElementalLava/ElementalLava.mdl`: **8 DETAIL surfaces** and `Detail.tex`, but its diffuse `Lava04Fx.tex` has **FXDT**, currently not renderable by this decoder.
- `Headman.mdl`, `Boneman.mdl`, `Werebull.mdl`, `Scorpman.mdl`, `Beast.mdl`, `Gizmo.mdl`, and `Walker.mdl` successfully opened, exported and rendered with their corresponding `.tex` variants. These monsters' `.mdl` surfaces do **not** request BUMP or DETAIL in this package, so forcing those maps for a beauty preview would not reflect the original model state.
- `Scorpman` has **24 surfaces marked SPECULAR**, which still need a proper pass when we implement specular.
- A 12-sample Headman RUN sequence produces 12 distinct real-model OBJ frames and a GIF preview, with the original Kamikaze diffuse atlas.
- `SeriousSam.amc` and its four model parts still export and render using the namespaced texture scheme.

The real original assets used for these tests were extracted locally from **the user's uploaded `Serious-Engine-master.zip`**; originals are **not redistributed** in this source ZIP.

## Build (Win32 MSYS2 MINGW32)

```sh
sh build_msys2_mingw32.sh
```

or on Linux/POSIX:

```sh
sh build.sh
python3 tests/run_tests.py
python3 tests/test_resources.py
python3 tests/test_skinning.py
python3 tests/test_amc_scene.py
python3 tests/test_present_fx.py
./bin/test_fxpasses
```

## Commands: real Headman standalone

Extract `Headman.mdl`, `Kamikaze.tex` from your own `SE1_10.gro` into `D:/SeriousSamAssets` preserving the archive's directory structure.

```sh
./bin/ss89probe.exe D:/SeriousSamAssets/Models/Enemies/Headman/Headman.mdl
./bin/ss89scene.exe unused.amc --root D:/SeriousSamAssets \
 --model Models/Enemies/Headman/Headman.mdl \
 --texture Models/Enemies/Headman/Kamikaze.tex \
 --root-clip RUN --time-ms 120 --obj Headman.obj
./bin/ss89present.exe Headman.obj Headman.ppm 145 --haze 40 --shadow 150 --decal
```

`ss89scene` creates `Headman.obj`, `Headman.mtl`, `Headman.fx89`, `Headman_part_00.pam/ppm`. The presenter locates the atlas by the OBJ name, so it does not collide with other character exports.

## Commands: actual bump and detail materials

```sh
./bin/ss89resource.exe PillBump.tex --ppm PillBump.ppm
./bin/ss89present.exe Pill.obj PillBumpView.ppm 145 --bump --aux-map PillBump.ppm

./bin/ss89resource.exe detail.tex --ppm Detail.ppm
./bin/ss89present.exe RollingStone.obj StoneDetailView.ppm 145 --detail --aux-map Detail.ppm
```

`--bump`/`--detail` honor the source flags; `--demo-bump`/`--demo-detail` force a visual experiment on models without those flags. Explicit overrides must not be mistaken for historical source material behavior.
