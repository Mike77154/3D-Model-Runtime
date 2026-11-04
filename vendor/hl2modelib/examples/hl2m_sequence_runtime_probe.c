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
static hl2m_engine_model g_base_model;
static hl2m_engine_model g_runtime_model;
static hl2m_engine_model g_lod_model;
static hl2m_flex_controller_state g_flex_state;
static hl2m_compile_options g_opt;
static hl2m_sequence_runtime_report g_runtime_report;
static hl2m_sequence_event_out g_events[HL2M_EVENT_WINDOW_MAX];
static hl2m_anim_sample_report g_blend_report;
static hl2m_ik_pose g_bind_pose;
static hl2m_ik_pose g_anim_pose;
static hl2m_ik_pose g_blend_pose;
static hl2m_ik_pose g_add_pose;
static hl2m_pose_matrices g_pose_world;
static hl2m_attachment_pose g_attachments[HL2M_MAX_QC_ATTACHMENTS];
static hl2m_attachment_report g_attachment_report;
static hl2m_mdl g_mdl;
static hl2m_world_hitbox g_world_hitboxes[HL2M_MAX_HITBOXES];
static hl2m_hitbox_transform_report g_hitbox_report;
static hl2m_compile_result g_compile_result;
static hl2m_draw_filter_report g_draw_report;

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
    int attachment_count;
    int hitbox_count;

    if (argc < 5) {
        printf("usage: hl2m_sequence_runtime_probe model.qc ref.smd anim.smd flex.vta\n");
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
    r = hl2m_build_engine_model_from_smd(&g_ref_smd, &g_base_model); if (r != HL2M_OK) return 7;
    r = hl2m_make_compile_options_from_qc(&g_qc, &g_opt); if (r != HL2M_OK) return 8;
    r = hl2m_flex_state_from_qc(&g_qc, &g_flex_state); if (r != HL2M_OK) return 9;
    if (g_flex_state.controller_count > 0) {
        r = hl2m_flex_state_set(&g_flex_state, g_flex_state.names[0], HL2M_FIXED_ONE / 2); if (r != HL2M_OK) return 10;
    }

    event_count = 0;
    r = hl2m_runtime_eval_sequence(&g_qc, &g_ref_smd, &g_anim_smd, &g_base_model, &g_vta, &g_flex_state, &g_opt,
                                   "idle", HL2M_FIXED_ONE / 2, 25 * HL2M_FIXED_ONE,
                                   HL2M_POSE_INCLUDE_ROTATION, &g_runtime_model,
                                   g_events, HL2M_EVENT_WINDOW_MAX, &event_count, &g_runtime_report);
    if (r != HL2M_OK) return 11;

    r = hl2m_sample_smd_pose_time(&g_ref_smd, 0, &g_bind_pose, &g_blend_report); if (r != HL2M_OK) return 12;
    r = hl2m_sample_smd_pose_time(&g_anim_smd, HL2M_FIXED_ONE, &g_anim_pose, &g_blend_report); if (r != HL2M_OK) return 13;
    r = hl2m_blend_poses(&g_bind_pose, &g_anim_pose, HL2M_FIXED_ONE / 2, &g_blend_pose, &g_blend_report); if (r != HL2M_OK) return 14;
    r = hl2m_apply_additive_pose(&g_bind_pose, &g_bind_pose, &g_anim_pose, HL2M_FIXED_ONE / 2, &g_add_pose, &g_blend_report); if (r != HL2M_OK) return 15;
    r = hl2m_pose_matrices_from_pose(&g_blend_pose, HL2M_POSE_INCLUDE_ROTATION, &g_pose_world); if (r != HL2M_OK) return 16;

    attachment_count = 0;
    r = hl2m_build_attachment_poses_from_qc(&g_qc, &g_pose_world, g_attachments, HL2M_MAX_QC_ATTACHMENTS, &attachment_count, &g_attachment_report);
    if (r != HL2M_OK) return 17;

    g_opt.checksum = 10000;
    g_opt.collision_mode = HL2M_COLLISION_BONE_MATERIAL_AABB;
    r = hl2m_compile_qc_smd_to_binary(&g_qc, &g_ref_smd, &g_anim_smd, &g_vta, &g_opt,
                                      g_mdl_bytes, BIN_BUF_SIZE, &smdl,
                                      g_vvd_bytes, BIN_BUF_SIZE, &svvd,
                                      g_vtx_bytes, BIN_BUF_SIZE, &svtx,
                                      g_phy_bytes, BIN_BUF_SIZE, &sphy,
                                      g_ani_bytes, BIN_BUF_SIZE, &sani,
                                      &g_compile_result);
    if (r != HL2M_OK) return 18;
    r = hl2m_decode_mdl(g_mdl_bytes, smdl, &g_mdl); if (r != HL2M_OK) return 19;
    hitbox_count = 0;
    r = hl2m_transform_hitboxes_to_world(g_mdl.hitboxes, g_mdl.hitbox_count, &g_pose_world, g_world_hitboxes, HL2M_MAX_HITBOXES, &hitbox_count, &g_hitbox_report);
    if (r != HL2M_OK) return 20;
    r = hl2m_engine_filter_draws_for_lod(&g_runtime_model, g_runtime_report.selected_lod, &g_lod_model, &g_draw_report);
    if (r != HL2M_OK) return 21;

    printf("sequence runtime bones=%d skinned=%d flex=%d events=%d lod=%d draws=%d attachments=%d hitboxes=%d blendbones=%d addz=",
           g_runtime_report.sampled_bones, g_runtime_report.skinned_vertices,
           g_runtime_report.active_flex_count, event_count, g_runtime_report.selected_lod,
           g_lod_model.draw_count, attachment_count, hitbox_count, g_blend_pose.bone_count);
    if (g_add_pose.bone_count > 1) print_fixed(g_add_pose.pos[1].z);
    else printf("0");
    printf(" attz=");
    if (attachment_count > 0) print_fixed(g_attachments[0].world_pos.z);
    else printf("0");
    printf(" warnings=0x%x\n", g_runtime_report.warning_flags);

    if (g_runtime_report.skinned_vertices < 2) return 22;
    if (event_count < 1) return 23;
    if (g_runtime_report.active_flex_count < 1) return 24;
    if (g_runtime_report.selected_lod < 1) return 25;
    if (attachment_count < 1) return 26;
    if (hitbox_count < 2) return 27;
    if (g_blend_pose.bone_count < 2) return 28;
    if (g_draw_report.kept_draw_count < 1) return 29;
    return 0;
}
