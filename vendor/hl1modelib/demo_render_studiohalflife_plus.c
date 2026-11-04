#include "hlfp_softmdl_plus.h"

#include <stdio.h>
#include <string.h>

#define DEMO_BUF_CAP 196608
#define DEMO_W 160
#define DEMO_H 160

static int file_write(void *user, const void *data, size_t size) {
    FILE *f;
    f = (FILE *)user;
    return fwrite(data, 1U, size, f) == size;
}

static int die_if(int rc, hlfp_error *err, const char *what) {
    if (rc != HLFP_OK) {
        fprintf(stderr, "plus demo failed at %s: %s\n", what, err != 0 ? err->message : "unknown");
        return 1;
    }
    return 0;
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
    hlfp_soft_framebuffer fb;
    hlfp_soft_camera cam;
    hlfp_soft_options clearopt;
    hlfp_soft2_options ropt;
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
    strcpy(opts.name, "studiohalflife_plus_preview.mdl");
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
    rc = hlfp_shl_parse_extensions(&mdl, &ext, &err);
    if (die_if(rc, &err, "parse extensions")) return 1;

    fb.width = DEMO_W;
    fb.height = DEMO_H;
    fb.stride_bytes = DEMO_W * 4;
    fb.rgba = rgba;
    fb.zbuffer = zbuf;
    hlfp_soft_camera_init(&cam, DEMO_W, DEMO_H);
    cam.scale = (hlfp_fixed)(36 << HLFP_FIXED_FRAC_BITS);
    cam.origin.y = 0;
    hlfp_soft_options_init(&clearopt);
    clearopt.clear_rgba[0] = 40;
    clearopt.clear_rgba[1] = 46;
    clearopt.clear_rgba[2] = 60;
    clearopt.clear_rgba[3] = 255;
    rc = hlfp_soft_clear(&fb, &clearopt, &err);
    if (die_if(rc, &err, "clear")) return 1;
    hlfp_soft2_options_init(&ropt);
    ropt.wire_overlay = HLFP_TRUE;
    ropt.skeleton_overlay = HLFP_TRUE;
    rc = hlfp_soft2_render_mdl_sequence_frame_textured(&mdl, &ext, 0, 0, 0, 0, &fb, &cam, &ropt, &scratch, &err);
    if (die_if(rc, &err, "render plus")) return 1;

    f = fopen("studiohalflife_plus_preview.tga", "wb");
    if (f == 0) {
        fprintf(stderr, "could not open studiohalflife_plus_preview.tga\n");
        return 1;
    }
    rc = hlfp_soft2_write_tga32(&fb, file_write, f, &err);
    fclose(f);
    if (die_if(rc, &err, "write tga")) return 1;
    printf("wrote studiohalflife_plus_preview.tga\n");
    return 0;
}
