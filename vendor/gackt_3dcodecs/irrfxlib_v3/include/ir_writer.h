#ifndef IR_WRITER_H
#define IR_WRITER_H

#include "ir_mesh.h"
#include "ir_scene.h"

#ifdef __cplusplus
extern "C" {
#endif

enum ir_writer_encoding {
    IR_WRITER_UTF8 = 0,
    IR_WRITER_UTF16LE = 1
};

typedef struct ir_writer_options_tag {
    int encoding;
    int emit_bom;
    int preserve_source_if_possible;
    char* scratch_utf8;
    size_t scratch_utf8_cap;
} ir_writer_options;

void ir_writer_options_init(ir_writer_options* opt);
int ir_utf8_to_utf16le(const char* utf8, size_t utf8_len, void* out_mem, size_t out_cap, size_t* out_len, int emit_bom);
int ir_mesh_write_memory_ex(const ir_mesh_doc* doc, void* out_mem, size_t out_cap, size_t* out_len, const ir_writer_options* opt, char* err, size_t err_cap);
int ir_scene_write_memory_ex(const ir_scene_doc* doc, void* out_mem, size_t out_cap, size_t* out_len, const ir_writer_options* opt, char* err, size_t err_cap);

#ifdef __cplusplus
}
#endif

#endif
