# SeriousSamClassicCodec89 v0.5 — animation tracks and fixed-function preview

## Actual behavior reconstructed from source and real Sam assets

The Classic `SeriousSam.amc` config declares **Player.mdl: STAND (animation 1)** and **Body.mdl: MINIGUN_STAND (animation 23)**. These are editor/assembly defaults, not a claim that all gameplay actions use the same animation. In the engine, model attachments and child animation objects have their own playback state. The parent's animated attachment positions are recalculated each render.

v0.5 maintains a *separate clip selector/clock per node*, and recalculates the entire hierarchy after interpolating every part's vertices. You can request coordinated cycles without forcing identical frame indices:

```
./bin/ss89scene Models/Player/SeriousSam.amc --root EXTRACTED_ASSETS \
 --root-clip RUN --body-clip NORMALWALK --sync-body --time-ms 250 \
 --obj Sam_frame.obj
```

`--sync-body` maps the *normalized phase* of Player.RUN (23 keys) to Body.NORMALWALK (24 keys), **not** an index-for-index match. This is an optional experimental pairing, **not** a discovered rule for which weapon/torso clip the original gameplay controller picks while running. Without `--sync-body`, both animated parts advance according to their own `seconds_per_frame`; without `--body-clip`, the AMC's static minigun stance remains active. With `--once` the selected clips stop at their final key instead of looping. `--frame`/`--key` still work for legacy static extraction.

The program exports animated, UV/textured OBJ/MTL and a `.fx89` material-state companion file. `--sequence DIR/prefix --count N --step-ms M` exports an animation sequence. Supported sequences are capped at 500 frames per invocation to avoid runaways.

## Material states

The added `ss89fx` component translates the official Serious Engine 1 model surface types to backend-independent states:

| Mode | Original GL-style blending | Depth writing |
|---|---|---|
| OPAQUE (0) | blending disabled | yes |
| TRANSLUCENT (1) | SRC_ALPHA, ONE_MINUS_SRC_ALPHA | no |
| ALPHAGOURAUD (2) | obsolete; approximate as translucent | no |
| ADD (3) | SRC_ALPHA, ONE | no |
| TRANSPARENT (4) | alpha test | yes |
| MULTIPLY (5) | ZERO, ONE_MINUS_SRC_COLOR | no |

Also exports diffuse, reflection, specular, bump/detail, fullbright/matte/flat, double-sided and fog/haze flags and source surface colors.

The `ss89present` **C89 integer-only CPU preview** interprets the diffuse map, basic flat/ambient illumination, diffuse tint, alpha testing, Z buffer, and the four blending classes. It consumes OBJ/FX89/PPM/PAM in a folder and writes a PPM image:

```
./bin/ss89present Sam_frame.obj Sam_frame.ppm 145
```

The presenter currently supports four-part Sam scenes, max 512x512 textures per part, nearest-neighbor UV texture wrapping, orthographic camera, and two-sided drawing. It does *not* yet reproduce original OpenGL/DirectX lighting equation, perspective correction, face culling, separate reflection/specular/bump/detail texture passes, normal-map transforms, fog/haze distance calculation, particles, decals, shadows, post effects, or GPU state interactions. Material flags for those modes are **parsed and preserved**; they are **not depicted** by the CPU preview. Neither source engine is linked: this is a standalone C89 reimplementation and a research preview, not binary- or pixel-identical to the original renderer.

## Origin conventions

Models keep the game's raw MEX coordinates; OBJ exporter emits standard bottom-origin V. A PPM/PAM image's rows have top origin, so the CPU presenter explicitly samples `1 - OBJ_v`. This matters: not doing so produced a black shirt and warped face even when the original `.mdl` reader was correct.

## Test coverage

Run:

```
sh build.sh
./bin/test_fx_api
python3 tests/run_tests.py
python3 tests/test_resources.py
python3 tests/test_skinning.py
python3 tests/test_amc_scene.py
```

Optionally with the user-provided game assets:

```
python3 tests/test_real_sam_sync_optional.py PATH/SeriousSam.amc ASSET_ROOT
```

This checks real 23-key and 24-key clip selection, separately advancing tracks, changed body frame with phase-sync, looping vs. one-shot behavior, attachment hierarchy and material-state sidecars. Windows MSYS2 MINGW32 build script is included but not executed in this environment.

## Next parity work (not completed)

1. Parse model `.ini` material layer paths: diffuse, reflections, specular and bump; resolve textures from GRO archives or extracted files.
2. Implement the exact multipass render order / texcoords / texture combine / depth function and front-face culling from `RenderModel_View.cpp`.
3. Replicate fog/haze, per-vertex lighting and specular, then shadows/particles/decals; compare reference captures frame by frame.
4. Add a real-time Win32 OpenGL, DX9, or double-buffered GDI presenter reading `.fx89` or the in-memory structs.
5. Connect gameplay state + weapon overlays so that torso clips are chosen by action; phase-matching walk cycles is not enough for gameplay parity.
