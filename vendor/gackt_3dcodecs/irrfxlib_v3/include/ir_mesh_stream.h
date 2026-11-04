#ifndef IR_MESH_STREAM_H
#define IR_MESH_STREAM_H

#include "ir_mesh.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Todas las cadenas expuestas por callbacks apuntan a memoria temporal.
 * Si el caller quiere conservarlas, debe copiarlas durante el callback.
 */
typedef struct ir_mesh_stream_events_tag {
    int (*on_mesh_begin)(void* user, const char* xmlns, const char* version, unsigned long line, unsigned long col);
    int (*on_mesh_bbox)(void* user, const ir_box3fx* bbox, unsigned long line, unsigned long col);
    int (*on_buffer_begin)(void* user, ir_u32 buffer_index, unsigned long line, unsigned long col);
    int (*on_buffer_bbox)(void* user, ir_u32 buffer_index, const ir_box3fx* bbox, unsigned long line, unsigned long col);
    int (*on_material_attr)(void* user, ir_u32 buffer_index, const ir_attr* attr);
    int (*on_vertices_begin)(void* user, ir_u32 buffer_index, int vertex_type, ir_u32 vertex_count, unsigned long line, unsigned long col);
    int (*on_vertex)(void* user, ir_u32 buffer_index, ir_u32 vertex_index, const ir_vertex* vertex);
    int (*on_indices_begin)(void* user, ir_u32 buffer_index, ir_u32 index_count, unsigned long line, unsigned long col);
    int (*on_index)(void* user, ir_u32 buffer_index, ir_u32 index_index, ir_u32 value);
    int (*on_buffer_end)(void* user, ir_u32 buffer_index);
    int (*on_mesh_end)(void* user);

    /* V8: attr callbacks with the temporary string pool still alive.
     * Use these when you need ir_attr_name_cstr()/ir_attr_value_cstr() during streaming.
     */
    int (*on_material_attr_ex)(void* user, ir_u32 buffer_index, const ir_attr* attr, const ir_strpool* pool);
} ir_mesh_stream_events;

int ir_mesh_parse_memory_stream(const void* data, size_t size, const ir_mesh_stream_events* events, void* user, char* err, size_t err_cap);

#ifdef __cplusplus
}
#endif

#endif
