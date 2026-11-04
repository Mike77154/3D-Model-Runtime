#include <stdio.h>
#include <string.h>
#include "../include/xps.h"

static FILE *g_out;
static xps_u32 g_base;
static xps_u32 g_count;

#define EXAMPLE_FX_ONE ((xps_fx)65536L)

static void print_fx(FILE *f, xps_fx v)
{
    long ip;
    long fp;
    if (v < 0) { fputc('-', f); v = (xps_fx)(-v); }
    ip = (long)(v >> 16);
    fp = (long)((((unsigned long)(v & 65535L)) * 1000000UL + 32768UL) >> 16);
    fprintf(f, "%ld.%06ld", ip, fp);
}

static int on_model_begin(void *user, const xps_model_info *model)
{
    (void)user;
    fprintf(g_out, "# xps obj dump bones=%lu meshes=%lu\n", (unsigned long)model->bone_count, (unsigned long)model->mesh_count);
    g_base = 1U;
    g_count = 0U;
    return 0;
}

static int on_mesh_begin(void *user, const xps_mesh_info *mesh)
{
    (void)user;
    fprintf(g_out, "o mesh_%lu_%s\n", (unsigned long)mesh->index, mesh->name);
    g_count = 0U;
    return 0;
}

static int on_vertex(void *user, xps_u32 mesh_index, const xps_vertex *v)
{
    xps_u32 i;
    (void)user; (void)mesh_index;
    fprintf(g_out, "v "); print_fx(g_out, v->x); fputc(' ', g_out); print_fx(g_out, v->y); fputc(' ', g_out); print_fx(g_out, v->z); fputc('\n', g_out);
    fprintf(g_out, "vn "); print_fx(g_out, v->nx); fputc(' ', g_out); print_fx(g_out, v->ny); fputc(' ', g_out); print_fx(g_out, v->nz); fputc('\n', g_out);
    for (i = 0; i < v->uv_count; ++i) {
        fprintf(g_out, "vt "); print_fx(g_out, v->uv[i][0]); fputc(' ', g_out); print_fx(g_out, (xps_fx)(EXAMPLE_FX_ONE - v->uv[i][1])); fputc('\n', g_out);
        break;
    }
    ++g_count;
    return 0;
}

static int on_face(void *user, xps_u32 mesh_index, const xps_face *f)
{
    unsigned long a, b, c;
    (void)user; (void)mesh_index;
    a = (unsigned long)g_base + (unsigned long)f->i0;
    b = (unsigned long)g_base + (unsigned long)f->i1;
    c = (unsigned long)g_base + (unsigned long)f->i2;
    fprintf(g_out, "f %lu/%lu/%lu %lu/%lu/%lu %lu/%lu/%lu\n", a,a,a,b,b,b,c,c,c);
    return 0;
}

static int on_mesh_end(void *user, const xps_mesh_info *mesh)
{
    (void)user; (void)mesh;
    g_base += g_count;
    return 0;
}

static const xps_sink SINK = { on_model_begin, 0, on_mesh_begin, 0, on_vertex, on_face, on_mesh_end, 0 };

int main(int argc, char **argv)
{
    int rc;
    if (argc != 3) { fprintf(stderr, "usage: example_obj in.xps out.obj\n"); return 1; }
    g_out = fopen(argv[2], "w");
    if (!g_out) { return 2; }
    rc = xps_parse_file(argv[1], &SINK, NULL);
    fclose(g_out);
    if (rc != XPS_OK) { fprintf(stderr, "parse failed: %s\n", xps_error_string(rc)); return 3; }
    return 0;
}
