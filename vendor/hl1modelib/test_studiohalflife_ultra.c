#include "hlfp_softmdl_ultra.h"

#include <stdio.h>
#include <string.h>

#define TEST_BUF_CAP 196608
#define TEXT_BUF_CAP 65536
#define FB_W 128
#define FB_H 128
#define TGA_BUF_CAP (18 + FB_W * FB_H * 4)

static int ok(int rc, const hlfp_error *err, const char *what) {
    if (rc != HLFP_OK) {
        printf("FAIL studiohalflife_ultra %s: code=%d msg=%s\n", what, err != 0 ? err->code : rc, err != 0 ? err->message : "");
        return 0;
    }
    return 1;
}

static int has_text(const hlfp_u8 *buf, size_t n, const char *needle) {
    size_t i;
    size_t m;
    m = strlen(needle);
    if (m == 0U || n < m) return 0;
    for (i = 0U; i + m <= n; ++i) if (memcmp(buf + i, needle, m) == 0) return 1;
    return 0;
}

static void add_manual_debug_data(hlfp_mdl *mdl) {
    if (mdl == 0) return;
    mdl->num_hitboxes = 1;
    mdl->hitboxes[0].bone = 1;
    mdl->hitboxes[0].group = 4;
    mdl->hitboxes[0].bbmin.x = -HLFP_FIXED_HALF;
    mdl->hitboxes[0].bbmin.y = -HLFP_FIXED_HALF;
    mdl->hitboxes[0].bbmin.z = -HLFP_FIXED_HALF;
    mdl->hitboxes[0].bbmax.x = HLFP_FIXED_HALF;
    mdl->hitboxes[0].bbmax.y = HLFP_FIXED_HALF;
    mdl->hitboxes[0].bbmax.z = HLFP_FIXED_HALF;
    mdl->num_attachments = 1;
    strcpy(mdl->attachments[0].name, "muzzle");
    mdl->attachments[0].bone = 2;
    mdl->attachments[0].org.x = 0;
    mdl->attachments[0].org.y = 0;
    mdl->attachments[0].org.z = 0;
    mdl->attachments[0].vectors[0].x = HLFP_FIXED_ONE;
    mdl->attachments[0].vectors[0].y = 0;
    mdl->attachments[0].vectors[0].z = 0;
    mdl->attachments[0].vectors[1].x = 0;
    mdl->attachments[0].vectors[1].y = HLFP_FIXED_ONE;
    mdl->attachments[0].vectors[1].z = 0;
    mdl->attachments[0].vectors[2].x = 0;
    mdl->attachments[0].vectors[2].y = 0;
    mdl->attachments[0].vectors[2].z = HLFP_FIXED_ONE;
    mdl->num_events = 1;
    mdl->events[0].frame = 0;
    mdl->events[0].event = 5004;
    mdl->events[0].type = 0;
    strcpy(mdl->events[0].options, "step");
    if (mdl->num_seqdesc > 0) {
        mdl->seqdesc[0].linearmovement.x = 2 * HLFP_FIXED_ONE;
        mdl->seqdesc[0].linearmovement.y = 0;
        mdl->seqdesc[0].linearmovement.z = 0;
        mdl->seqdesc[0].motionbone = 0;
    }
}

static void add_manual_extensions(hlfp_shl_extensions *ext) {
    hlfp_shl_autolayer al;
    if (ext == 0) return;
    memset(&al, 0, sizeof(al));
    al.parent_sequence = 0;
    al.sequence = 0;
    al.pose = 0;
    al.flags = HLFP_SHL_STUDIO_AL_SPLINE;
    al.start = 0;
    al.peak = HLFP_FIXED_ONE / 4;
    al.tail = HLFP_FIXED_ONE / 2;
    al.end = HLFP_FIXED_ONE;
    ext->num_autolayers = 1;
    ext->autolayers[0] = al;
    ext->num_ikchains = 1;
    strcpy(ext->ikchains[0].name, "leg_chain");
    ext->ikchains[0].linktype = HLFP_SHL_IK_WORLD;
    ext->ikchains[0].numlinks = 3;
    ext->ikchains[0].first_link = 0;
    ext->num_iklinks = 3;
    ext->iklinks[0].bone = 0;
    ext->iklinks[1].bone = 1;
    ext->iklinks[2].bone = 2;
    ext->iklinks[0].knee_dir.y = HLFP_FIXED_ONE;
    ext->iklinks[1].knee_dir.y = HLFP_FIXED_ONE;
    ext->iklinks[2].knee_dir.y = HLFP_FIXED_ONE;
    ext->num_ikrules = 1;
    ext->ikrules[0].index = 0;
    ext->ikrules[0].type = HLFP_SHL_IK_WORLD;
    ext->ikrules[0].chain = 0;
    ext->ikrules[0].bone = 2;
    ext->ikrules[0].pos.x = 1 * HLFP_FIXED_ONE;
    ext->ikrules[0].pos.y = 3 * HLFP_FIXED_ONE;
    ext->ikrules[0].pos.z = 0;
    ext->ikrules[0].start = 0;
    ext->ikrules[0].peak = HLFP_FIXED_ONE / 4;
    ext->ikrules[0].tail = HLFP_FIXED_ONE;
    ext->ikrules[0].end = HLFP_FIXED_ONE;
}

int main(void) {
    static hlfp_u8 mdl_bytes[TEST_BUF_CAP];
    static hlfp_u8 qc_text[TEXT_BUF_CAP];
    static hlfp_u8 manifest_text[TEXT_BUF_CAP];
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
    hlfp_shlu_body_map body;
    hlfp_shlu_skin_map skin;
    hlfp_shlu_anim_driver driver;
    hlfp_shlu_layer_stack stack;
    hlfp_shlu_ik_eval ik_eval;
    hlfp_shlu_attachment_world at[2];
    hlfp_shlu_deep_audit audit;
    hlfp_soft_framebuffer fb;
    hlfp_soft_camera cam;
    hlfp_softu_options uopt;
    hlfp_mem_writer w;
    hlfp_error err;
    size_t mdl_size;
    int at_count;
    int rc;

    hlfp_error_clear(&err);
    hlfp_smd_init(&ref);
    hlfp_smd_init(&seq);
    rc = hlfp_smd_parse_buffer(reference_text, strlen(reference_text), &ref, &err);
    if (!ok(rc, &err, "parse ref")) return 1;
    rc = hlfp_smd_parse_buffer(seq_text, strlen(seq_text), &seq, &err);
    if (!ok(rc, &err, "parse seq")) return 1;

    memset(&seqs, 0, sizeof(seqs));
    memset(&opts, 0, sizeof(opts));
    strcpy(opts.name, "studiohalflife_ultra.mdl");
    opts.create_dummy_textures = HLFP_TRUE;
    opts.min.x = -HLFP_FIXED_ONE;
    opts.min.y = -HLFP_FIXED_ONE;
    opts.min.z = -HLFP_FIXED_ONE;
    opts.max.x = HLFP_FIXED_ONE;
    opts.max.y = HLFP_FIXED_ONE;
    opts.max.z = HLFP_FIXED_ONE;
    opts.bbmin.x = -2 * HLFP_FIXED_ONE;
    opts.bbmin.y = -2 * HLFP_FIXED_ONE;
    opts.bbmin.z = -2 * HLFP_FIXED_ONE;
    opts.bbmax.x = 2 * HLFP_FIXED_ONE;
    opts.bbmax.y = 2 * HLFP_FIXED_ONE;
    opts.bbmax.z = 2 * HLFP_FIXED_ONE;
    strcpy(seqs[0].label, "walk");
    seqs[0].fps = 30;
    seqs[0].flags = HLFP_STUDIO_LOOPING;
    seqs[0].animation = &seq;
    rc = hlfp_mdl_compile_from_smd(&ref, seqs, 1, 0, 0, &opts, mdl_bytes, sizeof(mdl_bytes), &mdl_size, &err);
    if (!ok(rc, &err, "compile")) return 1;
    rc = hlfp_mdl_parse_main(mdl_bytes, mdl_size, &mdl, &err);
    if (!ok(rc, &err, "parse mdl")) return 1;
    add_manual_debug_data(&mdl);

    rc = hlfp_shl_parse_extensions(&mdl, &ext, &err);
    if (!ok(rc, &err, "parse ext")) return 1;
    add_manual_extensions(&ext);

    rc = hlfp_shlu_resolve_body_map(&mdl, 0, &body, &err);
    if (!ok(rc, &err, "body map")) return 1;
    if (body.count <= 0 || body.choices[0].model_index < 0) return 1;
    rc = hlfp_shlu_resolve_skin_map(&mdl, 0, 0, &skin, &err);
    if (!ok(rc, &err, "skin map")) return 1;
    if (skin.texture_index < 0) return 1;

    hlfp_shlu_anim_driver_init(&driver);
    driver.sequence_index = 0;
    driver.blend_index = 0;
    driver.frame_position = HLFP_FIXED_HALF;
    driver.cycle = HLFP_FIXED_ONE / 3;
    driver.apply_autolayers = HLFP_TRUE;
    driver.apply_ikrules = HLFP_TRUE;
    driver.apply_root_motion = HLFP_TRUE;
    rc = hlfp_shlu_build_layer_stack(&mdl, &ext, &driver, &stack, &err);
    if (!ok(rc, &err, "layer stack")) return 1;
    if (stack.count < 2) return 1;
    rc = hlfp_shlu_decode_world_driver(&mdl, &ext, &driver, scratch.mats, HLFP_MDL_MAX_BONES, &ik_eval, &err);
    if (!ok(rc, &err, "decode world driver")) return 1;
    if (ik_eval.active_rules < 1 || ik_eval.applied_chains < 1) return 1;

    rc = hlfp_shlu_build_attachment_worlds(&mdl, scratch.mats, HLFP_MDL_MAX_BONES, at, 2, &at_count, &err);
    if (!ok(rc, &err, "attachment worlds")) return 1;
    if (at_count != 1 || at[0].bone != 2) return 1;

    rc = hlfp_shlu_deep_audit_model(&mdl, &ext, &audit, &err);
    if (!ok(rc, &err, "deep audit")) return 1;

    hlfp_mem_writer_init(&w, qc_text, sizeof(qc_text));
    rc = hlfp_shlu_write_qc_decompile_ultra(&mdl, &ext, "studiohalflife_ultra.mdl", hlfp_mem_writer_write, &w, &err);
    if (!ok(rc, &err, "qc ultra")) return 1;
    if (!has_text(qc_text, w.size, "StudioHalfLife Ultra supplemental QC") || !has_text(qc_text, w.size, "$attachment") || !has_text(qc_text, w.size, "ikchain")) return 1;

    hlfp_mem_writer_init(&w, manifest_text, sizeof(manifest_text));
    rc = hlfp_shlu_write_companion_manifest(&mdl, &ext, "studiohalflife_ultra", HLFP_FALSE, hlfp_mem_writer_write, &w, &err);
    if (!ok(rc, &err, "manifest")) return 1;
    if (!has_text(manifest_text, w.size, "companion manifest") || !has_text(manifest_text, w.size, "texture_companion")) return 1;

    hlfp_mem_writer_init(&w, audit_text, sizeof(audit_text));
    rc = hlfp_shlu_write_deep_audit_report(&mdl, &ext, &audit, hlfp_mem_writer_write, &w, &err);
    if (!ok(rc, &err, "audit report")) return 1;
    if (!has_text(audit_text, w.size, "StudioHalfLife Ultra deep audit")) return 1;

    fb.width = FB_W;
    fb.height = FB_H;
    fb.stride_bytes = FB_W * 4;
    fb.rgba = fb_rgba;
    fb.zbuffer = fb_z;
    hlfp_soft_camera_init(&cam, FB_W, FB_H);
    cam.scale = (hlfp_fixed)(24 << HLFP_FIXED_FRAC_BITS);
    hlfp_softu_options_init(&uopt);
    uopt.model.wire_overlay = HLFP_TRUE;
    uopt.model.skeleton_overlay = HLFP_TRUE;
    rc = hlfp_softu_render_debug_sheet(&mdl, &ext, &driver, &fb, &cam, &uopt, &scratch, &ik_eval, &err);
    if (!ok(rc, &err, "debug sheet")) return 1;
    hlfp_mem_writer_init(&w, tga_bytes, sizeof(tga_bytes));
    rc = hlfp_soft2_write_tga32(&fb, hlfp_mem_writer_write, &w, &err);
    if (!ok(rc, &err, "write tga")) return 1;
    if (w.size != TGA_BUF_CAP || tga_bytes[2] != 2 || tga_bytes[16] != 32) return 1;

    printf("studiohalflife_ultra_ok=1 mdl_size=%lu body=%d skin_tex=%d layers=%d ik_rules=%d attachments=%d audit_warn=%d qc=%lu manifest=%lu audit=%lu tga=%lu\n",
           (unsigned long)mdl_size, body.count, skin.texture_index, stack.count, ik_eval.active_rules, at_count, audit.warnings,
           (unsigned long)strlen((const char *)qc_text), (unsigned long)strlen((const char *)manifest_text), (unsigned long)strlen((const char *)audit_text), (unsigned long)w.size);
    printf("studiohalflife_ultra_checks=body_skin_maps layer_stack autolayer_pose_driver root_motion ikrule_chain attachment_worlds deep_audit qc_ultra companion_manifest debug_sheet_hitboxes_attachments\n");
    return 0;
}
