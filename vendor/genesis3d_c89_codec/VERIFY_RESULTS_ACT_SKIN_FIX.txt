Genesis3D C89 codec ACT skinning fix
======================================

Applied files:
- g3d_codec.c
  - g3d_mul_s32_fix16(): replaced overflow-prone 16.16 multiply with C89-safe hi/lo split math.
  - g3d_fixmat34_from_fxform(): maps Genesis3D geXForm3d layout correctly:
    AX AY AZ / BX BY BZ / CX CY CZ / Translation.X Translation.Y Translation.Z
    into internal row-major 3x4 mat34.
- g3d_tool.c
  - bones command reports attachment translation from f[9], f[10], f[11].
  - skin actor command defaults to bind-pose when motion index is omitted (motion_index = -1).
  - skin actor command prints bind-pose instead of unsigned 4294967295.
- selftest.c / mk_sample.c
  - synthetic identity geXForm3d updated to f[0], f[4], f[8] to match Genesis3D layout.

Protocol:
- C89-compatible.
- No malloc/realloc/free added to codec runtime path.
- No float/double math added to fixed-point sampling path.
- No long long used.
- Arena/caller-owned allocation model preserved.

Build/test:
- make: OK
- ./selftest: OK
- g3dtool skin /mnt/data/zombie.act 0 /mnt/data/zombie_from_patched_codec.obj: OK

Real ACT verification:
- Output: vertices=1017 faces=1473 bind-pose
- BBox: X[-36.3903, 37.4693] Y[-0.0516, 75.1609] Z[-5.5953, 8.8667]

Texture/preview note:
- The codec exports raw ACT/OBJ UVs.
- The final preview that matched the user's OBJ/FBX reference used Y-up rendering and did not force a vertical V flip.
