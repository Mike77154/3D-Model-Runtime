#ifndef DBO_INT_H
#define DBO_INT_H

#include <string.h>
#include <stdio.h>
#include "../include/dbo.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct dbo_reader
{
    dbo_scene* scene;
    const dbo_u8* data;
    dbo_u32 size;
    dbo_u32 pos;
} dbo_reader;

typedef struct dbo_writer
{
    const dbo_scene* scene;
    dbo_u8* data;
    dbo_u32 capacity;
    dbo_u32 pos;
    char* error_text;
    dbo_u32 error_text_size;
} dbo_writer;

void dbo__set_error(dbo_scene* scene, const char* text);
dbo_bool dbo__align_up(dbo_u32 value, dbo_u32 align, dbo_u32* out_value);
dbo_bool dbo__read_u8(dbo_reader* r, dbo_u8* out_v);
dbo_bool dbo__read_u32(dbo_reader* r, dbo_u32* out_v);
dbo_bool dbo__read_bytes(dbo_reader* r, void* out, dbo_u32 size);
dbo_bool dbo__skip(dbo_reader* r, dbo_u32 size);
dbo_bool dbo__peek_remaining(const dbo_reader* r, dbo_u32* out_size);

dbo_bool dbo__write_u8(dbo_writer* w, dbo_u8 v);
dbo_bool dbo__write_u32(dbo_writer* w, dbo_u32 v);
dbo_bool dbo__write_bytes(dbo_writer* w, const void* data, dbo_u32 size);
dbo_bool dbo__patch_u32(dbo_writer* w, dbo_u32 at, dbo_u32 v);
dbo_bool dbo__begin_chunk(dbo_writer* w, dbo_u32 code, dbo_u32* out_size_pos);
dbo_bool dbo__end_chunk(dbo_writer* w, dbo_u32 size_pos);
dbo_bool dbo__write_terminator(dbo_writer* w);

dbo_bool dbo__read_string_ref(dbo_reader* r, dbo_blob_ref* out_ref);
dbo_bool dbo__write_string_ref(dbo_writer* w, const dbo_scene* scene, dbo_blob_ref ref);

dbo_bool dbo__read_vec3_fx(dbo_reader* r, dbo_fx32 out3[3]);
dbo_bool dbo__write_vec3_fx(dbo_writer* w, const dbo_fx32 v[3]);

dbo_bool dbo__read_mat4_fx(dbo_reader* r, dbo_fx32 out16[16]);
dbo_bool dbo__write_mat4_fx(dbo_writer* w, const dbo_fx32 m[16]);

dbo_bool dbo__read_material_fx(dbo_reader* r, dbo_material* out_mat);
dbo_bool dbo__write_material_fx(dbo_writer* w, const dbo_material* mat);

dbo_bool dbo__scene_store_unknown_chunk(dbo_scene* scene, dbo_u32 scope_kind, dbo_u32 owner_index, dbo_u32 code, const void* payload, dbo_u32 size);
dbo_bool dbo__scene_store_chunk_event(dbo_scene* scene, dbo_u32 scope_kind, dbo_u32 owner_index, dbo_u32 code, dbo_u32 aux_index, dbo_u32* out_index);
dbo_bool dbo__write_unknown_chunks_for_scope(dbo_writer* w, dbo_u32 scope_kind, dbo_u32 owner_index);

#ifdef __cplusplus
}
#endif

#endif
