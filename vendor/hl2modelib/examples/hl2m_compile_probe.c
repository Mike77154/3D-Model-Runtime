#include "hl2modelib.h"
#include <stdio.h>

#define FILE_BUF_SIZE (1024u * 1024u)
#define BIN_BUF_SIZE (4u * 1024u * 1024u)

static unsigned char g_qc_text[FILE_BUF_SIZE];
static unsigned char g_ref_text[FILE_BUF_SIZE];
static unsigned char g_anim_text[FILE_BUF_SIZE];
static unsigned char g_vta_text[FILE_BUF_SIZE];
static unsigned char g_mdl_bytes[BIN_BUF_SIZE];
static unsigned char g_vvd_bytes[BIN_BUF_SIZE];
static unsigned char g_vtx_bytes[BIN_BUF_SIZE];
static unsigned char g_phy_bytes[BIN_BUF_SIZE];
static unsigned char g_ani_bytes[BIN_BUF_SIZE];
static hl2m_qc g_qc;
static hl2m_smd g_ref_smd;
static hl2m_smd g_anim_smd;
static hl2m_vta g_vta;
static hl2m_mdl g_mdl;
static hl2m_vvd g_vvd;
static hl2m_vtx g_vtx;
static hl2m_phy g_phy;
static hl2m_ani g_ani;
static hl2m_engine_model g_engine;
static hl2m_engine_model g_flexed;
static hl2m_compile_options g_opt;
static hl2m_compile_result g_result;
static hl2m_compile_result g_validation;
static hl2m_bundle_report g_bundle_report;
static unsigned char g_vta_roundtrip[FILE_BUF_SIZE];
static hl2m_vta g_vta_round;

static unsigned int load_file(const char *path, unsigned char *buffer, unsigned int cap)
{
    FILE *f;
    size_t got;
    f = fopen(path, "rb");
    if (!f) return 0u;
    got = fread(buffer, 1u, cap, f);
    fclose(f);
    return (unsigned int)got;
}

static void print_fixed(hl2m_fixed v)
{
    char text[32];
    hl2m_fixed_to_string(v, text, (unsigned int)sizeof(text), 4);
    printf("%s", text);
}

int main(int argc, char **argv)
{
    unsigned int sqc;
    unsigned int sref;
    unsigned int sanim;
    unsigned int svta;
    unsigned int smdl;
    unsigned int svvd;
    unsigned int svtx;
    unsigned int sphy;
    unsigned int sani;
    int r;

    if (argc < 5) {
        printf("usage: hl2m_compile_probe model.qc ref.smd anim.smd flex.vta\n");
        return 1;
    }
    sqc = load_file(argv[1], g_qc_text, FILE_BUF_SIZE);
    sref = load_file(argv[2], g_ref_text, FILE_BUF_SIZE);
    sanim = load_file(argv[3], g_anim_text, FILE_BUF_SIZE);
    svta = load_file(argv[4], g_vta_text, FILE_BUF_SIZE);
    if (sqc == 0u || sref == 0u || sanim == 0u || svta == 0u) {
        printf("could not read one or more inputs\n");
        return 2;
    }
    r = hl2m_decode_qc(g_qc_text, sqc, &g_qc);
    if (r != HL2M_OK) { printf("qc decode=%d\n", r); return 3; }
    r = hl2m_decode_smd(g_ref_text, sref, &g_ref_smd);
    if (r != HL2M_OK) { printf("ref smd decode=%d\n", r); return 4; }
    r = hl2m_decode_smd(g_anim_text, sanim, &g_anim_smd);
    if (r != HL2M_OK) { printf("anim smd decode=%d\n", r); return 5; }
    r = hl2m_decode_vta(g_vta_text, svta, &g_vta);
    if (r != HL2M_OK) { printf("vta decode=%d\n", r); return 6; }

    r = hl2m_make_compile_options_from_qc(&g_qc, &g_opt);
    if (r != HL2M_OK) { printf("qc options=%d\n", r); return 7; }
    g_opt.checksum = 5678;
    g_opt.collision_mode = HL2M_COLLISION_PER_BONE_AABB;
    r = hl2m_validate_compile_inputs(&g_qc, &g_ref_smd, &g_anim_smd, &g_vta, &g_validation);
    if (r != HL2M_OK) { printf("validate inputs=%d\n", r); return 7; }
    r = hl2m_compile_qc_smd_to_binary(&g_qc, &g_ref_smd, &g_anim_smd, &g_vta, &g_opt,
                                      g_mdl_bytes, BIN_BUF_SIZE, &smdl,
                                  g_vvd_bytes, BIN_BUF_SIZE, &svvd,
                                  g_vtx_bytes, BIN_BUF_SIZE, &svtx,
                                  g_phy_bytes, BIN_BUF_SIZE, &sphy,
                                  g_ani_bytes, BIN_BUF_SIZE, &sani,
                                  &g_result);
    if (r != HL2M_OK) { printf("compile=%d\n", r); return 7; }
    r = hl2m_validate_binary_bundle(g_mdl_bytes, smdl, g_vvd_bytes, svvd, g_vtx_bytes, svtx, g_phy_bytes, sphy, g_ani_bytes, sani, &g_bundle_report);
    if (r != HL2M_OK) { printf("bundle validate=%d\n", r); return 7; }

    r = hl2m_decode_mdl(g_mdl_bytes, smdl, &g_mdl);
    if (r != HL2M_OK) { printf("compiled mdl decode=%d\n", r); return 8; }
    r = hl2m_decode_vvd(g_vvd_bytes, svvd, 0, &g_vvd);
    if (r != HL2M_OK) { printf("compiled vvd decode=%d\n", r); return 9; }
    r = hl2m_decode_vtx(g_vtx_bytes, svtx, &g_vtx);
    if (r != HL2M_OK) { printf("compiled vtx decode=%d\n", r); return 10; }
    r = hl2m_decode_phy(g_phy_bytes, sphy, &g_phy);
    if (r != HL2M_OK) { printf("compiled phy decode=%d\n", r); return 11; }
    r = hl2m_decode_ani(g_ani_bytes, sani, &g_mdl, &g_ani);
    if (r != HL2M_OK) { printf("compiled ani decode=%d\n", r); return 12; }
    r = hl2m_build_engine_model(&g_mdl, &g_vvd, &g_vtx, 0, &g_engine);
    if (r != HL2M_OK) { printf("engine build=%d\n", r); return 13; }
    {
        unsigned int vta_used;
        r = hl2m_vta_write(&g_vta, (char *)g_vta_roundtrip, FILE_BUF_SIZE, &vta_used);
        if (r != HL2M_OK) { printf("vta write=%d\n", r); return 14; }
        r = hl2m_decode_vta(g_vta_roundtrip, vta_used, &g_vta_round);
        if (r != HL2M_OK) { printf("vta round=%d\n", r); return 15; }
    }
    r = hl2m_engine_apply_vta_flex(&g_engine, &g_vta, g_vta.flex_count > 1 ? 1 : 0, HL2M_FIXED_ONE / 2, &g_flexed);
    if (r != HL2M_OK) { printf("flex apply=%d\n", r); return 16; }

    printf("compile=0 mdl=%u vvd=%u vtx=%u phy=%u ani=%u warnings=%d bundle_warnings=%d input_warnings=%d\n",
           smdl, svvd, svtx, sphy, sani, g_result.warning_flags, g_bundle_report.warning_flags, g_validation.warning_flags);
    printf("decoded bones=%d mats=%d meshes=%d lods=%d vtx_indices=%d phy_solids=%d ani_blocks=%d ani_channels=%d engine_vertices=%d draws=%d vta_round_flexes=%d badbones=%d weightfix=%d vtarange=%d\n",
           g_mdl.bone_count, g_mdl.texture_count, g_mdl.mesh_count,
           g_vtx.lod_count, g_vtx.index_count, g_phy.solid_count,
           g_ani.animblock_count, g_ani.scanned_channel_count,
           g_engine.vertex_count, g_engine.draw_count, g_vta_round.flex_count,
           g_result.bad_bone_ref_count, g_result.weight_repair_count, g_result.vta_range_count);
    if (g_engine.vertex_count > 0) {
        printf("first_vertex=");
        print_fixed(g_engine.vertices[0].position.x);
        printf(",");
        print_fixed(g_engine.vertices[0].position.y);
        printf(",");
        print_fixed(g_engine.vertices[0].position.z);
        printf("\n");
        printf("flexed_first_vertex=");
        print_fixed(g_flexed.vertices[0].position.x);
        printf(",");
        print_fixed(g_flexed.vertices[0].position.y);
        printf(",");
        print_fixed(g_flexed.vertices[0].position.z);
        printf("\n");
    }
    return 0;
}
