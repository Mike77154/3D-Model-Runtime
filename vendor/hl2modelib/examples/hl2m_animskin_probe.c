#include "hl2modelib.h"
#include <stdio.h>

#define FILE_BUF_SIZE (1024u * 1024u)
#define BIN_BUF_SIZE (4u * 1024u * 1024u)

static unsigned char g_qc_text[FILE_BUF_SIZE];
static unsigned char g_ref_text[FILE_BUF_SIZE];
static unsigned char g_anim_text[FILE_BUF_SIZE];
static unsigned char g_vta_text[FILE_BUF_SIZE];
static hl2m_qc g_qc;
static hl2m_smd g_ref_smd;
static hl2m_smd g_anim_smd;
static hl2m_vta g_vta;
static hl2m_engine_model g_engine;
static hl2m_engine_model g_skinned;
static hl2m_ik_pose g_bind_pose;
static hl2m_ik_pose g_anim_pose;
static hl2m_pose_matrices g_inv_bind;
static hl2m_pose_matrices g_pose_world;
static hl2m_anim_sample_report g_anim_report;
static hl2m_skin_report g_skin_report;
static hl2m_event_cursor g_cursor;
static hl2m_sequence_event_out g_events[HL2M_EVENT_WINDOW_MAX];
static hl2m_compile_options g_opt;
static hl2m_lod_select_report g_lod_report;
static hl2m_mdl g_mdl;
static hl2m_vtx g_vtx;
static hl2m_phy g_phy;
static hl2m_ani g_ani;
static hl2m_studiomdl_parity_report g_parity;
static unsigned char g_mdl_bytes[BIN_BUF_SIZE];
static unsigned char g_vvd_bytes[BIN_BUF_SIZE];
static unsigned char g_vtx_bytes[BIN_BUF_SIZE];
static unsigned char g_phy_bytes[BIN_BUF_SIZE];
static unsigned char g_ani_bytes[BIN_BUF_SIZE];
static hl2m_compile_result g_compile;

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
    int first_frame;
    int last_frame;
    int frame_count;
    int event_count;
    int lod;

    if (argc < 5) {
        printf("usage: hl2m_animskin_probe model.qc ref.smd anim.smd flex.vta\n");
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
    r = hl2m_smd_frame_range(&g_anim_smd, &first_frame, &last_frame, &frame_count); if (r != HL2M_OK) return 8;
    r = hl2m_sample_smd_pose_time(&g_ref_smd, 0, &g_bind_pose, &g_anim_report); if (r != HL2M_OK) return 9;
    r = hl2m_sample_smd_pose_time(&g_anim_smd, HL2M_FIXED_ONE / 2, &g_anim_pose, &g_anim_report); if (r != HL2M_OK) return 10;
    r = hl2m_pose_inverse_bind_from_pose(&g_bind_pose, HL2M_POSE_TRANSLATION_ONLY, &g_inv_bind); if (r != HL2M_OK) return 11;
    r = hl2m_pose_matrices_from_pose(&g_anim_pose, HL2M_POSE_TRANSLATION_ONLY, &g_pose_world); if (r != HL2M_OK) return 12;
    r = hl2m_engine_cpu_skin(&g_engine, &g_inv_bind, &g_pose_world, &g_skinned, &g_skin_report); if (r != HL2M_OK) return 13;

    r = hl2m_event_cursor_init("idle", 0, last_frame, &g_cursor); if (r != HL2M_OK) return 14;
    event_count = 0;
    r = hl2m_event_cursor_advance(&g_qc, &g_cursor, 1, g_events, HL2M_EVENT_WINDOW_MAX, &event_count); if (r != HL2M_OK) return 15;
    r = hl2m_make_compile_options_from_qc(&g_qc, &g_opt); if (r != HL2M_OK) return 16;
    r = hl2m_select_lod_from_distance(&g_opt, 25 * HL2M_FIXED_ONE, &lod, &g_lod_report); if (r != HL2M_OK) return 17;

    g_opt.checksum = 9090;
    g_opt.collision_mode = HL2M_COLLISION_BONE_MATERIAL_AABB;
    r = hl2m_compile_qc_smd_to_binary(&g_qc, &g_ref_smd, &g_anim_smd, &g_vta, &g_opt,
                                      g_mdl_bytes, BIN_BUF_SIZE, &smdl,
                                      g_vvd_bytes, BIN_BUF_SIZE, &svvd,
                                      g_vtx_bytes, BIN_BUF_SIZE, &svtx,
                                      g_phy_bytes, BIN_BUF_SIZE, &sphy,
                                      g_ani_bytes, BIN_BUF_SIZE, &sani,
                                      &g_compile);
    if (r != HL2M_OK) return 18;
    r = hl2m_decode_mdl(g_mdl_bytes, smdl, &g_mdl); if (r != HL2M_OK) return 19;
    r = hl2m_decode_vtx(g_vtx_bytes, svtx, &g_vtx); if (r != HL2M_OK) return 20;
    r = hl2m_decode_phy(g_phy_bytes, sphy, &g_phy); if (r != HL2M_OK) return 21;
    r = hl2m_decode_ani(g_ani_bytes, sani, &g_mdl, &g_ani); if (r != HL2M_OK) return 22;
    r = hl2m_build_studiomdl_parity_report(&g_qc, &g_ref_smd, &g_vta, &g_mdl, &g_vtx, &g_phy, &g_ani, &g_parity); if (r != HL2M_OK) return 23;

    printf("animskin frames=%d..%d count=%d sample=%d/%d blend=", first_frame, last_frame, frame_count, g_anim_report.frame_a, g_anim_report.frame_b);
    print_fixed(g_anim_report.blend);
    printf(" skinned=%d events=%d lod=%d parity=0x%x missing=0x%x\n", g_skin_report.skinned_count, event_count, lod, g_parity.feature_flags, g_parity.missing_flags);
    printf("skinned1=");
    if (g_skinned.vertex_count > 1) {
        print_fixed(g_skinned.vertices[1].position.x);
        printf(",");
        print_fixed(g_skinned.vertices[1].position.y);
        printf(",");
        print_fixed(g_skinned.vertices[1].position.z);
    } else {
        printf("empty");
    }
    printf("\n");

    if (frame_count < 2) return 24;
    if (g_anim_report.interpolated_count < 2) return 25;
    if (g_skin_report.skinned_count < 2) return 26;
    if (event_count < 1) return 27;
    if (lod < 1) return 28;
    if ((g_parity.feature_flags & HL2M_PARITY_ANIM_SAMPLER) == 0) return 29;
    if ((g_parity.feature_flags & HL2M_PARITY_CPU_SKINNING) == 0) return 30;
    if ((g_parity.feature_flags & HL2M_PARITY_EVENT_CURSOR) == 0) return 31;
    if ((g_parity.feature_flags & HL2M_PARITY_LOD_RUNTIME) == 0) return 32;
    return 0;
}
