# R3 verified reverse engineering findings

1. The original fragMOTION 1.3.2 RAR **does ship an SDK** with `Includes/arg.h`, containing byte-exact serialized layout for all important ARG 1.1/1.2 structures.
2. ARG file header as read by SDK: little-endian DWORD version (`0x00010001` in all four real samples), five bytes `arg00` at offset 4, 64-byte character name at offset 9. Then: animation count+animations, skeleton count+recursive bones, mesh triangle count+triangles, vertex count+vertices, then textures, smoothing groups, materials, optional groups (1.2+).
3. ARG triangles (1.1) each occupy 84 bytes, with 3 u16 vertex indices and 3 per-corner normals/UV. Version 1.2 adds group/material-related data and occupies 116 bytes. Vertices occupy 30 bytes, with float position+3 explicit blend weights, 4 bone bytes and u16 flags.
4. ARG 1.1 files decoded end-to-end: `anchor`, `locked`, `jarjar`, `snowman`, with exact counts in README. Exported OBJ geometry includes original UV/normals; rig trace preserves IEEE-754 bit patterns to avoid loss in the reverse-engineering bridge.
5. Disassembly of user-supplied `FileTypeXMDL.dll`: native x86 function around `0x1000578D` calls CRT read wrapper for 16 bytes; at `0x100057AD` it compares a memory word to `xmdl` (`0x6c646d78`), at `0x100057BE` another to `0100` (`0x30303130`). Correct on-disk offsets are **not** established without a real native XMDL sample. `FUN_10004630` converts an internal descriptor array (16-byte descriptors) to mesh vertices and faces. This is NOT evidence the native XMDL stores XIR1 records.
6. The main `fragMOTION.exe` contains `ugh0` and `*.ugh` strings; the earlier R1 exporter tracing found a 16-byte header, but the body grammar remains unavailable. No native UGH sample provided.
7. The DBO code in R2 contained two guard problems: unsupported primitive printing to a null optional OBJ pointer, and size mismatch returning success. Both corrected in R3 and tested.

## Reproducibility

Source build: `make && ARG_CORPUS=/path/to/extracted/corpus make test`.
Extract examples from the user's original `fragMOTION 1.3.2.rar`; original binaries and models are not redistributed in the source ZIP.
