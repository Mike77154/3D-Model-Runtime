# SeriousSamClassicCodec89 v0.4 — UV, surfaces, AMC assembler

## Audited against both supplied repositories

1. `Serious-Engine-master/Sources/Engine/Models/RenderModel_View.cpp` — `PrepareModelForRendering` copies each `ModelTextureVertex::mtv_UV` to surface vertex coordinates. The diffuse pass normalizes with the **loaded texture's** `CTextureData::GetWidth()` and `GetHeight()` in **MEX**, not pixels.
2. `SeriousSamClassic-main/SamTFE/Sources/Engine/Models/RenderModel_View.cpp` — the same rule for diffuse mapping. `Texture::GetPixWidth()` is a shifted dimension, not the scale denominator for model UVs.
3. `Serious-Engine-master/Sources/Engine/Models/Model_internal.h` — `MappingSurface` contains shading type, translucency and rendering flags, including `SRF_INVISIBLE` (bit 3) and `SRF_DIFFUSE` (bit 5).
4. `SeriousSamClassic-main/SamTFE/Sources/Engine/Models/Model.cpp` and `Serious-Engine-master/Sources/Engine/Models/Model.cpp` — `GetAttachmentPlacement` uses three vertex indices: `center`, `front`, `up`; normalizes an orientation basis and applies relative placement.
5. `Serious-Engine-master/Sources/Engine/Graphics/Texture.cpp` and `SeriousSamClassic-main/SamTFE/Sources/Engine/Graphics/Texture.cpp` — `.tex` stores original MEX width and height; first mip controls pixel dimensions. For Sam's `Glasses`, native model's mapping height is 512 MEX whereas the actual `Glasses.tex` height is 1024 MEX.

## Root issue found in visual preview

The C89 codec exported the raw model V value to OBJ, and the previous software presenter sampled `1 - V` on a top-origin image. The actual Sam atlas needs a direct top-origin V sample, which is equivalent to **minus V in OBJ** for conventional bottom-origin texture mappings with repeat/wrap. OBJ UV export in `ss89.c` and `ss89scene.c` now uses negative V; the model parser still retains the untouched raw MEX coordinates.

The correction was tested against `SeriousSam.amc` and all 4 actual model parts. In the updated preview his shirt emblem, face and shoes land on the intended texture regions. This is not a pixel-for-pixel imitation of the original GPU renderer.

## New C89 scene tool

```
./bin/ss89scene Models/Player/SeriousSam.amc \
  --root PATH_TO_EXTRACTED_GAME_ASSETS --frame 47 --obj sam_run.obj
```

Or use AMC's root animation and `--key N` (rather than `--frame`) to select a key within its declared clip. The assembler parses nested `Attachment` blocks and paths tagged `TFNM`, resolves references, extracts textures (`.ppm` or `.pam`), computes parent-anchor transforms and emits a unified OBJ + MTL with per-surface material entries. It omits `PreviewOnly` attachments (the separate knife include in Sam's AMC).

The `.mdl` parser exposes attachment triplets and Q16.16 offsets instead of discarding them. The C89 assembler uses integer normalized vectors / fixed-point transform composition; no floating point, 64-bit integers, or heap. It validates missing references and unsupported attachment states.

### Important limitations

- Nonzero **relative Euler angles** in source attachment placements are identified but not yet calculated: an AMC child depending on one will fail clearly. The Sam `Player -> Body -> Head -> Glasses` chain uses attachment index 0 and works.
- Supported text AMC subset: nested `Attachment`, `Animation`, `Model`, `Texture`, `Name`, `PreviewOnly`; `Include` directives are skipped. Other actors may use more complex AMC fields; reject rather than silently claiming full compatibility.
- MTL materials approximate Croteam's diffuse colors and transparency flags; dynamic bump, reflections, specular passes, custom blend equations, decals, perspective-correct interpolation, and shaded normal lighting are not emulated.
- Root clip frames change; attached submodels currently remain at the first frame of their AMC-selected animation. Weapon animations and body movement synchronization need a higher-level scene animator.
- OBJ exporters use conventional negative V with repeating textures. For external software using a different origin/wrap convention, configure that viewer accordingly.
- The C89 core does not encode PNG; it emits portable PPM/PAM. A separate *optional* diagnostic script uses Python for render previews, not for the codec.

## Tests

From project root on Linux:

```
sh build.sh
python3 tests/run_tests.py
python3 tests/test_resources.py
python3 tests/test_skinning.py
python3 tests/test_amc_scene.py
```

On MinGW32 use `sh build_msys2_mingw32.sh` (cross-platform source, but only Linux compilation was executed here).

The original test assets supplied by the user are not bundled with the source ZIP. An optional local run over `SeriousSam.amc`, `Player.mdl`, `Body.mdl`, `Head.mdl`, `Glasses.mdl` and matching `.tex` data succeeded; no claim is made for arbitrary other actor hierarchies.
