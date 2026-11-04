# FragFormats R18 — ARG clip sampler and focused Ghidra (2026-11-02)

## Files inspected

- User-supplied `fragMOTION.exe` and `fragMOTION 1.3.2.rar` containing `SDK/Includes/arg.h`, `FileTypeXMDL.dll`, and ARG examples.
- R17 code, R17 Ghidra traces. Original binaries are **not** distributed in the source package.
- Ghidra 12.0.4 on the original PE32 binary and DLL, using a focused `-noanalysis` disassemble/createFunction/decompile script. This makes the output FAST and REPRODUCIBLE, but graph and function instruction counts are *not* comparable to a full program autoanalysis. Where decompiler stack recovery is uncertain, do not infer file offsets.

## R18 C89 additions: ARG

`--arg-clip=N --arg-frame=N` requires `.arg` and TSV output. It selects an original clip ID and keyframe number, then samples ROTATE Euler, TRANSLATE and SCALE tracks independently per bone by bracketing keys and Q16.16 interpolation; absent channels get rotation/translation zero or scale identity. It clamps before first and after last key. It validates clip existence and frame range, undefined bone influence IDs and duplicate bone IDs. It preserves original key lines as well as emitting `POSE_CURVE_Q16` and `POSE_BONE_Q16` records. No malloc, no floats, no long long.

Important caveat: `arg.h` names the rotation values `m_euler[3]` and documents `KeyRotate * KeyScale * KeyTranslate * Bone`, followed by composition with a parent. It does NOT establish Euler angle units, axis interpretation, winding or matrix layout. The sampled numeric curves therefore do **not** yet imply a correct skinned/animated OBJ; all OBJ exports in these tests are *bind/base geometry*. No motion is silently fabricated.

Validated (source-provided authentic samples):

- `jarjar.arg`, clip 0, frame 4: 25 bones, 75 channels with keys, 344 vertices, 694 triangles.
- `snowman.arg`, clip 0 `jump`, frame 8: 7 bones, 8 channels with keys, 367 vertices, 678 triangles.
- `snowman.arg`, clip 1 `walk`, frame 8: 7 bones, 7 channels with keys, 367 vertices, 678 triangles.
- Synthetic fixture: 0->10 frames, frame 5 produces rotation X=32768, translation X=327680, scale XYZ=98304 (all Q16.16). Rejects missing clips, out-of-range frames, undefined bone index, duplicate CLI flags.

## New Ghidra results: UGH export path

The program's `ugh0` reference leads to `0x0046B621`, which creates an `IStream` and writes a 16-byte header with `ugh0`. Additional focused decompilation identifies:

- `0x0046B751`: invokes `IStream::Seek` (vtable `+0x14`) and returns the resulting position. This is an IO helper, NOT an UGH geometry decoder.
- `0x0046C0D0`: opens the destination via `CreateFileA` and calls `0x0046C128` before closing it. It is a file-output wrapper.
- `0x0046C128`: reads from the `IStream` using virtual method offset `+0x0C` in chunks of `0x10000` (65,536) bytes, and calls Win32 `WriteFile` until a zero-byte read.

`0x0046B621` calls the unknown model serializer via a COM interface vtable offset `+0x178`. That serializer is still the major blocker. The 65,536-byte IO buffer is NOT an UGH record size or chunk format, and the 16-byte header is NOT evidence of subsequent block boundaries. There are no UGH source models in this corpus.

## New Ghidra results: Blade3D XMDL

With a targeted non-autoanalysis script on `FileTypeXMDL.dll`, `0x10005720` is a large import routine (753 disassembled instructions recorded, 24 distinct direct call targets) and still compares `xmdl`/`0100` as in R17. The internal mesh function at `0x10004630` partially decompiles. No authentic `.xmdl` file has been provided. Internal XIR is a diagnostic bridge and must not be confused with native XMDL. There is currently **no proven native mesh/UV/texture/clip decoder for `.xmdl`**.

## Follow-up empirical work

1. ARG: determine Euler order and units by exporting controlled rotations from fragMOTION; then animate geometry with inverse bind + 4-weight LBS and preserve correct transformed normals/UV/materials.
2. UGH: export two files differing in exactly one feature (vertex, UV, texture, keyframe, bone) and diff after the 16-byte header; trace serializer interface vtable +0x178 using interface GUID references, then implement chunk parser.
3. XMDL: obtain a minimal cube `.xmdl` produced by same plugin; confirm exact positions of `xmdl`/`0100`, geometry records, texture and clip tracks; only then attach native decoding to the XIR bridge.
