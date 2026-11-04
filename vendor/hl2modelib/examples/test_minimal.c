#include "hl2modelib.h"
#include <stdio.h>

static const unsigned char g_smd_text[] =
"version 1\n"
"nodes\n"
"0 \"root\" -1\n"
"end\n"
"skeleton\n"
"time 0\n"
"0 0 0 0 0 0 0\n"
"end\n"
"triangles\n"
"debug_material\n"
"0 0 0 0 0 0 1 0 0 1 0 1.0\n"
"0 1 0 0 0 0 1 1 0 1 0 1.0\n"
"0 0 1 0 0 0 1 0 1 1 0 1.0\n"
"end\n";

static const unsigned char g_vmt_text[] =
"VertexLitGeneric\n"
"{\n"
"  \"$basetexture\" \"models/debug/diffuse\"\n"
"  \"$surfaceprop\" \"metal\"\n"
"  \"Proxies\" { \"AnimatedTexture\" { \"animatedtexturevar\" \"$basetexture\" } }\n"
"}\n";

static hl2m_smd g_smd;
static hl2m_smd g_smd_round;
static hl2m_vmt g_vmt;
static hl2m_material_desc g_mat;
static hl2m_engine_model g_engine;
static hl2m_qc g_qc;
static char g_text[32768];
static char g_obj[32768];

int main(void)
{
    int r1;
    int r2;
    int r3;
    int r4;
    int r5;
    int r6;
    int r7;
    int r8;
    unsigned int used;
    hl2m_vec3 bbmin;
    hl2m_vec3 bbmax;
    r1 = hl2m_decode_smd(g_smd_text, (unsigned int)(sizeof(g_smd_text) - 1u), &g_smd);
    r2 = hl2m_decode_vmt(g_vmt_text, (unsigned int)(sizeof(g_vmt_text) - 1u), &g_vmt);
    r3 = hl2m_vmt_to_material_desc(&g_vmt, &g_mat);
    r4 = hl2m_build_engine_model_from_smd(&g_smd, &g_engine);
    r5 = hl2m_engine_model_bounds(&g_engine, &bbmin, &bbmax);
    r6 = hl2m_smd_write(&g_smd, g_text, (unsigned int)sizeof(g_text), &used);
    r7 = hl2m_decode_smd((const hl2m_u8 *)g_text, used, &g_smd_round);
    r8 = hl2m_engine_model_write_obj(&g_engine, "minimal", g_obj, (unsigned int)sizeof(g_obj), &used);
    if (r1 == HL2M_OK) (void)hl2m_make_basic_qc(&g_smd, "models/debug/generated.mdl", "body", "generated_ref.smd", "models/debug", 1, &g_qc);
    printf("smd=%d nodes=%d tris=%d verts=%d mat=%s\n", r1, g_smd.node_count, g_smd.triangle_count, g_smd.vertex_count, g_smd.materials[0]);
    printf("vmt=%d shader=%s params=%d first=%s=%s matflags=%d base=%s\n", r2, g_vmt.shader, g_vmt.param_count, g_vmt.params[0].key, g_vmt.params[0].value, g_mat.flags, g_mat.base_texture);
    printf("engine_from_smd=%d verts=%d indices=%d draws=%d bounds=%d qccmds=%d smdwrite=%d smdround=%d roundverts=%d obj=%d\n",
           r4, g_engine.vertex_count, g_engine.index_count, g_engine.draw_count, r5, g_qc.command_count, r6, r7, g_smd_round.vertex_count, r8);
    return (r1 == HL2M_OK && r2 == HL2M_OK && r3 == HL2M_OK && r4 == HL2M_OK && r5 == HL2M_OK && r6 == HL2M_OK && r7 == HL2M_OK && r8 == HL2M_OK) ? 0 : 1;
}
