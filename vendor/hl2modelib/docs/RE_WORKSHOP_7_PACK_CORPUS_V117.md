# v1.0.17 — Resident Evil Workshop Source 49 stress test

Test corpus (not bundled): seven uploaded packages with 30 compiled Source 49 MDLs. All 30 MDL/VVD/DX90.VTX bundles now assemble, vs 21 in 1.0.16. Across the test: 2,097,072 triangles, 131 draws, zero skipped/warnings. Source material VTF audit: 172/174 decoded, two `painted_silver_ldr.vtf` copies are environment cubemaps intentionally not supported by the 2D decoder. 472/472 raw motions readable at start/middle/end, 13 anomalous under the diagnostic threshold. The motion sweep does NOT establish visual parity with Source, especially additive `@subtract_` and Licker tongue.

## Actual code modifications

* VVD bone-byte interpretation changed from signed to unsigned (the raw ID 132 must remain bone 132, not -124). G1 `@att1` middle-frame skinning missing bone influences fell from 49,679 to 0; G2/G3 and Hunter sampled with zero missing influences.
* Compile-time maxima: VVD vertices 65,536 -> 131,072; VTX vertices 65,536 -> 262,144; VTX indices 262,144 -> 1,048,576; engine vertices and indices 262,144 -> 1,048,576. Static/caller-owned arrays remain; **significantly higher process memory requirement**, especially on Win32 32-bit. These are conservative test caps, NOT infinite-sized asset support.
* New `tests/test_vvd_unsigned_bones117.c`, compiled and exercised in `make check` under strict C89.
* Source module hierarchy and previous master entry points preserved. No malloc/free, float/double, int64 added to codec.

**ABI CHANGE:** `HL2M_MASTER_ABI89=0x0117`. Recompile all client projects depending on public structs; do not reuse older prebuilt `.o`/`.a`.

## Visual scope and limitations

The verified static renders use exported OBJ/MTL and source VTF albedo sampled by a separate diagnostic renderer, with brightness increased to aid inspection. They are not GPU Source shaders. `RE2RTyrantMrX.zip` contains only `typants` and `xhat`, not a complete Mr X body. The zombie package uses interchangeable bonemerge parts; unassembled previews do not represent complete zombies. `g1` references `geye_sphere_alb` which is not present in that package, so the preview has a conspicuous white eye region. The Licker can still show outlier tongue animations (`@att1`, `@att2`, `@climb_3`), inherited from previous corpus.

Build check:

```sh
make CFLAGS='-std=c89 -O2 -Wall -Wextra -Werror -pedantic -Iinclude' check
```
