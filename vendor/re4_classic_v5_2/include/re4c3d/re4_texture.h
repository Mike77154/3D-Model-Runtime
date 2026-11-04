#ifndef RE4_TEXTURE_H
#define RE4_TEXTURE_H

#include "re4_types.h"
#include "re4_arena.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum re4_tpl_format_e {
    RE4_TPL_I4 = 0,
    RE4_TPL_I8 = 1,
    RE4_TPL_IA4 = 2,
    RE4_TPL_IA8 = 3,
    RE4_TPL_RGB565 = 4,
    RE4_TPL_RGB5A3 = 5,
    RE4_TPL_RGBA8 = 6,
    RE4_TPL_CI4 = 8,
    RE4_TPL_CI8 = 9,
    RE4_TPL_CI14X2 = 10,
    RE4_TPL_CMPR = 14
} re4_tpl_format;

typedef enum re4_tpl_palette_format_e {
    RE4_TPL_PAL_IA8 = 0,
    RE4_TPL_PAL_RGB565 = 1,
    RE4_TPL_PAL_RGB5A3 = 2
} re4_tpl_palette_format;

typedef struct re4_tpl_image_s {
    re4_u16 width;
    re4_u16 height;
    re4_u32 format;
    re4_u32 data_offset;
    re4_u32 wrap_s;
    re4_u32 wrap_t;
    re4_u32 min_filter;
    re4_u32 mag_filter;
    re4_u32 lod_bias_bits;
    re4_u8 edge_lod;
    re4_u8 min_lod;
    re4_u8 max_lod;
    re4_u32 palette_offset;
    re4_u16 palette_items;
    re4_u32 palette_format;
    re4_u32 palette_data_offset;
} re4_tpl_image;

typedef struct re4_tpl_s {
    re4_u32 image_count;
    re4_tpl_image *images;
    re4_endian endian;
} re4_tpl;

typedef struct re4_tplcue_record_s {
    re4_u32 a;
    re4_u32 b;
    re4_u32 likely_texture_id;
    re4_u32 likely_pack_id;
} re4_tplcue_record;

typedef struct re4_tplcue_s {
    re4_u32 record_count;
    re4_tplcue_record *records;
} re4_tplcue;


typedef struct re4_uhd_tpl_image_s {
    re4_u16 height;
    re4_u16 width;
    re4_u32 pixel_format;
    re4_u32 second_offset;
    re4_u32 wrap_s;
    re4_u32 wrap_t;
    re4_u32 min_filter;
    re4_u32 mag_filter;
    re4_u32 lod_bias_bits;
    re4_u8 enable_lod;
    re4_u8 min_lod;
    re4_u8 max_lod;
    re4_u8 is_compressed;
    re4_u32 pack_id;
    re4_u32 texture_id;
    re4_u32 image_header_offset;
    re4_u32 palette_header_offset;
    re4_u8 has_palette;
    re4_u16 colors_count;
    re4_u8 unpacked;
    re4_u8 pad;
    re4_u32 palette_format;
} re4_uhd_tpl_image;


#define RE4_IDXUHDTPL_SEEN_PACK_ID        0x00000001U
#define RE4_IDXUHDTPL_SEEN_TEXTURE_ID     0x00000002U
#define RE4_IDXUHDTPL_SEEN_PIXEL_FORMAT   0x00000004U
#define RE4_IDXUHDTPL_SEEN_WIDTH          0x00000008U
#define RE4_IDXUHDTPL_SEEN_HEIGHT         0x00000010U

typedef struct re4_idxuhdtpl_entry_s {
    re4_u32 id;
    re4_u32 pack_id;
    re4_u32 texture_id;
    re4_u32 pixel_format_type;
    re4_u32 width;
    re4_u32 height;
    re4_u32 wrap_s;
    re4_u32 wrap_t;
    re4_u32 min_filter;
    re4_u32 mag_filter;
    re4_q16 lod_bias_q16;
    re4_u8 enable_lod;
    re4_u8 min_lod;
    re4_u8 max_lod;
    re4_u8 is_compressed;
    re4_u32 seen_mask;
} re4_idxuhdtpl_entry;

typedef struct re4_idxuhdtpl_s {
    re4_u32 entry_count;
    re4_idxuhdtpl_entry *entries;
} re4_idxuhdtpl;


/* RE4 PS2 TPL helper text formats from JADERLINK RE4-PS2-TPL-TOOL. */
#define RE4_IDXPS2TPL_SEEN_IMAGE_FLIP_Y 0x00000001U
#define RE4_IDXPS2TPL_SEEN_IMAGE_FOLDER 0x00000002U
#define RE4_IDXPS2TPL_SEEN_IMAGE_FORMAT 0x00000004U

typedef struct re4_idxps2tpl_s {
    re4_bool image_flip_y;
    re4_strref image_folder;
    re4_strref image_format;
    re4_u32 seen_mask;
} re4_idxps2tpl;

#define RE4_IDXTPL_HEADER_SEEN_BITDEPTH 0x00000001U
#define RE4_IDXTPL_HEADER_SEEN_INTERLACE 0x00000002U
#define RE4_IDXTPL_HEADER_SEEN_WIDTH    0x00000004U
#define RE4_IDXTPL_HEADER_SEEN_HEIGHT   0x00000008U
#define RE4_IDXTPL_HEADER_SEEN_NEXT     0x00000010U
#define RE4_IDXTPL_HEADER_SEEN_QWC      0x00000020U
#define RE4_IDXTPL_HEADER_SEEN_GSTEX    0x00000040U
#define RE4_IDXTPL_HEADER_SEEN_MIPMAP_STATUS 0x00000080U
#define RE4_IDXTPL_HEADER_SEEN_REF      0x00000100U
#define RE4_IDXTPL_HEADER_SEEN_MIP1OFF  0x00000200U
#define RE4_IDXTPL_HEADER_SEEN_MIP2OFF  0x00000400U
#define RE4_IDXTPL_HEADER_SEEN_GSMIP    0x00000800U
#define RE4_IDXTPL_HEADER_SEEN_INDEXES_OFFSET 0x00001000U
#define RE4_IDXTPL_HEADER_SEEN_PALETTE_OFFSET 0x00002000U

#define RE4_PS2TPL_INTERLACE_ROTATE_IMAGE 0x01U
#define RE4_PS2TPL_INTERLACE_SWIZZLE      0x02U

#define RE4_PS2TPL_MIPMAP_NONE 0x00U
#define RE4_PS2TPL_MIPMAP_TWO  0x02U

#define RE4_IDXTPL_WARN_ZERO_DIM        0x00000001U
#define RE4_IDXTPL_WARN_WIDTH_LT_HEIGHT 0x00000002U
#define RE4_IDXTPL_WARN_BAD_BITDEPTH    0x00000004U
#define RE4_IDXTPL_WARN_BAD_INTERLACE   0x00000008U
#define RE4_IDXTPL_WARN_ZERO_QWC        0x00000010U
#define RE4_IDXTPL_WARN_BAD_MIPMAP      0x00000020U
#define RE4_IDXTPL_WARN_MIP_OFFSET_GAP  0x00000040U

typedef struct re4_idxtpl_header_s {
    re4_u32 bitdepth;
    re4_u32 interlace;
    re4_u32 width;
    re4_u32 height;
    re4_u32 next;
    re4_u32 qwc;
    re4_u32 gs_tex;
    re4_u32 mipmap_status;
    re4_u32 ref;
    re4_u32 mipmap_header1_offset;
    re4_u32 mipmap_header2_offset;
    re4_u32 gs_mip_lo;
    re4_u32 gs_mip_hi;
    re4_u32 indexes_offset;
    re4_u32 palette_offset;
    re4_u32 gs_tex_hi;
    re4_u32 seen_mask;
} re4_idxtpl_header;

typedef struct re4_ps2_tpl_image_header_s {
    re4_u16 width;
    re4_u16 height;
    re4_u16 bitdepth;
    re4_u16 interlace;
    re4_u16 next;
    re4_u16 mipmap_status;
    re4_u16 qwc;
    re4_u16 ref;
    re4_u32 mipmap_header1_offset;
    re4_u32 mipmap_header2_offset;
    re4_u32 gs_mip_lo;
    re4_u32 gs_mip_hi;
    re4_u32 indexes_offset;
    re4_u32 palette_offset;
    re4_u32 gs_tex_lo;
    re4_u32 gs_tex_hi;
    re4_u32 source_offset;
} re4_ps2_tpl_image_header;

typedef struct re4_ps2_tpl_image_s {
    re4_ps2_tpl_image_header main_header;
    re4_ps2_tpl_image_header mipmap1_header;
    re4_ps2_tpl_image_header mipmap2_header;
    re4_u8 has_mipmap1;
    re4_u8 has_mipmap2;
    re4_u8 pad0;
    re4_u8 pad1;
} re4_ps2_tpl_image;

typedef struct re4_ps2_tpl_s {
    re4_u32 magic;
    re4_u32 image_count;
    re4_u32 start_offset;
    re4_u32 header_unk1;
    re4_ps2_tpl_image *images;
    re4_endian endian;
} re4_ps2_tpl;

typedef struct re4_uhd_tpl_s {
    re4_u32 magic;
    re4_u32 image_count;
    re4_u32 table_offset;
    re4_uhd_tpl_image *images;
    re4_endian endian;
    re4_u8 wide_offsets;
} re4_uhd_tpl;

re4_err re4_tpl_read_gc(re4_span src, re4_arena *arena, re4_tpl *out);
re4_err re4_tpl_image_payload_size(const re4_tpl_image *img, re4_size *out_bytes);
re4_err re4_tpl_palette_payload_size(const re4_tpl_image *img, re4_size *out_bytes);
re4_err re4_tpl_image_span(re4_span src, const re4_tpl_image *img, re4_span *out);
re4_err re4_tpl_palette_span(re4_span src, const re4_tpl_image *img, re4_span *out);
re4_bool re4_tpl_format_is_indexed(re4_u32 fmt);
re4_err re4_tpl_format_block_info(re4_u32 fmt, re4_u32 *block_w, re4_u32 *block_h, re4_u32 *block_bytes);
re4_err re4_tpl_decode_image_rgba32(re4_span src, const re4_tpl *tpl, re4_u32 image_index,
                                    re4_u8 *dst, re4_size dst_size, re4_u32 dst_stride);
re4_err re4_tplcue_read_le(re4_span src, re4_arena *arena, re4_tplcue *out);
re4_u32 re4_tplcue_record_pack_id(const re4_tplcue_record *rec);
re4_u32 re4_tplcue_record_texture_id(const re4_tplcue_record *rec);
re4_err re4_tplcue_find(const re4_tplcue *cue, re4_u32 pack_id, re4_u32 texture_id, re4_u32 *out_index);
re4_err re4_tplcue_emit_idxuhdtpl(const re4_tplcue *cue, re4_u32 pixel_format_type,
                                  re4_u32 width, re4_u32 height,
                                  re4_emit_fn emit, void *emit_user);
re4_err re4_uhd_tpl_read(re4_span src, re4_endian endian, re4_bool wide_offsets,
                            re4_arena *arena, re4_uhd_tpl *out);

re4_err re4_idxps2tpl_read_text(re4_span text, re4_idxps2tpl *out);
re4_err re4_idxtpl_header_read_text(re4_span text, re4_idxtpl_header *out);
re4_err re4_idxtpl_header_emit_text(const re4_idxtpl_header *h, re4_emit_fn emit, void *emit_user);
re4_bool re4_idxtpl_header_has_interlace(const re4_idxtpl_header *h, re4_u32 flag);
re4_bool re4_idxtpl_header_has_mipmaps(const re4_idxtpl_header *h);
re4_u32 re4_ps2tpl_bitdepth_palette_limit(re4_u32 bitdepth);
re4_err re4_idxtpl_header_validate_basic(const re4_idxtpl_header *h, re4_u32 *out_warnings);
const char *re4_idxtpl_warning_label(re4_u32 warning);
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
re4_err re4_idxuhdtpl_read_text(re4_span text, re4_arena *arena, re4_idxuhdtpl *out);
re4_err re4_idxuhdtpl_find(const re4_idxuhdtpl *idx, re4_u32 pack_id, re4_u32 texture_id, re4_u32 *out_index);
re4_err re4_idxuhdtpl_emit_text(const re4_idxuhdtpl *idx, re4_emit_fn emit, void *emit_user);
re4_err re4_idxuhdtpl_entry_emit_text(const re4_idxuhdtpl_entry *entry, re4_emit_fn emit, void *emit_user);

const char *re4_tpl_format_name(re4_u32 fmt);

#ifdef __cplusplus
}
#endif

#endif
