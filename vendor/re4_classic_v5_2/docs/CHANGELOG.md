## v3.4
- Fixed complete decimal PMD bone identity parsing (10 vs 100 collision).
- Added per-bone link origins, native C89 hierarchy/duplicate-target audit,
  a C89 CLI and an offline report driver for the existing 484 candidate pairs.
- Added opt-in strict mode to multipart clip export and raised maximum PMD
  input size to 16 MiB for large boss models.
- Added 2 regression suites; 17 suites pass normal and ASan/UBSan builds.

## v3.0
- Added deterministic fixed-point time-varying crossfade curves (linear, smoothstep, smootherstep), CLI ramp support and per-frame weight telemetry.
- Added monotonic full-range tests, validated 4-part Leon/Ganado OBJ+VTK previews, and sanitizer regressions.
- Root/IK native behavior not yet validated.

## v2.9

- Added two-FCV local skeleton blending with shortest Euler arc, independent duration normalization, multipart OBJ export and CSV telemetry.
- Added root-relative crossfade with overflow protection and optional bounded CCD axis correction.
- Added regression tests and verified Leon and Ganado on real FCV/PMD files via VTK.

## v2.8 (2026-11-01)
- Added type-1 primary root-position sample/delta API; optional translated master pose with checked Q16.16 arithmetic and derived accessory movement.
- Added root catalog tool and exported two user-source real-data CSVs separately.
- Added opt-in IK contact window to bound the original diagnostic CCD locks.
- Added root-motion test suite; 10 total passing with GCC C89; sanitizer validation. See ROOT_MOTION_CONTACT_V2_8.md.

## v2.7 (2026-11-01)
- Added FCV extraction CLI, complete multipart clip-frame exporter, IK root candidates and opt-in axis CCD contact locks; nearest mapped ancestor fix. See `CHARACTER_IK_CLIPS_V2_7.md`.

# Changelog

## v1.9-alpha

- Added `texture/re4_ps2_tpl_codec.c` with caller-buffer PS2 TPL RGBA32 decode for clear non-swizzled paths: bitdepth `0x0008` 4-bit indexed, bitdepth `0x0009` 8-bit indexed, and bitdepth `0x0006` direct RGBA.
- Added `re4_ps2_tpl_palette_source_index()` and `re4_ps2_tpl_palette_entry_rgba32()` for the PS2 16-color and 256-color palette ordering seen in the public extractor source.
- Added payload-size helpers for PS2 TPL image indices and palettes. The 4-bit indexed palette path now reports the required 0x80-byte source palette span, matching the public extractor source.
- Added `re4_ps2_tpl_decode_header_rgba32()` and `re4_ps2_tpl_decode_image_rgba32()`; swizzled interlace values intentionally return `RE4_ERR_UNSUPPORTED` until a verified block mapper is added.
- Expanded selftests for PS2 palette remapping, 4-bit indexed decode, 8-bit indexed decode, direct RGBA decode, and swizzle refusal behavior.
- Re-ran C89 build, selftest, and `.c/.h` source-token audit for heap and C real-number type names.

## v1.7-alpha

- Added PS2 binary TPL parser for the 16-byte main header and 0x30-byte per-image `TplImageHeader` records used by JADERLINK's PS2 TPL workflow.
- The PS2 TPL parser exposes Width, Height, Bitdepth, Interlace, Next, MipmapStatus, Qwc, Ref, MipmapHeader offsets, GsMip words, IndexesOffset, PaletteOffset, and GsTex words.
- Added mipmap-header following for `MipmapStatus == 0x02`, plus index-span and palette-span helpers derived from `Qwc`, `IndexesOffset`, `PaletteOffset`, and bitdepth palette limits.
- Extended text `IdxtplHeader` parser and added emitter/validator helpers for the newly exposed PS2 fields.
- Extended `re4_probe` so `.TPL` probing tries GameCube/Wii TPL, UHD companion TPL, PS2 TPL, and then cue-style TPL records.
- Expanded selftests for PS2 binary TPL, extended IdxtplHeader parsing, warning masks, mipmap offsets, index spans, and palette spans.
- Re-ran C89 build, selftest, and `.c/.h` source-token audit for heap and C real-number type names.

## v1.5-alpha

- Added `re4_idxps2bin_read_text()` and `re4_idxps2bin_find_first()` for PS2 editable BIN metadata helpers such as `UseIdxMaterial`, `AutoCalcBoundingBox`, `MaterialCount`, `BoneCount`, and `WeightMapCount`.
- Added `re4_idxps2tpl_read_text()` for PS2 TPL repack metadata: `ImageFlipY`, `ImageFolder`, and `ImageFormat`.
- Added `re4_idxtpl_header_read_text()`, `re4_idxtpl_header_has_interlace()`, and `re4_ps2tpl_bitdepth_palette_limit()` for PS2 per-image `IdxtplHeader` data: `Bitdepth`, `Interlace`, `Width`, `Height`, `Next`, `Qwc`, and `GsTex`.
- Added `re4_mtl_ps2_texture_id_from_path()` for PS2 MTL numeric texture IDs.
- Added `re4_scenario_r100_read_text()` for R100/BLK helper files with `ExtraParameter_#`, `ExtraSmdFileName_#`, `ExtraFileName_#`, and `ExtraTplFileName_#`.
- Added `re4_scenario_alignment_for_prefix()` so `GGSCENARIO`/GC-Wii flows report 32-byte alignment and other scenario targets report 16-byte default alignment.
- Extended `re4_probe` with `.idxps2bin`, `.idxps2tpl`, `.IdxtplHeader`, `.idxr100extract`, `.idxuur100repack`, `.idxggr100repack`, and alignment diagnostics for Scenario OBJ groups.
- Added selftests for PS2 TPL helper text, PS2 BIN helper text, PS2 MTL numeric texture IDs, R100 extras, and scenario alignment.
- Re-ran C89 build, selftest, and `.c/.h` source-token audit for heap and C real-number type names.

## v1.3-alpha

- Added `re4_scenario_idx_read_text()` for full Scenario IDX documents, including `SMD_###` entries, Q16.16 Position/Angle/Scale fields, `TplFileID`, `BinFileID`, `SmxID`, `ObjectStatus`, `FixedFF`, and `Unused1..7`.
- Added `re4_scenario_idx_entry_has_status()` for documented Scenario `ObjectStatus` bit checks.
- Added `re4_idxuubin_read_text()` and `re4_idxuubin_find_first()` as a conservative parser for the decimal `idxuubin` metadata bridge introduced by public V1.4+ tooling.
- Added IDXMATERIAL transparency detection, order validation, and a stable order-plan builder that places opaque materials before transparent materials and sorts names case-insensitively inside each group.
- Extended `re4_probe` with `.idxuubin` inspection and richer Scenario IDX entry diagnostics.
- Added selftests for Scenario per-entry parsing, IDXUUBIN key/value parsing, material-order validation, and order-plan generation.
- Re-ran C89 build, selftest, and `.c/.h` source-token audit for heap and C real-number type names.


## v1.1-alpha

- Added full MTL document parser for `newmtl`, `Ks`, `map_Kd`, `map_Bump`, `Bump`, `map_d`, and `map_Ns -s` blocks.
- Added `re4_mtl_read_text()`, `re4_mtl_collect_idxuhdtpl()`, `re4_mtl_material_make_core()`, and `re4_idxmaterial_from_mtl()`.
- Added MTL material structs and seen flags, including the documented `Bump` override behavior over `map_Bump`.
- Added MTL-to-IDXUHDTPL collection with duplicate PackID/TextureID elimination and role-derived PixelFormatType values.
- Added MTL-to-IDXMATERIAL material-core synthesis with bump, opacity, generic specular, custom specular, specular scale, and Ks color conversion through integer math only.
- Extended `re4_probe` so `.mtl` now prints material blocks and previews generated IDXUHDTPL/IDXMATERIAL metadata.
- Expanded selftests for full MTL parsing, duplicate texture elimination, `Bump` override, custom specular, generic specular, and material flag synthesis.
- Re-ran C89 build, selftest, and `.c/.h` source-token audit for heap and C real-number type names.

## v0.9-alpha

- Added `re4_idxuhdtpl_read_text()`, `re4_idxuhdtpl_find()`, `re4_idxuhdtpl_emit_text()`, and `re4_idxuhdtpl_entry_emit_text()`.
- Added IDXUHDTPL entry fields for PackID, TextureID, PixelFormatType, width, height, wrap/filter/lod metadata, enable_lod, min/max_lod, and is_compressed.
- Added `re4_idxmaterial_read_text()` and `re4_idxmaterial_emit_text()` for editable material metadata backed by the existing UHD24/PS2-12 material core structure.
- Added MTL texture-reference scanner for `map_Kd`, `map_Bump`, `Bump`, `map_d`, and `map_Ns -s` lines.
- Added MTL patch-path extraction for PackID folder and TextureID file stems, plus DDS/TGA role mapping for common PixelFormatType values.
- Added `re4_mtl_specular_scale_pack()` and `re4_mtl_specular_scale_unpack()` for the two-nibble specular-scale bridge.
- Added scenario group parser for `UHDSCENARIO#SMD_000#SMX_000#TYPE_08#BIN_000#` and underscore variants.
- Added scenario IDX option parser for Magic, SmdFileName, TplFileName, BinFolder, SharedFileName, UseIdxUhdTpl, UseIdxMaterial, EnableVertexColor, and EnableDinamicVertexColor/EnableDynamicVertexColor.
- Extended `re4_probe` to inspect `.idxuhdtpl`, `.idxmaterial`, `.mtl`, `.idxuuscenario`, `.idxggscenario`, `.idxuusmd`, `.idxggsmd`, and scenario `.obj` group lines.
- Expanded selftests for IDXUHDTPL parse/find/emit, IDXMATERIAL parse/emit, MTL path + pixel-format mapping, specular-scale packing, scenario group parsing, and scenario IDX options.
- Re-ran C89 build, selftest, and `.c/.h` source-token audit for heap and C real-number type names.

## v0.7-alpha

- Added public `re4_emit_fn` callback type for heapless text views.
- Added TPL cue PackID/TextureID accessors, lookup by pair, and IDXUHDTPL-style callback emitter.
- Added UHD24 and PS2-12 BIN material-core readers, material flag constants, flag labels, active-flag helper, and IDXMATERIAL-style callback emitter.
- Added SMD bone-name ID extraction for `BONE_`, `bone_`, and `b0ne_` patterns.
- Added SMD `replacebone` line parser for `old_name|new_ID|new_name|parent_ID`.
- Extended `re4_probe` TPL/BIN diagnostics to show cue PackID/TextureID and material-core flags.
- Expanded selftests for TPL cue lookup/emission, material-core mapping/emission, PS2-12 material mapping, and SMD bone text helpers.
- Re-ran C89 build, selftest, and `.c/.h` source-token audit for heap and C real-number type names.

## v0.6-alpha

- Added UHD/modern BIN structural reader for header offsets/counts, materials, face-buffer strips, bones, weight maps, optional bone pairs, and true-vertex/triangle hints.
- Added BIN vertex reader exposing position, normal, UV, color, and weight-index channels as Q16.16/integer data without C real-number types.
- Added UHD-style TPL metadata reader for image count, table offsets, optional wide offsets, image headers, palette headers, compression flag, PackID, and TextureID.
- Added PMD 2007 survey reader for public name tables, skeleton blocks, node/mesh metadata, and material metadata.
- Added Scenario SMX table/record reader with color, flags, and texture-movement fields converted to Q16.16.
- Expanded `re4_probe` for UHD TPL, BIN, PMD, SMD, and SMX diagnostics.
- Added selftests for UHD TPL metadata, BIN structural reads, PMD survey, and SMX records.
- Re-ran C89 build, selftest, and `.c/.h` source-token audit for heap and C real-number type names.

## v0.5-alpha

- Added stored/uncompressed LFS chunk-table writer plus roundtrip selftest.
- Fixed RDLX-stream heuristic so valid chunk-table LFS files are not misclassified as stream wrappers.
- Added TPL indexed-format detection, block footprint helper, palette payload size, and palette span helper.
- Added EVD file-kind classifier and manifest builder for embedded files, packet counts, and BIN/TPL/model/parent references.
- Added endian-aware `re4_evd_packet_resource_ref_e()`.
- Added FCV cumulative-time lookup, nearest-axis sampler, and block XYZ nearest sampler into Q16.16.
- Added SMD room BIN/TPL/tail span helpers and line-reference checks for TPL/SMX ids.
- Added BIN position Q16 helper around the structural vertex reader.
- Expanded `re4_probe` output for TPL/EVD/FCV/SMD diagnostics.
- Expanded selftests for LFS writing, EVD manifests, TPL helpers, BIN position, SMD refs, and FCV sampling.
- Re-ran C89 build, selftest, and `.c/.h` source-token audit for heap and C real-number type names.

## v0.4-alpha

- Corrected FCV axis-view sizing to follow per-axis scalar patterns instead of dividing tuple size evenly.
- Added public `re4_fcv_axis_value_size()`, `re4_fcv_axis_value_q16()`, `re4_fcv_bone_type_name()`, and `re4_fcv_scalar_name()`.
- FCV key values now expose binary32 words, signed 16-bit values, and signed 8-bit values as Q16.16 without C real-number source types.
- Expanded `re4_probe` for FCV block 0 diagnostics: bone type, data type, tuple size, axis scalar widths, tag counts, and scalar kind names.
- Added selftest coverage for FCV data type `0x10` axis sizes and Q16.16 key conversion.
- Re-ran C89 build, selftest, and `.c/.h` source-token audit for heap and C real-number type names.

## v0.3-alpha

- Added public `re4_gca_read()`.
- Added GCAX wrapper detection and inner GCA3 metadata parsing.
- Added GCA file-name references, flags, packed/unpacked sizes, and inferred sequential payload spans for uncompressed archives.
- Added DAT/UDAS offset-only fallback table parsing.
- Removed duplicate archive-span implementation from the DAT reader path.
- Corrected EVD SetOm resource offsets to `NameMod[12]`, `NameBin[48]`, `NameTpl[48]` layout.
- Expanded `re4_probe` for `.GCA`, `.DAT`, `.UDAS`, and `.ESL`.
- Added public archive-entry flags for inferred offsets, compressed-size hints, and name references.
- Added selftests for GCA metadata, DAT offset-only tables, EVD SetParts references, TPL payload spans, LFS extraction, and ESL.
- Re-ran source token audit for heap and C real-number type names in `.c/.h` files.

## v0.2-alpha

- Fixed LFS endian detection for X360-style big-endian headers.
- Added `re4_lfs_chunk_span()` and `re4_archive_entry_span()` helpers.
- Added TPL payload byte estimator and image payload span helper.
- Added early TPL payload bounds validation before decode.
- Added typed EVD resource reference extraction for SetOm, SetParts, Mot, Shp, Cam, Lit, Fog, Focus, SetEff, SetMdt, Pos, ParentOn, and common model-name-only packets.
- Added EVD embedded-file span helper.
- Added model probe hints for embedded TPL signatures and possible FCV signatures.
- Added `re4_model_kind_name()` helper.
- Added FCV structural validator.
- Expanded `re4_probe` output.

## v0.1-alpha

- Initial C89/arena-only core.
- Initial GameCube/Wii TPL parser and RGBA8 decoders.
- Initial EVD, LFS, AFS, ESL parsers.
- Initial FCV structural read and BIN/PMD/SMD probes.
