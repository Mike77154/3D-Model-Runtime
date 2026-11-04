#ifndef USD3D_SHADER_BYTECODE_H
#define USD3D_SHADER_BYTECODE_H

#include "usd3d.h"
#include "usd3d_shader_vm.h"

#ifndef USD3D_SHADER_BYTECODE_MAX_WORDS
#define USD3D_SHADER_BYTECODE_MAX_WORDS 1024
#endif

#define USD3D_SHADER_BC_MAGIC 0x55334243UL
#define USD3D_SHADER_BC_VERSION 1UL

typedef struct usd3d_shader_bytecode_s {
    unsigned long words[USD3D_SHADER_BYTECODE_MAX_WORDS];
    unsigned long word_count;
    unsigned long symbol_count;
    unsigned long instruction_count;
    unsigned long flags;
} usd3d_shader_bytecode;

void usd3d_shader_bytecode_init(usd3d_shader_bytecode *bc);
int usd3d_shader_bytecode_pack(const usd3d_shader_program *program, usd3d_shader_bytecode *bc);
int usd3d_shader_bytecode_unpack(const usd3d_shader_bytecode *bc, usd3d_shader_program *program);
int usd3d_shader_bytecode_link(const usd3d_shader_program *a, const usd3d_shader_program *b, usd3d_shader_program *out_program);
int usd3d_shader_bytecode_optimize(usd3d_shader_program *program);
int usd3d_shader_bytecode_emit_c_array(const usd3d_shader_bytecode *bc, const char *name, char *out_text, unsigned long out_cap);

#endif
