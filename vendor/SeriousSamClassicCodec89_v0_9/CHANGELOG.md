## v0.9 — reflejos y especular controlados por el modelo
- `ss89settings` lee `WTEX/DFNM/FXTS/FXTR/FXTB` con validaciones.
- `ss89env` calcula dos proyecciones esféricas distintas, reflexión (X/Z) y especular (X/Y), en aritmética entera.
- El presenter interpola coordenadas por vértice e interpreta color de superficie y alfa de mapas; nunca fuerza capas no declaradas.
- `ss89present --settings Modelo.ini --assets-root Assets` resuelve y convierte los `.tex` asociados automáticamente.
- Nuevas regresiones sintéticas C89 y reales opcionales de Blade y Kleer.
- Equivalencia visual completa con el Serious Engine original NO afirmada.

## v0.8 — 8-bit indexed normals and FMIP LOD selection

- New C89 Gouraud Q15 reader with separate, locally generated normals table derived from the supplied engine sources (no GPL data embedded).
- AFVX and legacy AV16 indexed normals now decoded and interpolated in `ss89scene`, exported as OBJ `vn`; AV17 continues to use H/P.
- Parse original 32 FMIP factors; ignore garbage/NaNs in unused slots. New `ss89_choose_mip` API and `--auto-mip-factor` CLI.
- Regression: all 390 original MDL resources in SE1_10.gro parsed successfully; Mental 8-bit interpolation, Kleer 16-bit unchanged and LOD0/LOD3 comparison validated.
- No new unsupported shader claims; FXDT and scene/world-dependent lighting still require separate work.

## v0.7
- Normales H/P AV17 decodificadas y exportadas como OBJ vn, interpolación Q15 normalizada entre keys.
- CPU presenter con shading basado en normales, filtro bilineal, alfa configurable, ordenado de transparencia, capas optativas de reflexión/especular y fade de entidad.
- CLI para LOD y frame de textura; test material sintético y prueba real Kleer RUNLOOP x10.
- Consulta MODEL_MATERIAL_V07.md para límites y comandos.

# Serious Sam Classic Codec 89 — changelog

## v0.2.0 — Serious Engine 1: tested on original assets

- Fixed the `DFNM` resource filename marker in `.mdl` patch slots and legacy patch paths.
- Verified the MDAT loader against all **390** real `.mdl` files inside the supplied `SE1_10.gro` (370 V010, 16 V009, 2 V008 and 2 V007).
- Added an independent **streaming `.gro` ZIP/DEFLATE** reader; CRC32 and verified 69 byte-exact random extractions.
- Added raw texture `.tex` readers for **TVER 3/4**, including old 16-bit RGBA4444/RGB packed formats, newer raw RGB/RGBA and PPM/PAM output; parsed **724** real textures (procedural textures metadata-only).
- Added `.bm` MESH v11/v12 reader; recovered static vertices, UVs, triangle indices, surfaces; exported 2 original skeletal meshes to OBJ.
- Added `.skl` SKEL v6 structure inspection and `.ba` ANIM v14 envelope inspection (synthetic regression fixtures; no original animation/skeleton files in supplied archive).
- Added `.ani` ADAT inspector for 3 genuine standalone animation files.
- Added text previews for `.smc`, `.amc`, `.sha` and ASCII SKA format files.
- Added `ss89resources.h` skeletal mesh static-buffer C89 getter APIs.
- Extended native C89 builds to three CLIs and added synthetic GRO, MESH, SKEL, ANIM, TEX and ADAT tests.

NOT INCLUDED: full skeletal animation playback, decoding procedural TEX effect graphs, `.wld` world parsing, original binary writer, or materials/shaders reconstruction.

## v0.1.0 — original MDAT reader

- Read frame animations, UVs, surfaces and MIPs for `.mdl` V002–V010.
- Q16.16 interpolation-ready vertex geometry and OBJ export.
- Synthetic tests only in v0.1.

## v0.6 – model FX and multi-creature renderer fixes
- Mapped real TFE/TSE model rendering stages against Croteam code.
- Added independent ISO C89 fixed-point auxiliary bump/detail, haze, fog, simple ground shadow and triangle-following decal demo modules.
- Added `ss89probe` to inspect real model material stage flags.
- Added single model export through `ss89scene --model ... --texture ...`.
- Fixed v0.5 shared `ss_part_00` texture collision across different scenes.
- Corrected haze depth range and widened camera autocomposition for large models.
- Added self-contained numerical and framebuffer regression tests; pass only where stages actually work.
- Non-equivalences (FXDT, original bump tangent frames, exact projected shadows and decals, specular/reflection, original fog/haze context) are documented.
