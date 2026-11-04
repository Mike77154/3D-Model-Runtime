#include "usd3d_shader_compiler.h"

#include <string.h>
#include <ctype.h>

static void sc_copy(char *dst, unsigned int cap, const char *src)
{
    unsigned int i;
    if (cap == 0U) return;
    if (src == 0) src = "";
    for (i = 0U; i + 1U < cap && src[i] != '\0'; ++i) dst[i] = src[i];
    dst[i] = '\0';
}

static void sc_append(char *dst, unsigned int cap, const char *src)
{
    unsigned int n;
    unsigned int i;
    if (dst == 0 || src == 0 || cap == 0U) return;
    n = (unsigned int)strlen(dst);
    i = 0U;
    while (n + 1U < cap && src[i] != '\0') dst[n++] = src[i++];
    dst[n] = '\0';
}

void usd3d_shader_program_init(usd3d_shader_program *p)
{
    if (p == 0) return;
    memset(p, 0, sizeof(*p));
}

const char *usd3d_shader_op_name(int op)
{
    switch (op) {
    case USD3D_SHADER_OP_CONST_COLOR: return "const_color";
    case USD3D_SHADER_OP_CONST_SCALAR: return "const_scalar";
    case USD3D_SHADER_OP_TEXTURE: return "texture";
    case USD3D_SHADER_OP_MUL: return "mul";
    case USD3D_SHADER_OP_ADD: return "add";
    case USD3D_SHADER_OP_STANDARD_SURFACE: return "standard_surface";
    case USD3D_SHADER_OP_OUTPUT_SURFACE: return "output_surface";
    case USD3D_SHADER_OP_OUTPUT_DISPLACEMENT: return "output_displacement";
    case USD3D_SHADER_OP_OUTPUT_VOLUME: return "output_volume";
    default: return "nop";
    }
}

int usd3d_shader_add_symbol(usd3d_shader_program *p, const char *name, const char *type)
{
    usd3d_shader_symbol *s;
    if (p == 0 || name == 0) return USD3D_ERR_BAD_ARG;
    if (p->symbol_count >= (unsigned long)USD3D_SHADER_MAX_SYMBOLS) return USD3D_ERR_LIMIT;
    s = &p->symbols[p->symbol_count++];
    memset(s, 0, sizeof(*s));
    sc_copy(s->name, sizeof(s->name), name);
    sc_copy(s->type, sizeof(s->type), type);
    s->active = 1;
    return (int)(p->symbol_count - 1UL);
}

int usd3d_shader_add_instruction(usd3d_shader_program *p, int op, int dst, int a, int b, const usd3d_fixed *imm4, const char *text)
{
    usd3d_shader_instruction *ins;
    unsigned int i;
    if (p == 0) return USD3D_ERR_BAD_ARG;
    if (p->instruction_count >= (unsigned long)USD3D_SHADER_MAX_INSTRUCTIONS) return USD3D_ERR_LIMIT;
    ins = &p->instructions[p->instruction_count++];
    memset(ins, 0, sizeof(*ins));
    ins->op = op;
    ins->dst = dst;
    ins->a = a;
    ins->b = b;
    if (imm4 != 0) for (i = 0U; i < 4U; ++i) ins->imm[i] = imm4[i];
    sc_copy(ins->text, sizeof(ins->text), text);
    return USD3D_OK;
}

int usd3d_shader_compile_preview_surface(const usd3d_material_eval_input *input, usd3d_shader_program *program)
{
    int base;
    int rough;
    int metal;
    int opacity;
    usd3d_fixed imm[4];
    if (input == 0 || program == 0) return USD3D_ERR_BAD_ARG;
    usd3d_shader_program_init(program);
    program->source_kind = 1;
    program->supports_surface = 1;
    base = usd3d_shader_add_symbol(program, "baseColor", "color3");
    rough = usd3d_shader_add_symbol(program, "roughness", "float");
    metal = usd3d_shader_add_symbol(program, "metallic", "float");
    opacity = usd3d_shader_add_symbol(program, "opacity", "float");
    if (base < 0 || rough < 0 || metal < 0 || opacity < 0) return USD3D_ERR_LIMIT;
    imm[0] = input->has_diffuse ? input->diffuse[0] : USD3D_FIXED_ONE;
    imm[1] = input->has_diffuse ? input->diffuse[1] : USD3D_FIXED_ONE;
    imm[2] = input->has_diffuse ? input->diffuse[2] : USD3D_FIXED_ONE;
    imm[3] = USD3D_FIXED_ONE;
    usd3d_shader_add_instruction(program, USD3D_SHADER_OP_CONST_COLOR, base, -1, -1, imm, "baseColor");
    imm[0] = input->has_roughness ? input->roughness : USD3D_FIXED_HALF;
    usd3d_shader_add_instruction(program, USD3D_SHADER_OP_CONST_SCALAR, rough, -1, -1, imm, "roughness");
    imm[0] = input->has_metallic ? input->metallic : 0L;
    usd3d_shader_add_instruction(program, USD3D_SHADER_OP_CONST_SCALAR, metal, -1, -1, imm, "metallic");
    imm[0] = input->has_opacity ? input->opacity : USD3D_FIXED_ONE;
    usd3d_shader_add_instruction(program, USD3D_SHADER_OP_CONST_SCALAR, opacity, -1, -1, imm, "opacity");
    usd3d_shader_add_instruction(program, USD3D_SHADER_OP_STANDARD_SURFACE, -1, base, rough, 0, "UsdPreviewSurface");
    usd3d_shader_add_instruction(program, USD3D_SHADER_OP_OUTPUT_SURFACE, -1, base, -1, 0, "outputs:surface");
    return USD3D_OK;
}

int usd3d_shader_compile_material_graph(const usd3d_material_graph *graph, const char *surface_node, usd3d_shader_program *program)
{
    usd3d_material_eval_output out;
    usd3d_material_eval_input input;
    if (graph == 0 || surface_node == 0 || program == 0) return USD3D_ERR_BAD_ARG;
    memset(&input, 0, sizeof(input));
    if (usd3d_material_graph_eval_node(graph, surface_node, &out) != USD3D_OK) return USD3D_ERR_PARSE;
    input.diffuse[0] = out.base_color[0];
    input.diffuse[1] = out.base_color[1];
    input.diffuse[2] = out.base_color[2];
    input.has_diffuse = 1;
    input.roughness = out.roughness; input.has_roughness = 1;
    input.metallic = out.metallic; input.has_metallic = 1;
    input.opacity = out.opacity; input.has_opacity = 1;
    usd3d_shader_compile_preview_surface(&input, program);
    if (strstr(out.evaluator, "mtlx") != 0) program->source_kind = 2;
    if (strstr(out.evaluator, "mdl") != 0) program->source_kind = 3;
    return USD3D_OK;
}

static int sc_find_attr(const char *text, const char *attr, char *out, unsigned int cap)
{
    const char *p;
    unsigned int n;
    p = strstr(text, attr);
    if (p == 0 || out == 0 || cap == 0U) return 0;
    p += strlen(attr);
    while (*p != '\0' && *p != '"' && *p != '\'') p++;
    if (*p == '\0') return 0;
    p++;
    n = 0U;
    while (*p != '\0' && *p != '"' && *p != '\'' && n + 1U < cap) out[n++] = *p++;
    out[n] = '\0';
    return n > 0U;
}

int usd3d_shader_parse_mtlx_document_lite(const char *text, usd3d_material_graph *out_graph, usd3d_shader_program *out_program)
{
    char name[USD3D_TOKEN_MAX];
    char type[USD3D_TOKEN_MAX];
    usd3d_material_eval_input input;
    if (text == 0 || out_graph == 0 || out_program == 0) return USD3D_ERR_BAD_ARG;
    usd3d_material_graph_init(out_graph);
    memset(&input, 0, sizeof(input));
    sc_copy(name, sizeof(name), "mtlx_standard_surface");
    sc_copy(type, sizeof(type), "standard_surface");
    sc_find_attr(text, "name=", name, sizeof(name));
    sc_find_attr(text, "nodedef=", type, sizeof(type));
    usd3d_material_graph_add_node(out_graph, "/MaterialX/Surface", type);
    if (strstr(text, "base_color") != 0 || strstr(text, "baseColor") != 0) {
        input.diffuse[0] = USD3D_FIXED_ONE; input.diffuse[1] = USD3D_FIXED_ONE; input.diffuse[2] = USD3D_FIXED_ONE; input.has_diffuse = 1;
    }
    if (strstr(text, "image") != 0 || strstr(text, "filename") != 0) {
        usd3d_material_graph_add_node(out_graph, "/MaterialX/Image", "ND_image_color3");
        usd3d_material_graph_add_edge(out_graph, "/MaterialX/Image", "out", "/MaterialX/Surface", "base_color");
    }
    input.roughness = USD3D_FIXED_HALF; input.has_roughness = 1;
    input.opacity = USD3D_FIXED_ONE; input.has_opacity = 1;
    usd3d_shader_compile_preview_surface(&input, out_program);
    out_program->source_kind = 2;
    return USD3D_OK;
}

int usd3d_shader_parse_mdl_document_lite(const char *text, usd3d_shader_program *out_program)
{
    usd3d_material_eval_input input;
    char module_name[USD3D_TOKEN_MAX];
    if (text == 0 || out_program == 0) return USD3D_ERR_BAD_ARG;
    memset(&input, 0, sizeof(input));
    input.diffuse[0] = USD3D_FIXED_ONE; input.diffuse[1] = USD3D_FIXED_ONE; input.diffuse[2] = USD3D_FIXED_ONE; input.has_diffuse = 1;
    input.roughness = USD3D_FIXED_HALF; input.has_roughness = 1;
    input.opacity = USD3D_FIXED_ONE; input.has_opacity = 1;
    usd3d_shader_compile_preview_surface(&input, out_program);
    out_program->source_kind = 3;
    out_program->supports_displacement = strstr(text, "displacement") != 0;
    out_program->supports_volume = strstr(text, "volume") != 0;
    sc_copy(module_name, sizeof(module_name), "mdl_module");
    sc_find_attr(text, "module", module_name, sizeof(module_name));
    usd3d_shader_add_instruction(out_program, USD3D_SHADER_OP_OUTPUT_SURFACE, -1, -1, -1, 0, module_name);
    return USD3D_OK;
}

static void sc_fixed_to_c(usd3d_fixed v, char *buf, unsigned int cap)
{
    char tmp[64];
    usd3d_fixed_to_string(v, tmp, sizeof(tmp), 3U);
    sc_copy(buf, cap, tmp);
}

int usd3d_shader_emit_glsl_lite(const usd3d_shader_program *program, char *out_text, unsigned int out_cap)
{
    char nr[32]; char ng[32]; char nb[32]; char no[32];
    usd3d_material_eval_output ev;
    if (program == 0 || out_text == 0 || out_cap == 0U) return USD3D_ERR_BAD_ARG;
    out_text[0] = '\0';
    usd3d_shader_eval_constant(program, &ev);
    sc_fixed_to_c(ev.base_color[0], nr, sizeof(nr));
    sc_fixed_to_c(ev.base_color[1], ng, sizeof(ng));
    sc_fixed_to_c(ev.base_color[2], nb, sizeof(nb));
    sc_fixed_to_c(ev.opacity, no, sizeof(no));
    sc_append(out_text, out_cap, "vec4 usd3d_surface(){return vec4(");
    sc_append(out_text, out_cap, nr); sc_append(out_text, out_cap, ",");
    sc_append(out_text, out_cap, ng); sc_append(out_text, out_cap, ",");
    sc_append(out_text, out_cap, nb); sc_append(out_text, out_cap, ",");
    sc_append(out_text, out_cap, no); sc_append(out_text, out_cap, ");}\n");
    return USD3D_OK;
}

int usd3d_shader_emit_mdl_stub(const usd3d_shader_program *program, char *out_text, unsigned int out_cap)
{
    if (program == 0 || out_text == 0 || out_cap == 0U) return USD3D_ERR_BAD_ARG;
    out_text[0] = '\0';
    sc_append(out_text, out_cap, "export material usd3d_material() = material(surface: material_surface());\n");
    return USD3D_OK;
}

int usd3d_shader_eval_constant(const usd3d_shader_program *program, usd3d_material_eval_output *out)
{
    unsigned long i;
    const usd3d_shader_instruction *ins;
    if (program == 0 || out == 0) return USD3D_ERR_BAD_ARG;
    memset(out, 0, sizeof(*out));
    out->base_color[0] = USD3D_FIXED_ONE;
    out->base_color[1] = USD3D_FIXED_ONE;
    out->base_color[2] = USD3D_FIXED_ONE;
    out->opacity = USD3D_FIXED_ONE;
    out->roughness = USD3D_FIXED_HALF;
    out->metallic = 0L;
    sc_copy(out->evaluator, sizeof(out->evaluator), program->source_kind == 2 ? "materialx-compiler-lite" : (program->source_kind == 3 ? "mdl-compiler-lite" : "preview-compiler-lite"));
    for (i = 0UL; i < program->instruction_count; ++i) {
        ins = &program->instructions[i];
        if (ins->op == USD3D_SHADER_OP_CONST_COLOR && ins->dst == 0) {
            out->base_color[0] = ins->imm[0]; out->base_color[1] = ins->imm[1]; out->base_color[2] = ins->imm[2];
        }
        if (ins->op == USD3D_SHADER_OP_CONST_SCALAR) {
            if (strstr(ins->text, "opacity") != 0) out->opacity = ins->imm[0];
            if (strstr(ins->text, "roughness") != 0) out->roughness = ins->imm[0];
            if (strstr(ins->text, "metallic") != 0) out->metallic = ins->imm[0];
        }
    }
    return USD3D_OK;
}
