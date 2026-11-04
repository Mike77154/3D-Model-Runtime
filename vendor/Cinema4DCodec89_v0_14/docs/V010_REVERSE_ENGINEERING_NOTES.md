# V0.10 — Cinema 4D C4D6 material and animation investigation

## Material path records

Observed in FemaleHobo, MalePunk and EGF2, including files from Land of the Dead and Resident Evil Darkside Chronicles:

- Material type 5703 (`0x1647`), typed class bytes including `02 01 00 00 16 47`.
- Bitmap filename parameter 1000 (`0x03E8`) represented by `0F 00 00 03 E8`.
- UTF-16BE path string prefixed `83 83 [length_BE32]`.
- The parser requires both the typed material and the typed parameter/path within its local material record. It stops if another material declaration appears first.
- In `FemaleHobo.c4d` the paths include `FemaleHoboHeadA.jpg` and `FemaleHoboBody.jpg`. `MalePunk.c4d` similarly returns `MalePunkHeadA.jpg` and `MalePunkBody.jpg`.
- `EGF2.c4d` returns seven material bitmap paths including textures named `_normal.jpg`. They are **not** to be treated as diffuse just because they are JPGs.

**260-file audit:** 260 native files processed successfully; 258 have one or more typed bitmap paths, totaling 877 paths across the corpus. Only `splash screen.c4d` and `ETY4 no texture.c4d` returned zero. Not all files have unique image names; path occurrences are counted per `.c4d`.

The chain remains:

`TextureTag 5616 → restriction 1006 → polygon selection` [v0.9, verified in supported layouts]

`Mmaterial 5703 → bitmap filename 1000 → typed bitmap path` [v0.10, verified in samples]

`TextureTag → material pointer 1010` [UNKNOWN]

`bitmap file path → embedded JPEG` [proximity hypothesis, confirmed for representative atlas groups; not a guaranteed global relationship]

These are two independently recovered branches, not an established end-to-end link. Never pair the two by list order.

## Animation sampling

Native `0xAF50` key records and indexed per-channel 109-byte strides from v0.9 are re-used unchanged. `c4d89_sample_track_linear_file()` performs binary search over the key times while the engine's `FILE*` stays open. The returned parameter is a `long` storing signed Q16.16, after clamping outside the first/last key.

The sampler rejects tracks without verified regular stride rather than guessing. It also rejects interpolation that exceeds 32-bit Q16.16 bounds.

Checked **8 tracks from each of 4 `.c4d` files (32)**: `L_walk.c4d` (EGF2), `L_walk_b.c4d` (EGF1), `appearance.c4d` (EGF4), `at_jump_knock_a.c4d` (EGF2). For each, sample at first key, last key, and middle of time range; endpoint values match native keys. The sprite/GIF in previews is an animation of a **parameter plot**, not a rigged mesh.

Native interpolation mode and tangents still require decoding. Rotational discontinuities at ±π / 2π and axis order cannot be assumed to be resolved by direct scalar interpolation.

## Platform limits

Validated in this run using GCC and Clang, `-std=c89 -pedantic -Wall -Wextra -Werror`. The Windows-specific MinGW32 build script exists but no Windows binary was tested here. All C codec source files are free of `malloc`, `free`, `float`, `double`, and 64-bit integer arithmetic.

No proprietary SDK file was incorporated. The corpus is for validation and is not redistributed in the code ZIP.
