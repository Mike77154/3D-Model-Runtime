#ifndef MD3_DECODE_H
#define MD3_DECODE_H

#include "md3_types.h"

int md3_decoder_open(md3_decoder_t *decoder, FILE *fp);
void md3_decoder_close(md3_decoder_t *decoder);

const md3_header_info_t *md3_decoder_header(const md3_decoder_t *decoder);
int md3_decoder_get_surface_info(const md3_decoder_t *decoder,
    md3_i32 surface_index, md3_surface_info_t *out_info);
int md3_decoder_find_surface_index(const md3_decoder_t *decoder,
    const char *surface_name, md3_i32 *out_index);

int md3_decoder_read_frame(const md3_decoder_t *decoder,
    md3_i32 frame_index, md3_frame_t *out_frame);

int md3_decoder_read_tags_for_frame(const md3_decoder_t *decoder,
    md3_i32 frame_index, md3_tag_t *out_tags, md3_i32 tag_capacity);

int md3_decoder_read_surface_shaders(const md3_decoder_t *decoder,
    md3_i32 surface_index, md3_shader_t *out_shaders, md3_i32 shader_capacity);

int md3_decoder_read_surface_triangles(const md3_decoder_t *decoder,
    md3_i32 surface_index, md3_triangle_t *out_triangles,
    md3_i32 triangle_capacity);

int md3_decoder_read_surface_st(const md3_decoder_t *decoder,
    md3_i32 surface_index, md3_st_t *out_st, md3_i32 st_capacity);

int md3_decoder_read_surface_vertices_for_frame(const md3_decoder_t *decoder,
    md3_i32 surface_index, md3_i32 frame_index, md3_vertex_t *out_vertices,
    md3_i32 vertex_capacity);

#endif
