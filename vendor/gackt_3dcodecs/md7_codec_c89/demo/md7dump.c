#include <stdio.h>
#include "md7_codec.h"

#define MD7DUMP_MAX_FILE       (16UL * 1024UL * 1024UL)
#define MD7DUMP_MAX_BONES      1024UL
#define MD7DUMP_MAX_GROUPS     256UL
#define MD7DUMP_MAX_SKINS      2048UL
#define MD7DUMP_MAX_MATERIALS  2048UL
#define MD7DUMP_MAX_UVS        131072UL
#define MD7DUMP_MAX_TRIS       131072UL
#define MD7DUMP_MAX_VERTS      131072UL
#define MD7DUMP_MAX_FRAMES     8192UL
#define MD7DUMP_MAX_FRAMEVERTS 262144UL
#define MD7DUMP_MAX_TRANSFORMS 262144UL
#define MD7DUMP_MAX_DEFORMERS  1024UL
#define MD7DUMP_MAX_ELEMENTS   4096UL
#define MD7DUMP_MAX_WEIGHTS    262144UL

static unsigned char g_file[MD7DUMP_MAX_FILE];
static struct md7_bone g_bones[MD7DUMP_MAX_BONES];
static struct md7_group g_groups[MD7DUMP_MAX_GROUPS];
static struct md7_skin g_skins[MD7DUMP_MAX_SKINS];
static struct md7_material g_materials[MD7DUMP_MAX_MATERIALS];
static struct md7_uv g_uvs[MD7DUMP_MAX_UVS];
static struct md7_triangle g_tris[MD7DUMP_MAX_TRIS];
static struct md7_vertex g_verts[MD7DUMP_MAX_VERTS];
static struct md7_frame g_frames[MD7DUMP_MAX_FRAMES];
static struct md7_vertex g_frameverts[MD7DUMP_MAX_FRAMEVERTS];
static struct md7_bonetransform g_transforms[MD7DUMP_MAX_TRANSFORMS];
static struct md7_deformer g_deformers[MD7DUMP_MAX_DEFORMERS];
static struct md7_deformer_element g_elements[MD7DUMP_MAX_ELEMENTS];
static struct md7_deformer_weight g_weights[MD7DUMP_MAX_WEIGHTS];

static void fx_print(md7_fx16 v)
{
    md7_i32 whole;
    md7_i32 frac;
    whole = v / MD7_FX_ONE;
    frac = v % MD7_FX_ONE;
    if (frac < 0) {
        frac = -frac;
    }
    printf("%ld.%04ld", (long)whole, (long)((frac * 10000L) / MD7_FX_ONE));
}

static void bounds_print(const struct md7_bounds *b)
{
    if (!b->valid) {
        printf("<none>");
        return;
    }
    printf("min("); fx_print(b->min_x); printf(","); fx_print(b->min_y); printf(","); fx_print(b->min_z);
    printf(") max("); fx_print(b->max_x); printf(","); fx_print(b->max_y); printf(","); fx_print(b->max_z); printf(")");
}

static void print_warnings(unsigned long warnings)
{
    unsigned long bit;
    int any;
    any = 0;
    for (bit = 1UL; bit != 0UL && bit <= 0x80000000UL; bit <<= 1) {
        if ((warnings & bit) != 0UL) {
            printf("  %s\n", md7_warning_name((md7_u32)bit));
            any = 1;
        }
    }
    if (!any) {
        printf("  <none>\n");
    }
}

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
    while (n < MD7DUMP_MAX_FILE) {
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
    m->bones = g_bones; m->bone_cap = MD7DUMP_MAX_BONES;
    m->groups = g_groups; m->group_cap = MD7DUMP_MAX_GROUPS;
    m->skins = g_skins; m->skin_cap = MD7DUMP_MAX_SKINS;
    m->materials = g_materials; m->material_cap = MD7DUMP_MAX_MATERIALS;
    m->uvs = g_uvs; m->uv_cap = MD7DUMP_MAX_UVS;
    m->triangles = g_tris; m->triangle_cap = MD7DUMP_MAX_TRIS;
    m->vertices = g_verts; m->vertex_cap = MD7DUMP_MAX_VERTS;
    m->frames = g_frames; m->frame_cap = MD7DUMP_MAX_FRAMES;
    m->framevertices = g_frameverts; m->framevertex_cap = MD7DUMP_MAX_FRAMEVERTS;
    m->transforms = g_transforms; m->transform_cap = MD7DUMP_MAX_TRANSFORMS;
    m->deformers = g_deformers; m->deformer_cap = MD7DUMP_MAX_DEFORMERS;
    m->deformer_elements = g_elements; m->deformer_element_cap = MD7DUMP_MAX_ELEMENTS;
    m->deformer_weights = g_weights; m->deformer_weight_cap = MD7DUMP_MAX_WEIGHTS;
}

int main(int argc, char **argv)
{
    struct md7_model m;
    unsigned long n;
    int r;
    unsigned long i;

    if (argc < 2) {
        printf("usage: md7dump model.mdl\n");
        return 2;
    }

    n = read_file(argv[1]);
    if (n == 0UL) {
        printf("could not read file or file is empty\n");
        return 3;
    }

    bind_model(&m);
    r = md7_load_memory(g_file, (md7_u32)n, &m);
    printf("result: %s (%d)\n", md7_result_name(r), r);
    printf("warnings: 0x%08lx\n", (unsigned long)m.warnings);
    print_warnings((unsigned long)m.warnings);
    if (r != MD7_OK && r != MD7_E_CAPACITY) {
        return 1;
    }

    printf("ident=%s version=%ld data_size=%lu\n", m.header.ident, (long)m.header.version, (unsigned long)m.header.data_size);
    printf("counts: bones %lu/%lu groups %lu/%lu skins %lu/%lu materials %lu/%lu uvs %lu/%lu tris %lu/%lu verts %lu/%lu frames %lu/%lu frameverts %lu/%lu transforms %lu/%lu deformers %lu/%lu elements %lu/%lu weights %lu/%lu\n",
        (unsigned long)m.bone_count, (unsigned long)m.bone_needed,
        (unsigned long)m.group_count, (unsigned long)m.group_needed,
        (unsigned long)m.skin_count, (unsigned long)m.skin_needed,
        (unsigned long)m.material_count, (unsigned long)m.material_needed,
        (unsigned long)m.uv_count, (unsigned long)m.uv_needed,
        (unsigned long)m.triangle_count, (unsigned long)m.triangle_needed,
        (unsigned long)m.vertex_count, (unsigned long)m.vertex_needed,
        (unsigned long)m.frame_count, (unsigned long)m.frame_needed,
        (unsigned long)m.framevertex_count, (unsigned long)m.framevertex_needed,
        (unsigned long)m.transform_count, (unsigned long)m.transform_needed,
        (unsigned long)m.deformer_count, (unsigned long)m.deformer_needed,
        (unsigned long)m.deformer_element_count, (unsigned long)m.deformer_element_needed,
        (unsigned long)m.deformer_weight_count, (unsigned long)m.deformer_weight_needed);

    printf("struct sizes: bone=%u skin=%u color=%u material=%u uv=%u tri=%u vtx=%u framevtx=%u btx=%u frame=%u\n",
        (unsigned int)m.header.bone_stc_size,
        (unsigned int)m.header.skin_stc_size,
        (unsigned int)m.header.colorvalue_stc_size,
        (unsigned int)m.header.material_stc_size,
        (unsigned int)m.header.skinpoint_stc_size,
        (unsigned int)m.header.triangle_stc_size,
        (unsigned int)m.header.mainvertex_stc_size,
        (unsigned int)m.header.framevertex_stc_size,
        (unsigned int)m.header.bonetrans_stc_size,
        (unsigned int)m.header.frame_stc_size);

    printf("model bounds: "); bounds_print(&m.bounds); printf("\n");

    for (i = 0UL; i < m.group_count; ++i) {
        printf("group[%lu] name='%s' type=%u skins=%lu uvs=%lu tris=%lu verts=%lu frames=%lu deformers=%lu extra=%lu invalid(v/uv/mat)=%lu/%lu/%lu bounds=",
            i, m.groups[i].name, (unsigned int)m.groups[i].typ,
            (unsigned long)m.groups[i].numskins, (unsigned long)m.groups[i].num_stpts,
            (unsigned long)m.groups[i].numtris, (unsigned long)m.groups[i].numverts,
            (unsigned long)m.groups[i].numframes, (unsigned long)m.groups[i].num_deformers_parsed,
            (unsigned long)m.groups[i].extra_size,
            (unsigned long)m.groups[i].invalid_vertex_indices,
            (unsigned long)m.groups[i].invalid_uv_indices,
            (unsigned long)m.groups[i].invalid_material_indices);
        bounds_print(&m.groups[i].bounds); printf("\n");
    }

    for (i = 0UL; i < m.skin_count && i < 8UL; ++i) {
        printf("skin[%lu] typ=0x%02x %ldx%ld bpp=%u tex='%s' material=%lu raw=%lu asc=%lu\n",
            i, (unsigned int)m.skins[i].typ, (long)m.skins[i].width, (long)m.skins[i].height,
            (unsigned int)m.skins[i].bytes_per_pixel, m.skins[i].texture_name,
            (unsigned long)m.skins[i].material_index, (unsigned long)m.skins[i].raw_pixels_size,
            (unsigned long)m.skins[i].ascdef_size);
    }

    for (i = 0UL; i < m.bone_count && i < 8UL; ++i) {
        printf("bone[%lu] parent=%u name='%s' pos=", i, (unsigned int)m.bones[i].parent_index, m.bones[i].name);
        fx_print(m.bones[i].x); printf(","); fx_print(m.bones[i].y); printf(","); fx_print(m.bones[i].z); printf("\n");
    }

    for (i = 0UL; i < m.vertex_count && i < 8UL; ++i) {
        printf("v[%lu] bone/index=%u pos=", i, (unsigned int)m.vertices[i].index);
        fx_print(m.vertices[i].x); printf(","); fx_print(m.vertices[i].y); printf(","); fx_print(m.vertices[i].z); printf(" normal_mode=%u\n", (unsigned int)m.vertices[i].normal_mode);
    }

    for (i = 0UL; i < m.deformer_element_count && i < 8UL; ++i) {
        printf("deformer_element[%lu] name='%s' index=%ld weights=%lu\n", i, m.deformer_elements[i].element_name, (long)m.deformer_elements[i].element_index, (unsigned long)m.deformer_elements[i].weights);
    }

    return 0;
}
