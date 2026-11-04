# Blade3D XMDL audit and Runtime75 bridge

## Source evidence

`vendor/fragformats_r22/vendor_blade3d/xmdl_vendor.c` is only a header probe. The R17/R18 reverse-engineering notes state that the original `FileTypeXMDL.dll` reads 16 bytes, compares `xmdl` and `0100`, but exact on-disk offsets and layouts were **not established**. There is **no authentic `.xmdl` file in the available attached corpus**. The source archive cannot justify claims of actual native Blade3D geometry, UV, textures or animations.

`vendor/fragformats_r22/vendor_blade3d/xir_vendor.c` reads `XIR1`. This was INVENTED by the earlier research as an intermediate diagnostic container; the vendor itself expressly says this is not the native format.

## Additions in this release

- Provider `110` / `MODELRT_CODEC_BLADE_XMDL`: limited signature probe for `.xmdl` and an explicit `MODELRT_ERR_UNSUPPORTED` when the `xmdl`/`0100` candidate header is recognized. It **does not report a successful model opening**.
- Provider `111` / `MODELRT_CODEC_BLADE_XIR`: direct streaming visitor for synthetic `.xir` geometry with fixed-point positions, normals and UV0. No OBJ export, no heap, no floating-point. Separate IDs prevent confusing XIR tests with genuine `.xmdl` compatibility.
- Strong structural validation: bounded record counts, file offsets, vertex/face indices, finite and Q16-representable coordinates, supported attributes, malformed/truncated fixtures and callback-error propagation.
- Existing ARG, UGH, DirectX X, DBO, GameStudio, Blender and other providers remain untouched.

## Reproducible test

Run `make blade3d-xir-runtime-test` with GCC, or set `CC` to a C89 compiler. Fixtures are invented diagnostic samples under `vendor/fragformats_r22/tests` and `tests/`; they are **not commercial game models or native Blade3D models**.

## Still missing (hard blocker)

Obtain at least one authentic `.xmdl` file exported by the Blade3D/fragMOTION plugin, ideally a cube with known vertices, UV, single skin, 2 bones and 2 animation frames. Compare the file against the existing Ghidra notes and reconstruct each native record before enabling geometry in provider 110. This release does not solve that unknown schema.
