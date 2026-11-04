#include <stdio.h>
#include "md7_codec.h"

#define MAX_FILE       (16UL * 1024UL * 1024UL)
#define MAX_BONES      1024UL
#define MAX_GROUPS     256UL
#define MAX_SKINS      2048UL
#define MAX_MATERIALS  2048UL
#define MAX_UVS        131072UL
#define MAX_TRIS       131072UL
#define MAX_VERTS      131072UL
#define MAX_FRAMES     8192UL
#define MAX_FRAMEVERTS 262144UL
#define MAX_TRANSFORMS 262144UL
#define MAX_DEFORMERS  1024UL
#define MAX_ELEMENTS   4096UL
#define MAX_WEIGHTS    262144UL

static unsigned char g_file[MAX_FILE];
static struct md7_bone g_bones[MAX_BONES];
static struct md7_group g_groups[MAX_GROUPS];
static struct md7_skin g_skins[MAX_SKINS];
static struct md7_material g_materials[MAX_MATERIALS];
static struct md7_uv g_uvs[MAX_UVS];
static struct md7_triangle g_tris[MAX_TRIS];
static struct md7_vertex g_verts[MAX_VERTS];
static struct md7_frame g_frames[MAX_FRAMES];
static struct md7_vertex g_frameverts[MAX_FRAMEVERTS];
static struct md7_bonetransform g_transforms[MAX_TRANSFORMS];
static struct md7_deformer g_deformers[MAX_DEFORMERS];
static struct md7_deformer_element g_elements[MAX_ELEMENTS];
static struct md7_deformer_weight g_weights[MAX_WEIGHTS];

static unsigned long read_file(const char *path)
{
    FILE *f;
    unsigned long n;
    int c;
    f = fopen(path, "rb");
    if (f == 0) {
        return 0UL;
    }
    n = 0UL;
    while (n < MAX_FILE) {
        c = fgetc(f);
        if (c == EOF) {
            break;
        }
        g_file[n++] = (unsigned char)c;
    }
    fclose(f);
    return n;
}

static void bind_model(struct md7_model *m)
{
    m->bones = g_bones; m->bone_cap = MAX_BONES;
    m->groups = g_groups; m->group_cap = MAX_GROUPS;
    m->skins = g_skins; m->skin_cap = MAX_SKINS;
    m->materials = g_materials; m->material_cap = MAX_MATERIALS;
    m->uvs = g_uvs; m->uv_cap = MAX_UVS;
    m->triangles = g_tris; m->triangle_cap = MAX_TRIS;
    m->vertices = g_verts; m->vertex_cap = MAX_VERTS;
    m->frames = g_frames; m->frame_cap = MAX_FRAMES;
    m->framevertices = g_frameverts; m->framevertex_cap = MAX_FRAMEVERTS;
    m->transforms = g_transforms; m->transform_cap = MAX_TRANSFORMS;
    m->deformers = g_deformers; m->deformer_cap = MAX_DEFORMERS;
    m->deformer_elements = g_elements; m->deformer_element_cap = MAX_ELEMENTS;
    m->deformer_weights = g_weights; m->deformer_weight_cap = MAX_WEIGHTS;
}

static void fprint_fx(FILE *f, md7_fx16 v)
{
    md7_i32 whole;
    md7_i32 frac;
    if (v < 0 && v > -MD7_FX_ONE) {
        fputc('-', f);
    }
    whole = v / MD7_FX_ONE;
    frac = v % MD7_FX_ONE;
    if (frac < 0) {
        frac = -frac;
    }
    fprintf(f, "%ld.%06ld", (long)whole, (long)((frac * 1000000L) / MD7_FX_ONE));
}

int main(int argc, char **argv)
{
    struct md7_model m;
    unsigned long n;
    unsigned long gi;
    unsigned long i;
    int r;
    FILE *out;
    struct md7_group *g;
    struct md7_vertex *v;
    struct md7_uv *uv;
    struct md7_triangle *t;
    unsigned long v0;
    unsigned long v1;
    unsigned long v2;
    unsigned long u0;
    unsigned long u1;
    unsigned long u2;

    if (argc < 3) {
        printf("usage: md7toobj input.mdl output.obj\n");
        return 2;
    }
    n = read_file(argv[1]);
    if (n == 0UL) {
        printf("could not read input\n");
        return 3;
    }
    bind_model(&m);
    r = md7_load_memory(g_file, (md7_u32)n, &m);
    if (r != MD7_OK && r != MD7_E_CAPACITY) {
        printf("parse failed: %s (%d)\n", md7_result_name(r), r);
        return 1;
    }
    out = fopen(argv[2], "w");
    if (out == 0) {
        printf("could not open output\n");
        return 4;
    }
    fprintf(out, "# OBJ preview exported by md7toobj\n");
    fprintf(out, "# warning: MDL7 skeleton/frame animation is not represented in Wavefront OBJ\n");

    for (i = 0UL; i < m.vertex_count; ++i) {
        v = &m.vertices[i];
        fprintf(out, "v "); fprint_fx(out, v->x); fprintf(out, " "); fprint_fx(out, v->y); fprintf(out, " "); fprint_fx(out, v->z); fprintf(out, "\n");
    }
    for (i = 0UL; i < m.uv_count; ++i) {
        uv = &m.uvs[i];
        fprintf(out, "vt "); fprint_fx(out, uv->u); fprintf(out, " "); fprint_fx(out, uv->v); fprintf(out, "\n");
    }

    for (gi = 0UL; gi < m.group_count; ++gi) {
        g = &m.groups[gi];
        fprintf(out, "g %s\n", g->name[0] ? g->name : "md7_group");
        for (i = 0UL; i < g->numtris; ++i) {
            if (g->first_triangle + i >= m.triangle_count) {
                break;
            }
            t = &m.triangles[g->first_triangle + i];
            if (t->index_warning) {
                fprintf(out, "# skipped invalid triangle %lu\n", i);
                continue;
            }
            v0 = g->first_vertex + (unsigned long)t->v_index[0] + 1UL;
            v1 = g->first_vertex + (unsigned long)t->v_index[1] + 1UL;
            v2 = g->first_vertex + (unsigned long)t->v_index[2] + 1UL;
            if (t->has_uv1) {
                u0 = g->first_uv + (unsigned long)t->uv1_index[0] + 1UL;
                u1 = g->first_uv + (unsigned long)t->uv1_index[1] + 1UL;
                u2 = g->first_uv + (unsigned long)t->uv1_index[2] + 1UL;
                fprintf(out, "f %lu/%lu %lu/%lu %lu/%lu\n", v0, u0, v1, u1, v2, u2);
            } else {
                fprintf(out, "f %lu %lu %lu\n", v0, v1, v2);
            }
        }
    }
    fclose(out);
    printf("wrote %s\n", argv[2]);
    return 0;
}
