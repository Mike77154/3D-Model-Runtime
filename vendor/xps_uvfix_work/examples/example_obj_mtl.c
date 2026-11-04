#include <stdio.h>
#include <string.h>
#include "../include/xps.h"

#define EXAMPLE_FX_ONE ((xps_fx)65536L)

static FILE *g_obj;
static FILE *g_mtl;
static xps_u32 g_base;
static xps_u32 g_count;
static xps_u32 g_current_mesh;
static char g_diffuse[XPS_CFG_MAX_TEXNAME];
static char g_normal[XPS_CFG_MAX_TEXNAME];

static void copy_text(char *dst, unsigned long dst_size, const char *src)
{
    unsigned long i;
    if (dst_size == 0UL) {
        return;
    }
    for (i = 0UL; i + 1UL < dst_size && src[i] != '\0'; ++i) {
        dst[i] = src[i];
    }
    dst[i] = '\0';
}

static int has_token_nrm(const char *s)
{
    while (*s != '\0') {
        if ((s[0] == '_' || s[0] == '-' || s[0] == '.') &&
            (s[1] == 'n' || s[1] == 'N') &&
            (s[2] == 'r' || s[2] == 'R') &&
            (s[3] == 'm' || s[3] == 'M')) {
            return 1;
        }
        ++s;
    }
    return 0;
}

static const char *path_base(const char *path)
{
    const char *p;
    const char *last;
    last = path;
    for (p = path; *p != '\0'; ++p) {
        if (*p == '/' || *p == '\\') {
            last = p + 1;
        }
    }
    return last;
}

static void print_fx(FILE *f, xps_fx v)
{
    long ip;
    long fp;
    if (v < 0) {
        fputc('-', f);
        v = (xps_fx)(-v);
    }
    ip = (long)(v >> 16);
    fp = (long)((((unsigned long)(v & 65535L)) * 1000000UL + 32768UL) >> 16);
    fprintf(f, "%ld.%06ld", ip, fp);
}

static void print_fx_obj_v(FILE *f, xps_fx raw_v)
{
    xps_fx flipped;
    flipped = (xps_fx)(EXAMPLE_FX_ONE - raw_v);
    print_fx(f, flipped);
}

static void print_material_name(FILE *f, xps_u32 mesh_index)
{
    fprintf(f, "Model001_Material%03lu", (unsigned long)mesh_index + 1UL);
}

static int mtl_on_mesh_begin(void *user, const xps_mesh_info *mesh)
{
    (void)user;
    g_current_mesh = mesh->index;
    g_diffuse[0] = '\0';
    g_normal[0] = '\0';
    return 0;
}

static int mtl_on_texture(void *user, xps_u32 mesh_index, const xps_texture *texture)
{
    (void)user;
    (void)mesh_index;
    if (has_token_nrm(texture->file)) {
        if (g_normal[0] == '\0') {
            copy_text(g_normal, sizeof(g_normal), texture->file);
        }
    } else if (g_diffuse[0] == '\0') {
        copy_text(g_diffuse, sizeof(g_diffuse), texture->file);
    }
    return 0;
}

static int mtl_on_mesh_end(void *user, const xps_mesh_info *mesh)
{
    (void)user;
    fprintf(g_mtl, "newmtl ");
    print_material_name(g_mtl, g_current_mesh);
    fputc('\n', g_mtl);
    if (g_diffuse[0] != '\0') {
        fprintf(g_mtl, "map_Kd %s\n", g_diffuse);
    }
    if (g_normal[0] != '\0') {
        fprintf(g_mtl, "map_bump %s\n", g_normal);
    }
    fputc('\n', g_mtl);
    (void)mesh;
    return 0;
}

static int obj_on_model_begin(void *user, const xps_model_info *model)
{
    const char *mtl_base;
    (void)user;
    mtl_base = (const char *)user;
    fprintf(g_obj, "mtllib %s\n", mtl_base);
    fprintf(g_obj, "# xps obj dump bones=%lu meshes=%lu uv_v=one_minus_raw_xps_v\n",
            (unsigned long)model->bone_count,
            (unsigned long)model->mesh_count);
    g_base = 1U;
    g_count = 0U;
    return 0;
}

static int obj_on_mesh_begin(void *user, const xps_mesh_info *mesh)
{
    (void)user;
    fprintf(g_obj, "g Model001_%s\n", mesh->name);
    fprintf(g_obj, "usemtl ");
    print_material_name(g_obj, mesh->index);
    fputc('\n', g_obj);
    g_count = 0U;
    return 0;
}

static int obj_on_vertex(void *user, xps_u32 mesh_index, const xps_vertex *v)
{
    xps_u32 i;
    (void)user;
    (void)mesh_index;
    fprintf(g_obj, "v ");
    print_fx(g_obj, v->x);
    fputc(' ', g_obj);
    print_fx(g_obj, v->y);
    fputc(' ', g_obj);
    print_fx(g_obj, v->z);
    fputc('\n', g_obj);

    fprintf(g_obj, "vn ");
    print_fx(g_obj, v->nx);
    fputc(' ', g_obj);
    print_fx(g_obj, v->ny);
    fputc(' ', g_obj);
    print_fx(g_obj, v->nz);
    fputc('\n', g_obj);

    if (v->uv_count == 0U) {
        fprintf(g_obj, "vt 0.000000 0.000000\n");
    } else {
        for (i = 0U; i < v->uv_count; ++i) {
            fprintf(g_obj, "vt ");
            print_fx(g_obj, v->uv[i][0]);
            fputc(' ', g_obj);
            print_fx_obj_v(g_obj, v->uv[i][1]);
            fputc('\n', g_obj);
            break;
        }
    }
    ++g_count;
    return 0;
}

static int obj_on_face(void *user, xps_u32 mesh_index, const xps_face *f)
{
    unsigned long a;
    unsigned long b;
    unsigned long c;
    (void)user;
    (void)mesh_index;
    a = (unsigned long)g_base + (unsigned long)f->i0;
    b = (unsigned long)g_base + (unsigned long)f->i1;
    c = (unsigned long)g_base + (unsigned long)f->i2;
    fprintf(g_obj, "f %lu/%lu/%lu %lu/%lu/%lu %lu/%lu/%lu\n", a, a, a, b, b, b, c, c, c);
    return 0;
}

static int obj_on_mesh_end(void *user, const xps_mesh_info *mesh)
{
    (void)user;
    (void)mesh;
    g_base += g_count;
    return 0;
}

static const xps_sink MTL_SINK = {
    0,
    0,
    mtl_on_mesh_begin,
    mtl_on_texture,
    0,
    0,
    mtl_on_mesh_end,
    0
};

static const xps_sink OBJ_SINK = {
    obj_on_model_begin,
    0,
    obj_on_mesh_begin,
    0,
    obj_on_vertex,
    obj_on_face,
    obj_on_mesh_end,
    0
};

int main(int argc, char **argv)
{
    const char *mtl_base;
    int rc;

    if (argc != 4) {
        fprintf(stderr, "usage: example_obj_mtl in.xps out.obj out.mtl\n");
        return 1;
    }

    g_mtl = fopen(argv[3], "w");
    if (!g_mtl) {
        return 2;
    }
    rc = xps_parse_file(argv[1], &MTL_SINK, NULL);
    fclose(g_mtl);
    if (rc != XPS_OK) {
        fprintf(stderr, "mtl parse failed: %s\n", xps_error_string(rc));
        return 3;
    }

    g_obj = fopen(argv[2], "w");
    if (!g_obj) {
        return 4;
    }
    mtl_base = path_base(argv[3]);
    rc = xps_parse_file(argv[1], &OBJ_SINK, (void *)mtl_base);
    fclose(g_obj);
    if (rc != XPS_OK) {
        fprintf(stderr, "obj parse failed: %s\n", xps_error_string(rc));
        return 5;
    }
    return 0;
}
