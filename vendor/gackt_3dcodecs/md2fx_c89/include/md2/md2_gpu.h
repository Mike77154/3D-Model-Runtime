#ifndef MD2_GPU_H
#define MD2_GPU_H

#include "md2_runtime.h"

#define MD2_GPU_AOS16_STRIDE 12
#define MD2_GPU_SOA16_U16_STRIDE 2
#define MD2_GPU_SOA16_U8_STRIDE 1

#define MD2_GPU_STREAM_POSITION_X 0
#define MD2_GPU_STREAM_POSITION_Y 1
#define MD2_GPU_STREAM_POSITION_Z 2
#define MD2_GPU_STREAM_TEXCOORD_U 3
#define MD2_GPU_STREAM_TEXCOORD_V 4
#define MD2_GPU_STREAM_NORMAL_INDEX 5
#define MD2_GPU_STREAM_COUNT 6

typedef struct md2_gpu_bounds_s
{
    md2_fx_vec3_t min;
    md2_fx_vec3_t max;
} md2_gpu_bounds_t;

typedef struct md2_gpu_quant_desc_s
{
    md2_fx_vec3_t translate;
    md2_fx_vec3_t scale;
} md2_gpu_quant_desc_t;

typedef struct md2_gpu_stream_s
{
    md2_u8 *data;
    md2_i32 stride;
} md2_gpu_stream_t;

typedef struct md2_gpu_soa16_streams_s
{
    md2_gpu_stream_t x;
    md2_gpu_stream_t y;
    md2_gpu_stream_t z;
    md2_gpu_stream_t u;
    md2_gpu_stream_t v;
    md2_gpu_stream_t normal_index;
} md2_gpu_soa16_streams_t;

void md2_gpu_bounds_init(md2_gpu_bounds_t *bounds);
int md2_gpu_compute_pose_bounds(const md2_runtime_vertex_t *vertices,
                                md2_i32 num_vertices,
                                md2_gpu_bounds_t *out_bounds);
int md2_gpu_compute_runtime_bounds(const md2_model_t *model,
                                   const md2_runtime_mesh_t *mesh,
                                   md2_gpu_bounds_t *out_bounds);
int md2_gpu_make_quant_desc(const md2_gpu_bounds_t *bounds,
                            md2_gpu_quant_desc_t *out_desc);

md2_u16 md2_gpu_pack_unorm16(md2_fx32_t value);
md2_fx32_t md2_gpu_unpack_unorm16(md2_u16 value);
md2_u16 md2_gpu_pack_pos16(md2_fx32_t value,
                           md2_fx32_t translate,
                           md2_fx32_t scale);
md2_fx32_t md2_gpu_unpack_pos16(md2_u16 value,
                                md2_fx32_t translate,
                                md2_fx32_t scale);

int md2_gpu_export_aos16(const md2_runtime_vertex_t *vertices,
                         md2_i32 num_vertices,
                         const md2_gpu_quant_desc_t *desc,
                         md2_u8 *out_bytes,
                         md2_i32 out_stride);

int md2_gpu_export_soa16(const md2_runtime_vertex_t *vertices,
                         md2_i32 num_vertices,
                         const md2_gpu_quant_desc_t *desc,
                         const md2_gpu_soa16_streams_t *out_streams);

int md2_gpu_pose_frame_aos16(const md2_model_t *model,
                             const md2_runtime_mesh_t *mesh,
                             md2_i32 frame_index,
                             const md2_gpu_quant_desc_t *desc,
                             md2_u8 *out_bytes,
                             md2_i32 out_stride);

int md2_gpu_pose_lerp_aos16(const md2_model_t *model,
                            const md2_runtime_mesh_t *mesh,
                            md2_i32 frame_a_index,
                            md2_i32 frame_b_index,
                            md2_fx32_t t,
                            const md2_gpu_quant_desc_t *desc,
                            md2_u8 *out_bytes,
                            md2_i32 out_stride);

int md2_gpu_pose_frame_soa16(const md2_model_t *model,
                             const md2_runtime_mesh_t *mesh,
                             md2_i32 frame_index,
                             const md2_gpu_quant_desc_t *desc,
                             const md2_gpu_soa16_streams_t *out_streams);

int md2_gpu_pose_lerp_soa16(const md2_model_t *model,
                            const md2_runtime_mesh_t *mesh,
                            md2_i32 frame_a_index,
                            md2_i32 frame_b_index,
                            md2_fx32_t t,
                            const md2_gpu_quant_desc_t *desc,
                            const md2_gpu_soa16_streams_t *out_streams);

#endif /* MD2_GPU_H */
