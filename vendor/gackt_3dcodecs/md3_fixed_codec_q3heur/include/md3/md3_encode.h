#ifndef MD3_ENCODE_H
#define MD3_ENCODE_H

#include "md3_types.h"

int md3_surface_compute_layout(md3_surface_info_t *surface_info);
int md3_model_compute_layout(md3_header_info_t *header_info,
    md3_surface_info_t *surface_infos, md3_i32 surface_count);

void md3_frame_accumulator_begin(md3_frame_accumulator_t *accumulator);
int md3_frame_accumulator_add_vertices(md3_frame_accumulator_t *accumulator,
    const md3_vertex_t *vertices, md3_i32 vertex_count);
int md3_frame_accumulator_finish(md3_frame_accumulator_t *accumulator,
    const char *frame_name, md3_frame_t *out_frame);

int md3_encoder_begin(md3_encoder_t *encoder, FILE *fp,
    const md3_header_info_t *header_info,
    const md3_surface_info_t *surface_infos);

int md3_encoder_write_frame(md3_encoder_t *encoder, const md3_frame_t *frame);
int md3_encoder_write_tags_for_frame(md3_encoder_t *encoder,
    const md3_tag_t *tags, md3_i32 tag_count);

int md3_encoder_begin_surface(md3_encoder_t *encoder, md3_i32 surface_index);
int md3_encoder_write_surface_shaders(md3_encoder_t *encoder,
    const md3_shader_t *shaders, md3_i32 shader_count);
int md3_encoder_write_surface_triangles(md3_encoder_t *encoder,
    const md3_triangle_t *triangles, md3_i32 triangle_count);
int md3_encoder_write_surface_st(md3_encoder_t *encoder,
    const md3_st_t *st, md3_i32 st_count);
int md3_encoder_write_surface_vertices_for_frame(md3_encoder_t *encoder,
    const md3_vertex_t *vertices, md3_i32 vertex_count);
int md3_encoder_end_surface(md3_encoder_t *encoder);
int md3_encoder_finish(md3_encoder_t *encoder);

#endif
