#include "hlfp.h"

#include <stdio.h>
#include <string.h>

#define TEST_BUF_CAP 131072
#define PI_OVER_4_Q16 ((hlfp_fixed)51472)

static hlfp_bool check(int rc, const hlfp_error *err, const char *what) {
    if (rc != HLFP_OK) {
        printf("FAIL %s: code=%d line=%d msg=%s\n", what, err != 0 ? err->code : rc, err != 0 ? err->line : 0, err != 0 ? err->message : "");
        return HLFP_FALSE;
    }
    return HLFP_TRUE;
}

static hlfp_bool check_eq_int(const char *what, int a, int b) {
    if (a != b) {
        printf("FAIL %s: %d != %d\n", what, a, b);
        return HLFP_FALSE;
    }
    return HLFP_TRUE;
}

int main(void) {
    static hlfp_mdl_binary_cache cache;
    static hlfp_u8 mdl_bytes[TEST_BUF_CAP];
    static hlfp_u8 regen_bytes[TEST_BUF_CAP];
    const char *reference_text =
        "version 1\n"
        "nodes\n"
        "0 \"root\" -1\n"
        "end\n"
        "skeleton\n"
        "time 0\n"
        "0 0 0 0 0 0 0\n"
        "end\n"
        "triangles\n"
        "mat\n"
        "0 1 0 0 0 0 1 0 0\n"
        "0 0 1 0 0 0 1 1 0\n"
        "0 0 0 0 0 0 1 0 1\n"
        "end\n";
    const char *seq0_text =
        "version 1\n"
        "nodes\n"
        "0 \"root\" -1\n"
        "end\n"
        "skeleton\n"
        "time 0\n"
        "0 0 0 0 0 0 0\n"
        "end\n";
    const char *seq1_text =
        "version 1\n"
        "nodes\n"
        "0 \"root\" -1\n"
        "end\n"
        "skeleton\n"
        "time 0\n"
        "0 0 0 0 0 0 3.141593\n"
        "end\n";
    hlfp_smd_model reference_model;
    hlfp_smd_model seq0_model;
    hlfp_smd_model seq1_model;
    hlfp_mdl_compile_sequence seqs[2];
    hlfp_mdl_compile_options opts;
    hlfp_mdl mdl;
    hlfp_mdl mdl2;
    hlfp_error err;
    size_t mdl_size;
    size_t regen_size;
    hlfp_fixed seconds[2];
    hlfp_fixed weights[2];
    int sequence_indices[2];
    hlfp_mat3x4 blend_mats[HLFP_MDL_MAX_BONES];
    hlfp_mat3x4 ctrl_mats[HLFP_MDL_MAX_BONES];
    hlfp_mdl_controller_state ctrl;
    hlfp_mdl_quat_pose_set qposes;
    hlfp_mdl_pose_set p0;
    hlfp_mdl_pose_set p1;
    hlfp_vec3 v0;
    hlfp_vec3 v1;
    int b0;
    int b1;
    int rc;

    hlfp_smd_init(&reference_model);
    hlfp_smd_init(&seq0_model);
    hlfp_smd_init(&seq1_model);
    hlfp_error_clear(&err);

    rc = hlfp_smd_parse_buffer(reference_text, strlen(reference_text), &reference_model, &err);
    if (!check(rc, &err, "parse reference")) return 1;
    rc = hlfp_smd_parse_buffer(seq0_text, strlen(seq0_text), &seq0_model, &err);
    if (!check(rc, &err, "parse seq0")) return 1;
    rc = hlfp_smd_parse_buffer(seq1_text, strlen(seq1_text), &seq1_model, &err);
    if (!check(rc, &err, "parse seq1")) return 1;

    memset(&seqs, 0, sizeof(seqs));
    memset(&opts, 0, sizeof(opts));
    strcpy(opts.name, "tiny.mdl");
    opts.create_dummy_textures = HLFP_TRUE;
    seqs[0].animation = &seq0_model;
    strcpy(seqs[0].label, "idle");
    seqs[0].fps = 1;
    seqs[0].flags = HLFP_STUDIO_LOOPING;
    seqs[1].animation = &seq1_model;
    strcpy(seqs[1].label, "turn");
    seqs[1].fps = 1;
    seqs[1].flags = HLFP_STUDIO_LOOPING;

    rc = hlfp_mdl_compile_from_smd(&reference_model, seqs, 2, 0, 0, &opts, mdl_bytes, sizeof(mdl_bytes), &mdl_size, &err);
    if (!check(rc, &err, "compile mdl")) return 1;

    rc = hlfp_mdl_parse_main_cached(mdl_bytes, mdl_size, &mdl, &cache, &err);
    if (!check(rc, &err, "parse cached mdl")) return 1;

    memset(regen_bytes, 0xCD, sizeof(regen_bytes));
    rc = hlfp_mdl_regenerate_main_binary(&mdl, regen_bytes, sizeof(regen_bytes), &regen_size, &err);
    if (!check(rc, &err, "semantic regenerate main")) return 1;

    rc = hlfp_mdl_parse_main(regen_bytes, regen_size, &mdl2, &err);
    if (!check(rc, &err, "parse semantic regen")) return 1;

    if (!check_eq_int("num_bones", mdl.num_bones, mdl2.num_bones)) return 1;
    if (!check_eq_int("num_seqdesc", mdl.num_seqdesc, mdl2.num_seqdesc)) return 1;
    if (!check_eq_int("num_models", mdl.num_models, mdl2.num_models)) return 1;
    if (!check_eq_int("num_meshes", mdl.num_meshes, mdl2.num_meshes)) return 1;
    if (!check_eq_int("num_textures", mdl.num_textures, mdl2.num_textures)) return 1;

    rc = hlfp_mdl_read_model_vertex(&mdl, 0, 0, &v0, &b0, &err);
    if (!check(rc, &err, "read orig vertex")) return 1;
    rc = hlfp_mdl_read_model_vertex(&mdl2, 0, 0, &v1, &b1, &err);
    if (!check(rc, &err, "read regen vertex")) return 1;
    if (!check_eq_int("vertex bone", b0, b1)) return 1;
    if (!check_eq_int("vertex x", v0.x, v1.x)) return 1;
    if (!check_eq_int("vertex y", v0.y, v1.y)) return 1;
    if (!check_eq_int("vertex z", v0.z, v1.z)) return 1;

    rc = hlfp_mdl_decode_sequence_frame(&mdl, 1, 0, 0, &p0, &err);
    if (!check(rc, &err, "decode orig seq")) return 1;
    rc = hlfp_mdl_decode_sequence_frame(&mdl2, 1, 0, 0, &p1, &err);
    if (!check(rc, &err, "decode regen seq")) return 1;
    if (!check_eq_int("pose rot z", p0.poses[0].rot.z, p1.poses[0].rot.z)) return 1;

    sequence_indices[0] = 0;
    sequence_indices[1] = 1;
    seconds[0] = 0;
    seconds[1] = 0;
    weights[0] = HLFP_FIXED_HALF;
    weights[1] = HLFP_FIXED_HALF;
    rc = hlfp_mdl_blend_sequence_world_matrices_times_quat_ctrl(&mdl2, sequence_indices, seconds, 0, weights, 2, blend_mats, HLFP_MDL_MAX_BONES, &err);
    if (!check(rc, &err, "quat blend world")) return 1;

    hlfp_mdl_controller_state_init(&ctrl);
    mdl2.num_bonecontrollers = 1;
    mdl2.bonecontrollers[0].index = 1;
    mdl2.bones[0].bonecontroller[5] = 0;
    ctrl.bone[0] = PI_OVER_4_Q16;
    rc = hlfp_mdl_decode_sequence_time_quat_ctrl(&mdl2, 0, 0, &ctrl, &qposes, &err);
    if (!check(rc, &err, "controller quat decode")) return 1;
    rc = hlfp_mdl_quat_pose_to_world_matrices(&mdl2, &qposes, ctrl_mats, HLFP_MDL_MAX_BONES, &err);
    if (!check(rc, &err, "controller world")) return 1;

    printf("semantic_regen_ok=1 orig_size=%lu regen_size=%lu\n", (unsigned long)mdl_size, (unsigned long)regen_size);
    printf("seqs=%d meshes=%d textures=%d\n", mdl2.num_seqdesc, mdl2.num_meshes, mdl2.num_textures);
    printf("vertex0=(%d,%d,%d) bone=%d\n", v1.x, v1.y, v1.z, b1);
    printf("pose_z=%d\n", p1.poses[0].rot.z);
    printf("quat_blend_xrot_m11=%d m21=%d m12=%d m22=%d\n", blend_mats[0].m[1][1], blend_mats[0].m[2][1], blend_mats[0].m[1][2], blend_mats[0].m[2][2]);
    printf("controller_xrot_m11=%d m21=%d m12=%d m22=%d\n", ctrl_mats[0].m[1][1], ctrl_mats[0].m[2][1], ctrl_mats[0].m[1][2], ctrl_mats[0].m[2][2]);
    printf("done\n");
    return 0;
}
