#ifndef USD3D_SHADER_COMPILER_H
#define USD3D_SHADER_COMPILER_H

#include "usd3d.h"
#include "usd3d_material_graph.h"

#ifndef USD3D_SHADER_MAX_INSTRUCTIONS
#define USD3D_SHADER_MAX_INSTRUCTIONS 256
#endif

#ifndef USD3D_SHADER_MAX_SYMBOLS
#define USD3D_SHADER_MAX_SYMBOLS 128
#endif

#define USD3D_SHADER_OP_NOP 0
#define USD3D_SHADER_OP_CONST_COLOR 1
#define USD3D_SHADER_OP_CONST_SCALAR 2
#define USD3D_SHADER_OP_TEXTURE 3
#define USD3D_SHADER_OP_MUL 4
#define USD3D_SHADER_OP_ADD 5
#define USD3D_SHADER_OP_STANDARD_SURFACE 6
#define USD3D_SHADER_OP_OUTPUT_SURFACE 7
#define USD3D_SHADER_OP_OUTPUT_DISPLACEMENT 8
#define USD3D_SHADER_OP_OUTPUT_VOLUME 9

typedef struct usd3d_shader_symbol_s {
    char name[USD3D_TOKEN_MAX];
    char type[USD3D_TOKEN_MAX];
    usd3d_fixed value[4];
    char asset_path[USD3D_PATH_MAX];
    int active;
} usd3d_shader_symbol;

typedef struct usd3d_shader_instruction_s {
    int op;
    int dst;
    int a;
    int b;
    usd3d_fixed imm[4];
    char text[USD3D_TOKEN_MAX];
} usd3d_shader_instruction;

typedef struct usd3d_shader_program_s {
    usd3d_shader_symbol symbols[USD3D_SHADER_MAX_SYMBOLS];
    usd3d_shader_instruction instructions[USD3D_SHADER_MAX_INSTRUCTIONS];
    unsigned long symbol_count;
    unsigned long instruction_count;
    int supports_surface;
    int supports_displacement;
    int supports_volume;
    int source_kind; /* 1 PreviewSurface, 2 MaterialX, 3 MDL */
} usd3d_shader_program;

void usd3d_shader_program_init(usd3d_shader_program *p);
const char *usd3d_shader_op_name(int op);
int usd3d_shader_add_symbol(usd3d_shader_program *p, const char *name, const char *type);
int usd3d_shader_add_instruction(usd3d_shader_program *p, int op, int dst, int a, int b, const usd3d_fixed *imm4, const char *text);
int usd3d_shader_compile_preview_surface(const usd3d_material_eval_input *input, usd3d_shader_program *program);
int usd3d_shader_compile_material_graph(const usd3d_material_graph *graph, const char *surface_node, usd3d_shader_program *program);
int usd3d_shader_parse_mtlx_document_lite(const char *text, usd3d_material_graph *out_graph, usd3d_shader_program *out_program);
int usd3d_shader_parse_mdl_document_lite(const char *text, usd3d_shader_program *out_program);
int usd3d_shader_emit_glsl_lite(const usd3d_shader_program *program, char *out_text, unsigned int out_cap);
int usd3d_shader_emit_mdl_stub(const usd3d_shader_program *program, char *out_text, unsigned int out_cap);
int usd3d_shader_eval_constant(const usd3d_shader_program *program, usd3d_material_eval_output *out);

#endif
