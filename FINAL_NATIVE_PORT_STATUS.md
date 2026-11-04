# ModelRuntime89 Experimental — audited native completion checkpoint

## What's actually implemented

This checkpoint builds on the 70-provider baseline and **registers five additional native readers** (75 total), without a GACKT-LANG route or cooked intermediate file.

| ID | Source vendor / format | What is now functional | Important limitation |
| ---: | --- | --- | --- |
| 97 | mot89: `.mot` / `.env` | Parse and sample native LightWave motion channel keys. Source data exposed via accessor. | Does not produce meshes, skeletons or externally bind to an LWO object yet. |
| 98 | step89: `.step` / `.stp` / `.exp` | STEP entity/B-rep reader and native model context. | Does not tessellate B-rep surfaces into renderable triangles. |
| 99 | scn3d: `.scn` | TrueSpace / simple-volume streaming decoder, visitor emits triangle callbacks. | Positive test covers simple volume; SceneKit and Godot `.scn` are not supported by the vendor parser. |
| 100 | usd3d: `.usd`, `.usda`, `.usdc`, `.usdz` | Original USD decoder linked with native callback triangulation; simple USDA triangle tested. | USDC/USDZ and complex composition/materials are not yet certified; scope limited by upstream decoder. |
| 101 | IQM v2: `.iqm` | Strict ISO C89 direct reader; 2,988 triangles and original skeleton records for `mrfixit.iqm`. UV/normal/byte skin arrays exposed; read-only source bytes accessor. | Native IQM v2 frame sampling now exposes **local bone translations** for authored dense frames; rotation and scale values stay in the original bytes because `modelrt_bone_view` has no quaternion fields. Mesh triangles remain bind-pose (no CPU/GPU deformation); full skinning and some vertex array formats remain unsupported. |

**No additional 3D provider is registered for** `prj_c89_lib`: this is a coordinate reference system parser, not a model reader. It is now linked and offered via `modelrt_prj89_summary()` with caller-provided workspace.

**No duplicate BVH reader** is registered for `motion_bvh_poses`. Two previously integrated BVH readers remain available. The optional Pose/Retarget vendor fails `-std=c89 -pedantic-errors` due to `long long`; it remains source-only and is *not* called during import.

## Verified results (these were actually executed)

- Full library built under `-std=c89 -pedantic-errors -Wall -Wextra -O2`.
- Registered providers: 75.
- Native fixtures: MOT motion / STEP entities / SCN simple volume (12 triangles) / USDA triangle (1 triangle) all visited successfully.
- PRJ WGS 84 WKT parsed; summary returned GEOGCS, datum and ellipsoid.
- Both valid and malformed fixtures checked for MOT, STEP, SCN and USD; malformed ones rejected.
- Assimp corpus: `mrfixit.iqm` = 2,988 triangles; `InvalidMeshOffset.iqm` rejected.
- Baseline regressions: `box.fbx` = 12, `duck.dae` = 4,212, `box.3mf` = 12, `faerie.md2` = 654, `Wuson.off` = 3,732 triangles; malformed 3MF property index rejected.

These tests establish limited decoding and API compatibility, **not complete visual fidelity, animation accuracy or production certification**.

## Architecture and restrictions

- ModelRuntime89 continues to use `modelrt_open_file()` / `modelrt_visit()` and provider-owned native structures.
- New code uses ISO C89, integer/fixed-point and runtime-managed bounded arenas.
- No GACKT language, no mandatory cooked `.gmesh/.ganim/.gscene`, no heap APIs or floating-point calculations in the new adapters.
- The generic runtime provider registry is now sized for 112 providers; no existing IDs renumbered.
- The final distribution contains sources and tests, not compiled objects or model assets.

## To complete before claiming comprehensive coverage

1. Complete IQM quaternion/world-pose composition and skinned vertex deformation; local translation sampling already implemented and tested against 101 frames of `mrfixit.iqm`.
2. Port optional `motion_bvh_poses` retargeting math away from 64-bit extensions, without registering a duplicate BVH importer.
3. Implement actual STEP CAD tessellation; strengthen USDC/USDZ/OpenUSD scene graph and materials handling.
4. Exercise SCN trueSpace binary and complicated USD with independent, diverse corpus.
5. Resolve previously reported incomplete providers (RenderWare DFF no positive corpus; Ogre binary .mesh; and other documented failures). This checkpoint does not claim those issues repaired.

## Reproduce the small native fixture tests

From the source root:

```sh
make libmodel_runtime89.a
cc -std=c89 -pedantic-errors -Wall -Wextra -O2 tests/generate_native_final89_fixtures.c -o /tmp/gen_native89
mkdir -p /tmp/modelrt_fixtures && (cd /tmp/modelrt_fixtures && /tmp/gen_native89)
cc -std=c89 -pedantic-errors -Wall -Wextra -O2 -Iinclude tests/native_final89_regression.c libmodel_runtime89.a -lm -o /tmp/native_final89_regression
/tmp/native_final89_regression /tmp/modelrt_fixtures/simple.mot /tmp/modelrt_fixtures/simple.step /tmp/modelrt_fixtures/simple.scn /tmp/modelrt_fixtures/simple.usda
```

External Assimp corpus assets are not redistributed in this source-only ZIP.
