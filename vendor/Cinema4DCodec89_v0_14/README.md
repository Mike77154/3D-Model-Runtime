# Cinema4DCodec89 v0.14 — Five independent modules

This is a **structural refactor** of the v0.13 native C4D codec, not a newly discovered file-format specification.

- `Animer`: native scalar tracks, joints/weight evidence, fixed-point skin kernel
- `Shaderer`: Q10 crease-aware normals and OBJ diagnostic export
- `Mesher`: native vertex/face recovery and streaming
- `UV Texturer`: UV/face selection restrictions and TextureTag->Selection binding
- `Texturer Opener`: image catalog/resolver and fully vendored PNG, TGA, BMP, JPEG decoders
- `src/` core: shared native byte reader and Q16.16 helpers

Each has a dedicated `include/c4d89_<module>.h`, `modules/<module>/` implementation, and its own `.a` static library. `include/c4d89.h` and `include/c4d89_image.h` continue to expose the previous v0.13 API unchanged.

## Quick start

```sh
./scripts/build_posix.sh
./scripts/test_v14_modules.sh
./scripts/build_module.sh animer
./scripts/build_module.sh shaderer
```

Compiler checks: C89 / pedantic / Werror. Use `build/libc4d89.a` to link all modules together, or build modules separately and manage dependencies explicitly. Full technical map: `docs/V014_MODULE_ARCHITECTURE.md`.

## Limitations

Exact material BaseLink bindings and native full skeletal parenting/weights remain **unresolved**. This release only reorganizes working code without inventing capabilities. PNG and JPEG profile limitations documented in `docs/IMAGE_CODECS_V012.md` remain. The native C4D parser remains reverse-engineered and should not be assumed compatible with arbitrary releases.

---

## Carried-over v0.13 reference

# Cinema4DCodec89 v0.13 — Engine Runtime Hardening

Preserves the complete v0.12 native C4D6 parser and all four vendored image decoders. No Maxon SDK code, heap allocations, float/double or explicit 64-bit integers in the added modules.

**New:** `c4d89_img_probe_info` returns required RGBA8 buffer size from PNG, JPEG, BMP or TGA headers before decoding; `c4d89_sample_animation_frame_file` samples all fixed-stride native scalar animation tracks against one opened file and reports unsupported tracks without hallucinating values.

**Fixes:** rejects TGA RLE packets overflowing the declared pixel count; parses negative BMP image height without implementation-defined signed conversion on 32-bit builds.

**Run:** `./scripts/build_posix.sh`, `./scripts/build_vendor_library.sh`, `./scripts/test_v13_core.sh`. GCC and Clang supported. Windows MinGW32 build script included but not yet exercised in Windows.

**Unchanged caveats:** `TextureTag -> material` serialized BaseLink, scene parenting, genuine vertex weights, inverse bind matrices and complete skeletal animation are still unresolved. JPEG baseline only; PNG 8-bit noninterlaced. Image header preflight is not pixel validation; decoder must still check the encoded image.

Full notes: `docs/V013_ENGINE_UPGRADE.md`.

---

# Cinema4DCodec89 v0.12 — four vendored texture image decoders

**New:** native C89 RGB(A) decode for PNG, TGA, BMP and baseline JPEG, using a unified autodetect API. The modules are self-contained and compiled into optional `libc4d89_images.a` or the existing `libc4d89.a` archive. No runtime DLL or third-party image library, heap allocations, floating point, or 64-bit integers. The original native C4D geometry, UV, material hints, and animation modules from v0.11 are preserved.

- Full technical details: [`docs/IMAGE_CODECS_V012.md`](docs/IMAGE_CODECS_V012.md).
- Build: `./scripts/build_image_codecs.sh`, or `./scripts/build_posix.sh` / `./scripts/build_msys2_mingw32.sh`.
- Use `./build/c4d89_imgtool input.jpg output.ppm` for visual inspection.
- Tests: `./scripts/test_image_codecs.sh` (Pillow required only for reference-image comparison).
- Image buffers are caller-owned; the decoders are **not reentrant** and enforce hard input/output limits.
- **Important:** image decoding and finding a bitmap file do not prove automatic `TextureTag -> Material` assignment. That final C4D link remains a separate codec task.

---

# Cinema4DCodec89 v0.11 — Full Corpus Texture QA + Optional Angular Animation

Native XC4DC4D6 prototype, **modular C89**, independent of Maxon SDK and binary exports.
All v0.10 capabilities are retained: mesh/UV/index recovery, polygon selections, TextureTag restrictions, embedded JPEG scan, smooth normals, native scalar animation tracks, experimental joint evidence and a stand-alone skinning kernel.

## New in v0.11

1. Recovers **all** typed bitmap filename parameters in each material, not only the first. Preserves native record/material offsets so multiple bitmap maps remain grouped.
2. Adds a safe external bitmap file resolver for a game engine's own VFS. It reports same-model matches, unique package fallbacks, collisions and missing files. It never uses absolute paths from scene data for filesystem access.
3. Audits every supplied native scene against the original external resources: **260 C4D files; 1,901 typed bitmap references; 1,823 exact matches; 78 missing references to one TGA filename; 2 scenes have no bitmap**.
4. Adds an opt-in Q16.16 shortest-arc angular interpolator for the rotation keyframes that cross 0 and 2π.
5. All existing modules are retained; no Maxon SDK code has been vendored.

**Still pending:** TextureTag→Material serialized BaseLink, exact shader channel semantics, full scene hierarchy, skin weights and verified skeletal playback. This release does **not** claim that all 260 models are automatically rendered with correct materials.

See [V011_CORPUS_MATERIAL_ANIMATION_REPORT.md](docs/V011_CORPUS_MATERIAL_ANIMATION_REPORT.md) for bytes, exceptions, test outcomes and integration usage.

## Compile (POSIX GCC/Clang)

```sh
./scripts/build_posix.sh
./scripts/build_vendor_library.sh
./build/test_multi_bitmap_catalog
./build/test_asset_resolve
./build/test_angle_lerp
```

## Compile (MSYS2 MINGW32)

```sh
./scripts/build_msys2_mingw32.sh
./scripts/build_vendor_library.sh
```

## Commands

```sh
./build/c4d89_inspect bitmaps "EGF2.c4d"
./build/c4d89_asset_audit "EGF2.c4d" "redc/EGF2/EGF2.c4d" docs/RESOURCE_MANIFEST_518.txt
./build/c4d89.exe bindings "EGF2.c4d" # use c4d89 on POSIX
./build/c4d89_inspect sampleangle "L_walk.c4d" 3 3200
./build/c4d89_inspect sampleanglecsv "L_walk.c4d" 3 300 31 angles.csv
```

The `.exe` names are for Windows; omit `.exe` on POSIX. Full-Corpus test data is not included in the source ZIP; the manifest includes file paths only.

**License:** existing CC0 codebase, as in previous prototype releases.
