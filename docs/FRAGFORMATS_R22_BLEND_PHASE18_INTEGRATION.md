# Runtime75 — FragFormats R22 + BLEND Phase18 native integration

## Summary

- `107` fragMOTION ARG: **R16 callback importer preserved** for triangle/UV/normal streaming, bone/animation *metadata* only. R22's `arg_vendor` changed ABI and removed `arg_decode_native`; replacing R16 would regress existing runtime geometry. The complete R22 toolkit is available separately under `vendor/fragformats_r22/`, including its sampled ARG pose investigation, but it is **not** linked as a competing ARG provider.
- `108` fragMOTION UGH: R22 **two-stage nested-header** metadata inspection available through `modelrt_frag_r22_ugh_header`. R22 also includes `ughnativeprobe` and `ughtree` with true linked offset walking (root/firstChild/nextSibling are stream offsets, not table indices). **No UGH mesh, texture or clip playback** and no genuine UGH corpus validation. `modelrt_open_file` still reports UGH as UNSUPPORTED.
- `8` DirectX X, `69` DarkBASIC DBO: untouched primary readers; R22 forensics available as standalone tooling; original regression tests verified.
- `106` GameStudio Fase15: preserved, including old + file-space matrix modes. R22 math/diagnostic paths remain offline rather than silently replacing more mature Fase15.
- `109` BLEND Phase18 **new provider**: direct in-process C89 streamed triangles, up to multiple Mesh datablocks, Q16.16 positions and first UV layer when serialized. No conversion to OBJ, and no third-party binaries linked into Runtime75.

## Architecture

BLEND vendor source under `vendor/blend_c89_phase18/`. Runtime provider `src/providers/provider_blend89.c`, streaming adapter `vendor/blend_c89_phase18/src/bc_runtime_stream.c`, API in `bc_runtime_stream.h`. All Phase18 C89 modules are compiled into the archive, omitting `main.c` and the nonportable compression backend `bc_wrapper.c`. `bc_wrapper_stub.c` reports unsupported compressed payloads. Original command-line toolkit source and its 3–8-bone IK kernel are included and can be built separately with source-only configuration.

The open operation indexes the SDNA schema, and preflights every Mesh. A fresh stream traversal emits triangle callbacks to the existing Runtime75 viewer, **without filesystem OBJ staging or temporary geometry conversions**. Supported topologies are MFace, MPoly/MLoop and modern Blender corner-offset arrays. Unsupported or damaged Blender layouts fail closed. Mesh positions are datablock-local, not evaluated object world space. A polygon fan is used for faces with more than three corners; non-convex polygon triangulation can differ from Blender.

### Honest limitations

1. The direct-runtime provider currently handles **uncompressed `.blend` static geometry**. Blender `.blend.gz` and `.blend.zst` require future C89-compliant gzip/zstd integration or external decompression. Prebuilt Linux compression libraries distributed in the upstream Phase18 archive have **not** been included in the runtime.
2. Material/texture bindings, evaluated modifier stack, Object transform instances, full normal layer support, cameras/lights, animation/NLA, and constraints are **not connected** to the runtime viewport. `MODELRT_ANIM_BIND_POSE` only. Standalone Phase18 retains its independent IK / rig tooling.
3. The BLEND codec needs a **workspace of at least ~20 MiB** per open model because `BC_File` has a 16 MiB DNA buffer plus block and field indexes; use 24 MiB to permit future growth.
4. UV0 exposure is conditional on a supported UV layer in the Mesh. No fabricated UV, normal, material, FPS or bone data.
5. No MinGW32/Win32 build or genuine Blender scene parity has been certified here. Synthetic Phase13 and Phase16 Blender fixtures verify the C89 adapter; they are not proof of broad Blender cross-version completeness.
6. ARG live deformation and UGH end-to-end models are pending; R22's position/rotation metadata alone is not a skinning solution.

## Reproducible tests

```sh
make -j4 libmodel_runtime89.a
make blend-phase18-runtime-test
make frag-r22-ugh-runtime-test
make frag-r16-runtime-test frag-r16-audit-test frag-r16-original-providers-test
make gs3d-phase15-runtime-test
make re4classic-animated-synthetic
make native-iqm-interpolation-smoke
```

Two built-in BLEND examples are tiny **synthetic test fixtures** without commercial content. R22 tests generate fake, structural UGH bytes; they do **not** demonstrate valid fragMOTION export/import parity. Source build uses GCC C89 on Linux.
