# API sketch

```c
#include "re4c3d/re4c3d.h"

static unsigned char arena_mem[1 << 20];
re4_arena arena;
re4_span file;

re4_arena_init(&arena, arena_mem, sizeof(arena_mem));
file.data = bytes;
file.size = bytes_len;
```

## TPL

```c
re4_tpl tpl;
re4_err er;
re4_size payload_bytes;
re4_span image_payload;

er = re4_tpl_read_gc(file, &arena, &tpl);
er = re4_tpl_image_payload_size(&tpl.images[0], &payload_bytes);
er = re4_tpl_image_span(file, &tpl.images[0], &image_payload);
```

Decode an image into caller-owned RGBA8 memory:

```c
er = re4_tpl_decode_image_rgba32(file, &tpl, 0, out_rgba, out_size, width * 4);
```

## TPL cue tables used beside RE4 BIN models

```c
re4_tplcue cue;
er = re4_tplcue_read_le(file, &arena, &cue);
```

## Archive readers

```c
re4_archive arc;
re4_span payload;

er = re4_gca_read(file, &arena, &arc);
er = re4_archive_entry_span(file, &arc.entries[0], &payload);
```

For guessed DAT/UDAS tables:

```c
er = re4_dat_read_guess(file, RE4_ENDIAN_LITTLE, &arena, &arc);
er = re4_udas_read_guess(file, RE4_ENDIAN_LITTLE, &arena, &arc);
```

## EVD

```c
re4_evd evd;
re4_u32 i;

er = re4_evd_read(file, RE4_ENDIAN_LITTLE, &arena, &evd);
for (i = 0; i < evd.packet_count; ++i) {
    re4_evd_resource_ref ref;
    if (re4_evd_packet_resource_ref(&evd.packets[i], &ref) == RE4_OK) {
        /* ref.model_name, ref.parent_name, ref.bin_name, ref.tpl_name */
    }
}
```

Get an embedded file payload:

```c
re4_span embedded;
er = re4_evd_file_span(file, &evd, 0, &embedded);
```

## LFS

```c
re4_lfs lfs;
re4_span packed_chunk;
re4_size expected_unpacked;

er = re4_lfs_read(file, &arena, &lfs);
er = re4_lfs_chunk_span(file, &lfs.chunks[0], &packed_chunk, &expected_unpacked);
```

Compressed LFS chunks require a host callback:

```c
re4_err my_xmem(void *user, const re4_u8 *src, re4_size src_size,
                re4_u8 *dst, re4_size *dst_size);

er = re4_lfs_extract(file, &lfs, output, output_cap, &written, my_xmem, user_data);
```

## FCV

```c
re4_fcv fcv;
re4_fcv_axis_view x;
re4_fcv_axis_view y;
re4_fcv_axis_view z;
re4_q16 value;

er = re4_fcv_read(file, RE4_ENDIAN_LITTLE, &arena, &fcv);
er = re4_fcv_block_axis_views(file, &fcv.blocks[0], &x, &y, &z);
er = re4_fcv_axis_value_q16(&x, 0, RE4_ENDIAN_LITTLE, &value);
```

`re4_fcv_axis_value_size()` maps the known data-type byte to per-axis scalar byte widths. Values are exposed as Q16.16. Binary32 payload words are converted with integer bit handling only.

## Fixed point

`re4_f32bits_to_q16()` converts raw IEEE-754 binary32 words to signed Q16.16 with integer bit handling only.


## v0.5 additions

### TPL block and palette helpers

```c
re4_u32 bw;
re4_u32 bh;
re4_u32 bb;
re4_span pal;

if (re4_tpl_format_block_info(img.format, &bw, &bh, &bb) == RE4_OK) {
    /* bw/bh/bb describe the tiled encoded payload footprint. */
}
if (re4_tpl_format_is_indexed(img.format)) {
    er = re4_tpl_palette_span(file, &img, &pal);
}
```

### Stored LFS writer

```c
re4_size need;
re4_size written;
re4_u32 chunks;

re4_lfs_store_size(raw.size, &need, &chunks);
er = re4_lfs_write_store(raw, RE4_ENDIAN_LITTLE, out_bytes, out_cap, &written);
```

This writer only emits stored/uncompressed chunk-table LFS. It is useful for safe tests and research bundles; XMem compression is still intentionally host-provided.

### EVD manifest

```c
re4_evd_manifest manifest;

er = re4_evd_build_manifest(&evd, &manifest);
/* manifest.file_counts[RE4_EVD_FILE_BIN], manifest.packet_counts[RE4_EVP_MOT], ... */
```

`re4_evd_packet_resource_ref_e()` is the endian-aware version of the packet reference extractor.

### FCV nearest sampling

```c
re4_q16 xyz[3];

er = re4_fcv_block_sample_nearest_q16(file, &fcv.blocks[0], time, RE4_ENDIAN_LITTLE, xyz);
```

The sampler uses cumulative `Time_shift` values from each axis and selects the nearest key without host-side C real-number operations.

### SMD/BIN convenience helpers

```c
re4_span bin_table;
re4_span tpl_table;
re4_q16 pos[3];

re4_smd_room_bin_table_span(file, &room, &bin_table);
re4_smd_room_tpl_table_span(file, &room, &tpl_table);
er = re4_bin_position_q16(file, &bin_model, 0, pos);
```


## v0.6 additions

### UHD-style TPL metadata

```c
re4_uhd_tpl tpl;

/* wide_offsets is useful for PS4/NS-style table entries; leave false for the
   common UHD/PC layout. */
er = re4_uhd_tpl_read(file, RE4_ENDIAN_LITTLE, RE4_FALSE, &arena, &tpl);
/* tpl.images[0].pack_id, tpl.images[0].texture_id, tpl.images[0].pixel_format */
```

### UHD/modern BIN structural reader

```c
re4_bin_model model;
re4_bin_vertex v;
re4_span strip_bytes;

 er = re4_bin_model_read(file, RE4_ENDIAN_LITTLE, &arena, &model);
 er = re4_bin_vertex_read(file, &model, 0, &v);
 er = re4_bin_material_strip_span(file, &model.materials[0], &strip_bytes);
```

The reader exposes model offsets/counts, material face-buffer records, bone transforms, weight maps, optional bone pairs, and vertex channel spans. Binary32 words are converted through integer-only Q16.16 helpers.

### PMD survey and Scenario SMX

```c
re4_pmd_model pmd;
re4_smx_table smx;

 er = re4_pmd_read_survey(file, &arena, &pmd);
 er = re4_smx_table_read(file, RE4_ENDIAN_LITTLE, &arena, &smx);
```

The PMD path is intentionally a survey reader: it preserves public name, skeleton, node, mesh, and material metadata, but does not yet claim universal PMD vertex/material roundtrip. The SMX path reads fixed-size records into integer and Q16.16 fields.


## v0.7 additions

### TPL cue lookup and IDXUHDTPL-style emission

```c
re4_tplcue cue;
re4_u32 idx;

er = re4_tplcue_read_le(file, &arena, &cue);
er = re4_tplcue_find(&cue, 0x41000001U, 0U, &idx);
/* re4_tplcue_record_pack_id(&cue.records[idx]) */
/* re4_tplcue_record_texture_id(&cue.records[idx]) */
```

To write a text view, provide a callback; the library never opens files and never owns heap memory:

```c
static re4_err my_emit(void *user, const char *text, re4_size len);

er = re4_tplcue_emit_idxuhdtpl(&cue, 0x0EU, 512U, 512U, my_emit, user);
```

### BIN material core and IDXMATERIAL-style emission

```c
re4_bin_material_core mat;
re4_span raw24;

raw24.data = model.materials[0].raw;
raw24.size = 24;
er = re4_bin_material_core_read_uhd24(raw24, &mat);
if (re4_bin_material_core_has(&mat, RE4_BIN_MATERIAL_F_OPACITY_MAP)) {
    /* mat.opacity_map is active */
}
er = re4_bin_material_core_emit_idxmaterial(&mat, 0, my_emit, user);
```

`re4_bin_material_core_read_ps2_12()` maps the compact 12-byte PS2-style core into the same public structure.

### SMD bone text helpers

```c
re4_u32 id;
re4_smd_bone_rule rule;
re4_strref name;
re4_strref line;

er = re4_smd_bone_id_from_name(name, &id);
er = re4_smd_replacebone_line_parse(line, &rule);
```

The bone-name helper accepts `BONE_003`, `bone_3`, and names containing `b0ne_003`. The replacebone parser follows `old_name|new_ID|new_name|parent_ID` lines.

## v0.9 additions

### IDXUHDTPL editable text

```c
re4_idxuhdtpl idx;
re4_u32 found;

er = re4_idxuhdtpl_read_text(text_file, &arena, &idx);
er = re4_idxuhdtpl_find(&idx, 0x41000001U, 15U, &found);
er = re4_idxuhdtpl_emit_text(&idx, my_emit_callback, user_data);
```

The parser accepts `TPL_###`, `PackID`, `TextureID`, `PixelFormatType`, `width`, `height`, `wrap_s`, `wrap_t`, `min_filter`, `mag_filter`, `lod_bias`, `enable_lod`, `min_lod`, `max_lod`, and `is_compressed`. `lod_bias` is stored as Q16.16 by parsing decimal text manually.

### IDXMATERIAL editable text

```c
re4_idxmaterial mats;

 er = re4_idxmaterial_read_text(text_file, &arena, &mats);
 er = re4_idxmaterial_emit_text(&mats, my_emit_callback, user_data);
```

Each entry uses the existing `re4_bin_material_core` fields. The parser accepts `UseMaterial`, `material_flag`, `diffuse_map`, `bump_map`, `opacity_map`, `generic_specular_map`, `intensity_specular_r/g/b`, `specular_scale`, `custom_specular_map`, and known unknown/padding fields.

### MTL texture bridge

```c
re4_mtl_texture_ref ref;

if (re4_mtl_texture_ref_parse(line, &ref) == RE4_OK) {
    /* ref.role, ref.pack_id, ref.texture_id, ref.ext, ref.scale_x_units */
}
```

Supported commands include `map_Kd`, `map_Bump`, `Bump`, `map_d`, and `map_Ns -s X Y Z path`. Patch paths are decoded as PackID folder + TextureID file stem, for example `41000001/0004.tga`.

```c
re4_u8 packed;
re4_u32 sx;
re4_u32 sy;

packed = re4_mtl_specular_scale_pack(4U, 3U);
re4_mtl_specular_scale_unpack(packed, &sx, &sy);
```

### Scenario OBJ/IDX text helpers

```c
re4_scenario_group_ref group;
re4_scenario_idx_options opt;

 er = re4_scenario_group_name_parse(re4_str_lit("UHDSCENARIO#SMD_000#SMX_001#TYPE_08#BIN_002#"), &group);
 er = re4_scenario_idx_options_read_text(text_file, &opt);
```

Group parsing accepts `UHDSCENARIO`, `UUSCENARIO`, `GGSCENARIO`, and `MAINSCENARIO` prefixes with `#` or `_` separators. It also recognizes simple `FILE_00_SMD_003` style references.

## v1.1 additions

### Full MTL material blocks

```c
re4_mtl mtl;
re4_idxuhdtpl generated_tpl;
re4_idxmaterial generated_materials;

er = re4_mtl_read_text(mtl_text, &arena, &mtl);
er = re4_mtl_collect_idxuhdtpl(&mtl, 512U, 512U, &arena, &generated_tpl);
er = re4_idxmaterial_from_mtl(&mtl, &generated_tpl, &arena, &generated_materials);
```

`re4_mtl_read_text()` groups `newmtl` material blocks and captures `Ks`, `map_Kd`, `map_Bump`, `Bump`, `map_d`, and `map_Ns -s`. The `Bump` command is treated as the higher-priority bump/normal source when both `map_Bump` and `Bump` appear for a material.

`re4_mtl_collect_idxuhdtpl()` creates a unique, ordered `PackID/TextureID` table from all material texture refs. Pixel format type is inferred from the material role and extension using the public JADERLINK mapping: DDS diffuse/specular as `0E`, TGA opacity as `00`, TGA bump/normal as `03`, and TGA custom specular as `06`.

`re4_mtl_material_make_core()` and `re4_idxmaterial_from_mtl()` resolve material texture paths against an IDXUHDTPL order, then build `re4_bin_material_core` entries with diffuse, bump, opacity, generic specular, custom specular, RGB specular intensity, and packed specular scale fields.



## v1.3 text-index APIs

### Scenario IDX full document

```c
re4_err re4_scenario_idx_read_text(re4_span text, re4_arena *arena, re4_scenario_idx_doc *out);
re4_bool re4_scenario_idx_entry_has_status(const re4_scenario_idx_entry *entry, re4_u32 flag);
```

`re4_scenario_idx_read_text()` extends the earlier option-only reader. It keeps the existing `re4_scenario_idx_options` block and allocates an arena array of `re4_scenario_idx_entry` records for `SMD_###` / `FILE_##_SMD_###` sections. Transform values are parsed into Q16.16 integers.

### IDXMATERIAL order checks

```c
re4_bool re4_idxmaterial_entry_is_transparent(const re4_idxmaterial_entry *entry);
re4_err re4_idxmaterial_validate_order(const re4_idxmaterial *idx, re4_idxmaterial_order_report *out);
re4_err re4_idxmaterial_make_order_plan(const re4_idxmaterial *idx, re4_arena *arena, re4_u32 **out_indices, re4_u32 *out_count);
```

The validator reports whether opaque materials appear after transparent materials and whether material names are out of case-insensitive alphabetical order. The order plan returns source indices in a safe repack order without moving user data.

### IDXUUBIN key/value parser

```c
re4_err re4_idxuubin_read_text(re4_span text, re4_arena *arena, re4_idxuubin *out);
re4_err re4_idxuubin_find_first(const re4_idxuubin *doc, const char *key, re4_idxuubin_kv *out);
```

`re4_idxuubin_read_text()` is intentionally generic: it records section headers plus `key:value` pairs, line numbers, optional decimal integers, booleans, and Q16.16 values. It promotes common fields such as `UseVertexColor`, `MaterialCount`, `BoneCount`, `WeightMapCount`, `TexCount`, and `MorphCount` into summary fields.


## v1.5 PS2/R100 helper APIs

```c
re4_err re4_idxps2bin_read_text(re4_span text, re4_arena *arena, re4_idxps2bin *out);
re4_err re4_idxps2bin_find_first(const re4_idxps2bin *doc, const char *key, re4_idxuubin_kv *out);

re4_err re4_idxps2tpl_read_text(re4_span text, re4_idxps2tpl *out);
re4_err re4_idxtpl_header_read_text(re4_span text, re4_idxtpl_header *out);
re4_bool re4_idxtpl_header_has_interlace(const re4_idxtpl_header *h, re4_u32 flag);
re4_u32 re4_ps2tpl_bitdepth_palette_limit(re4_u32 bitdepth);

re4_err re4_mtl_ps2_texture_id_from_path(re4_strref path, re4_u32 *out_texture_id);
re4_u32 re4_scenario_alignment_for_prefix(re4_scenario_prefix prefix);
re4_err re4_scenario_r100_read_text(re4_span text, re4_arena *arena, re4_scenario_r100 *out);
```

The PS2 helpers are text/metadata readers. They do not claim to rebuild PS2 binary assets by themselves; they expose the public helper-file fields to host tools that own buffers and write policy.


## v1.7 PS2 binary TPL APIs

```c
re4_err re4_ps2_tpl_read(re4_span src, re4_endian endian, re4_arena *arena, re4_ps2_tpl *out);
re4_bool re4_ps2_tpl_image_has_mipmaps(const re4_ps2_tpl_image *img);
re4_err re4_ps2_tpl_indexes_span(re4_span src, const re4_ps2_tpl_image_header *h, re4_span *out);
re4_err re4_ps2_tpl_palette_span(re4_span src, const re4_ps2_tpl_image_header *h, re4_span *out);
re4_err re4_ps2_tpl_main_header_as_idxtpl(const re4_ps2_tpl_image *img, re4_idxtpl_header *out);
re4_size re4_ps2_tpl_index_payload_size_for_header(const re4_ps2_tpl_image_header *h);
re4_size re4_ps2_tpl_palette_payload_size_for_header(const re4_ps2_tpl_image_header *h);
re4_u32 re4_ps2_tpl_palette_source_index(re4_u32 bitdepth, re4_u32 logical_index);
re4_err re4_ps2_tpl_palette_entry_rgba32(re4_span palette, re4_u32 bitdepth, re4_u32 logical_index, re4_u8 out_rgba[4]);
re4_err re4_ps2_tpl_decode_header_rgba32(re4_span src, const re4_ps2_tpl_image_header *h,
                                         re4_u8 *dst, re4_size dst_size, re4_u32 dst_stride);
re4_err re4_ps2_tpl_decode_image_rgba32(re4_span src, const re4_ps2_tpl *tpl, re4_u32 image_index,
                                        re4_u8 *dst, re4_size dst_size, re4_u32 dst_stride);

re4_err re4_idxtpl_header_emit_text(const re4_idxtpl_header *h, re4_emit_fn emit, void *emit_user);
re4_bool re4_idxtpl_header_has_mipmaps(const re4_idxtpl_header *h);
re4_err re4_idxtpl_header_validate_basic(const re4_idxtpl_header *h, re4_u32 *out_warnings);
const char *re4_idxtpl_warning_label(re4_u32 warning);
```

`re4_ps2_tpl_read()` is a defensive metadata reader for the PS2 TPL layout documented by the public extractor/repacker source: a main header followed by 0x30-byte image headers. v1.9 adds caller-buffer RGBA32 decode for the non-swizzled bitdepth paths `0x0008`, `0x0009`, and `0x0006`. If the `Interlace` swizzle bit is set, the decoder returns `RE4_ERR_UNSUPPORTED` rather than guessing.
