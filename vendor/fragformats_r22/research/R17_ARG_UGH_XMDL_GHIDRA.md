# FragFormats R17 — ARG / UGH / Blade3D XMDL focus

Date: 2026-11-02. Evidence is the **user-provided** fragMOTION 1.3.2 RAR (including `SDK/Includes/arg.h` and the `FileTypeXMDL.dll` plugin) and `fragMOTION.exe`, analyzed with Ghidra 12.0.4 in the working runtime. The original proprietary binaries and SDK are not redistributed in the source archive.

## Confirmed: ARG

`arg.h` defines the ARG stream in detail: `CAnimation`, recursive `CBone`, per-vertex bone IDs and three IEEE754 weights (fourth residual), `CArgTriangle` per-corner UV and normals, `CTexture` with external filename, `CMaterial` and `CTextureLayer`, optional v1.2 corner colors/UV-group IDs. It documents the animation composition order as:

```
Bone Space Transform = KeyRotate * KeyScale * KeyTranslate * Bone
Character Space Transform = Bone Space Transform * Character Space Transform of Parent Bone
```

**R17 new C89 code:** optional fourth ARG output `out.mtl`, OBJ `mtllib` + `usemtl`, MTL color `Ka/Kd/Ks`, `map_Kd` for ARG color texture layers, normalized clean basename (no image data fabricated), original file paths in TSV, per-corner 1.2 color/specular/UV group indices, original face normals in raw bits, clip `ANIMATION_META`, event names/parameters/frames, `WEIGHTS_Q16` records with four Q16.16 weights including residual. Nonfinite and clearly invalid weights rejected. Old three-file and one-file ARG CLI remain supported. These features do not generate animated vertices.

**Ghidra original EXE:** `arg00` at VA `0x00621CCC`, referred by code at `0x00456238` in `FUN_004561F1`. The function confirms the ARG version is ≤ 0x00010002, checks 5-byte signature, and dispatches to `FUN_00456580`, `FUN_004567BD`, `FUN_00456672`. Its recovered order agrees with the SDK layout.

**Original ARG samples** from RAR: `anchor` 31 vertices/31 faces; `locked` 12/10; `jarjar` 344/694, 25 bones/1 clip; `snowman` 367/678, 7 bones/2 clips. R17 OBJ/MTL/TSV passed on all four. `jarjar.arg` references `jarjar.jpg`, external to ARG, and MTL produces `map_Kd jarjar.jpg`. No pixel data inside the ARG source were claimed.

## Confirmed: fragMOTION UGH

Ghidra executable marker `ugh0` VA `0x00622DBC`. XREF at `0x0046B65D` in `FUN_0046B621`. In decompilation, `CreateStreamOnHGlobal` creates a stream; a 16-byte header is zeroed, first 4 bytes assigned `ugh0`, and COM stream write emits 16 bytes. A virtual method at vtable offset `+0x178` processes the stream, followed by `FUN_0046C0D0`. This proves **a writing pathway**, not the layout of the remaining UGH objects or the inverse reader. The R17 UGH probe reports the other header words as *opaque*; it does not decode meshes/skins/clips.

Headless Ghidra on the full EXE exceeded 70 sec autoanalysis budget, **but the focused marker script did run** and produced a function decompilation. See separate analysis ZIP for tool logs and trace.

## Confirmed: Blade3D XMDL

Ghidra fully analyzed `FileTypeXMDL.dll` (autanalysis 54s). Bytes `xmdl` appear at VA `0x10021368` and `0100` at VA `0x10021360`. Code at `0x100057AD` and `0x100057BE` compares those words after a 16-byte stream read operation near `0x1000578D`. The mesh-related function `0x10004630` decompiled partially; this is a large COM-model processing routine, not proof of the on-disk schema. No authentic `.xmdl` file was available to validate offsets, mesh arrays, texture references or animation tracks. R17 retains only the probe + internal XIR experimental bridge; does not claim Blade3D geometry decoded.

## Explicit status and next reverse-engineering targets

- **ARG:** geometry, UV, normals, materials, external texture paths, bones, weights, clip/key/event metadata recovered; no accurate animated skinning yet. Implement evaluation of Euler keys in original transform order; cross-check four-influence skinning against fragMOTION export snapshots before declaring posed model fidelity.
- **UGH:** header + exporter verified, rest undecoded. Reverse from `0x0046B621` through `0x0046B751`, COM vtable `+0x178`, and `0x0046C0D0`; find reciprocal reader and compare two original UGH exports with one controlled change (geometry, UV, material, bone, then animation).
- **XMDL:** import signature checks + intermediate code verified; lack of authentic sample blocks mapping the on-disk layout. Obtain a Blade3D `.xmdl` exported from the same fragMOTION build to validate the 16-byte header and compare a deliberately simple cube with skeleton, UV, texture and clips.

## Constraints

C89 strictly, no heap allocations, no `float`, `double`, `long long`, no third-party runtime dependencies. Q16.16 conversion used, raw IEEE-754 bits preserved in diagnostics. ARG static buffer for texture path lookup: first 1024 texture records; all file records are still parsed. External textures are not embedded or generated. Test fixtures are generated synthetically by the included Python test script; C runtime does not require Python. Source only bundle excludes proprietary DLLs and original images.
