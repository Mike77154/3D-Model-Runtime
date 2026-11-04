# RE4 Classic Codec Notes

This is a defensive parser/codecs starter for RE4 classic-era research. It does not include or derive from any game asset. It expects the caller to pass file bytes as memory spans.

## Source-grounded map

| Format | Implementation state | Notes |
|---|---|---|
| TPL, GameCube/Wii | parser + RGBA32 decode + block/palette helpers | Supports `0x0020AF30` TPL table, image headers, palettes, tiled formats I4/I8/IA4/IA8/RGB565/RGB5A3/RGBA8/CI4/CI8/CI14X2/CMPR. |
| TPL, RE4 BIN cue style | cue parser + text bridge | RE4UHD BIN/TPL tooling describes TPL as a cue/list that maps model materials to texture pack ids; this library keeps raw pair values, normalizes PackID/TextureID, provides lookup, and emits IDXUHDTPL-style text through a callback. |
| TPL, RE4 UHD metadata | metadata parser | Reads magic, image count, image/palette offset table, image header fields, palette count/format, compression flag, PackID and TextureID. |
| EVD | parser + manifest | Header, packet walk, file table, file-kind classification, common packet type ids, typed resource references, and manifest counts. SetOm/SetParts offsets follow the public 010 template layout. |
| LFS | table parser + extraction shell + stored writer | Uses the public header/chunk layout: magic, sizes, count, chunk table, 0x10000 default chunk size, low bit as compressed flag, and payload offset base/alignment. Compressed chunks need an external callback. |
| GCA | metadata parser | Supports GCAX wrapper and GCA3 metadata table. Payload offsets are inferred sequentially for uncompressed retail-style files; compressed records are marked but not decompressed. |
| AFS | table parser | PS2 archive offset/size table. |
| DAT/UDAS | experimental guess parser | Offset/size table guess plus offset-only fallback. |
| FCV | conservative parser + Q16 key view/sampler | Reads max time, block count, bone/data arrays, per-axis scalar sizes, tag counts, cumulative time shifts, key values, and nearest samples as Q16.16 for known data types. |
| SEQ | probe | Size/first word/possible 16-byte commands only. |
| BIN, UHD/modern | partial structural reader + material text bridge | Reads header offsets, flags, material records, primitive buffers, bone records, weight maps, optional bone-pair table, vertex channels, Q16.16 position/normal/UV values, and UHD24/PS2-12 material-core fields. Morph and full repack are intentionally out of scope. |
| PMD, 2007 | survey reader | Reads public name-table, skeleton, node/mesh, and material metadata, but keeps vertex decode conservative until more samples are validated. |
| SMD room | partial reader + text helpers | Reads scenario lines, BIN/TPL arrays and tail spans, validates TPL/SMX references, and parses bone/replacebone text helpers used by public SMD workflows. |
| SMX | metadata reader | Reads fixed-size Scenario material/effect records, flags, color, and texture movement in Q16.16. |
| LIT/EFF | probe | Basic sizing only. |
| ESL | parser | Fixed spawn-list layout from public template research. |

## Design rules

- C89 source.
- Caller-owned arenas only.
- No library heap usage.
- No C real-number types in source; IEEE-754 words can be converted to Q16.16 with integer bit handling.
- No hidden I/O in the library; examples are separate.

## Suggested reverse-engineering workflow

1. Unwrap `.lfs` when present. If a chunk is compressed, provide an XMem-compatible callback in the host.
2. Open `.EVD` and enumerate packet refs and embedded file table entries.
3. For `.TPL`, first try GameCube/Wii TPL. If magic fails, try UHD metadata TPL, then cue parsing when the file looks like 8-byte pairs.
4. For `.GCA/.DAT/.UDAS/.AFS`, enumerate spans before feeding embedded files into model/texture/event parsers.
5. For `.BIN/.PMD/.SMD/.SMX`, use the structural readers first, then fall back to probes when variant detection fails. Use the v0.7 callback emitters for editable metadata text, but keep binary writers disabled until roundtrip data is available.
6. For `.FCV`, inspect block arrays, per-axis scalar widths, and axis views before converting tracks to engine animation channels.

## Safety

Only the stored/uncompressed LFS writer exists in this alpha, and it has a roundtrip selftest. Repacking proprietary game archives can corrupt files quickly; keep other writers read-only until sample-backed byte-for-byte roundtrip tests exist.


## v0.9 text bridge notes

### IDXUHDTPL

The editable IDXUHDTPL view is treated as a lossy-safe metadata bridge rather than a binary writer. The parser stores each `TPL_###` entry in an arena array and records a `seen_mask` so tooling can distinguish defaults from explicitly present values. Decimal values are parsed without host real-number types; `lod_bias` is stored as Q16.16.

Key fields:

- `PackID`: 8-character hexadecimal PACK id.
- `TextureID`: decimal texture index inside that PACK.
- `PixelFormatType`: common public values include DDS diffuse/specular (`0E`), TGA opacity (`00`), TGA normal/bump (`03`), and TGA custom specular (`06`).
- `width` / `height`: decimal image dimensions.
- Optional wrap/filter/lod/compression fields are accepted and emitted.

### IDXMATERIAL

IDXMATERIAL text is parsed into `re4_bin_material_core`. The library deliberately keeps both the compact PS2-12 and UHD-24 variants represented by the same public core structure. Flags exposed through `RE4_BIN_MATERIAL_F_*` cover bump map, generic specular, opacity, custom specular, and normals effect.

### MTL bridge

The MTL scanner is line-oriented and suitable for conversion tools that want to rebuild IDXUHDTPL/IDXMATERIAL metadata from editor-generated material files. It recognizes:

- `map_Kd` as diffuse.
- `map_Bump` and `Bump` as bump/normal.
- `map_d` as opacity.
- `map_Ns -s X Y Z path` as generic or custom specular depending on the PACK id.

Patch paths are decoded as `PACKID/TEXTUREID.ext`. The library does not open texture files or inspect image payloads.

### Scenario OBJ/IDX text

Scenario OBJ groups are parsed for the naming contract used by public Scenario SMD tools: `UHDSCENARIO#SMD_000#SMX_000#TYPE_08#BIN_000#` and equivalent underscore-separated forms. The parser returns prefix, SMD id, SMX id, object-status flags, and BIN id. Scenario IDX parsing currently covers the high-level file-name and option fields; per-entry transform parsing remains binary-reader territory.

## v1.1 MTL conversion bridge

The v1.1 MTL path is now a material-block reader instead of only a single-line texture-reference scanner. It is intended for tools that want to reconstruct the two editable RE4 metadata files generated by public BIN/TPL workflows:

- `MTL -> IDXUHDTPL`: collect unique texture refs from all materials, preserve order of first use, infer `PixelFormatType`, and assign sequential `TPL_###` ids.
- `MTL -> IDXMATERIAL`: resolve material texture refs against that IDXUHDTPL order and build material-core records.

Recognized material commands:

- `newmtl name`: starts a material block.
- `Ks R G B`: stored as 8-bit RGB specular intensity. Decimal text is parsed manually with integer math.
- `map_Kd path`: diffuse map.
- `map_Bump path` / `Bump path`: bump/normal map; `Bump` wins when both appear.
- `map_d path`: opacity map.
- `map_Ns -s X Y Z path`: specular map. Pack `07000000` with DDS is treated as generic specular; other pack/format combos are treated as custom specular and resolved through IDXUHDTPL.

The bridge still does not open image files. Callers provide default width/height for generated IDXUHDTPL entries or later patch dimensions after inspecting actual texture payloads.



## v1.3 Scenario IDX and IDXUUBIN notes

Public Scenario SMD tooling documents Scenario OBJ groups such as `UHDSCENARIO#SMD_000#SMX_000#TYPE_08#BIN_000#`, plus IDX fields for global options and per-SMD entries. The v1.3 parser now treats Scenario IDX files as full documents: it reads global options and per-entry transform/link/status fields into Q16.16 and integer structures.

`ObjectStatus` remains a bitfield. The public labels used by this package are: scripted by EXE, assigned to an SMX group, ends an SMX group, uses shared BIN, uses shared TPL, and uses shared MOT. Unknown bits are preserved in the raw value.

Public BIN/TPL tooling V1.4+ replaced older `idxuhdbin`/`idxuhdbinbig` text with a decimal `idxuubin` bridge. The public web page for IDXUUBIN has little field-level detail, so v1.3 deliberately implements a generic key/value parser instead of a binary writer. Common summary fields are promoted when present; all other fields remain accessible through `re4_idxuubin_kv`.

IDXMATERIAL order helpers were added because public notes say transparent materials should be placed at the end and material names are sorted alphabetically by the tooling. The library reports bad order and can generate a source-index plan without rewriting the document.


## v1.5 PS2 and R100 notes

- `.idxps2bin` is treated as an editable key/value metadata bridge for PS2 BIN repack workflows. Known promoted keys are `UseIdxMaterial`, `AutoCalcBoundingBox`, `MaterialCount`, `BoneCount`, and `WeightMapCount`; unknown keys remain available through the stored key/value array.
- `.idxps2tpl` is treated as the PS2 TPL repack manifest with `ImageFlipY`, `ImageFolder`, and `ImageFormat`.
- `IdxtplHeader` is treated as per-image PS2 TPL header metadata. The current reader exposes `Bitdepth`, `Interlace`, `Width`, `Height`, `Next`, `Qwc`, and `GsTex`; bitdepth 9 reports a 256-color indexed limit and bitdepth 8 reports a 16-color indexed limit.
- `Interlace` flag `0x01` is exposed as rotate-image and `0x02` as swizzle.
- R100/BLK Scenario text helpers are parsed as lists of `ExtraParameter_#`, `ExtraSmdFileName_#`/`ExtraFileName_#`, and `ExtraTplFileName_#`.
- `re4_scenario_alignment_for_prefix(RE4_SCENARIO_PREFIX_GG)` returns 32 to reflect the GC/Wii Scenario SMD tooling update; other prefixes currently return 16.


## v1.7 PS2 TPL binary note

JADERLINK's PS2 TPL extractor source names a main reader with `Magic`, `TplCount`, `StartOffset`, and `HeaderUnk1`, then reads `TplCount` headers at `StartOffset + index * 0x30`. Each 0x30-byte image header is interpreted as: `Width`, `Height`, `BitDepth`, `Interlace`, `Next`, `MipmapStatus`, `Qwc`, `Ref`, `MipmapHeader1Offset`, `MipmapHeader2Offset`, `GsMip`, `IndexesOffset`, `PaletteOffset`, and `GsTex`. When `MipmapStatus == 0x02`, two mipmap headers may be referenced.

`re4_ps2_tpl_read()` follows those offsets and exposes the raw header information. `re4_ps2_tpl_indexes_span()` treats `Qwc` as a count of 16-byte units for the image payload. `re4_ps2_tpl_palette_span()` now uses the byte span required by the public extractor behavior: bitdepth 9 reads 256 RGBA palette entries, bitdepth 8 reads the 0x80-byte palette area needed by the 16 logical colors, and bitdepth 6 does not require a palette span. v1.9 adds non-swizzled RGBA32 decode for bitdepth 8, bitdepth 9, and bitdepth 6; swizzled interlace values still return `RE4_ERR_UNSUPPORTED`.


## v1.9 PS2 TPL decode note

The public PS2 TPL extractor dispatches bitdepth `0x0008`, `0x0009`, and `0x0006` separately and treats interlace `0x0002/0x0003` as swizzled paths. The v1.9 decoder implements the clear non-swizzled paths: 4-bit indexed, 8-bit indexed, and direct RGBA byte streams. Palette alpha is expanded from the public extractor's `alpha * 255 / 0x80` behavior and clamped for safety.

For bitdepth `0x0008`, the extractor reads a 0x80-byte palette even though only 16 logical colors are emitted; the second logical half comes from entries starting at source palette index 16. For bitdepth `0x0009`, the 256-color palette order uses 8-entry groups with source order 0, 2, 1, 3 within each 32-color block.
