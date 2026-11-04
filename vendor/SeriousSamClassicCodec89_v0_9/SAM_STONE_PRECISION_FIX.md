# Serious Sam Stone real-model precision correction — 0.3.1

* Maintains ISO C89 restrictions, no `float`, `double`, `malloc`, `free`, or 64-bit integers in the codec itself.
* Root issue found with real Serious Sam model: MDL per-component stretch can be 2.8e-6 on `Head.mdl`; the old Q16.16 stretch quantization collapsed all head vertices.
* New SS89_Model.stretch_ieee[] preserves binary32 source scale as its raw 32-bit bit pattern and computes `raw * stretch` directly to Q16.16 through a portable two-limb 32-bit integer product.
* Public SS89_Model.stretch[] remains as legacy Q16.16 approximate metadata. For vertex coordinates, use decoded SS89_Vertex records, not stretch[] to re-decompress manually.
* Verified on original user-supplied SeriousSam Player, Body, Head and Glasses `.mdl`, including 16/8-bit vertex encodings. Maximum absolute positional difference versus source binary32 multiplication across the four tested sample frames: 0.000520 world units (OBJ uses 3 decimals).
* Tested Linux build C89 -pedantic-errors -Werror and synthetic reader/resource/skinning tests.

## Important scene construction warning
The canonical `Models/Player/SeriousSam.amc` chains four parts:
`Player.mdl -> Body.mdl -> Head.mdl -> Glasses.mdl` (attachment index 0 at each link).
`Player.mdl` by itself contains only the legs/hips. Automatic loading and attachment hierarchy parsing of `.amc` are NOT yet implemented in the independent C89 library. The screenshot/MP4 was assembled using a separate Python diagnostic preview script, with native reference triangles, and verified against source asset metadata.

The C89 tools preserve original model geometry and can individually decode each constituent part. A proper in-engine AMC scene/runtime loader and complete upper-body weapon animation synchronization remain future work.

## Example (MSYS2 MINGW32)
```sh
bash build_msys2_mingw32.sh
./bin/ss89dump.exe Player.mdl --frame 47 --obj legs.obj
./bin/ss89dump.exe Body.mdl --frame 84 --obj torso.obj
./bin/ss89dump.exe Head.mdl --frame 0 --obj head.obj
./bin/ss89dump.exe Glasses.mdl --frame 0 --obj glasses.obj
```
