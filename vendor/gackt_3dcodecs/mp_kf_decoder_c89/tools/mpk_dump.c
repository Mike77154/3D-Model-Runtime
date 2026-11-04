#include <stdio.h>
#include <string.h>
#include "mpk_reader.h"

static void usage(void)
{
    puts("mpk_dump - Max Payne KF2/KFS/SKD fixed-point decoder");
    puts("usage:");
    puts("  mpk_dump input.kf2");
    puts("  mpk_dump input.kfs --obj output.obj");
    puts("  mpk_dump input.kfs --posed-obj output.obj");
    puts("  mpk_dump input.kf2 --camera-dump frame_number");
    puts("  mpk_dump input.skd");
}

static unsigned long parse_ul(const char *s)
{
    unsigned long v;
    v = 0UL;
    while (*s >= '0' && *s <= '9') {
        v = v * 10UL + (unsigned long)(*s - '0');
        ++s;
    }
    return v;
}

static void dump_cameras(const mpk_model *model, mpk_u32 frame)
{
    mpk_u32 i;
    for (i = 0; i < model->camera_count; ++i) {
        mpk_camera_pose pose;
        char sx[32];
        char sy[32];
        char sz[32];
        if (mpk_eval_camera_path(model, i, frame, &pose)) {
            mpk_fixed_to_string(pose.transform.m[9], sx, 32);
            mpk_fixed_to_string(pose.transform.m[10], sy, 32);
            mpk_fixed_to_string(pose.transform.m[11], sz, 32);
            printf("camera[%lu] name='%s' frame=%lu pos=%s,%s,%s animated=%d anim=%lu\n",
                (unsigned long)i,
                model->cameras[i].node.name,
                (unsigned long)frame,
                sx, sy, sz,
                pose.has_animation,
                (unsigned long)pose.source_animation);
        }
    }
}

int main(int argc, char **argv)
{
    static mpk_model model;
    const char *obj_path;
    const char *posed_obj_path;
    int do_camera_dump;
    mpk_u32 camera_frame;
    int i;

    obj_path = (const char *)0;
    posed_obj_path = (const char *)0;
    do_camera_dump = 0;
    camera_frame = 0UL;
    if (argc < 2) {
        usage();
        return 1;
    }
    for (i = 2; i < argc; ++i) {
        if (strcmp(argv[i], "--obj") == 0 && i + 1 < argc) {
            obj_path = argv[++i];
        } else if (strcmp(argv[i], "--posed-obj") == 0 && i + 1 < argc) {
            posed_obj_path = argv[++i];
        } else if (strcmp(argv[i], "--camera-dump") == 0 && i + 1 < argc) {
            do_camera_dump = 1;
            camera_frame = (mpk_u32)parse_ul(argv[++i]);
        } else {
            usage();
            return 1;
        }
    }
    if (!mpk_read_file(argv[1], &model)) {
        fprintf(stderr, "decode failed: %s\n", model.last_error);
        return 2;
    }
    mpk_build_resolved_nodes(&model);
    if (posed_obj_path != (const char *)0) {
        mpk_apply_skin_to_bind_pose(&model);
    }
    mpk_print_summary(stdout, &model);
    if (do_camera_dump) {
        dump_cameras(&model, camera_frame);
    }
    if (obj_path != (const char *)0) {
        if (!mpk_write_obj(obj_path, &model)) {
            fprintf(stderr, "OBJ write failed\n");
            return 3;
        }
        fprintf(stdout, "wrote OBJ: %s\n", obj_path);
    }
    if (posed_obj_path != (const char *)0) {
        if (!mpk_write_posed_obj(posed_obj_path, &model)) {
            fprintf(stderr, "posed OBJ write failed\n");
            return 4;
        }
        fprintf(stdout, "wrote posed OBJ: %s\n", posed_obj_path);
    }
    return 0;
}
