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
static hl2m_flex_controller_state g_state;
static hl2m_flex_eval_report g_flex_report;
static hl2m_fixed g_weights[HL2M_MAX_VTA_FLEXES];
static hl2m_sequence_event_out g_events[HL2M_EVENT_WINDOW_MAX];
static hl2m_ik_pose g_pose;
static hl2m_ik_pose g_solved_pose;
static hl2m_ik_report g_ik_report;
static hl2m_mdl_hitbox g_hitboxes[HL2M_MAX_HITBOXES];
static hl2m_hitbox_build_report g_hitbox_report;
static hl2m_convex_decomp g_decomp;
static hl2m_convex_decomp g_split_decomp;
static hl2m_studiomdl_parity_report g_parity;

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
    int event_count;
    int hitbox_count;

    if (argc < 5) {
        printf("usage: hl2m_runtime_probe model.qc ref.smd anim.smd flex.vta\n");
        return 1;
    }
    sqc = load_file(argv[1], g_qc_text, FILE_BUF_SIZE);
    sref = load_file(argv[2], g_ref_text, FILE_BUF_SIZE);
    sanim = load_file(argv[3], g_anim_text, FILE_BUF_SIZE);
    svta = load_file(argv[4], g_vta_text, FILE_BUF_SIZE);
    if (sqc == 0u || sref == 0u || sanim == 0u || svta == 0u) return 2;

    r = hl2m_decode_qc(g_qc_text, sqc, &g_qc); if (r != HL2M_OK) return 3;
    r = hl2m_decode_smd(g_ref_text, sref, &g_ref_smd); if (r != HL2M_OK) return 4;
    r = hl2m_decode_smd(g_anim_text, sanim, &g_anim_smd); if (r != HL2M_OK) return 5;
    r = hl2m_decode_vta(g_vta_text, svta, &g_vta); if (r != HL2M_OK) return 6;
    r = hl2m_build_engine_model_from_smd(&g_ref_smd, &g_engine); if (r != HL2M_OK) return 7;

    r = hl2m_flex_state_from_qc(&g_qc, &g_state); if (r != HL2M_OK) return 8;
    if (g_state.controller_count > 0) {
        r = hl2m_flex_state_set(&g_state, g_state.names[0], HL2M_FIXED_ONE / 2); if (r != HL2M_OK) return 9;
    }
    r = hl2m_flex_state_clamp(&g_qc, &g_state, &g_flex_report); if (r != HL2M_OK) return 10;
    r = hl2m_flex_eval_weights(&g_qc, &g_vta, &g_state, g_weights, &g_flex_report); if (r != HL2M_OK) return 11;
    r = hl2m_engine_apply_flex_weights(&g_engine, &g_vta, g_weights, &g_flexed, &g_flex_report); if (r != HL2M_OK) return 12;

    event_count = 0;
    r = hl2m_collect_qc_sequence_events(&g_qc, "idle", 0, 99, g_events, HL2M_EVENT_WINDOW_MAX, &event_count);
    if (r != HL2M_OK) return 13;
    r = hl2m_ik_pose_from_smd_frame(&g_anim_smd, 0, &g_pose); if (r != HL2M_OK) return 14;
    r = hl2m_solve_ik_rules(&g_qc, &g_pose, -1, &g_solved_pose, &g_ik_report); if (r != HL2M_OK) return 15;
    hitbox_count = 0;
    r = hl2m_build_hitboxes_from_qc_smd(&g_qc, &g_ref_smd, g_hitboxes, HL2M_MAX_HITBOXES, &hitbox_count, &g_hitbox_report);
    if (r != HL2M_OK) return 16;
    r = hl2m_build_convex_decomp_from_smd(&g_ref_smd, HL2M_COLLISION_BONE_MATERIAL_AABB, &g_decomp); if (r != HL2M_OK) return 17;
    r = hl2m_convex_decomp_split_long_axes(&g_decomp, 1, &g_split_decomp); if (r != HL2M_OK) return 18;

    r = hl2m_make_compile_options_from_qc(&g_qc, &g_opt); if (r != HL2M_OK) return 19;
    g_opt.checksum = 8888;
    g_opt.collision_mode = HL2M_COLLISION_BONE_MATERIAL_AABB;
    r = hl2m_compile_qc_smd_to_binary(&g_qc, &g_ref_smd, &g_anim_smd, &g_vta, &g_opt,
                                      g_mdl_bytes, BIN_BUF_SIZE, &smdl,
                                      g_vvd_bytes, BIN_BUF_SIZE, &svvd,
                                      g_vtx_bytes, BIN_BUF_SIZE, &svtx,
                                      g_phy_bytes, BIN_BUF_SIZE, &sphy,
                                      g_ani_bytes, BIN_BUF_SIZE, &sani,
                                      &g_result);
    if (r != HL2M_OK) return 20;
    r = hl2m_decode_mdl(g_mdl_bytes, smdl, &g_mdl); if (r != HL2M_OK) return 21;
    r = hl2m_decode_vvd(g_vvd_bytes, svvd, 0, &g_vvd); if (r != HL2M_OK) return 22;
    r = hl2m_decode_vtx(g_vtx_bytes, svtx, &g_vtx); if (r != HL2M_OK) return 23;
    r = hl2m_decode_phy(g_phy_bytes, sphy, &g_phy); if (r != HL2M_OK) return 24;
    r = hl2m_decode_ani(g_ani_bytes, sani, &g_mdl, &g_ani); if (r != HL2M_OK) return 25;
    r = hl2m_build_studiomdl_parity_report(&g_qc, &g_ref_smd, &g_vta, &g_mdl, &g_vtx, &g_phy, &g_ani, &g_parity);
    if (r != HL2M_OK) return 26;

    printf("runtime flex=%d active=%d rules=%d applied=%d events=%d ik_solved=%d hitboxes=%d convex=%d split=%d parity=0x%x missing=0x%x warnings=0x%x\n",
           g_flex_report.flex_count, g_flex_report.active_flex_count, g_flex_report.rules_evaluated,
           g_flex_report.applied_vertex_count, event_count, g_ik_report.solved_count,
           hitbox_count, g_decomp.hull_count, g_split_decomp.hull_count,
           g_parity.feature_flags, g_parity.missing_flags, g_parity.warning_flags);
    printf("flexed0=");
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

    if (g_flex_report.rules_evaluated < 1) return 27;
    if (g_flex_report.applied_vertex_count < 1) return 28;
    if (event_count < 1) return 29;
    if (g_ik_report.solved_count < 1) return 30;
    if (hitbox_count < 2) return 31;
    if (g_split_decomp.hull_count <= g_decomp.hull_count) return 32;
    if ((g_parity.feature_flags & HL2M_PARITY_FLEX_RULE_EVAL) == 0) return 33;
    if ((g_parity.feature_flags & HL2M_PARITY_IK_RUNTIME) == 0) return 34;
    return 0;
}
