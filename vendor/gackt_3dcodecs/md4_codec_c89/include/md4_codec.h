#ifndef MD4_CODEC_H
#define MD4_CODEC_H

#include "md4_types.h"
#include "md4_fixed.h"

#ifdef __cplusplus
extern "C" {
#endif

void md4_model_init(md4_model_t *model);
const char *md4_result_string(md4_rc_t rc);

md4_rc_t md4_validate_model(const md4_model_t *model);
md4_rc_t md4_decode(md4_model_t *model, const void *data, md4_u32 size, const md4_decode_opts_t *opts);
void md4_decoder_init(md4_decoder_t *decoder);
md4_rc_t md4_decoder_begin(md4_decoder_t *decoder, void *storage, md4_u32 storage_capacity,
                           const md4_decode_opts_t *opts);
md4_rc_t md4_decoder_push(md4_decoder_t *decoder, const void *chunk, md4_u32 chunk_size);
md4_i32 md4_decoder_header_ready(const md4_decoder_t *decoder);
md4_i32 md4_decoder_is_complete(const md4_decoder_t *decoder);
md4_u32 md4_decoder_bytes_received(const md4_decoder_t *decoder);
md4_u32 md4_decoder_expected_size(const md4_decoder_t *decoder);
md4_rc_t md4_decoder_finish(md4_decoder_t *decoder, md4_model_t *out_model);

void md4_stream_parser_init(md4_stream_parser_t *parser);
md4_rc_t md4_stream_parser_begin(md4_stream_parser_t *parser,
                                 const md4_decode_opts_t *opts,
                                 const md4_stream_callbacks_t *callbacks,
                                 void *user);
md4_rc_t md4_stream_parser_push(md4_stream_parser_t *parser,
                                const void *chunk,
                                md4_u32 chunk_size);
md4_i32 md4_stream_parser_header_ready(const md4_stream_parser_t *parser);
md4_i32 md4_stream_parser_is_complete(const md4_stream_parser_t *parser);
md4_u32 md4_stream_parser_bytes_consumed(const md4_stream_parser_t *parser);
md4_u32 md4_stream_parser_expected_size(const md4_stream_parser_t *parser);
md4_rc_t md4_stream_parser_finish(md4_stream_parser_t *parser);

void md4_stream_decode_model_init(md4_stream_decode_model_t *decoder, md4_model_t *model);
md4_rc_t md4_stream_decode_model_begin(md4_stream_decode_model_t *decoder,
                                       const md4_decode_opts_t *opts);
md4_rc_t md4_stream_decode_model_push(md4_stream_decode_model_t *decoder,
                                      const void *chunk,
                                      md4_u32 chunk_size);
md4_i32 md4_stream_decode_model_header_ready(const md4_stream_decode_model_t *decoder);
md4_i32 md4_stream_decode_model_is_complete(const md4_stream_decode_model_t *decoder);
md4_u32 md4_stream_decode_model_bytes_consumed(const md4_stream_decode_model_t *decoder);
md4_u32 md4_stream_decode_model_expected_size(const md4_stream_decode_model_t *decoder);
md4_rc_t md4_stream_decode_model_finish(md4_stream_decode_model_t *decoder);

void md4_engine_stream_parser_init(md4_engine_stream_parser_t *parser);
md4_rc_t md4_engine_stream_parser_begin(md4_engine_stream_parser_t *parser,
                                        const md4_decode_opts_t *opts,
                                        const md4_engine_stream_callbacks_t *callbacks,
                                        void *user);
md4_rc_t md4_engine_stream_parser_push(md4_engine_stream_parser_t *parser,
                                       const void *chunk,
                                       md4_u32 chunk_size);
md4_i32 md4_engine_stream_parser_header_ready(const md4_engine_stream_parser_t *parser);
md4_i32 md4_engine_stream_parser_is_complete(const md4_engine_stream_parser_t *parser);
md4_u32 md4_engine_stream_parser_bytes_consumed(const md4_engine_stream_parser_t *parser);
md4_u32 md4_engine_stream_parser_expected_size(const md4_engine_stream_parser_t *parser);
md4_rc_t md4_engine_stream_parser_finish(md4_engine_stream_parser_t *parser);

md4_rc_t md4_encode_size(const md4_model_t *model, const md4_encode_opts_t *opts, md4_u32 *out_size);
md4_rc_t md4_encode(const md4_model_t *model, const md4_encode_opts_t *opts,
                    void *out_buf, md4_u32 out_buf_size, md4_u32 *out_written);

void md4_writer_init(md4_writer_t *writer);
md4_rc_t md4_writer_begin(md4_writer_t *writer, const md4_model_t *model, const md4_encode_opts_t *opts);
md4_i32 md4_writer_is_done(const md4_writer_t *writer);
md4_u32 md4_writer_total_size(const md4_writer_t *writer);
md4_u32 md4_writer_bytes_written(const md4_writer_t *writer);
md4_rc_t md4_writer_step(md4_writer_t *writer, md4_stream_sink_fn sink, void *user,
                         md4_u32 max_bytes, md4_u32 *out_emitted);
md4_rc_t md4_writer_write_all(md4_writer_t *writer, md4_stream_sink_fn sink, void *user);
md4_rc_t md4_encode_stream(const md4_model_t *model, const md4_encode_opts_t *opts,
                           md4_stream_sink_fn sink, void *user);

void md4_ir_init(md4_ir_model_t *ir);
md4_rc_t md4_ir_validate(const md4_ir_model_t *ir);
md4_rc_t md4_ir_from_model(const md4_model_t *model, md4_ir_model_t *out_ir);
md4_rc_t md4_model_from_ir(const md4_ir_model_t *ir, const md4_ir_export_opts_t *opts,
                           md4_model_t *out_model);

void md4_asset_ir_init(md4_asset_ir_model_t *asset);
md4_rc_t md4_asset_ir_validate(const md4_asset_ir_model_t *asset);
md4_rc_t md4_asset_ir_from_model(const md4_model_t *model, md4_asset_ir_model_t *out_asset);
md4_rc_t md4_model_from_asset_ir(const md4_asset_ir_model_t *asset,
                                 const md4_asset_export_opts_t *opts,
                                 md4_model_t *out_model);

void md4_pose_init(md4_pose_t *pose);
md4_rc_t md4_build_pose(const md4_model_t *model,
                        md4_i32 frame_index,
                        md4_pose_t *out_pose);
md4_rc_t md4_build_pose_lerp(const md4_model_t *model,
                             md4_i32 old_frame_index,
                             md4_i32 new_frame_index,
                             md4_fx_t lerp_t,
                             md4_pose_t *out_pose);
md4_rc_t md4_skin_surface_pose(const md4_model_t *model,
                               md4_i32 lod_index,
                               md4_i32 surface_index_in_lod,
                               const md4_pose_t *pose,
                               md4_fx_t *out_positions_xyz,
                               md4_fx_t *out_normals_xyz,
                               md4_u32 out_vertex_capacity);

md4_rc_t md4_skin_surface(const md4_model_t *model,
                          md4_i32 lod_index,
                          md4_i32 surface_index_in_lod,
                          md4_i32 frame_index,
                          md4_fx_t *out_positions_xyz,
                          md4_fx_t *out_normals_xyz,
                          md4_u32 out_vertex_capacity);

md4_rc_t md4_skin_surface_lerp(const md4_model_t *model,
                               md4_i32 lod_index,
                               md4_i32 surface_index_in_lod,
                               md4_i32 old_frame_index,
                               md4_i32 new_frame_index,
                               md4_fx_t lerp_t,
                               md4_fx_t *out_positions_xyz,
                               md4_fx_t *out_normals_xyz,
                               md4_u32 out_vertex_capacity);

#ifdef __cplusplus
}
#endif

#endif
