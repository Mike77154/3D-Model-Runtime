# Modelo Runtime Experimental — Batch 8: 70 native providers

## Verified source code changes (this checkpoint)

- 65 genuine provider registrations inherited unchanged.
- 5 new direct native adapters compiled: **92 M3D**, **93 CSM**, **94 LightWave LWS**, **95 Poser text**, **96 RenderWare DFF**.
- No GACKT-LANG router, no GACKT cooked outputs, no `malloc` or `free` in new wrappers.
- All 65 vendored source packages from previous checkpoint kept, including archives not yet integrated.

## Proven defect repairs

- 3MF `mf3d_xml.c`: cross-reference triangle property IDs and per-group property entry indices, reject invalid out-of-range indices. `malformed_property_index_oob.3mf` now rejected; `box.3mf` stays 12 triangles.
- COLLADA adapter: increase bounded vertex arena capacity from **8192** to **16384**. `duck.dae` now visits **4212 triangles**, **8500 vertices** decoded; `box_nested_animation.dae` stays 12 triangles.

## Positive/negative corpus checkpoints

| File | Native ID | Result | Details |
|---|---:|---|---|
| `duck.dae` | 81 | PASS | 4,212 triangles |
| `box_nested_animation.dae` | 81 | PASS | 12 triangles |
| `box.3mf` | 91 | PASS | 12 triangles |
| `malformed_property_index_oob.3mf` | 91 | REJECT | Correctly rejects out-of-bounds material index |
| `cube_with_vertexcolors.m3d` | 92 | PASS | 12 triangles, zlib-packed source |
| `WusonBlitz0.m3d` | 92 | PASS | 3,732 triangles, zlib-packed source |
| `ThomasFechten.csm` | 93 | PASS | Optical motion, 1 animation; zero geometry by design |
| `DropoutBufferOverflow.csm` | 93 | REJECT | Invalid CSM correctly rejected |
| `move_x.lws` | 94 | PASS | Native timeline, external geometry not resolved |
| `move_xz_linear.lws` | 94 | PASS | Native timeline, external geometry not resolved |
| `native_poser_triangle.pp2` | 95 | PASS (synthetic) | 1 triangle |
| `invalid.dff` | 96 | REJECT | No positive `.dff` in supplied Assimp corpus |
| `faerie.md2` | 45 | PASS | 654 triangles, regression |
| `Wuson.off` | 40 | PASS | 3,732 triangles, regression |
| `box.fbx` | 1 | PASS | 12 triangles, regression |

## Do not confuse these claims

- **70** = registered providers, **not 70 fully certified formats**.
- The 14 originally pending source folders have **5 direct adapters added**, leaving **9 source folders unresolved**. Two of those folders are format duplicates now covered by native providers, so `PENDING_VENDOR_PORTS_CURRENT.csv` identifies the remaining unique work.
- DFF only passed compilation and rejection of a malformed file, **not** a valid external DFF. Poser passed only a synthetic fixture.
- M3D packed zlib works; M3D animation sampling is not yet wired to callbacks. CSM retains marker timelines natively rather than inventing mesh geometry. LWS retains objects, paths and animation timeline, but does not automatically import the external LWO files.
- Outstanding 3MF `production_components.3mf` (multi-model assembly), VTA target-mesh binding, glTF animation, COLLADA advanced animations, MD4/MD7/TMD/MP-KF corpus fidelity, USD/STEP scene geometry, and IQM strict-C89 work remain **open**.

## Rebuild from source

```sh
make -j4 libmodel_runtime89.a
cc -std=c89 -pedantic-errors -Wall -Wextra -O2 -Iinclude tests/native_batch8_regression.c libmodel_runtime89.a -o batch8_test
./batch8_test /path/to/duck.dae /path/to/box.3mf /path/to/WusonBlitz0.m3d
```

The regression harness prints provider IDs and triangle/bone counts. Paths are supplied by the caller so copyrighted corpus data is not redistributed.
