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
static hl2m_compile_options g_opt;
static hl2m_compile_result g_result;
static hl2m_bundle_report g_bundle;
static hl2m_convex_decomp g_decomp;
static hl2m_engine_model g_engine;
static hl2m_engine_model g_flexed;
static hl2m_flex_controller_state g_state;

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
        printf("usage: hl2m_parity_probe model.qc ref.smd anim.smd flex.vta\n");
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
    if (r != HL2M_OK) { printf("ref decode=%d\n", r); return 4; }
    r = hl2m_decode_smd(g_anim_text, sanim, &g_anim_smd);
    if (r != HL2M_OK) { printf("anim decode=%d\n", r); return 5; }
    r = hl2m_decode_vta(g_vta_text, svta, &g_vta);
    if (r != HL2M_OK) { printf("vta decode=%d\n", r); return 6; }

    r = hl2m_make_compile_options_from_qc(&g_qc, &g_opt);
    if (r != HL2M_OK) { printf("options=%d\n", r); return 7; }
    g_opt.checksum = 7777;
    g_opt.collision_mode = HL2M_COLLISION_BONE_MATERIAL_AABB;
    g_opt.convex_max_hulls = HL2M_MAX_CONVEX_HULLS;

    r = hl2m_build_convex_decomp_from_smd(&g_ref_smd, g_opt.collision_mode, &g_decomp);
    if (r != HL2M_OK) { printf("convex=%d\n", r); return 8; }

    r = hl2m_build_engine_model_from_smd(&g_ref_smd, &g_engine);
    if (r != HL2M_OK) { printf("engine=%d\n", r); return 9; }
    r = hl2m_flex_state_from_qc(&g_qc, &g_state);
    if (r != HL2M_OK) { printf("state=%d\n", r); return 10; }
    if (g_qc.flexcontroller_count > 0) {
        r = hl2m_flex_state_set(&g_state, g_qc.flexcontrollers[0].name, HL2M_FIXED_ONE / 2);
        if (r != HL2M_OK) { printf("state set=%d\n", r); return 11; }
    }
    r = hl2m_engine_apply_flex_controllers(&g_engine, &g_vta, &g_qc, &g_state, &g_flexed);
    if (r != HL2M_OK) { printf("flex controllers=%d\n", r); return 12; }

    r = hl2m_compile_qc_smd_to_binary(&g_qc, &g_ref_smd, &g_anim_smd, &g_vta, &g_opt,
                                      g_mdl_bytes, BIN_BUF_SIZE, &smdl,
                                      g_vvd_bytes, BIN_BUF_SIZE, &svvd,
                                      g_vtx_bytes, BIN_BUF_SIZE, &svtx,
                                      g_phy_bytes, BIN_BUF_SIZE, &sphy,
                                      g_ani_bytes, BIN_BUF_SIZE, &sani,
                                      &g_result);
    if (r != HL2M_OK) { printf("compile=%d\n", r); return 13; }

    r = hl2m_validate_binary_bundle(g_mdl_bytes, smdl, g_vvd_bytes, svvd, g_vtx_bytes, svtx, g_phy_bytes, sphy, g_ani_bytes, sani, &g_bundle);
    if (r != HL2M_OK) { printf("bundle=%d\n", r); return 14; }
    r = hl2m_decode_mdl(g_mdl_bytes, smdl, &g_mdl);
    if (r != HL2M_OK) { printf("mdl=%d\n", r); return 15; }
    r = hl2m_decode_vvd(g_vvd_bytes, svvd, 0, &g_vvd);
    if (r != HL2M_OK) { printf("vvd=%d\n", r); return 16; }
    r = hl2m_decode_vtx(g_vtx_bytes, svtx, &g_vtx);
    if (r != HL2M_OK) { printf("vtx=%d\n", r); return 17; }
    r = hl2m_decode_phy(g_phy_bytes, sphy, &g_phy);
    if (r != HL2M_OK) { printf("phy=%d\n", r); return 18; }
    r = hl2m_decode_ani(g_ani_bytes, sani, &g_mdl, &g_ani);
    if (r != HL2M_OK) { printf("ani=%d\n", r); return 19; }

    printf("qc flexctrl=%d flexrules=%d events=%d ikchains=%d ikrules=%d hboxes=%d\n",
           g_qc.flexcontroller_count, g_qc.flexrule_count, g_qc.sequence_event_count,
           g_qc.ikchain_count, g_qc.ikrule_count, g_qc.qc_hitbox_count);
    printf("compile hitboxes=%d convex=%d flexctrl=%d flexrules=%d events=%d ikchains=%d ikrules=%d warnings=%d bundle=%d\n",
           g_result.hitbox_count, g_result.convex_hull_count, g_result.flex_controller_count,
           g_result.flex_rule_count, g_result.sequence_event_count, g_result.ik_chain_count,
           g_result.ik_rule_count, g_result.warning_flags, g_bundle.warning_flags);
    printf("decoded mdl_hboxes=%d mdl_events=%d mdl_ikrules=%d phy_solids=%d ani_channels=%d vtx_lods=%d\n",
           g_mdl.hitbox_count, g_mdl.event_count, g_mdl.ikrule_count,
           g_phy.solid_count, g_ani.scanned_channel_count, g_vtx.lod_count);
    printf("convex_decomp hulls=%d verts=%d flexed0=", g_decomp.hull_count, g_decomp.vertex_count);
    if (g_flexed.vertex_count > 0) {
        print_fixed(g_flexed.vertices[0].position.x);
        printf(",");
        print_fixed(g_flexed.vertices[0].position.y);
        printf(",");
        print_fixed(g_flexed.vertices[0].position.z);
    } else {
        printf("empty");
    }
    printf("\n");

    if (g_qc.qc_hitbox_count < 2) return 20;
    if (g_qc.sequence_event_count < 1) return 21;
    if (g_qc.ikrule_count < 1) return 22;
    if (g_qc.flexcontroller_count < 1) return 23;
    if (g_mdl.hitbox_count < 2) return 24;
    if (g_mdl.event_count < 1) return 25;
    if (g_mdl.ikrule_count < 1) return 26;
    if (g_phy.solid_count < 1) return 27;
    if (g_decomp.hull_count < 1) return 28;
    return 0;
}
