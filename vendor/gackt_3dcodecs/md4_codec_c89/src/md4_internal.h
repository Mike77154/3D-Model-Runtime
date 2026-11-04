#ifndef MD4_INTERNAL_H
#define MD4_INTERNAL_H

#include <stddef.h>
#include <string.h>
#include "../include/md4_codec.h"
#include "../include/md4_bytes.h"

#define MD4_IDENT_U32 0x34504449U
#define MD4_VERSION_I32 1

#define MD4_HEADER_LEGACY_SIZE 96
#define MD4_HEADER_EXTENDED_SIZE 100

#define MD4_FRAME_BASE_NO_NAME 40
#define MD4_FRAME_BASE_WITH_NAME 56
#define MD4_BONE_SIZE 48
#define MD4_LOD_SIZE 12
#define MD4_SURFACE_HEADER_SIZE 168
#define MD4_TRIANGLE_SIZE 12
#define MD4_RUNTIME_VERTEX_BASE 24
#define MD4_RUNTIME_WEIGHT_SIZE 20
#define MD4_PADDED_VERTEX_BASE 36
#define MD4_DRAFT_VERTEX_BASE 36
#define MD4_DRAFT_WEIGHT_SIZE 8

#define MD4_TRUE 1
#define MD4_FALSE 0

typedef struct md4_raw_header_s {
    md4_header_mode_t mode;
    md4_i32 num_frames;
    md4_i32 num_bones;
    md4_i32 num_lods;
    md4_i32 ofs_bone_names;
    md4_i32 ofs_frames;
    md4_i32 ofs_lods;
    md4_i32 ofs_end;
    md4_i32 frame_stride;
    md4_i32 frame_has_name;
} md4_raw_header_t;

void md4_copy_qpath(char dst[MD4_CFG_MAX_QPATH], const md4_u8 *src);
void md4_copy_frame_name(char dst[MD4_CFG_FRAME_NAME_LEN], const md4_u8 *src, md4_i32 has_name);
md4_i32 md4_range_ok(md4_u32 total_size, md4_u32 offset, md4_u32 length);
md4_i32 md4_choose_header(const md4_u8 *data, md4_u32 size, const md4_decode_opts_t *opts, md4_raw_header_t *out_raw);
md4_i32 md4_count_bits_u32(md4_u32 v);
md4_i32 md4_surface_vertex_block_end(md4_i32 ofs_end, md4_i32 ofs_bone_refs, md4_i32 num_bone_refs);
md4_i32 md4_parse_vertex_block_runtime(const md4_u8 *base, md4_u32 block_size, md4_i32 num_vertices,
                                       md4_i32 strict, md4_i32 *out_used_bytes,
                                       md4_i32 count_only, md4_model_t *model, md4_surface_t *surface);
md4_i32 md4_parse_vertex_block_draft(const md4_u8 *base, md4_u32 block_size, md4_i32 num_vertices,
                                     md4_i32 strict, md4_i32 *out_used_bytes,
                                     md4_i32 count_only, md4_model_t *model, md4_surface_t *surface);
md4_i32 md4_append_frame(md4_model_t *model);
md4_i32 md4_append_bone(md4_model_t *model);
md4_i32 md4_append_lod(md4_model_t *model);
md4_i32 md4_append_surface(md4_model_t *model);
md4_i32 md4_append_vertex(md4_model_t *model);
md4_i32 md4_append_triangle(md4_model_t *model);
md4_i32 md4_append_weight(md4_model_t *model);
md4_i32 md4_append_bone_ref(md4_model_t *model);
md4_i32 md4_get_global_surface_index(const md4_model_t *model, md4_i32 lod_index, md4_i32 surface_index_in_lod);
md4_i32 md4_resolve_bone_index(const md4_model_t *model, const md4_surface_t *surface, md4_i32 local_or_global);

#endif
