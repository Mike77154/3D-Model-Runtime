#include "gif_ms3d_codec.h"

#include <stdio.h>

#ifndef MS3D_INFO_FILE_MAX
#define MS3D_INFO_FILE_MAX (16UL * 1024UL * 1024UL)
#endif

static unsigned char g_file_data[MS3D_INFO_FILE_MAX];
static ms3d_model g_model;

static void print_fx(ms3d_fx value)
{
    char buf[32];
    if (ms3d_fx_to_ascii(value, buf, sizeof(buf), 3U) == MS3D_OK) {
        fputs(buf, stdout);
    } else {
        fputs("?", stdout);
    }
}

static int read_file(const char *path, unsigned char *dst, size_t cap, size_t *out_size)
{
    FILE *fp;
    size_t total;
    size_t got;

    fp = fopen(path, "rb");
    if (fp == (FILE *)0) {
        return 0;
    }
    total = 0U;
    while (total < cap) {
        got = fread(dst + total, 1U, cap - total, fp);
        total += got;
        if (got == 0U) {
            break;
        }
    }
    if (!feof(fp)) {
        fclose(fp);
        return 0;
    }
    fclose(fp);
    *out_size = total;
    return 1;
}

int main(int argc, char **argv)
{
    size_t file_size;
    int rc;
    ms3d_u16 i;

    if (argc != 2) {
        fputs("usage: ms3d_info model.ms3d\n", stderr);
        return 2;
    }

    if (!read_file(argv[1], g_file_data, sizeof(g_file_data), &file_size)) {
        fputs("could not read input or input exceeds static buffer\n", stderr);
        return 2;
    }

    rc = ms3d_parse(&g_model, g_file_data, file_size);
    if (rc != MS3D_OK) {
        fprintf(stderr, "ms3d_parse: %s (%d)\n", ms3d_error_string(rc), rc);
        return 1;
    }

    printf("MS3D version: %ld\n", g_model.version);
    printf("vertices: %u\n", (unsigned)g_model.num_vertices);
    printf("triangles: %u\n", (unsigned)g_model.num_triangles);
    printf("groups: %u\n", (unsigned)g_model.num_groups);
    printf("materials: %u\n", (unsigned)g_model.num_materials);
    printf("joints: %u\n", (unsigned)g_model.num_joints);
    printf("fps: ");
    print_fx(g_model.animation_fps);
    printf(" total_frames: %ld\n", g_model.total_frames);

    for (i = 0U; i < g_model.num_vertices && i < 8U; ++i) {
        printf("v%u = ", (unsigned)i);
        print_fx(g_model.vertices[i].position[0]);
        fputc(',', stdout);
        print_fx(g_model.vertices[i].position[1]);
        fputc(',', stdout);
        print_fx(g_model.vertices[i].position[2]);
        printf(" bone=%d\n", g_model.vertices[i].bone_id);
    }

    for (i = 0U; i < g_model.num_groups && i < 8U; ++i) {
        printf("group%u name='%s' tris=%u material=%d\n",
               (unsigned)i,
               g_model.groups[i].name,
               (unsigned)g_model.groups[i].triangle_count,
               g_model.groups[i].material_index);
    }

    return 0;
}
