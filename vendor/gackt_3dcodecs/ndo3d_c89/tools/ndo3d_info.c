#include <stdio.h>
#include "ndo3d.h"

#ifndef NDO3D_TOOL_MAX_FILE
#define NDO3D_TOOL_MAX_FILE 16777216UL
#endif

static unsigned char g_file[NDO3D_TOOL_MAX_FILE];
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

int main(int argc, char **argv)
{
    NDO3D_Diag d;
    unsigned long n;
    unsigned int i;
    int rc;
    if (argc < 2) {
        printf("usage: ndo3d_info file.ndo\n");
        return 2;
    }
    n = read_file_static(argv[1]);
    if (n == 0UL) {
        printf("could not read file or file is empty\n");
        return 2;
    }
    rc = ndo3d_decode(&g_model, g_file, n, &d);
    if (rc != NDO3D_OK) {
        printf("decode failed: %s at %lu: %s\n", ndo3d_strerror(rc), d.offset, d.message);
        return 1;
    }
    printf("NDO version: 1.%d\n", g_model.source_version == 11 ? 1 : 0);
    printf("objects: %u\n", g_model.object_count);
    for (i = 0U; i < g_model.object_count; i++) {
        const NDO3D_Object *o;
        o = &g_model.objects[i];
        printf("[%u] %s: vertices=%u edges=%u faces=%u visible=%u locked=%u\n",
               i, o->name, o->vertex_count, o->edge_count, o->face_count,
               (unsigned int)o->visible, (unsigned int)(o->sensitive == 0U));
    }
    return 0;
}
