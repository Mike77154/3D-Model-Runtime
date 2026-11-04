
#ifndef DBO_TOOL_COMMON_H
#define DBO_TOOL_COMMON_H

#include <stdio.h>
#include "../include/dbo.h"

#ifndef DBO_TOOL_MAX_FILE_BYTES
#define DBO_TOOL_MAX_FILE_BYTES (32UL * 1024UL * 1024UL)
#endif

int dbo_tool_read_file(const char* path, void* buffer, dbo_u32 capacity, dbo_u32* out_size, char* err, dbo_u32 err_size);
int dbo_tool_write_file(const char* path, const void* buffer, dbo_u32 size, char* err, dbo_u32 err_size);
int dbo_tool_load_scene(const char* path, dbo_scene* scene, void* buffer, dbo_u32 capacity, dbo_u32* out_size, char* err, dbo_u32 err_size);
void dbo_tool_fx_to_string(dbo_fx32 value, char* buffer, dbo_u32 buffer_size);
void dbo_tool_json_write_string(FILE* f, const char* text);
const char* dbo_tool_cstr_or_empty(const dbo_scene* scene, dbo_blob_ref ref);
void dbo_tool_write_fx_json(FILE* f, dbo_fx32 value);
const char* dbo_tool_scope_name(dbo_u32 scope_kind);

#endif
