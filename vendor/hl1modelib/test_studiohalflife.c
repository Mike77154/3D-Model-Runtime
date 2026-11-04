#include "hlfp_softmdl.h"

#include <stdio.h>
#include <string.h>

#define TEST_BUF_CAP 131072
#define TEXT_BUF_CAP 65536
#define FB_W 64
#define FB_H 64

static int ok(int rc, const hlfp_error *err, const char *what) {
    if (rc != HLFP_OK) {
        printf("FAIL studiohalflife %s: code=%d msg=%s\n", what, err != 0 ? err->code : rc, err != 0 ? err->message : "");
        return 0;
    }
    return 1;
}

static int has_text(const hlfp_u8 *buf, size_t n, const char *needle) {
    size_t i;
    size_t m;
    m = strlen(needle);
    if (m == 0U || n < m) return 0;
    for (i = 0U; i + m <= n; ++i) {
        if (memcmp(buf + i, needle, m) == 0) return 1;
    }
    return 0;
}

int main(void) {
    static hlfp_u8 mdl_bytes[TEST_BUF_CAP];
    static hlfp_u8 report_text[TEXT_BUF_CAP];
    static hlfp_u8 ppm_text[TEXT_BUF_CAP];
    static hlfp_u8 fb_rgba[FB_W * FB_H * 4];
    static hlfp_i32 fb_z[FB_W * FB_H];
    static hlfp_soft_scratch scratch;

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
    const char *seq_text =
        "version 1\n"
        "nodes\n"
        "0 \"root\" -1\n"
        "end\n"
        "skeleton\n"
        "time 0\n"
        "0 0 0 0 0 0 0\n"
        "end\n";
    hlfp_smd_model ref;
    hlfp_smd_model seq;
    hlfp_mdl_compile_sequence seqs[1];
    hlfp_mdl_compile_options opts;
    hlfp_mdl mdl;
    hlfp_shl_extensions ext;
    hlfp_shl_companion_info ci;
    hlfp_shl_autolayer al;
    hlfp_shl_ik_target target;
    hlfp_shl_procedural_state proc;
    hlfp_soft_framebuffer fb;
    hlfp_soft_camera cam;
    hlfp_soft_options ropt;
    hlfp_mdl_mesh_expanded mesh;
    hlfp_mdl_pose_set pose;
    hlfp_mat3x4 mats[HLFP_MDL_MAX_BONES];
    hlfp_mem_writer w;
    hlfp_error err;
    size_t mdl_size;
    hlfp_fixed layer_weight;
    int model_index;
    int rc;

    hlfp_error_clear(&err);
    hlfp_smd_init(&ref);
    hlfp_smd_init(&seq);
    rc = hlfp_smd_parse_buffer(reference_text, strlen(reference_text), &ref, &err);
    if (!ok(rc, &err, "parse ref")) return 1;
    rc = hlfp_smd_parse_buffer(seq_text, strlen(seq_text), &seq, &err);
    if (!ok(rc, &err, "parse seq")) return 1;

    memset(&opts, 0, sizeof(opts));
    memset(&seqs, 0, sizeof(seqs));
    strcpy(opts.name, "studiohalflife.mdl");
    opts.create_dummy_textures = HLFP_TRUE;
    strcpy(seqs[0].label, "idle");
    seqs[0].fps = 30;
    seqs[0].flags = HLFP_STUDIO_LOOPING;
    seqs[0].animation = &seq;
    rc = hlfp_mdl_compile_from_smd(&ref, seqs, 1, 0, 0, &opts, mdl_bytes, sizeof(mdl_bytes), &mdl_size, &err);
    if (!ok(rc, &err, "compile")) return 1;
    rc = hlfp_mdl_parse_main(mdl_bytes, mdl_size, &mdl, &err);
    if (!ok(rc, &err, "parse mdl")) return 1;

    rc = hlfp_shl_parse_extensions(&mdl, &ext, &err);
    if (!ok(rc, &err, "parse extensions")) return 1;
    hlfp_mem_writer_init(&w, report_text, sizeof(report_text));
    rc = hlfp_shl_write_report(&mdl, &ext, hlfp_mem_writer_write, &w, &err);
    if (!ok(rc, &err, "extension report")) return 1;
    if (!has_text(report_text, w.size, "StudioHalfLife")) return 1;

    rc = hlfp_shl_read_companion_header(mdl_bytes, mdl_size, "models/player/barney/barneyT.dol", &ci, &err);
    if (!ok(rc, &err, "DOL classify header")) return 1;
    if (ci.kind != HLFP_SHL_COMPANION_TEXTURE_DOL || !ci.is_dol) return 1;

    memset(&al, 0, sizeof(al));
    al.parent_sequence = 0;
    al.sequence = 0;
    al.start = 0;
    al.peak = HLFP_FIXED_ONE / 4;
    al.tail = HLFP_FIXED_ONE / 2;
    al.end = HLFP_FIXED_ONE;
    al.flags = HLFP_SHL_STUDIO_AL_SPLINE;
    rc = hlfp_shl_autolayer_weight(&al, HLFP_FIXED_ONE / 4, 0, &layer_weight, &err);
    if (!ok(rc, &err, "autolayer weight")) return 1;
    if (layer_weight <= 0) return 1;

    rc = hlfp_goldsrc_model_for_bodypart(&mdl, 0, 0, &model_index, &err);
    if (!ok(rc, &err, "body model")) return 1;
    rc = hlfp_goldsrc_make_bind_pose(&mdl, &pose, &err);
    if (!ok(rc, &err, "bind pose")) return 1;
    rc = hlfp_mdl_pose_to_world_matrices(&mdl, &pose, mats, HLFP_MDL_MAX_BONES, &err);
    if (!ok(rc, &err, "bind mats")) return 1;
    rc = hlfp_shl_bake_mesh_weighted(&mdl, model_index, mdl.models[model_index].first_mesh, 0, mats, HLFP_MDL_MAX_BONES, &mesh, &err);
    if (!ok(rc, &err, "weighted fallback bake")) return 1;

    target.bone = 0;
    target.target.x = HLFP_FIXED_ONE;
    target.target.y = HLFP_FIXED_ONE;
    target.target.z = 0;
    target.weight = HLFP_FIXED_HALF;
    rc = hlfp_shl_apply_ik_targets(&mdl, &target, 1, mats, HLFP_MDL_MAX_BONES, &err);
    if (!ok(rc, &err, "IK target")) return 1;

    hlfp_shl_procedural_state_init(&proc);
    proc.enable_billboard = HLFP_TRUE;
    proc.enable_jiggle_lite = HLFP_TRUE;
    proc.jiggle_amount = HLFP_FIXED_ONE / 8;
    mdl.bones[0].flags |= HLFP_SHL_BONE_SCREEN_ALIGN_SPHERE | HLFP_SHL_BONE_JIGGLE_PROCEDURAL;
    rc = hlfp_shl_apply_procedural_bones(&mdl, &ext, &proc, mats, HLFP_MDL_MAX_BONES, &err);
    if (!ok(rc, &err, "procedural bones")) return 1;

    fb.width = FB_W;
    fb.height = FB_H;
    fb.stride_bytes = FB_W * 4;
    fb.rgba = fb_rgba;
    fb.zbuffer = fb_z;
    hlfp_soft_camera_init(&cam, FB_W, FB_H);
    hlfp_soft_options_init(&ropt);
    ropt.use_weighted_bones = HLFP_TRUE;
    rc = hlfp_soft_clear(&fb, &ropt, &err);
    if (!ok(rc, &err, "soft clear")) return 1;
    rc = hlfp_soft_render_mdl_sequence_frame(&mdl, &ext, 0, 0, 0, &proc, &fb, &cam, &ropt, &scratch, &err);
    if (!ok(rc, &err, "soft render sequence")) return 1;
    hlfp_mem_writer_init(&w, ppm_text, sizeof(ppm_text));
    rc = hlfp_soft_write_ppm(&fb, hlfp_mem_writer_write, &w, &err);
    if (!ok(rc, &err, "ppm write")) return 1;
    if (!has_text(ppm_text, w.size, "P6\n64 64\n255\n")) return 1;

    printf("studiohalflife_extensions_ok=1 mdl_size=%lu studiohdr2=%d boneweights=%d autolayer_w=%d mesh_v=%d mesh_t=%d ppm=%lu\n",
           (unsigned long)mdl_size, ext.has_studiohdr2, ext.has_boneweights, layer_weight, mesh.num_vertices, mesh.num_triangles, (unsigned long)w.size);
    printf("studiohalflife_checks=studiohdr2_scan dol_companion procedural_bones weighted_bake ik_targets autolayers soft_renderer_ppm\n");
    return 0;
}
