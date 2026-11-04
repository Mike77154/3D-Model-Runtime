# RE1MV Build_Model / EDD / EMR implementation pass

This pass moves the RE1 EMD diagnostic animation path from the earlier generic
human parent tree into a source-compatible RE1MV matrix-stack interpreter.

## Implemented

- RE1 trailing 5-offset directory split as six runtime sections:
  - `EMR_A`, `EDD_A`, `EMR_B`, `EDD_B`, `TMD`, `TIM`.
- `TEMRHeader` interpretation from RE1MV:
  - `PartListOffset` at `EMR+0`.
  - `AnimationOffset` at `EMR+2`.
  - `ObjectCount` at `EMR+4`.
  - `FrameSize` at `EMR+6`.
- `Check_Id()`-style `EMD_Type` detection by XORing DWORDs in the EMR part list.
- `Load_Ans()`-style object offsets from `EMR+8`, with source order `Z,Y,X` mapped to viewer `X,Y,Z`.
- EDD clip/frame decode:
  - `Animation_Count = first_frame_start / 4`.
  - clip entry = `{ frame_count:u16, frame_start:u16 }`.
  - frame track = `{ frame_index:u16, step:u16 }`.
- EMR pose decode:
  - root translation is stored separately from object 0 bind offset, mirroring `Call_Oblect(0)`.
  - rotation words are masked to 12-bit PSX angle fields.
- RE1MV `Build_Model()` matrix stack tables for known RE1 `EMD_Type` values:
  - `0x3C0006`
  - `0x370000`
  - `0x4D0002`
  - `0x410000`
  - `0x480002`
  - `0x040000`
  - `0x4B0000`
  - `0x460000`
  - `0x0B000B`
  - `0x4D0006`
  - `0x0A0001`
  - `0x190009`
  - `0x000001`
  - `0x500000`
  - `0x010001`
  - `0x080005`

## EM1111 verification

Using `EM1111(5).EMD`:

```text
Bank A: EMD_Type=0x3C0006, build_model=1
Bank B: EMD_Type=0x410000, build_model=1
```

Geometry validation still reports:

```text
verts=352 normals=900 uvs=1944 faces=648 pieces=15
tris=648 quads=0 textured=648 invalid=0 degenerate=0 unused_vertices=0
status=ok
```

## Current scope

The matrix-stack interpreter is implemented in `tools/emd_textured_viewer.c` as
a diagnostic source-compatible renderer. It is deliberately table-driven so the
same operation lists can later be lifted into the core codec/exporter without
copying RE1MV code directly.

## Industrial protocol audit

The RE1MV Build_Model descriptor pass itself uses static tables and stack/local
arrays only. It does not add `malloc`, `calloc`, `realloc`, `free`, `stdin`, or
`long long` usage.

The inherited recodec codebase still contains legacy float and heap-based APIs
in the public model representation, CLI, texture decode, and diagnostic viewer.
So the full repository is still not a strict no-heap/no-float industrial build.
This patch preserves C89 compilation and does not increase those legacy classes
of violations in the new RE1 Build_Model path.
