#include <string.h>
#include "usd3d_shader_bytecode.h"

static unsigned long sb_pack_i(long v)
{
    return (unsigned long)v;
}

static long sb_unpack_i(unsigned long v)
{
    return (long)v;
}

static void sb_copy(char *dst, unsigned long cap, const char *src)
{
    unsigned long i;
    if (!dst || cap == 0UL) return;
    if (!src) src = "";
    i = 0UL;
    while (i + 1UL < cap && src[i] != '\0') { dst[i] = src[i]; i++; }
    dst[i] = '\0';
}

static unsigned long sb_append(char *out, unsigned long cap, unsigned long pos, const char *text)
{
    unsigned long i;
    if (!out || cap == 0UL) return 0UL;
    if (!text) text = "";
    i = 0UL;
    while (pos + 1UL < cap && text[i] != '\0') { out[pos++] = text[i++]; }
    out[pos] = '\0';
    return pos;
}

static void sb_ulong_to_dec(unsigned long v, char *out, unsigned long cap)
{
    char tmp[32];
    unsigned long n;
    unsigned long i;
    unsigned long j;
    if (!out || cap == 0UL) return;
    if (v == 0UL) { sb_copy(out, cap, "0"); return; }
    n = 0UL;
    while (v != 0UL && n < sizeof(tmp)) {
        tmp[n++] = (char)('0' + (v % 10UL));
        v /= 10UL;
    }
    j = 0UL;
    for (i = 0UL; i < n && j + 1UL < cap; ++i) out[j++] = tmp[n - 1UL - i];
    out[j] = '\0';
}

void usd3d_shader_bytecode_init(usd3d_shader_bytecode *bc)
{
    if (!bc) return;
    memset(bc, 0, sizeof(*bc));
}

int usd3d_shader_bytecode_pack(const usd3d_shader_program *program, usd3d_shader_bytecode *bc)
{
    unsigned long i;
    unsigned long pos;
    const usd3d_shader_instruction *ins;
    if (!program || !bc) return USD3D_ERR_BAD_ARG;
    usd3d_shader_bytecode_init(bc);
    pos = 0UL;
    if (USD3D_SHADER_BYTECODE_MAX_WORDS < 8UL) return USD3D_ERR_LIMIT;
    bc->words[pos++] = USD3D_SHADER_BC_MAGIC;
    bc->words[pos++] = USD3D_SHADER_BC_VERSION;
    bc->words[pos++] = program->symbol_count;
    bc->words[pos++] = program->instruction_count;
    bc->words[pos++] = (unsigned long)program->supports_surface;
    bc->words[pos++] = (unsigned long)program->supports_displacement;
    bc->words[pos++] = (unsigned long)program->supports_volume;
    bc->words[pos++] = (unsigned long)program->source_kind;
    for (i = 0UL; i < program->instruction_count; ++i) {
        if (pos + 8UL >= USD3D_SHADER_BYTECODE_MAX_WORDS) return USD3D_ERR_LIMIT;
        ins = &program->instructions[i];
        bc->words[pos++] = (unsigned long)ins->op;
        bc->words[pos++] = sb_pack_i((long)ins->dst);
        bc->words[pos++] = sb_pack_i((long)ins->a);
        bc->words[pos++] = sb_pack_i((long)ins->b);
        bc->words[pos++] = sb_pack_i(ins->imm[0]);
        bc->words[pos++] = sb_pack_i(ins->imm[1]);
        bc->words[pos++] = sb_pack_i(ins->imm[2]);
        bc->words[pos++] = sb_pack_i(ins->imm[3]);
    }
    bc->word_count = pos;
    bc->symbol_count = program->symbol_count;
    bc->instruction_count = program->instruction_count;
    return USD3D_OK;
}

int usd3d_shader_bytecode_unpack(const usd3d_shader_bytecode *bc, usd3d_shader_program *program)
{
    unsigned long i;
    unsigned long pos;
    usd3d_fixed imm[4];
    int r;
    if (!bc || !program) return USD3D_ERR_BAD_ARG;
    if (bc->word_count < 8UL || bc->words[0] != USD3D_SHADER_BC_MAGIC) return USD3D_ERR_FORMAT;
    usd3d_shader_program_init(program);
    program->supports_surface = (int)bc->words[4];
    program->supports_displacement = (int)bc->words[5];
    program->supports_volume = (int)bc->words[6];
    program->source_kind = (int)bc->words[7];
    pos = 8UL;
    for (i = 0UL; i < bc->words[3]; ++i) {
        if (pos + 8UL > bc->word_count) return USD3D_ERR_FORMAT;
        imm[0] = sb_unpack_i(bc->words[pos + 4UL]);
        imm[1] = sb_unpack_i(bc->words[pos + 5UL]);
        imm[2] = sb_unpack_i(bc->words[pos + 6UL]);
        imm[3] = sb_unpack_i(bc->words[pos + 7UL]);
        r = usd3d_shader_add_instruction(program, (int)bc->words[pos], (int)sb_unpack_i(bc->words[pos + 1UL]), (int)sb_unpack_i(bc->words[pos + 2UL]), (int)sb_unpack_i(bc->words[pos + 3UL]), imm, "bc");
        if (r != USD3D_OK) return r;
        pos += 8UL;
    }
    return USD3D_OK;
}

int usd3d_shader_bytecode_link(const usd3d_shader_program *a, const usd3d_shader_program *b, usd3d_shader_program *out_program)
{
    unsigned long i;
    int r;
    if (!a || !b || !out_program) return USD3D_ERR_BAD_ARG;
    usd3d_shader_program_init(out_program);
    out_program->supports_surface = a->supports_surface || b->supports_surface;
    out_program->supports_displacement = a->supports_displacement || b->supports_displacement;
    out_program->supports_volume = a->supports_volume || b->supports_volume;
    out_program->source_kind = a->source_kind ? a->source_kind : b->source_kind;
    for (i = 0UL; i < a->instruction_count; ++i) {
        r = usd3d_shader_add_instruction(out_program, a->instructions[i].op, a->instructions[i].dst, a->instructions[i].a, a->instructions[i].b, a->instructions[i].imm, a->instructions[i].text);
        if (r != USD3D_OK) return r;
    }
    for (i = 0UL; i < b->instruction_count; ++i) {
        r = usd3d_shader_add_instruction(out_program, b->instructions[i].op, b->instructions[i].dst, b->instructions[i].a, b->instructions[i].b, b->instructions[i].imm, b->instructions[i].text);
        if (r != USD3D_OK) return r;
    }
    return USD3D_OK;
}

int usd3d_shader_bytecode_optimize(usd3d_shader_program *program)
{
    unsigned long i;
    unsigned long out;
    if (!program) return USD3D_ERR_BAD_ARG;
    out = 0UL;
    for (i = 0UL; i < program->instruction_count; ++i) {
        if (program->instructions[i].op != USD3D_SHADER_OP_NOP) {
            if (out != i) program->instructions[out] = program->instructions[i];
            out++;
        }
    }
    program->instruction_count = out;
    (void)usd3d_shader_vm_fold_constants(program);
    return USD3D_OK;
}

int usd3d_shader_bytecode_emit_c_array(const usd3d_shader_bytecode *bc, const char *name, char *out_text, unsigned long out_cap)
{
    unsigned long pos;
    unsigned long i;
    char num[32];
    if (!bc || !name || !out_text || out_cap == 0UL) return USD3D_ERR_BAD_ARG;
    out_text[0] = '\0';
    pos = 0UL;
    pos = sb_append(out_text, out_cap, pos, "static const unsigned long ");
    pos = sb_append(out_text, out_cap, pos, name);
    pos = sb_append(out_text, out_cap, pos, "[] = {\n");
    for (i = 0UL; i < bc->word_count; ++i) {
        pos = sb_append(out_text, out_cap, pos, "    ");
        sb_ulong_to_dec(bc->words[i], num, sizeof(num));
        pos = sb_append(out_text, out_cap, pos, num);
        pos = sb_append(out_text, out_cap, pos, "UL");
        if (i + 1UL < bc->word_count) pos = sb_append(out_text, out_cap, pos, ",");
        pos = sb_append(out_text, out_cap, pos, "\n");
    }
    pos = sb_append(out_text, out_cap, pos, "};\n");
    if (pos + 1UL >= out_cap) return USD3D_ERR_LIMIT;
    return USD3D_OK;
}
