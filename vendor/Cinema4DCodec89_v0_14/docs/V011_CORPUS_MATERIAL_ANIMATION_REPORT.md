# Cinema4DCodec89 v0.11 — Corpus 260 material resources and native angular sampling

## Purpose and scope

This phase checks resource identification on **all** user-supplied native XC4DC4D6 scenes from the three RARs. The C codec uses no SDK, allocator, floating-point type, C99, or 64-bit integer. This phase does not assert complete Cinema 4D serialization support.

## Input corpora

| Pack | Native .c4d files | External images | Image encoding |
|:--|--:|--:|:--|
| Land of the Dead | 93 | 96 | JPEG |
| Resident Evil Umbrella Chronicles | 85 | 341 | TGA |
| Resident Evil Darkside Chronicles | 82 | 81 | JPEG |
| **Total** | **260** | **518** | 177 JPEG / 341 TGA |

All 518 externally supplied images were decoded independently during QA: 443 RGB and 75 RGBA; no unreadable inputs.

## Correction to the v0.10 bitmap inventory

The old `c4d89_bitmap_catalog.c` returned **only the first bitmap path per material**. The 877 references previously reported were genuine but incomplete. The new implementation scans *every* typed bitmap filename (`UTF-16BE`, marker `83 83`) with the nearby native parameter 1000, and groups the path with the preceding class 5703 (`Mmaterial`), constrained by native record proximity and input bounds.

Example: `EGF2.c4d` grows from 7 to **17** recovered paths, including body, alpha, and normal variants. Each entry retains its native `material_offset` so multiple bitmap channels can belong to the same provisional material group. The exact bitmap shader roles and TextureTag BaseLink are still unknown.

## Full audit results

| Pack | Material bitmap paths | Matched originals | Missing originals |
|:--|--:|--:|--:|
| Land of the Dead | 335 | 257 | 78 |
| Umbrella Chronicles | 818 | 818 | 0 |
| Darkside Chronicles | 748 | 748 | 0 |
| **TOTAL** | **1,901** | **1,823** | **78** |

- All 260 files could be parsed, and 258 have material bitmap paths; 2 lack typed bitmap references (`splash screen.c4d`, `ETY4 no texture.c4d`).
- Of the 1,901 typed bitmap references, **1,823** resolve by exact basename in the *same scene's `tex/` directory* against the full 518-file inventory. This is a verified external file match, not a polygon/material assignment.
- The **78 missing** paths all mention `MaleCopBodyBloodA.tga` in 39 `MaleCop` scenes; the archive contains `MaleCopBodyBloodA.jpg`. This should be treated as an extension mismatch, **not** automatically converted or declared equivalent.
- Forty paths in Umbrella Chronicles still contain an old Windows `F:\\...` source location. The resolver strips that origin *as an inert label* and looks up the basename only against caller-enumerated, checked assets. All 40 are unambiguous exact matches inside their model's `tex/` directory. No attempts are made to read the old absolute location.
- The improved scan now references all 518 distinct provided images at least once.
- Max path count in an individual scene was 64; none exceeded the catalog cap of 256.
- `CORPUS_260_RESOURCE_BINDING_AUDIT.csv` records all 1,901 path records plus the 2 scenes with none, including source scene and match category.
- `RESOURCE_MANIFEST_518.txt` records **names and paths only**, not image bytes. The original RAR assets remain outside the source ZIP.

## Polygon selection and unresolved material link

The unchanged previous native polygon-range scanner still reports:

- 1,487 `TextureTag` records across the 260 scenes.
- 1,376 restrictions connected to candidate validated polygon selection records.
- The last serialization edge `TextureTag -> Material BaseLink (1010)` remains **unresolved**. `material_offset` groups bitmap paths, but does NOT identify which TextureTag references that material.

Therefore: **this build cannot automatically reproduce correct material channels and face assignments in all 260 scenes**. The new guarantee is full corpus *resource inventory with explicit exception reporting*.

## Optional angular sampling improvement

The existing linear Q16.16 sampler is preserved. `src/c4d89_anim_angle.c` adds `c4d89_angle_lerp_q16`, and the animation sampler can use it in the new `sampleangle` / `sampleanglecsv` commands for `Rotation ...` channels.

In `L_walk.c4d` track 3, native keyframes cross from Q16=76 to Q16=409556, nearly a full revolution numerically but near-neighbor angles on the circle. Ordinary linear interpolation would rotate hundreds of degrees between them. The shortest-arc mode avoids that *specific* interpolation artifact. The mode is opt-in since full rotations can be intentional. C4D Bezier tangent reconstruction, parent links, inverse-bind matrices, and per-vertex weights are still not implemented.

## New engine API

- `c4d89_scan_material_bitmaps(path, catalog)` now returns **all typed bitmap paths**, not just the first.
- `c4d89_match_bitmap_asset(relative_scene, bitmap_reference, resource_index, &match)` verifies source paths using an index callback supplied by the engine/VFS, with no filesystem assumptions. Prefer same-model `tex/`; report ambiguity or missing assets rather than guessing.
- `c4d89_sample_track_angle_file(open_file, track, time_q16, &angle)` is an optional shortest-arc alternative to `sample_track_linear_file`.

## Compile and test

```sh
./scripts/build_posix.sh
./scripts/build_vendor_library.sh
./build/test_multi_bitmap_catalog
./build/test_asset_resolve
./build/test_angle_lerp
./build/test_skin_core
```

On MSYS2 MINGW32, run `scripts/build_msys2_mingw32.sh` first, then the `.exe` test variants. The Windows toolchain was not executed in the assistant container.

For corpus-based tests, after unpacking the original RARs into an equivalent package-root directory (e.g. `lotd/`, `reuc/`, `redc/`):

```sh
./build/c4d89_asset_audit /path/EGF2.c4d redc/EGF2/EGF2.c4d docs/RESOURCE_MANIFEST_518.txt
./build/c4d89_inspect bitmaps /path/EGF2.c4d
./build/c4d89_inspect sampleangle /path/L_walk.c4d 3 3200
```

The path manifest included with this project reflects the user's supplied archive folder structures and is intended for reproduction of this exact corpus QA.

## Nonclaims

- No inference that a matching JPEG is the actual diffuse texture without reading its shader channel.
- No inference that a matching TextureTag restriction proves a material BaseLink.
- No auto-aliasing `.tga` to `.jpg`.
- No rigged-zombie animation presented.
- No claim of full native `.c4d` compatibility beyond the tested C4D6 files.
