# Cinema4DCodec89 v0.14 — Modularization, not a format change

## Source of truth

- `scripts/module_sources.sh`: authoritative deterministic compilation lists
- `scripts/build_module.sh <name>`: one independent module at a time
- `scripts/build_vendor_library.sh`: all modules + combined legacy static library
- `include/c4d89_modules.h`: umbrella for the five new entry-point interfaces
- `include/c4d89.h`, `include/c4d89_image.h`: UNCHANGED public legacy API

## Module ownership

| Module | Files owned | Interface | Archive |
|---|---|---|---|
| core | file probing, endian conversions, JPEG location/stream and Q16 float conversion | `c4d89.h` | `libc4d89_core.a` |
| mesher | native geometry scanning/streaming + OBJ | `c4d89_mesher.h` | `libc4d89_mesher.a` |
| uv_texturer | face selection groups + TextureTag selection restrictions | `c4d89_uv_texturer.h` | `libc4d89_uv_texturer.a` |
| shaderer | crease-based Q10 smooth normals, OBJ with normals | `c4d89_shaderer.h` | `libc4d89_shaderer.a` |
| animer | animation key records/index, frame interpolation, joint evidence, skin kernel and weight inventory | `c4d89_animer.h` | `libc4d89_animer.a` |
| texturer_opener | bitmap path catalog, asset resolution, PNG/TGA/BMP/JPEG pixel decoders | `c4d89_texturer_opener.h` | `libc4d89_texturer_opener.a` |

## Dependency graph

```text
          [ game engine ]
                 |
      [ include/c4d89_modules.h ]
                 |
    +---------+--+------------+--------------+
    |         |               |              |
  mesher  uv_texturer       shaderer        animer
    |         |               |              |
    +---------+---------------+--------------+-- texturer_opener
                 |
             core / C89
```

These are physical compilation units and public entry-points, **not runtime plug-ins**. Modules do have link-time dependencies: `shaderer` uses `mesher` and `uv_texturer`; `texturer_opener` uses its asset resolver; `mesher`'s optional grouped OBJ export uses `uv_texturer`. For simple applications, link **`libc4d89.a`**, which contains the same code and legacy functions. For custom per-module linking, use the toolchain's linker group mechanism where mutual references require it. Do not link both combined and individual archives into the same executable.

The source code remains C89 and uses no heap, floating point or explicit 64-bit integers. Static buffers in existing codecs remain non-reentrant, so host engines should serialize calls or provide independent instances in a future redesign.

## What this refactor does NOT claim

- Does **not** decode `TextureTag -> material BaseLink` to prove exact material binding.
- Does **not** decode native C4D skin weights / inverse bind matrices or hierarchy fully.
- Does **not** promise new Cinema 4D versions beyond the previously tested sample family.
- Does **not** use material position heuristics in the new module API.
- Existing `UV Texturer` does not decode external bitmap pixels; `Texturer Opener` handles that.

## Build

```sh
./scripts/build_module.sh mesher
./scripts/build_module.sh texturer_opener
./scripts/build_vendor_library.sh
./scripts/build_posix.sh
./scripts/test_v14_modules.sh
```

MSYS2 MINGW32:

```sh
./scripts/build_msys2_mingw32.sh
```

`build_msys2_mingw32.sh` is a cross-platform script naming `.exe` targets; genuine MinGW32 validation must run in an installed MinGW32 environment. GCC and Clang on Linux do not prove Win32 ABI correctness.

## Example of linking existing engine unchanged

```sh
gcc -std=c89 -pedantic -Wall -Wextra -Werror -Iinclude \
    examples/engine_modular_smoke.c build/libc4d89.a -o build/modular_smoke
```

## Compatibility guarantee

Original exported functions and public structs from v0.13 are preserved. The old `build/libc4d89.a` and `libc4d89_images.a` filenames still exist. Files moved to modules are built exactly once using the manifest; no duplicate implementations have been copied.
