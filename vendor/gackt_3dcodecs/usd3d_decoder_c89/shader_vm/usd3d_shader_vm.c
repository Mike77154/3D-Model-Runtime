#include "usd3d_shader_vm.h"
#include <string.h>
#include <stdio.h>

static int sv_append(char *out, unsigned long cap, unsigned long *pos, const char *text)
{
    unsigned long i;
    if (!out || !pos || cap == 0UL) return USD3D_ERR_BAD_ARG;
    if (!text) text = "";
    i = 0UL;
    while (text[i]) {
        if (*pos + 1UL >= cap) return USD3D_ERR_LIMIT;
        out[*pos] = text[i];
        (*pos)++;
        i++;
    }
    out[*pos] = '\0';
    return USD3D_OK;
}

void usd3d_shader_vm_input_default(usd3d_shader_vm_input *in)
{
    if (!in) return;
    memset(in, 0, sizeof(*in));
    in->normal[2] = USD3D_FIXED_ONE;
    in->view[2] = USD3D_FIXED_ONE;
    in->vertex_color[0] = USD3D_FIXED_ONE;
    in->vertex_color[1] = USD3D_FIXED_ONE;
    in->vertex_color[2] = USD3D_FIXED_ONE;
    in->vertex_color[3] = USD3D_FIXED_ONE;
}

void usd3d_shader_vm_output_default(usd3d_shader_vm_output *out)
{
    if (!out) return;
    memset(out, 0, sizeof(*out));
    out->base_color[0] = USD3D_FIXED_ONE;
    out->base_color[1] = USD3D_FIXED_ONE;
    out->base_color[2] = USD3D_FIXED_ONE;
    out->opacity = USD3D_FIXED_ONE;
    out->roughness = USD3D_FIXED_HALF;
}

int usd3d_shader_vm_execute(const usd3d_shader_program *program, const usd3d_shader_vm_input *in, usd3d_shader_vm_output *out)
{
    usd3d_fixed regs[USD3D_SHADER_VM_MAX_REGS][4];
    unsigned long i;
    int dst;
    int a;
    int b;
    const usd3d_shader_instruction *ins;
    if (!program || !out) return USD3D_ERR_BAD_ARG;
    (void)in;
    memset(regs, 0, sizeof(regs));
    usd3d_shader_vm_output_default(out);
    for (i = 0UL; i < program->instruction_count; i++) {
        ins = &program->instructions[i];
        dst = ins->dst;
        a = ins->a;
        b = ins->b;
        if (dst < 0 || dst >= USD3D_SHADER_VM_MAX_REGS) dst = 0;
        if (a < 0 || a >= USD3D_SHADER_VM_MAX_REGS) a = 0;
        if (b < 0 || b >= USD3D_SHADER_VM_MAX_REGS) b = 0;
        if (ins->op == USD3D_SHADER_OP_CONST_COLOR || ins->op == USD3D_SHADER_OP_CONST_SCALAR) {
            regs[dst][0] = ins->imm[0]; regs[dst][1] = ins->imm[1]; regs[dst][2] = ins->imm[2]; regs[dst][3] = ins->imm[3];
        } else if (ins->op == USD3D_SHADER_OP_MUL) {
            regs[dst][0] = usd3d_fixed_mul(regs[a][0], regs[b][0]);
            regs[dst][1] = usd3d_fixed_mul(regs[a][1], regs[b][1]);
            regs[dst][2] = usd3d_fixed_mul(regs[a][2], regs[b][2]);
            regs[dst][3] = usd3d_fixed_mul(regs[a][3], regs[b][3]);
        } else if (ins->op == USD3D_SHADER_OP_ADD) {
            regs[dst][0] = regs[a][0] + regs[b][0];
            regs[dst][1] = regs[a][1] + regs[b][1];
            regs[dst][2] = regs[a][2] + regs[b][2];
            regs[dst][3] = regs[a][3] + regs[b][3];
        } else if (ins->op == USD3D_SHADER_OP_STANDARD_SURFACE || ins->op == USD3D_SHADER_OP_OUTPUT_SURFACE) {
            out->base_color[0] = regs[a][0]; out->base_color[1] = regs[a][1]; out->base_color[2] = regs[a][2];
            out->opacity = regs[a][3] ? regs[a][3] : USD3D_FIXED_ONE;
            out->has_surface = 1;
        } else if (ins->op == USD3D_SHADER_OP_OUTPUT_DISPLACEMENT) {
            out->displacement = regs[a][0];
            out->has_displacement = 1;
        } else if (ins->op == USD3D_SHADER_OP_OUTPUT_VOLUME) {
            out->has_volume = 1;
        }
    }
    if (program->supports_surface) out->has_surface = 1;
    if (program->supports_volume) out->has_volume = 1;
    if (program->supports_displacement) out->has_displacement = 1;
    return USD3D_OK;
}

int usd3d_shader_vm_fold_constants(usd3d_shader_program *program)
{
    unsigned long i;
    unsigned long folded;
    if (!program) return USD3D_ERR_BAD_ARG;
    folded = 0UL;
    for (i = 0UL; i < program->instruction_count; i++) {
        if (program->instructions[i].op == USD3D_SHADER_OP_TEXTURE && program->instructions[i].text[0] == '\0') {
            program->instructions[i].op = USD3D_SHADER_OP_CONST_COLOR;
            program->instructions[i].imm[0] = USD3D_FIXED_ONE;
            program->instructions[i].imm[1] = USD3D_FIXED_ONE;
            program->instructions[i].imm[2] = USD3D_FIXED_ONE;
            program->instructions[i].imm[3] = USD3D_FIXED_ONE;
            folded++;
        }
    }
    return (int)folded;
}

int usd3d_shader_vm_emit_c89_function(const usd3d_shader_program *program, const char *function_name, char *out_text, unsigned long out_cap)
{
    unsigned long pos;
    char line[256];
    if (!program || !function_name || !out_text) return USD3D_ERR_BAD_ARG;
    pos = 0UL;
    out_text[0] = '\0';
    if (sv_append(out_text, out_cap, &pos, "/* generated fixed-point shader stub */\n") != USD3D_OK) return USD3D_ERR_LIMIT;
    if (sv_append(out_text, out_cap, &pos, "static void ") != USD3D_OK) return USD3D_ERR_LIMIT;
    if (sv_append(out_text, out_cap, &pos, function_name) != USD3D_OK) return USD3D_ERR_LIMIT;
    if (sv_append(out_text, out_cap, &pos, "(long out_rgba[4])\n{\n") != USD3D_OK) return USD3D_ERR_LIMIT;
    sprintf(line, "    /* instructions: %lu */\n", program->instruction_count);
    if (sv_append(out_text, out_cap, &pos, line) != USD3D_OK) return USD3D_ERR_LIMIT;
    if (sv_append(out_text, out_cap, &pos, "    out_rgba[0] = 65536L;\n    out_rgba[1] = 65536L;\n    out_rgba[2] = 65536L;\n    out_rgba[3] = 65536L;\n}\n") != USD3D_OK) return USD3D_ERR_LIMIT;
    return USD3D_OK;
}
