#include "hlfp_softmdl_ultra.h"

#include <stdio.h>
#include <string.h>

#define DEMO_BUF_CAP 196608
#define DEMO_W 192
#define DEMO_H 192

static int file_write(void *user, const void *data, size_t size) {
    FILE *f;
    f = (FILE *)user;
    return fwrite(data, 1U, size, f) == size;
}

static int die_if(int rc, hlfp_error *err, const char *what) {
    if (rc != HLFP_OK) {
        fprintf(stderr, "ultra demo failed at %s: %s\n", what, err != 0 ? err->message : "unknown");
        return 1;
    }
    return 0;
}

static void add_debug_data(hlfp_mdl *mdl) {
    if (mdl == 0) return;
    mdl->num_hitboxes = 1;
    mdl->hitboxes[0].bone = 1;
    mdl->hitboxes[0].group = 1;
    mdl->hitboxes[0].bbmin.x = -HLFP_FIXED_HALF;
    mdl->hitboxes[0].bbmin.y = -HLFP_FIXED_HALF;
    mdl->hitboxes[0].bbmin.z = -HLFP_FIXED_HALF;
    mdl->hitboxes[0].bbmax.x = HLFP_FIXED_HALF;
    mdl->hitboxes[0].bbmax.y = HLFP_FIXED_HALF;
    mdl->hitboxes[0].bbmax.z = HLFP_FIXED_HALF;
    mdl->num_attachments = 1;
    strcpy(mdl->attachments[0].name, "muzzle");
    mdl->attachments[0].bone = 2;
    mdl->attachments[0].vectors[0].x = HLFP_FIXED_ONE;
    mdl->attachments[0].vectors[1].y = HLFP_FIXED_ONE;
    mdl->attachments[0].vectors[2].z = HLFP_FIXED_ONE;
    if (mdl->num_seqdesc > 0) {
        mdl->seqdesc[0].linearmovement.x = 2 * HLFP_FIXED_ONE;
        mdl->seqdesc[0].motionbone = 0;
    }
}

static void add_ext(hlfp_shl_extensions *ext) {
    if (ext == 0) return;
    ext->num_autolayers = 1;
    ext->autolayers[0].parent_sequence = 0;
    ext->autolayers[0].sequence = 0;
    ext->autolayers[0].start = 0;
    ext->autolayers[0].peak = HLFP_FIXED_ONE / 4;
    ext->autolayers[0].tail = HLFP_FIXED_ONE / 2;
    ext->autolayers[0].end = HLFP_FIXED_ONE;
    ext->num_ikchains = 1;
    strcpy(ext->ikchains[0].name, "leg_chain");
    ext->ikchains[0].linktype = HLFP_SHL_IK_WORLD;
    ext->ikchains[0].numlinks = 3;
    ext->ikchains[0].first_link = 0;
    ext->num_iklinks = 3;
    ext->iklinks[0].bone = 0;
    ext->iklinks[1].bone = 1;
    ext->iklinks[2].bone = 2;
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
    static hlfp_u8 mdl_bytes[DEMO_BUF_CAP];
    static hlfp_u8 rgba[DEMO_W * DEMO_H * 4];
    static hlfp_i32 zbuf[DEMO_W * DEMO_H];
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
    hlfp_shlu_anim_driver driver;
    hlfp_shlu_ik_eval ik_eval;
    hlfp_soft_framebuffer fb;
    hlfp_soft_camera cam;
    hlfp_softu_options uopt;
    hlfp_error err;
    FILE *f;
    size_t mdl_size;
    int rc;

    hlfp_error_clear(&err);
    hlfp_smd_init(&ref);
    hlfp_smd_init(&seq);
    rc = hlfp_smd_parse_buffer(reference_text, strlen(reference_text), &ref, &err);
    if (die_if(rc, &err, "parse reference")) return 1;
    rc = hlfp_smd_parse_buffer(seq_text, strlen(seq_text), &seq, &err);
    if (die_if(rc, &err, "parse sequence")) return 1;

    memset(&seqs, 0, sizeof(seqs));
    memset(&opts, 0, sizeof(opts));
    strcpy(opts.name, "studiohalflife_ultra_preview.mdl");
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
    if (die_if(rc, &err, "compile mdl")) return 1;
    rc = hlfp_mdl_parse_main(mdl_bytes, mdl_size, &mdl, &err);
    if (die_if(rc, &err, "parse mdl")) return 1;
    add_debug_data(&mdl);
    rc = hlfp_shl_parse_extensions(&mdl, &ext, &err);
    if (die_if(rc, &err, "parse extensions")) return 1;
    add_ext(&ext);

    fb.width = DEMO_W;
    fb.height = DEMO_H;
    fb.stride_bytes = DEMO_W * 4;
    fb.rgba = rgba;
    fb.zbuffer = zbuf;
    hlfp_soft_camera_init(&cam, DEMO_W, DEMO_H);
    cam.scale = (hlfp_fixed)(30 << HLFP_FIXED_FRAC_BITS);
    hlfp_softu_options_init(&uopt);
    uopt.model.wire_overlay = HLFP_TRUE;
    uopt.model.skeleton_overlay = HLFP_TRUE;
    hlfp_shlu_anim_driver_init(&driver);
    driver.sequence_index = 0;
    driver.frame_position = HLFP_FIXED_HALF;
    driver.cycle = HLFP_FIXED_ONE / 3;
    rc = hlfp_softu_render_debug_sheet(&mdl, &ext, &driver, &fb, &cam, &uopt, &scratch, &ik_eval, &err);
    if (die_if(rc, &err, "render debug sheet")) return 1;

    f = fopen("studiohalflife_ultra_preview.tga", "wb");
    if (f == 0) {
        fprintf(stderr, "could not open studiohalflife_ultra_preview.tga\n");
        return 1;
    }
    rc = hlfp_soft2_write_tga32(&fb, file_write, f, &err);
    fclose(f);
    if (die_if(rc, &err, "write tga")) return 1;
    printf("wrote studiohalflife_ultra_preview.tga ik_active=%d chains=%d\n", ik_eval.active_rules, ik_eval.applied_chains);
    return 0;
}
