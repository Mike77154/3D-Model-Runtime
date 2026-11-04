#ifndef IR_SCENE_STREAM_H
#define IR_SCENE_STREAM_H

#include "ir_scene.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Las cadenas y atributos recibidos por callback son temporales.
 * El caller debe copiarlos si quiere persistirlos.
 */
typedef struct ir_scene_stream_events_tag {
    int (*on_scene_begin)(void* user, unsigned long line, unsigned long col);
    int (*on_root_attr)(void* user, const ir_attr* attr);
    int (*on_root_user_attr)(void* user, const ir_attr* attr);
    int (*on_node_begin)(void* user, ir_u32 depth, const char* type, unsigned long line, unsigned long col);
    int (*on_node_attr)(void* user, ir_u32 depth, const char* type, const ir_attr* attr);
    int (*on_node_material_begin)(void* user, ir_u32 depth, const char* type, ir_u32 material_index);
    int (*on_node_material_attr)(void* user, ir_u32 depth, const char* type, ir_u32 material_index, const ir_attr* attr);
    int (*on_node_material_end)(void* user, ir_u32 depth, const char* type, ir_u32 material_index);
    int (*on_node_animator_begin)(void* user, ir_u32 depth, const char* type, ir_u32 animator_index);
    int (*on_node_animator_attr)(void* user, ir_u32 depth, const char* type, ir_u32 animator_index, const ir_attr* attr);
    int (*on_node_animator_end)(void* user, ir_u32 depth, const char* type, ir_u32 animator_index);
    int (*on_node_user_attr)(void* user, ir_u32 depth, const char* type, const ir_attr* attr);
    int (*on_node_end)(void* user, ir_u32 depth, const char* type);
    int (*on_scene_end)(void* user);

    /* V8: attr callbacks with the temporary string pool still alive.
     * Use these when you need ir_attr_name_cstr()/ir_attr_value_cstr() during streaming.
     */
    int (*on_root_attr_ex)(void* user, const ir_attr* attr, const ir_strpool* pool);
    int (*on_root_user_attr_ex)(void* user, const ir_attr* attr, const ir_strpool* pool);
    int (*on_node_attr_ex)(void* user, ir_u32 depth, const char* type, const ir_attr* attr, const ir_strpool* pool);
    int (*on_node_material_attr_ex)(void* user, ir_u32 depth, const char* type, ir_u32 material_index, const ir_attr* attr, const ir_strpool* pool);
    int (*on_node_animator_attr_ex)(void* user, ir_u32 depth, const char* type, ir_u32 animator_index, const ir_attr* attr, const ir_strpool* pool);
    int (*on_node_user_attr_ex)(void* user, ir_u32 depth, const char* type, const ir_attr* attr, const ir_strpool* pool);
} ir_scene_stream_events;

int ir_scene_parse_memory_stream(const void* data, size_t size, const ir_scene_stream_events* events, void* user, char* err, size_t err_cap);

#ifdef __cplusplus
}
#endif

#endif
