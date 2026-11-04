# RE4 Classic v4.1 — Native Runtime 75 integration

## Scope

`MODELRT_CODEC_RE4_CLASSIC = 104`. The new provider is additive and does not
replace providers RE3 7, Gun Survivor 102 or HOD2 103. No model, texture or
animation from Capcom is shipped here. The user's original RE4 v4.1 codec is
vendored, including its research diagnostics, tests and offline tools.

## Direct Runtime 75 support

| Input | Native bridge behavior | Honest status |
|---|---|---|
| RE4 2007 `.pmd` | Reads native PMD mesh survey and streams indexed bind-pose triangles, per-vertex positions, normals, UV, colors and material links | Static geometry available. **Explicit** external FCV binding now evaluates native rotation channels and CPU skinning via separate handles. |
| `.bin` | Native RE4 BIN parser, material strips, primitive types 5/6/7/8, Q16 vertices and optional normals/UV/colors/bone position | Parsed by v4.1 core; path is in runtime provider. Not individually smoke-tested against a real BIN here; game-version/strip semantics require corpus validation |
| GameCube `.tpl` | Reads image table, exposes each native image via `modelrt_texture_info_get`, decodes to RGBA8 with RE4 core | Tested using synthesized 8x4 I8 TPL; not a texture binding to arbitrary PMD by itself |
| `.fcv` | Opens FCV track bank and returns one **UNBOUND** clip, with native tick range; exposes all channel entries and nearest/Hermite sampling in native ticks | Synthetic FCV tested. Separate FCV binding to a PMD is now implemented for verified rotation channels and Q16 skinning. Native IK and root motion remain unresolved; no inferred FPS |

The source also includes the complete RE4 v4.1 archive readers (DAT, UDAS,
AFS, GCA/LFS), character assembler, animation engine, game-event and texture
research code as an independent vendored toolkit. Native multi-PMD character assembly is now available as an **explicit**
caller-supplied list of PMD handles with strict audited bone mapping; it does
not automatically select character parts/variants or guarantee canonically
correct combinations. Plaga head variant identity, special Bella mappings,
event timing and native IK remain experimental per source notes.

See `RE4_CLASSIC_ANIMATED_CHARACTER_RUNTIME.md` for animated PMD/FCV binding, multipart parts, test generation and the inspector.

## API

`#include "modelrt_re4classic89.h"`

- `modelrt_re4_fcv_track_count(handle, &count)`
- `modelrt_re4_fcv_track_info(handle, index, &track)`
- `modelrt_re4_fcv_sample(handle, index, native_tick, hermite, &xyz)`

Use `hermite=0` to sample a nearest native key and `hermite=1` for the
codec's Hermite interpolation. A native tick is **not a seconds value** and
is not automatically mapped onto the PMD skeleton. A `.fcv` handle is
required for these calls.

## Compile (Linux GCC; ISO C89)

```
make -j8 libmodel_runtime89.a
make re4classic-synthetic
make -C vendor/re4_classic_v4_1 check
make re3-regression gunsurvivor-synthetic hod2-synthetic hod2-phase12-tables
make native-iqm-interpolation-smoke
```

The synthetic `re4classic-synthetic` target compiles a pure C89 generator,
creates test-only RE4 PMD, FCV, TPL data, verifies the runtime and deletes the
three generated assets. Do not include test output binaries in source ZIP.

## Test results in this environment

- GCC static library full build: PASS (pre-existing warnings in other vendors).
- New provider isolated `-std=c89 -pedantic-errors -Wall -Wextra -Werror`: PASS.
- RE4 v4.1's `make check`: PASS (24 self-contained suites).
- New native PMD triangle, TPL RGBA and FCV catalog/sampling regression: PASS.
- RE3 v0.28 runtime & native tests: PASS.
- Gun Survivor v19 synthetic NAB/NAC: PASS.
- HOD2 synthetic POL/PRS/H2MC + Phase12 H2TB (86 x 2): PASS.
- IQM synthetic interpolated rotation/normals: PASS.

No Win32/MinGW32 build or full in-game asset/animation corpus tests were run
in this environment. Do not infer all character meshes and clips work from
these synthetic tests alone.

## Memory model

No malloc/free in the integration; uses the caller-provided `modelrt_handle`
workspace for binary payload and codec arena. Q16.16 coordinates only.
The vendored RE4 library remains the authority; conversion to an OBJ/GLB
intermediate is not required to visit PMD/BIN bind-pose geometry.
