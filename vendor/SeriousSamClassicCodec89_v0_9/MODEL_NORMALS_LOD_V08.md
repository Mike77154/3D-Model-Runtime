# Serious Sam Classic Codec89 v0.8 — Gouraud (AFVX/AV16) and FMIP

This release is **ISO C89, no malloc/free, no runtime float/double, no 64-bit integers**.

## Model-driven behavior implemented

- `AFVX` models (8-bit vertices) contain a one-byte **Gouraud normal index** per frame vertex.
- `AV16` stores 16-bit vertex coordinates but the same legacy one-byte normal index.
- `AV17` uses two bytes for heading/pitch. Its existing decoding is unchanged.
- Gouraud directions come from the original engine's `avGouraudNormals[256]` in `Sources/Engine/Models/Normals.cpp`. The codec reads a user-generated 1,536-byte Q15 table at runtime. It does not bundle GPL-derived lookup data.
- Animated vertex positions **and** vertex normals interpolate between keys. OBJ `vn` is exported where a valid normal source exists, then consumed by the CPU presenter.
- The original `FMIP` chunk stores 32 IEEE754 factors, but **only `mips` entries are meaningful**. Unused slots can contain arbitrary bytes, including NaNs. The parser validates active thresholds and ignores unused slots to avoid rejecting genuine models.
- The `ss89_choose_mip(model, factor_q16)` API implements Serious Engine's first-threshold-greater-than-factor selection; the new `ss89scene --auto-mip-factor INTEGER_Q16` command chooses a LOD based on those stored thresholds.

## Generate indexed normals from the engine ZIP you supplied

```bash
python3 tools/make_gouraud89.py --engine-zip Serious-Engine-master.zip ss89_gouraud.q15
```

Or, from an extracted copy:

```bash
python3 tools/make_gouraud89.py Sources/Engine/Models/Normals.cpp ss89_gouraud.q15
```

`ss89_gouraud.q15` is a **local generated resource** and is intentionally not in this source ZIP. It is not needed for `AV17` models like Kleer.

## Run real models (after extracting the GRO resources to your `assets` folder)

```bash
# Mental.mdl is AFVX and has 386 frames / 13 animation clips.
./bin/ss89scene unused.amc --root assets \
  --model Models/Enemies/Mental/Mental.mdl \
  --texture Models/Enemies/Mental/Mental.tex \
  --root-clip RUN --time-ms 250 \
  --gouraud-table ss89_gouraud.q15 --obj Mental.obj
./bin/ss89present Mental.obj Mental.ppm 145 --filter bilinear --lighting directional

# Kleer uses AV17, and the requested factor selects a LOD based on authentic FMIP.
./bin/ss89scene unused.amc --root assets \
  --model Models/Enemies/Boneman/Boneman.mdl \
  --texture Models/Enemies/Boneman/Boneman.tex \
  --root-clip RUNLOOP --time-ms 250 \
  --auto-mip-factor 458752 --obj Kleer_far.obj
```

`458752` is fixed-point Q16.16 representation of 7.0. **The camera projection's distance-dependent mip factor is not computed automatically by this command**; the caller supplies a precomputed factor. A full runtime should calculate Serious Engine's projection factor before calling `ss89_choose_mip`.

## Verified on genuine models

- AFVX `Models/Enemies/Mental/Mental.mdl`: 189 normals from indexed Gouraud table, 386 frames, 13 clips; RUN interpolation produced distinct vertex normals and an actual change in rendered lighting.
- AV17 `Models/Enemies/Boneman/Boneman.mdl`: 538 vertices. Selecting FMIP factor 0.0 yields LOD0 (893 triangles); factor 7.0 yields LOD3 (484 triangles). The Q15 Gouraud table does not alter its AV17 output.
- AFVX `Models/Enemies/Headman/Projectile/Blade.mdl`: 40 vertices, 33 frames, surfaces marked specular and reflection by their own flags. **Those material flags do not imply that the correct external reflection texture has been resolved.**
- Parsed all 390 original `.mdl` in `SE1_10.gro` after the FMIP validation fix.
- A synthetic AV16 header test exercises indexed versus H/P normal selection. **No genuine AV16 example was found** in the available GRO archive.

## Model-specific rendering policy

Only model-specified surfaces request bump, detail, specular or reflection. The presenter will not invent maps. Haze/fog/shadows/decals need world data or are explicitly preview-only; the codec does not claim Croteam-equivalent shaders. Dynamic renderer-side culling, texture-coordinate generators, GPU shader parity and texture FXDT remain future work.

## Licensing

The original Serious Engine source code is GPLv2. This project contains an independently authored decoder and a local data-conversion utility, not the 256-element lookup table or GPL source code. Generated lookup data must be handled according to the license of the input files.
