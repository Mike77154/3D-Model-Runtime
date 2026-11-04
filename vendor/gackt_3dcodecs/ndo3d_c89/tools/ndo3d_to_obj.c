#include <stdio.h>
#include "ndo3d.h"

#ifndef NDO3D_TOOL_MAX_FILE
#define NDO3D_TOOL_MAX_FILE 16777216UL
#endif
#ifndef NDO3D_TOOL_MAX_OBJ
#define NDO3D_TOOL_MAX_OBJ 33554432UL
#endif

static unsigned char g_file[NDO3D_TOOL_MAX_FILE];
static char g_obj[NDO3D_TOOL_MAX_OBJ];
static NDO3D_Model g_model;

static unsigned long read_file_static(const char *path)
{
    FILE *f;
    unsigned long n;
    int ch;
    f = fopen(path, "rb");
    if (f == 0) return 0UL;
    n = 0UL;
    while (n < NDO3D_TOOL_MAX_FILE) {
        ch = fgetc(f);
        if (ch == EOF) break;
        g_file[n++] = (unsigned char)ch;
    }
    fclose(f);
    return n;
}

static int write_file_static(const char *path, const char *data, unsigned long n)
{
    FILE *f;
    unsigned long i;
    f = fopen(path, "wb");
    if (f == 0) return 0;
    for (i = 0UL; i < n; i++) fputc((unsigned char)data[i], f);
    fclose(f);
    return 1;
}

int main(int argc, char **argv)
{
    NDO3D_Diag d;
    unsigned long n;
    unsigned long obj_n;
    int rc;
    if (argc < 3) {
        printf("usage: ndo3d_to_obj input.ndo output.obj\n");
        return 2;
    }
    n = read_file_static(argv[1]);
    if (n == 0UL) {
        printf("could not read input file or file is empty\n");
        return 2;
    }
    rc = ndo3d_decode(&g_model, g_file, n, &d);
    if (rc != NDO3D_OK) {
        printf("decode failed: %s at %lu: %s\n", ndo3d_strerror(rc), d.offset, d.message);
        return 1;
    }
    rc = ndo3d_write_obj_text(&g_model, g_obj, (unsigned long)sizeof(g_obj), &obj_n, &d);
    if (rc != NDO3D_OK) {
        printf("OBJ export failed: %s at %lu: %s\n", ndo3d_strerror(rc), d.offset, d.message);
        return 1;
    }
    if (!write_file_static(argv[2], g_obj, obj_n)) {
        printf("could not write output file\n");
        return 2;
    }
    printf("wrote %lu bytes\n", obj_n);
    return 0;
}
