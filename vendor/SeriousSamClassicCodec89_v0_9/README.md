# SeriousSamClassicCodec89 v0.9

**Nuevo**: reflexión/especular por normales, proyección esférica C89, autodescubrimiento FXTS/FXTR desde los `.ini` binarios `WTEX`. Véase [MODEL_REFLECTION_SPECULAR_V09.md](MODEL_REFLECTION_SPECULAR_V09.md).

# SeriousSamClassicCodec89 v0.9

**New:** AFVX/AV16 user-sourced Gouraud normals (256-direction table) and automatic LOD selection from original FMIP thresholds. See [MODEL_NORMALS_LOD_V08.md](MODEL_NORMALS_LOD_V08.md). The table is generated from your engine ZIP and is not redistributed with the source.

# Serious Sam Classic Codec89 v0.9

**NOVEDAD:** [MODEL_MATERIAL_V07.md](MODEL_MATERIAL_V07.md) describe renderizado de Kleer Skeleton/Boneman, normales por vértice AV17 interpoladas, materiales, transparencias ordenadas, textura bilineal, specular/reflection condicionales, LOD y test de Sam real.

# SeriousSamClassicCodec89 v0.6 — Model FX / criaturas / presenter C89

**Lee primero [MODEL_FX_V06.md](MODEL_FX_V06.md)** para descripción real de bump, detail, haze, fog, sombras, decals, qué se implementa y qué se aproxima, pruebas con Headman/Kleer/Werebull/Scorpman y ejemplos de comandos.

# SeriousSamClassicCodec89 v0.4 -- UV, materials and AMC assembly

**New in v0.4:** independent C89 `ss89scene` scene assembler for Serious Sam AMC models, MEX UV scaling based on actual `.tex`, corrected external OBJ V orientation, per-surface MTL material assignment and alpha textures. The v0.3 skeletal skinning tool `ss89pose` is preserved. Start with [UV_MATERIALS_AMC_V04.md](UV_MATERIALS_AMC_V04.md).

# Serious Sam Classic Codec 89 — v0.2

**Independent, strict ISO C89, static-memory, integer-only codec components for Serious Engine 1 / Serious Sam Classic: The First Encounter.**

Developed against the source layouts supplied by the user (`Serious-Engine-master.zip` and `SeriousSamClassic-main.zip`) and tested against the real sample resource archive `SE1_10.gro` included in the former. No original game meshes, textures, code or `.gro` assets are redistributed in this ZIP. Test fixtures are synthetic.

## What's implemented (honest coverage)

| Resource | Tool | Capabilities | Tested on original assets |
|---|---|---|---|
| `.mdl` / `MDAT` V002–V010 | `ss89dump` | Mesh, surfaces, UVs, multiple MIPs, vertex animation frames (`ADAT`), fixed-point reconstruction, OBJ export | **390/390** MDL files (370 V010, 16 V009, 2 V008, 2 V007) |
| `.gro` / ZIP | `ss89gro` | List filenames; extract stored/DEFLATE entries, CRC32 integrity checking | **69/69** selected files byte-equal to ZIP reference extractor |
| `.tex` / `TVER` 3/4 | `ss89resource` | Read metadata; decode raw first-mip RGB/RGBA frames, export PPM (`P6`) and PAM (`P7`, with alpha) | **724/724** accepted; some effects are metadata only |
| `.bm` / `MESH` 11/12 | `ss89resource` | Skeletal mesh geometry, UV0, triangles, LOD, surface/weight/morph counts; OBJ and public static-buffer getters | **2/2** original `.bm`, version 11 |
| `.skl` / `SKEL` 6 | `ss89resource` | Read LOD and bone names/parents/bind positions | Synthetic SKEL6 fixture (no original SKEL provided) |
| `.ba` / `ANIM` 14 | `ss89resource` | Parse clip metadata and envelopes, validate/skip keyframe payloads | Synthetic ANIM14 fixture (no original BA provided) |
| `.ani` / `ADAT` | `ss89resource` | Parse names, speed and frame index sequences | **3/3** `.ani` files |
| `.amc` (subset) | `ss89scene` | Assemble nested attachment hierarchy, materials/UV from MDL/TEX, OBJ+MTL | **SeriousSam.amc**, 4 authentic pieces |
| `.smc`, `.sha`, ASCII `.am/.as/.aa` | `ss89resource` | Text inspection, not compiled shaders / animations | Text fixtures |

**SKA is the Serious Engine skeletal-animation subsystem**, not a universal `.ska` file. Its binary layouts are `MESH`, `SKEL` and `ANIM`; disk filenames may use `.bm`, `.skl`, `.ba` respectively, with `.smc` as a model script.

### Important limitations

- The `.ba` data can be parsed; `ss89pose` produces linear-blend skinned OBJ frames from `.bm + .skl + .ba` in synthetic tests. Real-world skeletal triplet visual validation and morph playback are pending.
- No decoding of the native `.tex` procedural/effect textures (`FXDT`) into final images: they are inspected as metadata. Animated raw-frame texture sequences work using `--frame`.
- `.mdl` frame normals are preserved as encoded bytes; **not** yet regenerated as full vectors; topology is triangulated using triangle fans (concave faces need additional logic).
- `.bm` exporter uses the first UV map (if any), does not apply skinning or external material/shader files.
- `.amc` has a limited semantic scene assembler (`ss89scene`) supporting nested attachments, animation choices and textures. `.smc` / `.sha` remain inspectors, and worlds `.wld`, `.ecl`, `.tbn`, fonts, audio are not implemented.
- `GRO` supports the classic ZIP entries with compression methods **0 (stored)** and **8 (DEFLATE)**. Encryption, ZIP64 and multi-disk archives are not supported. Do not pass untrusted multi-gigabyte archives.
- Reading, extracting and exporting are supported; **no write-back of original `.mdl`/`.tex`/`.bm` files**.
- Fixed point Q16.16 may saturate on extreme geometric coordinates. The maximum fixed capacities are configurable in headers; per-model time and memory budgets are enforced.
- The 390/390 result means **all model records parsed successfully**, not that every frame is visually identical to the official Serious Modeler. A final visual renderer comparison is still recommended.
- This release was **compiled/tested on Linux with C89 warnings as errors**; the MSYS2 MINGW32 build script is supplied, but it was not executed against a real Windows environment here.

## Build

From the unpacked project root, on Windows **MSYS2 MinGW32**:

```sh
./build_msys2_mingw32.sh
```

On Linux/POSIX using `cc`:

```sh
./build.sh
python3 tests/run_tests.py
python3 tests/test_resources.py
```

The produced executables are `bin/ss89dump(.exe)`, `bin/ss89resource(.exe)`, `bin/ss89gro(.exe)`, `bin/ss89pose(.exe)` and `bin/ss89scene(.exe)`.

## Work with Serious Sam The First Encounter model assets

1. List `.gro` contents:

```sh
./bin/ss89gro SE1_10.gro --list
```

2. Extract one model and texture:

```sh
./bin/ss89gro SE1_10.gro --extract Models/Computer/Floor.mdl Floor.mdl
./bin/ss89gro SE1_10.gro --extract Models/Computer/Floor.tex Floor.tex
```

3. Inspect/extract 3D and texture image:

```sh
./bin/ss89dump Floor.mdl --mip 0 --frame 0 --obj Floor.obj
./bin/ss89resource Floor.tex --ppm Floor.ppm
./bin/ss89resource Floor.tex --pam Floor.pam
```

`.pam` keeps alpha transparency in four channels; `.ppm` exports three-channel RGB without alpha.

4. For skeletal mesh scripts, extract `.bm` and preview an `.smc` separately:

```sh
./bin/ss89gro SE1_10.gro --extract Models/SkaStudio/Lamp/Lamp.bm Lamp.bm
./bin/ss89resource Lamp.bm --obj Lamp.obj
./bin/ss89resource Lamp.smc
```

5. Read standalone model animations (no playback in this version):

```sh
./bin/ss89resource sample.skl
./bin/ss89resource sample.ba
./bin/ss89resource sample.ani
```

For animated MDL use `--frame N`, or use the `ss89_decode_anim_key(...)` C library API with the named ADAT animation index/key.

## Vendor integration

- `include/ss89.h`, `src/ss89.c`: independent C89 MDAT loader API. `ss89_open`, `ss89_select_mip`, `ss89_decode_frame`, `ss89_decode_anim_key`, `ss89_export_obj`, `ss89_close`. See original `README` source tests for usage.
- `include/ss89resources.h`, `src/ss89resources.c`: `ss89_resource(path,out,lod,frame)` plus skeletal static-buffer getters `ss89_ska_vertex_count`, `ss89_ska_triangle_count`, `ss89_ska_uv_count`, `ss89_ska_get_vertex`, `ss89_ska_get_triangle`, `ss89_ska_get_uv`. Positions and UVs Q16.16. Data is replaced on the next load; non-reentrant.
- `src/ss89gro.c`: standalone GRO/ZIP reader CLI, currently has `main` and must be extracted into a separate library translation unit before vendoring directly into the engine.
- `src/ss89resource_cli.c`, `src/ss89dump.c`: console frontends, not needed when linking the core loaders.

### C89 restrictions

- `-std=c89 -pedantic-errors -Wall -Wextra -Werror`
- No dynamic allocation (`malloc/free`), no float/double arithmetic and no 64-bit integers in the C decoders.
- No `zlib`, `libpng`, C++ or external engine runtime.
- Size and index checks; ZIP extraction validates output CRC32.
- `int` must be 32 bits and `short` 16 bits (the original `.mdl` library enforces this at compile time).

### Source schema references (for independently confirming layouts)

- https://github.com/Croteam-official/Serious-Engine/blob/master/Sources/Engine/Models/Model.cpp
- https://github.com/Croteam-official/Serious-Engine/blob/master/Sources/Engine/Ska/Mesh.cpp
- https://github.com/Croteam-official/Serious-Engine/blob/master/Sources/Engine/Ska/Skeleton.cpp
- https://github.com/Croteam-official/Serious-Engine/blob/master/Sources/Engine/Ska/AnimSet.cpp
- https://github.com/Croteam-official/Serious-Engine/blob/master/Sources/Engine/Graphics/Texture.cpp
- https://github.com/Croteam-official/Serious-Engine/blob/master/Sources/Engine/Base/FileName.cpp

Original Serious Engine material is **GPL-2.0**. These decoders are a newly authored implementation of documented layouts and contain no copied engine code; assess the applicable licenses and the user's asset permissions before redistribution or commercial incorporation.

## v0.4 UV / AMC scene assembler

See [UV_MATERIALS_AMC_V04.md](UV_MATERIALS_AMC_V04.md) for engine-source audit, C89 scene CLI, known limitations and tests.

## v0.5 animations/material preview
See [ANIMATION_RENDER_V05.md](ANIMATION_RENDER_V05.md) for independent parent/child animation clocks, phase synchronization, optional one-shot clips, blend state mapping, and the new integer-only `ss89present` preview tool.

## v0.6 – model FX test backend / multi-creature audit

See [MODEL_FX_V06.md](MODEL_FX_V06.md) for actual behavior and limitations. Adds auxiliary bump/detail stages, fog/haze preview, contact shadow, triangle-following decal preview, material flag inspector, generic `--model`/`--texture` scene export, and namespaced atlas files so multiple characters no longer share `ss_part_00`. Changes were exercised on Sam, Headman, Boneman/Kleer, Werebull, Scorpman, Beast, Walker, Gizmo, Health Pill and Rolling Stone.
