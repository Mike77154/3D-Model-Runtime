# FragFormats R16 integration into Model Runtime 75

## Verified implementations

- **Provider 107, ARG 1.1/1.2:** native triangle visitor (no OBJ stage), authored per-corner UV/normals, vertex positions in signed Q16.16, bone names/parent IDs as metadata, animation names/frame counts unbound, texture-name references and material labels. Parse is two-pass: validation during open and bounded caller-arena vertex scratch during visit. Rejects malformed indices and truncated binary blocks.
- **Provider 108, UGH:** header/probe-only (`ugh0` and at least 16 bytes). `modelrt_open_file` explicitly returns `MODELRT_ERR_UNSUPPORTED`, never a fabricated model. UGH geometry and animation internals are not decoded in R16.
- **DirectX X, provider 8:** preserved. R16's independent `x_decode` adds optional read-only audit information; native X parser remains the default authoritative loader, including existing animation functionality. The R16 vendor is not the model loader.
- **DBO, provider 69:** preserved. R16's independent `dbo_scan` adds optional structural diagnostics; existing native DBO parser remains authoritative. It has high working-memory requirements and may return NO_SPACE with a small arena.
- **GameStudio, provider 106:** Fase15 preserved, no replacement or shader/skin regression. R16 adds `modelrt_frag_affine_inverse_q16` as an optional matrix diagnostic. This does NOT automatically swap Fase15 pose transforms, and is not proof of true original MDL7 joint layout.

## Public API additions

- `modelrt_frag_r16_audit(path, report)`: independent scan for .arg, .x, .dbo. UGH has header recognition only and returns unsupported. Audit failures must NOT block native .x/.dbo opening.
- `modelrt_frag_affine_inverse_q16(input, output)`: bounded fixed-point affine inverse based on R16, strict ISO C89. Rejects singular/nonaffine matrices.
- Native `.arg` metadata textures expose unmodified references; external image files must exist separately. ARG animations remain catalogued but UNBOUND; skinning and animation reconstruction are not yet solved.

## Reproducible source-only tests

```sh
make -j4 libmodel_runtime89.a
make frag-r16-runtime-test frag-r16-audit-test frag-r16-original-providers-test
make gs3d-phase15-runtime-test
make -C vendor/fragformats_r16 fraginspect
cd vendor/fragformats_r16 && sh tests/run_tests_r15.sh && sh tests/run_tests_r16.sh
```

Tested with Linux GCC, strict C89 for new bridge and test harnesses. Native Win32 MinGW32 not verified. `UGH` is NOT counted as a successfully importable animated model format.

## Safe upgrade policy

Provider IDs for .x=8, DBO=69, 3DGS=106, RTCW=105, RE4=104 remain unchanged. Only source in `vendor/fragformats_r16/` is additive. No game models, commercial textures, fixture binaries or executables are included in the released ZIP.
