# Public research sources used

This package is clean-room style parsing and glue code based on public layout descriptions and tool documentation. It does not include game assets.

## Core public references

- Yet Another GameCube Documentation, section 15.35 TPL and section 17 graphic formats.
  - https://hitmen.c02.at/files/yagcd/yagcd/chap15.html
- emoose/re4-research public repository.
  - EVD template: https://raw.githubusercontent.com/emoose/re4-research/master/file-formats/RE4_EVD.bt
  - ESL template: https://raw.githubusercontent.com/emoose/re4-research/master/file-formats/RE4_ESL.bt
  - LFS tool: https://raw.githubusercontent.com/emoose/re4-research/master/re4lfs.cpp
  - GCA hexpat: https://raw.githubusercontent.com/emoose/re4-research/master/file-formats/RE4_2007_GCA.hexpat
- JADERLINK/RE4-2007-GCA-TOOL public README.
  - https://github.com/JADERLINK/RE4-2007-GCA-TOOL
- JADERLINK DAT/UDAS tools public documentation and posts.
  - https://github.com/JADERLINK
  - https://residentevilmodding.boards.net/thread/17598/jaderlink-datudas-tool

## BIN/TPL/material/model workflow references

- JADERLINK/RE4-UHD-BIN-TPL-TOOLS public README/release notes.
  - https://github.com/JADERLINK/RE4-UHD-BIN-TPL-TOOLS
  - Documents `.BIN` as the game 3D model, `.TPL` as texture references used by a model, and `.OBJ/.MTL/.SMD/.IDXUUBIN/.IDXUHDTPL/.IDXMATERIAL/.VTA` as editing/bridge files.
- JADERLINK IDXUHDTPL documentation.
  - https://jaderlink.blogspot.com/2023/11/RE4-UHD-BIN-TOOL-IDXUHDTPL.html
  - Documents `TPL_###`, `PackID`, `TextureID`, `PixelFormatType`, `width`, `height`, and optional wrap/filter/lod/compression fields.
- JADERLINK IDXMATERIAL documentation.
  - https://jaderlink.blogspot.com/2023/11/RE4-UHD-BIN-TOOL-IDXMATERIAL.html
  - Documents UHD 24-byte and PS2 12-byte material layouts, material flags, diffuse/bump/opacity/generic/custom specular texture IDs, intensity fields, and specular scale.
- JADERLINK MTL documentation.
  - https://jaderlink.blogspot.com/2023/11/RE4-UHD-BIN-TOOL-MTL.html
  - Documents the MTL bridge to IDXMATERIAL + IDXUHDTPL, texture commands, path-derived PackID/TextureID, and specular scale.
- JADERLINK/RE4-PS2-BIN-TOOL public releases/README.
  - https://github.com/JADERLINK/RE4-PS2-BIN-TOOL
- JADERLINK/JADERLINK_MODEL_VIEWER source/README.
  - https://github.com/JADERLINK/JADERLINK_MODEL_VIEWER

## Scenario/SMD workflow references

- JADERLINK/RE4-SCENARIO-SMD-TOOLS public README/release notes.
  - https://github.com/JADERLINK/RE4-SCENARIO-SMD-TOOLS
  - Documents `.idxuuscenario`, `.idxggscenario`, `.idxuusmd`, `.idxggsmd`, Scenario OBJ group names, and high-level IDX options.
- JADERLINK/SMD_BONE_TOOLS public README.
  - https://github.com/JADERLINK/SMD_BONE_TOOLS
  - Documents accepted bone naming patterns and the `replacebone` line format.

## Practical tutorials

- Public RE4UHD BIN/EVD modding tutorials for the practical model/cutscene asset map.

## v1.1 notes

The v1.1 push specifically used the JADERLINK MTL documentation to upgrade from texture-reference scanning to block-level MTL parsing and conversion into IDXUHDTPL/IDXMATERIAL metadata. The code intentionally treats the incomplete IDXUUBIN public page as insufficient for a binary writer and therefore does not fake an IDXUUBIN implementation.



## v1.3 source notes

The v1.3 push used the current public RE4-UHD-BIN-TPL-TOOLS README/release notes for `idxuubin`, material ordering, vertex-color switches, morph limitations, and the WeightMap >255 warning. It also used the public C# source for the UHD BIN header/flag names as a sanity check for the existing structural BIN reader.

The Scenario IDX document parser is based on the public RE4-SCENARIO-SMD-TOOLS README sections that document Scenario OBJ group names, `Magic`, file-name fields, `UseIdxUhdTpl`, `UseIdxMaterial`, vertex-color switches, and per-SMD entry fields such as `PositionX`, `TplFileID`, `BinFileID`, `SmxID`, `ObjectStatus`, `FixedFF`, and `Unused1..7`.

The public IDXUUBIN blog page itself is currently sparse, so this package does not claim exact binary semantics for every IDXUUBIN command. The parser stores unknown keys losslessly as text references and only promotes fields that are visible in public release notes or consistent with the source/header names.


## v1.5 source notes

The v1.5 push used the public RE4-PS2-BIN-TOOL README for the PS2 bridge: it documents `.idxps2bin`, `UseIdxMaterial`, `AutoCalcBoundingBox`, PS2 MTL numeric texture IDs, and the PS2 material/OBJ/SMD workflow.

The v1.5 push used the public RE4-PS2-TPL-TOOL README for `.idxps2tpl` and per-image `IdxtplHeader` helpers: it documents `ImageFlipY`, `ImageFolder`, `ImageFormat`, `Bitdepth`, `Interlace`, rotate/swizzle flags, `Width`, `Height`, `Next`, `Qwc`, and `GsTex`.

The Scenario update used the current public RE4-SCENARIO-SMD-TOOLS README for GC/Wii 32-byte alignment, `.idxggscenario/.idxggsmd`, `.idxuuscenario/.idxuusmd`, R100 extra SMD/TPL file names, `ExtraParameter_#`, and the `FILE_00_SMD_000` extra-entry pattern.


## v1.7 references

- JADERLINK/RE4-PS2-TPL-TOOL README: documents `Bitdepth`, `Interlace`, scenario mipmaps, `ImageFlipY`, `ImageFolder`, and `ImageFormat`.
- JADERLINK/RE4-PS2-TPL-TOOL `TplExtractHeaders.cs` / `TplImageHeader.cs`: names the 0x30-byte PS2 TPL image-header fields used by `re4_ps2_tpl_read()`.
- JADERLINK/RE4-PS2-BIN-TOOL README: confirms `.IDXPS2BIN`, `.IDXMATERIAL`, OBJ/SMD support, PS2 MTL numeric texture IDs, material ordering, and `AutoCalcBoundingBox`.
- JADERLINK/RE4-SCENARIO-SMD-TOOLS README: confirms GC/Wii 32-byte scenario alignment, `ObjectStatus` bitflags, R100/BLK extra parameters, and SMD/TPL/BIN folder workflows.

## v1.9 references

- JADERLINK/RE4-PS2-TPL-TOOL `TplImage.cs`: documents the extractor dispatch for bitdepth `0x0008`, `0x0009`, `0x0006`, the non-swizzled palette/index reads, the 16-color palette jump after logical color 7, the 256-color palette group reorder, and the note that bitdepth `0x0006` with swizzled interlace is not supported in that extractor path.
- JADERLINK/RE4-PS2-TPL-TOOL `TplExtract.cs`: documents generated `.idxps2tpl` fields and the `IdxtplHeader` text emitter fields used by the metadata bridge.
