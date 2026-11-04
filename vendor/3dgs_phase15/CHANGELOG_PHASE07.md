# Phase 07 — MDL7 skeletal TRS interpolation (C89, fixed-point)

## New runtime

- `src/anim/gs3d_trs_fixed.c`: Q16.16 matrix decomposition into position, scale, quaternion; shortest-arc normalized linear quaternion interpolation (NLERP); matrix composition. No heap, no floating-point types, no long long. Rejects reflection, large shear, singular scale and non-affine transforms. Endpoint matrices copied exactly.
- `src/anim/gs3d_skeletal_keys.c`: MDL7 group-0 bone key sampling, two frames, shared bones; identity for absent keys (the existing convention).
- `src/anim/gs3d_skeletal_morph.c`: raw vertex frame overrides sampled independently of skeletal skinning, so there is no cumulative deformation.
- `src/anim/gs3d_skeletal_sample.c`: fractional pose pipeline: frame keys -> TRS -> hierarchy and inverse bind -> per-frame morph -> weighted or rigid skinning once.
- `src/vendors/gs3d_vendor_skeletal_router.c` plus `src/vendors/a4..a8/gs3d_aN_skeletal.c`: generation-specific routes. A4/A5 refuse skeletal MDL7; A6/A7/A8 dispatch MDL7 only. Legacy morph sampling is unchanged.
- All four `SOURCES_*.txt` manifests contain the new sources; full and subset builds tested.

## Public API

```
int gs3d_sample_skeletal_q16(unsigned int group,unsigned int frame_a,
                             unsigned int frame_b,long alpha_q16);
int gs3d_vendor_sample_skeletal(unsigned int generation,unsigned int group,
                                unsigned int frame_a,unsigned int frame_b,long alpha_q16);
```

`alpha_q16` is 0..65536 (start..end). Frame numbers are deliberately not interpreted as seconds. The caller selects clip timing/FPS. `gs3d_sample_morph_q16` remains strictly morph-only, and refuses models with active skeletal keys.

CLI: `gs3d_export file.mdl prefix skeletal 0 1 32768` generates an OBJ/MTL at the halfway pose; the older `[frame_number]` argument still works. No real third-party models are included in the source archive.

## Validation performed in this environment

- C89 pedantic GCC / Clang; all previous tests: 6,231 prior robustness tests + 495 Phase06 robustness tests, plus tests for MDL3/MDL4/MDL5, weighted MDL7, material-only MDL7 and 8-group layout.
- `tests/test_phase07.c` on an **original synthetic** 660-byte animated MDL7: two bones (root and child), frames start/end, rigid pivot, two 50%-weighted influences, 0/50/100% sample, A4–A8 dispatch, repeatability. Also matrix translation/nonuniform scale, 90 and 180 degree rotations, reflection and shear rejections.
- `tests/test_phase07_robust.c`: all 0..659 prefix truncations, then 700 deterministic single-bit mutations of the 660-byte synthetic model. Rejected or parsed without sanitizer reports; does NOT establish coverage of all malformed files.
- Clang with AddressSanitizer+UBSan passed on both Phase07 new test executables.
- OBJ sample at halfway had vertex 1 = `(3.8536,0.3535,0.0000)`.
- MinGW32 / Win32 not available here, so that build is **not claimed** as tested.

## Limitations / next investigation

**Critical:** No actual MDL7 file with animated skeleton and skin weights has been obtained and executed here. Therefore, the deformer layout and bone matrix-space conventions remain **provisional** despite passing synthetic tests. An MDL7 file with eight real groups, but *no bones*, was studied in Phase06. No third-party assets in ZIP.

- NLERP approximates smooth orientation, but unlike SLERP it is not constant angular velocity.
- Mirrored transforms and sheared matrices rejected, not silently mangled.
- Negative scale and all MDL7 exporter variations not supported/tested.
- Missing bone keys currently use the existing identity-local convention; whether a particular exporter expects held keys requires corpus validation.
- No automatic clip segmentation / FPS inference or blend-tree, no PBR/modern shader textures, no actual Win32 test.
- Singleton static model memory means non-reentrant and not thread-safe.

## References

- https://opserver.de/ubb7/ubbthreads.php?Number=467876&ubb=showflat
- https://github.com/assimp/assimp/blob/master/code/AssetLib/MDL/MDLFileData.h
- https://github.com/assimp/assimp/issues/6633
- https://documentation.help/assimp/data.html
