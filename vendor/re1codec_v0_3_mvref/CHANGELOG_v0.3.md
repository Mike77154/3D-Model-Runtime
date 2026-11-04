# RE1codec v0.3-mvref changes

Reference: real `RE1MV` Delphi/Pascal source supplied by the user.

## Correctness fixes

- Added an independent RE1MV reference audit harness (`tools/re1codec_mv_audit.c`).
- EMD section slicing now trims TIM to its declared byte size, matching the stream RE1MV gives to `Tim.dll`; fixes the 4-byte compact-EMD padding case (`em1032`).
- TMD primitive traversal is now variable length (`4 + ILEN*4`) instead of `index*28`.
- FT4 `$3C00080C` 36-byte packets are decoded, including four vertex and normal indices.
- `re1c_angle12_to_turn_q16()` now matches RE1MV's `angle / 0xFFF * 360` convention (4095 denominator).
- EDD second WORD is exposed as `unknown_word`; v0.2 `step_units` remains a compatibility alias only.
- Added RE1MV timing semantics: fixed 40 ms per EDD frame ref / 25 FPS (`RE1C_MV_FRAME_MS`, `RE1C_MV_FPS`).
- Added `re1c_get_mv_frame_duration_ms()` and `re1c_get_clip_duration_ms()`.
- Added `re1c_get_bind_global_centi()` to reconstruct PartList bind positions without hardcoded humanoid hierarchy.

## Reference-source bugs intentionally not copied

- RE1MV `Skip_Byte` drifts by +2 bytes/frame on `EMD_Type=$10001` (`EM1013`, `EM1113`) even though `FrameSize=48`; codec trusts `FrameSize`.
- RE1MV `Build_Model()` is a hardcoded OpenGL hierarchy by `EMD_Type`; codec trusts EMR PartList.
- RE1MV FT4 branch skips storing normal indices; codec preserves them.

## Tests

- 106/106 supplied model files pass the v0.3 reference harness: 67 EMD + 32 EMW + 7 standalone TMD.
- 5,282,993 field/structure comparisons.
- 133 animation banks, 2,098 clips, 39,539 EDD frame refs.
- 37,185 EMR poses and 560,075 part-pose angle records.
- 1,027 TMD objects, 24,995 vertices, 55,103 normals, 43,243 primitive packets.
- Synthetic mixed-packet test verifies FT4(36 bytes) followed by FT3(28 bytes).
- C89/no-heap/no-float constraint audit still passes.
- EM1111 Golden Master remains `283734` canonical bytes / FNV-1a32 `C2104AE0`.
