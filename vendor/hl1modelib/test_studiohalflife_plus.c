#include "hlfp_softmdl_plus.h"

#include <stdio.h>
#include <string.h>

#define TEST_BUF_CAP 196608
#define TEXT_BUF_CAP 65536
#define FB_W 96
#define FB_H 96
#define TGA_BUF_CAP (18 + FB_W * FB_H * 4)

static int ok(int rc, const hlfp_error *err, const char *what) {
    if (rc != HLFP_OK) {
        printf("FAIL studiohalflife_plus %s: code=%d msg=%s\n", what, err != 0 ? err->code : rc, err != 0 ? err->message : "");
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
    static hlfp_u8 qc_text[TEXT_BUF_CAP];
    static hlfp_u8 audit_text[TEXT_BUF_CAP];
    static hlfp_u8 fb_rgba[FB_W * FB_H * 4];
    static hlfp_i32 fb_z[FB_W * FB_H];
    static hlfp_u8 tga_bytes[TGA_BUF_CAP];
    static hlfp_soft_scratch scratch;

    const char *reference_text =
        "version 1\n"
        "nodes\n"
        "0 \"root\" -1\n"
        "1 \"knee\" 0\n"
        "2 \"foot\" 1\n"
        "end\n"
        "skeleton\n"
        "time 0\n"
        "0 0 0 0 0 0 0\n"
        "1 0 1 0 0 0 0\n"
        "2 0 1 0 0 0 0\n"
        "end\n"
        "triangles\n"
        "mat\n"
        "0 -1 -1 0 0 0 1 0 0\n"
        "1 1 -1 0 0 0 1 1 0\n"
        "2 0 1 0 0 0 1 0 1\n"
        "end\n";
    const char *seq_text =
        "version 1\n"
        "nodes\n"
        "0 \"root\" -1\n"
        "1 \"knee\" 0\n"
        "2 \"foot\" 1\n"
        "end\n"
        "skeleton\n"
        "time 0\n"
        "0 0 0 0 0 0 0\n"
        "1 0 1 0 0 0 0\n"
        "2 0 1 0 0 0 0\n"
        "time 1\n"
        "0 1 0 0 0 0 0\n"
        "1 0 1 0 0 0 0\n"
        "2 0 1 0 0 0 0\n"
        "end\n";
    hlfp_smd_model ref;
    hlfp_smd_model seq;
    hlfp_mdl_compile_sequence seqs[1];
    hlfp_mdl_compile_options opts;
    hlfp_mdl mdl;
    hlfp_shl_extensions ext;
    hlfp_shlp_audit audit;
    hlfp_shlp_model_extra mx;
    hlfp_shl_boneweight weights[2];
    hlfp_shl_autolayer al;
    hlfp_shlp_root_motion motion;
    hlfp_mdl_pose_set pose;
    hlfp_mat3x4 mats[HLFP_MDL_MAX_BONES];
    hlfp_shlp_hitbox_world hba[4];
    hlfp_shlp_lod_rule lods[2];
    hlfp_vec3 target;
    hlfp_soft_framebuffer fb;
    hlfp_soft_camera cam;
    hlfp_soft_options clearopt;
    hlfp_soft2_options ropt;
    hlfp_mem_writer w;
    hlfp_error err;
    hlfp_u8 raw_weights[16];
    size_t mdl_size;
    int out_count;
    int body_value;
    int skin_family;
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
    strcpy(opts.name, "studiohalflife_plus.mdl");
    opts.create_dummy_textures = HLFP_TRUE;
    opts.min.x = -HLFP_FIXED_ONE;
    opts.min.y = -HLFP_FIXED_ONE;
    opts.min.z = -HLFP_FIXED_ONE;
    opts.max.x = HLFP_FIXED_ONE;
    opts.max.y = HLFP_FIXED_ONE;
    opts.max.z = HLFP_FIXED_ONE;
    strcpy(seqs[0].label, "walk");
    seqs[0].fps = 30;
    seqs[0].flags = HLFP_STUDIO_LOOPING;
    seqs[0].animation = &seq;
    rc = hlfp_mdl_compile_from_smd(&ref, seqs, 1, 0, 0, &opts, mdl_bytes, sizeof(mdl_bytes), &mdl_size, &err);
    if (!ok(rc, &err, "compile")) return 1;
    rc = hlfp_mdl_parse_main(mdl_bytes, mdl_size, &mdl, &err);
    if (!ok(rc, &err, "parse mdl")) return 1;

    mdl.header.flags |= (int)HLFP_SHL_STUDIO_HAS_BONEWEIGHTS;
    if (mdl.num_models > 0) {
        mdl.models[0].numgroups = (int)(mdl.main_buffer.size > 64U ? mdl.main_buffer.size - 64U : 0U);
        mdl.models[0].groupindex = (int)(mdl.main_buffer.size > 32U ? mdl.main_buffer.size - 32U : 0U);
    }
    mdl.num_hitboxes = 1;
    mdl.hitboxes[0].bone = 1;
    mdl.hitboxes[0].group = 2;
    mdl.hitboxes[0].bbmin.x = -HLFP_FIXED_HALF;
    mdl.hitboxes[0].bbmin.y = -HLFP_FIXED_HALF;
    mdl.hitboxes[0].bbmin.z = -HLFP_FIXED_HALF;
    mdl.hitboxes[0].bbmax.x = HLFP_FIXED_HALF;
    mdl.hitboxes[0].bbmax.y = HLFP_FIXED_HALF;
    mdl.hitboxes[0].bbmax.z = HLFP_FIXED_HALF;
    mdl.seqdesc[0].linearmovement.x = 4 * HLFP_FIXED_ONE;
    mdl.seqdesc[0].linearmovement.y = 0;
    mdl.seqdesc[0].linearmovement.z = 0;

    rc = hlfp_shl_parse_extensions(&mdl, &ext, &err);
    if (!ok(rc, &err, "parse ext")) return 1;
    memset(&al, 0, sizeof(al));
    al.parent_sequence = 0;
    al.sequence = 0;
    al.start = 0;
    al.peak = HLFP_FIXED_ONE / 4;
    al.tail = HLFP_FIXED_ONE / 2;
    al.end = HLFP_FIXED_ONE;
    al.flags = HLFP_SHL_STUDIO_AL_SPLINE;
    ext.num_autolayers = 1;
    ext.autolayers[0] = al;

    rc = hlfp_shlp_analyze_model_extra(&mdl, 0, &mx, &err);
    if (!ok(rc, &err, "model extra")) return 1;
    if (!mx.has_xash_boneweights) return 1;

    memset(raw_weights, 0, sizeof(raw_weights));
    raw_weights[0] = 128;
    raw_weights[1] = 64;
    raw_weights[2] = 0;
    raw_weights[3] = 0;
    raw_weights[4] = 0;
    raw_weights[5] = 1;
    raw_weights[6] = 255;
    raw_weights[7] = 255;
    raw_weights[8] = 255;
    raw_weights[9] = 0;
    raw_weights[10] = 0;
    raw_weights[11] = 0;
    raw_weights[12] = 2;
    raw_weights[13] = 255;
    raw_weights[14] = 255;
    raw_weights[15] = 255;
    rc = hlfp_shlp_read_xash_weight_table(raw_weights, sizeof(raw_weights), 2, weights, 2, &err);
    if (!ok(rc, &err, "read weights")) return 1;
    if (weights[0].count < 2 || weights[0].weight[0] + weights[0].weight[1] < HLFP_FIXED_ONE - 4) return 1;

    rc = hlfp_shlp_extract_root_motion(&mdl, 0, &motion, &err);
    if (!ok(rc, &err, "root motion")) return 1;
    if (motion.linearmovement.x != 4 * HLFP_FIXED_ONE) return 1;

    rc = hlfp_goldsrc_make_bind_pose(&mdl, &pose, &err);
    if (!ok(rc, &err, "bind pose")) return 1;
    rc = hlfp_mdl_pose_to_world_matrices(&mdl, &pose, mats, HLFP_MDL_MAX_BONES, &err);
    if (!ok(rc, &err, "world mats")) return 1;
    rc = hlfp_shlp_build_hitbox_world_aabbs(&mdl, mats, HLFP_MDL_MAX_BONES, hba, 4, &out_count, &err);
    if (!ok(rc, &err, "hitbox aabb")) return 1;
    if (out_count != 1 || hba[0].bone != 1) return 1;

    target.x = 2 * HLFP_FIXED_ONE;
    target.y = 2 * HLFP_FIXED_ONE;
    target.z = 0;
    rc = hlfp_shlp_apply_two_bone_ik_lite(0, 1, 2, &target, HLFP_FIXED_HALF, mats, HLFP_MDL_MAX_BONES, &err);
    if (!ok(rc, &err, "two bone IK lite")) return 1;

    lods[0].max_distance = 8 * HLFP_FIXED_ONE;
    lods[0].body_value = 0;
    lods[0].skin_family = 0;
    lods[1].max_distance = 64 * HLFP_FIXED_ONE;
    lods[1].body_value = 1;
    lods[1].skin_family = 0;
    rc = hlfp_shlp_select_lod(lods, 2, 16 * HLFP_FIXED_ONE, &body_value, &skin_family, &err);
    if (!ok(rc, &err, "lod select")) return 1;
    if (body_value != 1 || skin_family != 0) return 1;

    rc = hlfp_shlp_audit_model(&mdl, &ext, &audit, &err);
    if (!ok(rc, &err, "audit")) return 1;
    hlfp_mem_writer_init(&w, qc_text, sizeof(qc_text));
    rc = hlfp_shlp_write_qc_decompile(&mdl, &ext, "studiohalflife_plus.mdl", hlfp_mem_writer_write, &w, &err);
    if (!ok(rc, &err, "qc plus")) return 1;
    if (!has_text(qc_text, w.size, "$boneweights") || !has_text(qc_text, w.size, "$hbox") || !has_text(qc_text, w.size, "$sequence")) return 1;

    hlfp_mem_writer_init(&w, audit_text, sizeof(audit_text));
    rc = hlfp_shlp_write_audit_report(&mdl, &ext, &audit, hlfp_mem_writer_write, &w, &err);
    if (!ok(rc, &err, "audit report")) return 1;
    if (!has_text(audit_text, w.size, "StudioHalfLife Plus audit")) return 1;

    fb.width = FB_W;
    fb.height = FB_H;
    fb.stride_bytes = FB_W * 4;
    fb.rgba = fb_rgba;
    fb.zbuffer = fb_z;
    hlfp_soft_camera_init(&cam, FB_W, FB_H);
    cam.scale = (hlfp_fixed)(26 << HLFP_FIXED_FRAC_BITS);
    hlfp_soft_options_init(&clearopt);
    clearopt.clear_rgba[0] = 64;
    clearopt.clear_rgba[1] = 64;
    clearopt.clear_rgba[2] = 72;
    clearopt.clear_rgba[3] = 255;
    rc = hlfp_soft_clear(&fb, &clearopt, &err);
    if (!ok(rc, &err, "clear")) return 1;
    hlfp_soft2_options_init(&ropt);
    rc = hlfp_soft2_render_mdl_sequence_frame_textured(&mdl, &ext, 0, 0, 0, 0, &fb, &cam, &ropt, &scratch, &err);
    if (!ok(rc, &err, "render textured")) return 1;
    hlfp_mem_writer_init(&w, tga_bytes, sizeof(tga_bytes));
    rc = hlfp_soft2_write_tga32(&fb, hlfp_mem_writer_write, &w, &err);
    if (!ok(rc, &err, "write tga")) return 1;
    if (w.size != TGA_BUF_CAP || tga_bytes[2] != 2 || tga_bytes[16] != 32) return 1;

    printf("studiohalflife_plus_ok=1 mdl_size=%lu xash_weights=%d hitbox_aabbs=%d lod_body=%d qc=%lu audit=%lu tga=%lu\n",
           (unsigned long)mdl_size, mx.has_xash_boneweights, out_count, body_value,
           (unsigned long)strlen((const char *)qc_text), (unsigned long)strlen((const char *)audit_text), (unsigned long)w.size);
    printf("studiohalflife_plus_checks=xash_model_offsets normalized_weights root_motion hitbox_world_aabb two_bone_ik_lite lod_select qc_decompile_plus audit_report affine_textured_tga\n");
    return 0;
}
