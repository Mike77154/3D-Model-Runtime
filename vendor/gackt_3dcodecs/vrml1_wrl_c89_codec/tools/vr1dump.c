#include <stdio.h>
#include <string.h>
#include "vr1_parser.h"
#include "vr1_export_obj.h"

static char g_input[VR1_MAX_INPUT_BYTES];
static vr1_scene g_scene;
static vr1_parser_context g_parser;

static long read_file_static(const char *path)
{
    FILE *f;
    long n;
    long got;

    f = fopen(path, "rb");
    if (f == 0) return -1;
    n = 0;
    while (n < VR1_MAX_INPUT_BYTES) {
        got = (long)fread(g_input + n, 1, (size_t)(VR1_MAX_INPUT_BYTES - n), f);
        if (got <= 0) break;
        n += got;
    }
    fclose(f);
    return n;
}

int main(int argc, char **argv)
{
    long len;
    int r;
    FILE *out;

    if (argc < 2) {
        fprintf(stderr, "usage: vr1dump input.wrl [out.obj] [out.mtl]\n");
        return 2;
    }
    len = read_file_static(argv[1]);
    if (len <= 0) {
        fprintf(stderr, "could not read input\n");
        return 2;
    }
    r = vr1_parse_buffer(&g_parser, &g_scene, g_input, len);
    printf("parse: %s (%d)\n", vr1_result_name(r), r);
    vr1_print_summary(stdout, &g_scene);
    if (argc >= 3) {
        out = fopen(argv[2], "wb");
        if (out == 0) {
            fprintf(stderr, "could not open obj output\n");
            return 2;
        }
        (void)vr1_export_obj(out, &g_scene, "vrml1_scene");
        fclose(out);
    }
    if (argc >= 4) {
        out = fopen(argv[3], "wb");
        if (out == 0) {
            fprintf(stderr, "could not open mtl output\n");
            return 2;
        }
        (void)vr1_export_mtl(out, &g_scene);
        fclose(out);
    }
    return (r == 0) ? 0 : 1;
}
