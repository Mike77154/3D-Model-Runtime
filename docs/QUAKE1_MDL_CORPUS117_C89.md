# Quake 1 alias MDL v6 — Runtime 75 bridge / corpus audit

**Source:** user-supplied `quake_authmdl-master.zip` (Quake authentic MDL corpus). This package deliberately excludes its game models, map BSP, QC scripts and other game files. Codec ID remains **71**.

## Measured result

- 117 / 117 IDPO version 6 `.mdl` files successfully opened and walked using the **original** runtime bridge (before patch); there were no format-level rejects.
- On the **patched** runtime: 117 / 117 opened and fully visited, `56,170` triangles in bind pose, 3,544 top-level animation entries, **3,655** frame/pose visits (grouped frames expanded), **138** 8-bit indexed skin images, **3** frame-group animations.
- The test also inspected texture metadata, borrowed index bytes, and conversion with an explicitly supplied 768-byte RGB palette; every file passed.
- Runtime full library build: GCC C89 successful. Strict `-std=c89 -pedantic-errors -Wall -Wextra -Werror` standalone codec adapter compilation successful.
- Regression checks: Gun Survivor NAB/NAC synthetic, HOD2 POL/H2MC synthetic + H2TB 1999/2000, RE4 PMD/TPL/FCV synthetic and animated skeletal fixture all passed.
- Win32/MinGW32 **not** executed here; viewport screenshots of all models **not** inspected. This is an API / corpus-load / geometry callback check, not a pixel-by-pixel visual QA report.

## Improvements

1. **Quake triangle UV seam:** `qmdl_resolve_seam_s()` is now applied per triangle, respecting `facesfront` and `onseam` (previous adapter used raw `stvert.s`).
2. **True vertex normals:** normals are decoded from 162-entry Quake normal table when valid; invalid normal indices do not get invented normals.
3. **All native frame poses:** `modelrt_visit_animation_frame(&handle, animation_index, pose_index, ...)` visits each real subframe. Grouped frames expose source key counts and last time when available.
4. **Frame groups by time:** `modelrt_visit(..., animation_index, time_q16, ...)` selects the appropriate group subframe based on its cumulative Q16 intervals (no invented FPS); each single-frame animation keeps its authored pose.
5. **Embedded skin catalog:** `modelrt_texture_count()` enumerates all images including grouped skins; `modelrt_texture_info_get()` exposes sizes. The borrowed 8-bit skin planes can be requested via `modelrt_quake_skin_indices89()`.
6. **Optional exact color reproduction:** `modelrt_quake_skin_rgba_palette89()` converts indexed skin to RGBA using a **caller-supplied Quake `gfx/palette.lmp`** (768 RGB bytes). Neither the Quake palette nor game resources are shipped in this ZIP, and no fake RGB palette is used.
7. Retains stable provider ID **71**, native qmdl v9, C89 fixed-point math, arena/no-heap operation, and all previous model providers.

## Reproduce corpus test

```sh
make quake-corpus-regression-build
find /PATH/TO/quake_authmdl-master -type f -iname '*.mdl' -print0 \
  | xargs -0 ./tests/quake_mdl_corpus89 > quake_corpus_results.csv
```

Run `make` first for all source dependencies. Command above expects a Unix shell and GCC for tests; the runtime itself is designed for portable C89. The test does not modify the source `.mdl` files. A per-model audit from this run is provided in `docs/QUAKE1_CORPUS117_RESULTS.csv`.

## API usage

```c
#include "modelrt_native_batch6_89.h"
/* after modelrt_open_file() and verifying codec id == 71: */
const modelrt_u8 *indices;
modelrt_u32 w, h;
modelrt_quake_skin_indices89(&handle, 0, &indices, &w, &h);
/* If palette_rgb contains exactly Quake's authentic 256 RGB triplets: */
modelrt_quake_skin_rgba_palette89(&handle, 0, palette_rgb, 768u,
                                  rgba_buffer, rgba_buffer_size);
```

**Limitation:** the generic `modelrt_texture_decode_rgba()` interface cannot automatically reconstruct authentic colors from this corpus because it does not supply `gfx/palette.lmp`. Use the explicit palette API for accurate color, or arrange to provide that palette to your viewer. Skin animation interval selection is exposed in the native qmdl structure but not automatically played by the generic rendering visitor.
