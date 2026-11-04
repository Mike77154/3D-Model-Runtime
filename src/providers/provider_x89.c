#include "../modelrt_internal.h"
#include "xp.h"
#include "xp_anim_pose.h"

#include <stdio.h>
#include <string.h>
#include <limits.h>

#define X89_STACK_MAX 64
#define X89_NO_INDEX  ((modelrt_i32)-1)

typedef struct x89_vertex_s {
    modelrt_vec3 position;
    modelrt_vec3 normal;
    modelrt_vec2 uv;
    modelrt_u8 rgba[4];
    modelrt_u8 flags;
    modelrt_u8 weight_count;
    modelrt_i32 cluster[4];
    modelrt_q16 weight[4];
} x89_vertex;

typedef struct x89_face_s {
    modelrt_u32 first_index;
    modelrt_u32 degree;
    modelrt_u32 first_normal_index;
    modelrt_u32 normal_degree;
    modelrt_i32 material_slot;
} x89_face;

typedef struct x89_mesh_s {
    char name[MODELRT_NAME_MAX];
    modelrt_u32 first_vertex;
    modelrt_u32 vertex_count;
    modelrt_u32 first_face;
    modelrt_u32 face_count;
    modelrt_u32 triangle_count;
    modelrt_u32 normal_base;
    modelrt_u32 normal_count;
    modelrt_u32 material_slot_base;
    modelrt_u32 material_slot_count;
    modelrt_i32 frame_index;
} x89_mesh;

typedef struct x89_material_s {
    char name[MODELRT_NAME_MAX];
    modelrt_u8 rgba[4];
    modelrt_q16 power;
    modelrt_vec3 specular;
    modelrt_vec3 emissive;
    modelrt_i32 texture_index;
} x89_material;

typedef struct x89_texture_s {
    char path[MODELRT_PATH_MAX];
} x89_texture;

typedef struct x89_material_slot_s {
    modelrt_i32 material_index;
    char ref_name[MODELRT_NAME_MAX];
} x89_material_slot;

typedef struct x89_frame_s {
    char name[MODELRT_NAME_MAX];
    modelrt_u32 node_id;
    modelrt_i32 parent;
    modelrt_q16 local[16];
    modelrt_q16 world[16];
} x89_frame;

typedef struct x89_cluster_s {
    char bone_name[MODELRT_NAME_MAX];
    modelrt_i32 frame_index;
    modelrt_u32 mesh_index;
    modelrt_q16 offset[16];
} x89_cluster;

typedef struct x89_anim_set_s {
    char name[MODELRT_NAME_MAX];
    int min_tick;
    int max_tick;
} x89_anim_set;

typedef struct x89_counts_s {
    modelrt_u32 mesh_count;
    modelrt_u32 vertex_count;
    modelrt_u32 face_count;
    modelrt_u32 face_index_count;
    modelrt_u32 triangle_count;
    modelrt_u32 normal_count;
    modelrt_u32 normal_index_count;
    modelrt_u32 uv_count;
    modelrt_u32 color_count;
    modelrt_u32 material_defs;
    modelrt_u32 texture_count;
    modelrt_u32 material_slots;
    modelrt_u32 frame_count;
    modelrt_u32 cluster_count;
    modelrt_u32 max_cluster_weights;
    modelrt_u32 animation_sets;
    modelrt_u32 ticks_per_second;
    int bad;
    char error[160];
} x89_counts;

typedef struct x89_state_s {
    x89_counts count;
    x89_mesh *meshes;
    x89_vertex *vertices;
    x89_face *faces;
    modelrt_u32 *indices;
    modelrt_vec3 *normals;
    modelrt_u32 *normal_indices;
    x89_material *materials;
    modelrt_u32 material_capacity;
    modelrt_u32 material_count;
    x89_texture *textures;
    modelrt_u32 texture_count;
    x89_material_slot *material_slots;
    x89_frame *frames;
    x89_cluster *clusters;
    modelrt_u32 *skin_index_scratch;
    x89_anim_set *animations;
    modelrt_u32 animation_count;
    modelrt_u32 ticks_per_second;

    /* v0.5 native animation database. Source keys stay in xparser records;
       dense poses are reconstructed on demand into caller-owned scratch. */
    xp_anim_capture anim_capture;
    xp_anim_requirements anim_req;
    xp_anim_node_record *anim_nodes;
    xp_anim_clip_record *anim_clips;
    xp_anim_track_record *anim_tracks;
    xp_anim_key_record *anim_keys;
    xp_anim_local_pose *anim_pose_scratch;
    modelrt_q16 *anim_world;
    modelrt_u8 *anim_world_state;
    int anim_pose_active;

    modelrt_u32 mesh_cursor;
    modelrt_u32 vertex_cursor;
    modelrt_u32 face_cursor;
    modelrt_u32 index_cursor;
    modelrt_u32 normal_cursor;
    modelrt_u32 normal_index_cursor;
    modelrt_u32 material_slot_cursor;
    modelrt_u32 material_cursor;
    modelrt_u32 texture_cursor;
    modelrt_u32 frame_cursor;
    modelrt_u32 cluster_cursor;
    modelrt_u32 animation_cursor;

    modelrt_i32 current_mesh;
    modelrt_i32 current_material;
    modelrt_i32 current_cluster;
    modelrt_i32 current_animation;
    modelrt_u32 current_slot_next;
    int material_list_depth;
    modelrt_i32 frame_stack[X89_STACK_MAX];
    int frame_depth;
    modelrt_i32 pending_mesh_frame;
    int parse_bad;
    char parse_error[160];
} x89_state;

static int x89_str_ieq(const char *a, const char *b)
{
    unsigned char ca;
    unsigned char cb;
    if (a == 0 || b == 0) return 0;
    while (*a != '\0' && *b != '\0') {
        ca = (unsigned char)*a++;
        cb = (unsigned char)*b++;
        if (ca >= 'A' && ca <= 'Z') ca = (unsigned char)(ca - 'A' + 'a');
        if (cb >= 'A' && cb <= 'Z') cb = (unsigned char)(cb - 'A' + 'a');
        if (ca != cb) return 0;
    }
    return *a == '\0' && *b == '\0';
}

static modelrt_u8 x89_color_u8(modelrt_q16 v)
{
    modelrt_i32 x;
    if (v <= 0) return 0u;
    if (v >= MODELRT_Q16_ONE) return 255u;
    x = (modelrt_i32)(((modelrt_u32)v * 255u) >> 16);
    if (x < 0) x = 0;
    if (x > 255) x = 255;
    return (modelrt_u8)x;
}

static void x89_mat_identity(modelrt_q16 m[16])
{
    int i;
    for (i = 0; i < 16; ++i) m[i] = 0;
    m[0] = MODELRT_Q16_ONE;
    m[5] = MODELRT_Q16_ONE;
    m[10] = MODELRT_Q16_ONE;
    m[15] = MODELRT_Q16_ONE;
}

static modelrt_q16 x89_sat_add(modelrt_q16 a, modelrt_q16 b)
{
    if (b > 0 && a > 2147483647 - b) return 2147483647;
    if (b < 0 && a < (-2147483647 - 1) - b) return (-2147483647 - 1);
    return (modelrt_q16)(a + b);
}

static modelrt_q16 x89_dot4(modelrt_q16 a0, modelrt_q16 b0,
                            modelrt_q16 a1, modelrt_q16 b1,
                            modelrt_q16 a2, modelrt_q16 b2,
                            modelrt_q16 a3, modelrt_q16 b3)
{
    modelrt_q16 v;
    v = modelrt_q16_mul(a0, b0);
    v = x89_sat_add(v, modelrt_q16_mul(a1, b1));
    v = x89_sat_add(v, modelrt_q16_mul(a2, b2));
    v = x89_sat_add(v, modelrt_q16_mul(a3, b3));
    return v;
}

/* DirectX frame matrices use the conventional _41/_42/_43 translation slots.
 * Runtime views preserve the authored basis; no humanoid or axis convention is
 * invented by this provider. */
static void x89_mat_mul(const modelrt_q16 a[16], const modelrt_q16 b[16],
                        modelrt_q16 o[16])
{
    modelrt_q16 t[16];
    int r;
    int c;
    for (r = 0; r < 4; ++r) {
        for (c = 0; c < 4; ++c) {
            t[r * 4 + c] = x89_dot4(a[r * 4 + 0], b[0 * 4 + c],
                                    a[r * 4 + 1], b[1 * 4 + c],
                                    a[r * 4 + 2], b[2 * 4 + c],
                                    a[r * 4 + 3], b[3 * 4 + c]);
        }
    }
    for (r = 0; r < 16; ++r) o[r] = t[r];
}

static modelrt_vec3 x89_transform_point(const modelrt_q16 m[16], modelrt_vec3 p)
{
    modelrt_vec3 o;
    o.x = x89_dot4(p.x, m[0], p.y, m[4], p.z, m[8], MODELRT_Q16_ONE, m[12]);
    o.y = x89_dot4(p.x, m[1], p.y, m[5], p.z, m[9], MODELRT_Q16_ONE, m[13]);
    o.z = x89_dot4(p.x, m[2], p.y, m[6], p.z, m[10], MODELRT_Q16_ONE, m[14]);
    return o;
}

static modelrt_vec3 x89_transform_vector(const modelrt_q16 m[16], modelrt_vec3 p)
{
    modelrt_vec3 o;
    o.x = x89_dot4(p.x, m[0], p.y, m[4], p.z, m[8], 0, 0);
    o.y = x89_dot4(p.x, m[1], p.y, m[5], p.z, m[9], 0, 0);
    o.z = x89_dot4(p.x, m[2], p.y, m[6], p.z, m[10], 0, 0);
    return o;
}

static int x89_add_u32(modelrt_u32 *dst, modelrt_u32 add)
{
    if (*dst > 0xffffffffu - add) return 0;
    *dst += add;
    return 1;
}

static void x89_count_error(void *u, int line, int col, const char *msg)
{
    x89_counts *c;
    (void)line;
    (void)col;
    c = (x89_counts *)u;
    if (c == 0) return;
    c->bad = 1;
    modelrt_copy_string(c->error, sizeof(c->error), msg);
}

static void x89_count_mesh(void *u, const char *name, int nV, int nF)
{
    x89_counts *c;
    (void)name;
    (void)nF;
    c = (x89_counts *)u;
    if (c == 0 || nV < 0) return;
    if (!x89_add_u32(&c->mesh_count, 1u) ||
        !x89_add_u32(&c->vertex_count, (modelrt_u32)nV)) c->bad = 1;
}

static void x89_count_face(void *u, int i, int n, const int *idx)
{
    x89_counts *c;
    (void)i;
    (void)idx;
    c = (x89_counts *)u;
    if (c == 0 || n < 0) return;
    if (!x89_add_u32(&c->face_count, 1u) ||
        !x89_add_u32(&c->face_index_count, (modelrt_u32)n)) c->bad = 1;
    if (n >= 3 && !x89_add_u32(&c->triangle_count, (modelrt_u32)(n - 2))) c->bad = 1;
}

static void x89_count_normals(void *u, int nNormals, int nFaceNormals)
{
    x89_counts *c;
    (void)nFaceNormals;
    c = (x89_counts *)u;
    if (c == 0 || nNormals < 0) return;
    if (!x89_add_u32(&c->normal_count, (modelrt_u32)nNormals)) c->bad = 1;
}

static void x89_count_face_normals(void *u, int face_i, int n, const int *nidx)
{
    x89_counts *c;
    (void)face_i;
    (void)nidx;
    c = (x89_counts *)u;
    if (c == 0 || n < 0) return;
    if (!x89_add_u32(&c->normal_index_count, (modelrt_u32)n)) c->bad = 1;
}


static void x89_count_uvs(void *u, int nUV)
{
    x89_counts *c;
    c = (x89_counts *)u;
    if (c == 0 || nUV < 0) return;
    if (!x89_add_u32(&c->uv_count, (modelrt_u32)nUV)) c->bad = 1;
}

static void x89_count_colors(void *u, int nColors)
{
    x89_counts *c;
    c = (x89_counts *)u;
    if (c == 0 || nColors < 0) return;
    if (!x89_add_u32(&c->color_count, (modelrt_u32)nColors)) c->bad = 1;
}

static void x89_count_matslots(void *u, int nMaterials, int nFaceIndices)
{
    x89_counts *c;
    (void)nFaceIndices;
    c = (x89_counts *)u;
    if (c == 0 || nMaterials < 0) return;
    if (!x89_add_u32(&c->material_slots, (modelrt_u32)nMaterials)) c->bad = 1;
}

static void x89_count_material(void *u, const char *name)
{
    x89_counts *c;
    (void)name;
    c = (x89_counts *)u;
    if (c != 0 && !x89_add_u32(&c->material_defs, 1u)) c->bad = 1;
}

static void x89_count_texture(void *u, const char *filename)
{
    x89_counts *c;
    (void)filename;
    c = (x89_counts *)u;
    if (c != 0 && !x89_add_u32(&c->texture_count, 1u)) c->bad = 1;
}

static void x89_count_frame_node(void *u, unsigned int frame_id,
                                 unsigned int parent_frame_id, const char *name,
                                 unsigned int frame_depth)
{
    x89_counts *c;
    (void)frame_id;
    (void)parent_frame_id;
    (void)name;
    (void)frame_depth;
    c = (x89_counts *)u;
    if (c != 0 && !x89_add_u32(&c->frame_count, 1u)) c->bad = 1;
}

static void x89_count_skin(void *u, const char *bone_name, int nWeights)
{
    x89_counts *c;
    (void)bone_name;
    c = (x89_counts *)u;
    if (c == 0 || nWeights < 0) return;
    if (!x89_add_u32(&c->cluster_count, 1u)) c->bad = 1;
    if ((modelrt_u32)nWeights > c->max_cluster_weights)
        c->max_cluster_weights = (modelrt_u32)nWeights;
}

static void x89_count_anim_set(void *u, const char *name)
{
    x89_counts *c;
    (void)name;
    c = (x89_counts *)u;
    if (c != 0 && !x89_add_u32(&c->animation_sets, 1u)) c->bad = 1;
}

static void x89_count_ticks(void *u, unsigned int ticks)
{
    x89_counts *c;
    c = (x89_counts *)u;
    if (c != 0) c->ticks_per_second = (modelrt_u32)ticks;
}

static int x89_parse_file(const char *path, const xp_callbacks *cb, void *user,
                          char *detail, size_t detail_cap)
{
    FILE *f;
    xp_context ctx;
    xp_header hdr;
    xp_result xr;
    f = fopen(path, "rb");
    if (f == 0) {
        if (detail != 0) modelrt_copy_string(detail, detail_cap, "open failed");
        return MODELRT_ERR_IO;
    }
    xr = xp_open_file(&ctx, f, &hdr);
    if (xr == XP_OK) xr = xp_parse(&ctx, cb, user);
    xp_close(&ctx);
    fclose(f);
    if (xr == XP_OK) return MODELRT_OK;
    if (detail != 0 && detail[0] == '\0') modelrt_copy_string(detail, detail_cap, xp_result_str(xr));
    if (xr == XP_EIO) return MODELRT_ERR_IO;
    if (xr == XP_EFORMAT) return MODELRT_ERR_FORMAT;
    if (xr == XP_ELIMIT) return MODELRT_ERR_LIMIT;
    if (xr == XP_ENOTSUP) return MODELRT_ERR_UNSUPPORTED;
    return MODELRT_ERR_CODEC;
}

static int x89_count_file(const char *path, x89_counts *c)
{
    xp_callbacks cb;
    int rc;
    modelrt_zero(c, sizeof(*c));
    modelrt_zero(&cb, sizeof(cb));
    cb.on_mesh_begin = x89_count_mesh;
    cb.on_face = x89_count_face;
    cb.on_mesh_normals_begin = x89_count_normals;
    cb.on_face_normals = x89_count_face_normals;
    cb.on_mesh_texcoords_begin = x89_count_uvs;
    cb.on_mesh_colors_begin = x89_count_colors;
    cb.on_mesh_material_list_begin = x89_count_matslots;
    cb.on_material_begin = x89_count_material;
    cb.on_material_texture_filename = x89_count_texture;
    cb.on_frame_node_begin = x89_count_frame_node;
    cb.on_skin_weights_begin = x89_count_skin;
    cb.on_animation_set_begin = x89_count_anim_set;
    cb.on_anim_ticks_per_second = x89_count_ticks;
    cb.on_error = x89_count_error;
    rc = x89_parse_file(path, &cb, c, c->error, sizeof(c->error));
    if (rc != MODELRT_OK) return rc;
    if (c->bad) return MODELRT_ERR_LIMIT;
    return MODELRT_OK;
}

static int x89_anim_requirements_file(const char *path, xp_anim_requirements *out_req,
                                      char *detail, size_t detail_cap)
{
    xp_anim_capture count_cap;
    xp_callbacks cb;
    int rc;
    if (out_req == 0) return MODELRT_ERR_BAD_ARG;
    modelrt_zero(out_req, sizeof(*out_req));
    xp_anim_capture_init(&count_cap, 0, 0ul, 0, 0ul, 0, 0ul, 0, 0ul);
    xp_anim_capture_callbacks(&cb);
    rc = x89_parse_file(path, &cb, &count_cap, detail, detail_cap);
    if (rc != MODELRT_OK) return rc;
    xp_anim_capture_get_requirements(&count_cap, out_req);
    return MODELRT_OK;
}

static void *x89_array(modelrt_handle *h, size_t elem, modelrt_u32 count)
{
    size_t bytes;
    if (count == 0u) return 0;
    if (elem != 0u && (size_t)count > ((size_t)-1) / elem) return 0;
    bytes = elem * (size_t)count;
    return modelrt_arena_alloc(h, bytes, 8u);
}

static void *x89_array_ul(modelrt_handle *h, size_t elem, unsigned long count)
{
    size_t bytes;
    size_t n;
    if (count == 0ul) return 0;
    n = (size_t)count;
    if ((unsigned long)n != count) return 0;
    if (elem != 0u && n > ((size_t)-1) / elem) return 0;
    bytes = elem * n;
    return modelrt_arena_alloc(h, bytes, 8u);
}

static void x89_fill_error(void *u, int line, int col, const char *msg)
{
    x89_state *s;
    (void)line;
    (void)col;
    s = (x89_state *)u;
    if (s == 0) return;
    s->parse_bad = 1;
    modelrt_copy_string(s->parse_error, sizeof(s->parse_error), msg);
}

static void x89_fill_object_begin(void *u, const char *type, const char *name)
{
    x89_state *s;
    (void)name;
    s = (x89_state *)u;
    if (s == 0) return;
    if (x89_str_ieq(type, "MeshMaterialList")) ++s->material_list_depth;
}

static void x89_fill_object_end(void *u, const char *type, const char *name)
{
    x89_state *s;
    (void)name;
    s = (x89_state *)u;
    if (s == 0) return;
    if (x89_str_ieq(type, "MeshMaterialList") && s->material_list_depth > 0)
        --s->material_list_depth;
    if (x89_str_ieq(type, "Mesh")) s->current_mesh = -1;
}

static void x89_fill_mesh_frame_node(void *u, const char *mesh_name,
                                     unsigned int owner_frame_id)
{
    x89_state *s;
    (void)mesh_name;
    s = (x89_state *)u;
    if (s == 0) return;
    if (owner_frame_id == XP_FRAME_ID_NONE) s->pending_mesh_frame = -1;
    else if (owner_frame_id <= 0x7fffffffu) s->pending_mesh_frame = (modelrt_i32)owner_frame_id;
    else s->pending_mesh_frame = -1;
}

static void x89_fill_mesh(void *u, const char *name, int nV, int nF)
{
    x89_state *s;
    x89_mesh *m;
    (void)nF;
    s = (x89_state *)u;
    if (s == 0 || nV < 0 || s->mesh_cursor >= s->count.mesh_count) {
        if (s != 0) s->parse_bad = 1;
        return;
    }
    m = &s->meshes[s->mesh_cursor];
    modelrt_zero(m, sizeof(*m));
    modelrt_copy_string(m->name, sizeof(m->name), name);
    m->first_vertex = s->vertex_cursor;
    m->vertex_count = (modelrt_u32)nV;
    m->first_face = s->face_cursor;
    m->frame_index = s->pending_mesh_frame;
    m->normal_base = s->normal_cursor;
    s->current_mesh = (modelrt_i32)s->mesh_cursor;
    s->vertex_cursor += (modelrt_u32)nV;
    ++s->mesh_cursor;
}

static void x89_fill_face_count(void *u, int nF)
{
    x89_state *s;
    x89_mesh *m;
    s = (x89_state *)u;
    if (s == 0 || s->current_mesh < 0 || nF < 0) return;
    m = &s->meshes[(modelrt_u32)s->current_mesh];
    m->face_count = (modelrt_u32)nF;
}

static void x89_fill_vertex(void *u, int i, xp_fx32 x, xp_fx32 y, xp_fx32 z)
{
    x89_state *s;
    x89_mesh *m;
    x89_vertex *v;
    s = (x89_state *)u;
    if (s == 0 || s->current_mesh < 0 || i < 0) return;
    m = &s->meshes[(modelrt_u32)s->current_mesh];
    if ((modelrt_u32)i >= m->vertex_count) { s->parse_bad = 1; return; }
    v = &s->vertices[m->first_vertex + (modelrt_u32)i];
    v->position.x = (modelrt_q16)x;
    v->position.y = (modelrt_q16)y;
    v->position.z = (modelrt_q16)z;
    v->rgba[0] = v->rgba[1] = v->rgba[2] = v->rgba[3] = 255u;
}

static void x89_fill_face(void *u, int i, int n, const int *idx)
{
    x89_state *s;
    x89_mesh *m;
    x89_face *f;
    int j;
    (void)i;
    s = (x89_state *)u;
    if (s == 0 || s->current_mesh < 0 || n < 0 || idx == 0) return;
    if (s->face_cursor >= s->count.face_count ||
        s->index_cursor + (modelrt_u32)n > s->count.face_index_count) {
        s->parse_bad = 1;
        return;
    }
    m = &s->meshes[(modelrt_u32)s->current_mesh];
    f = &s->faces[s->face_cursor];
    modelrt_zero(f, sizeof(*f));
    f->first_index = s->index_cursor;
    f->degree = (modelrt_u32)n;
    f->first_normal_index = 0u;
    f->normal_degree = 0u;
    f->material_slot = -1;
    for (j = 0; j < n; ++j) {
        if (idx[j] < 0) { s->parse_bad = 1; return; }
        s->indices[s->index_cursor++] = (modelrt_u32)idx[j];
    }
    if (n >= 3) m->triangle_count += (modelrt_u32)(n - 2);
    ++s->face_cursor;
}

static void x89_fill_normals_begin(void *u, int nNormals, int nFaceNormals)
{
    x89_state *s;
    x89_mesh *m;
    (void)nFaceNormals;
    s = (x89_state *)u;
    if (s == 0 || s->current_mesh < 0 || nNormals < 0) return;
    m = &s->meshes[(modelrt_u32)s->current_mesh];
    m->normal_base = s->normal_cursor;
    m->normal_count = (modelrt_u32)nNormals;
    if (s->normal_cursor + (modelrt_u32)nNormals > s->count.normal_count)
        s->parse_bad = 1;
    else
        s->normal_cursor += (modelrt_u32)nNormals;
}

static void x89_fill_normal(void *u, int i, xp_fx32 x, xp_fx32 y, xp_fx32 z)
{
    x89_state *s;
    x89_mesh *m;
    modelrt_vec3 *v;
    s = (x89_state *)u;
    if (s == 0 || s->current_mesh < 0 || i < 0) return;
    m = &s->meshes[(modelrt_u32)s->current_mesh];
    if ((modelrt_u32)i >= m->normal_count) { s->parse_bad = 1; return; }
    v = &s->normals[m->normal_base + (modelrt_u32)i];
    v->x = (modelrt_q16)x;
    v->y = (modelrt_q16)y;
    v->z = (modelrt_q16)z;
}

static void x89_fill_face_normals(void *u, int face_i, int n, const int *nidx)
{
    x89_state *s;
    x89_mesh *m;
    x89_face *f;
    int j;
    s = (x89_state *)u;
    if (s == 0 || s->current_mesh < 0 || face_i < 0 || n < 0 || nidx == 0) return;
    m = &s->meshes[(modelrt_u32)s->current_mesh];
    if ((modelrt_u32)face_i >= m->face_count ||
        s->normal_index_cursor + (modelrt_u32)n > s->count.normal_index_count) {
        s->parse_bad = 1;
        return;
    }
    f = &s->faces[m->first_face + (modelrt_u32)face_i];
    f->first_normal_index = s->normal_index_cursor;
    f->normal_degree = (modelrt_u32)n;
    for (j = 0; j < n; ++j) {
        if (nidx[j] < 0) { s->parse_bad = 1; return; }
        s->normal_indices[s->normal_index_cursor++] = (modelrt_u32)nidx[j];
    }
}

static void x89_fill_texcoords_begin(void *u, int nUV)
{
    x89_state *s;
    x89_mesh *m;
    s = (x89_state *)u;
    if (s == 0 || s->current_mesh < 0 || nUV < 0) return;
    m = &s->meshes[(modelrt_u32)s->current_mesh];
    if ((modelrt_u32)nUV > m->vertex_count) s->parse_bad = 1;
}

static void x89_fill_texcoord(void *u, int i, xp_fx32 u0, xp_fx32 v0)
{
    x89_state *s;
    x89_mesh *m;
    x89_vertex *v;
    s = (x89_state *)u;
    if (s == 0 || s->current_mesh < 0 || i < 0) return;
    m = &s->meshes[(modelrt_u32)s->current_mesh];
    if ((modelrt_u32)i >= m->vertex_count) { s->parse_bad = 1; return; }
    {
        xp_fx32 gu;
        xp_fx32 gv;
        v = &s->vertices[m->first_vertex + (modelrt_u32)i];
        xp_uv_dx_to_gl(u0, v0, &gu, &gv);
        v->uv.x = (modelrt_q16)gu;
        v->uv.y = (modelrt_q16)gv;
        v->flags |= MODELRT_VTX_UV0;
    }
}

static void x89_fill_color(void *u, int vertex_index, xp_fx32 r, xp_fx32 g, xp_fx32 b, xp_fx32 a)
{
    x89_state *s;
    x89_mesh *m;
    x89_vertex *v;
    s = (x89_state *)u;
    if (s == 0 || s->current_mesh < 0 || vertex_index < 0) return;
    m = &s->meshes[(modelrt_u32)s->current_mesh];
    if ((modelrt_u32)vertex_index >= m->vertex_count) { s->parse_bad = 1; return; }
    v = &s->vertices[m->first_vertex + (modelrt_u32)vertex_index];
    v->rgba[0] = x89_color_u8((modelrt_q16)r);
    v->rgba[1] = x89_color_u8((modelrt_q16)g);
    v->rgba[2] = x89_color_u8((modelrt_q16)b);
    v->rgba[3] = x89_color_u8((modelrt_q16)a);
    v->flags |= MODELRT_VTX_COLOR;
}

static void x89_fill_matlist(void *u, int nMaterials, int nFaceIndices)
{
    x89_state *s;
    x89_mesh *m;
    modelrt_u32 i;
    (void)nFaceIndices;
    s = (x89_state *)u;
    if (s == 0 || s->current_mesh < 0 || nMaterials < 0) return;
    m = &s->meshes[(modelrt_u32)s->current_mesh];
    if (s->material_slot_cursor + (modelrt_u32)nMaterials > s->count.material_slots) {
        s->parse_bad = 1;
        return;
    }
    m->material_slot_base = s->material_slot_cursor;
    m->material_slot_count = (modelrt_u32)nMaterials;
    s->current_slot_next = 0u;
    for (i = 0u; i < (modelrt_u32)nMaterials; ++i) {
        s->material_slots[s->material_slot_cursor + i].material_index = -1;
        s->material_slots[s->material_slot_cursor + i].ref_name[0] = '\0';
    }
    s->material_slot_cursor += (modelrt_u32)nMaterials;
}

static void x89_fill_face_material(void *u, int face_i, int material_index)
{
    x89_state *s;
    x89_mesh *m;
    s = (x89_state *)u;
    if (s == 0 || s->current_mesh < 0 || face_i < 0) return;
    m = &s->meshes[(modelrt_u32)s->current_mesh];
    if ((modelrt_u32)face_i >= m->face_count) { s->parse_bad = 1; return; }
    s->faces[m->first_face + (modelrt_u32)face_i].material_slot = (modelrt_i32)material_index;
}

static void x89_default_material(x89_material *m)
{
    modelrt_zero(m, sizeof(*m));
    m->rgba[0] = m->rgba[1] = m->rgba[2] = m->rgba[3] = 255u;
    m->texture_index = -1;
}

static void x89_fill_material_begin(void *u, const char *name)
{
    x89_state *s;
    x89_material *m;
    x89_material_slot *slot;
    s = (x89_state *)u;
    if (s == 0 || s->material_cursor >= s->material_capacity) {
        if (s != 0) s->parse_bad = 1;
        return;
    }
    m = &s->materials[s->material_cursor];
    x89_default_material(m);
    modelrt_copy_string(m->name, sizeof(m->name), name);
    s->current_material = (modelrt_i32)s->material_cursor;
    if (s->material_list_depth > 0 && s->current_mesh >= 0) {
        x89_mesh *mesh;
        mesh = &s->meshes[(modelrt_u32)s->current_mesh];
        if (s->current_slot_next < mesh->material_slot_count) {
            slot = &s->material_slots[mesh->material_slot_base + s->current_slot_next];
            slot->material_index = (modelrt_i32)s->material_cursor;
            ++s->current_slot_next;
        }
    }
    ++s->material_cursor;
}

static void x89_fill_material_color(void *u, xp_fx32 r, xp_fx32 g, xp_fx32 b, xp_fx32 a)
{
    x89_state *s;
    x89_material *m;
    s = (x89_state *)u;
    if (s == 0 || s->current_material < 0) return;
    m = &s->materials[(modelrt_u32)s->current_material];
    m->rgba[0] = x89_color_u8((modelrt_q16)r);
    m->rgba[1] = x89_color_u8((modelrt_q16)g);
    m->rgba[2] = x89_color_u8((modelrt_q16)b);
    m->rgba[3] = x89_color_u8((modelrt_q16)a);
}

static void x89_fill_material_power(void *u, xp_fx32 power)
{
    x89_state *s;
    s = (x89_state *)u;
    if (s != 0 && s->current_material >= 0)
        s->materials[(modelrt_u32)s->current_material].power = (modelrt_q16)power;
}

static void x89_fill_material_specular(void *u, xp_fx32 r, xp_fx32 g, xp_fx32 b)
{
    x89_state *s;
    x89_material *m;
    s = (x89_state *)u;
    if (s == 0 || s->current_material < 0) return;
    m = &s->materials[(modelrt_u32)s->current_material];
    m->specular.x = (modelrt_q16)r;
    m->specular.y = (modelrt_q16)g;
    m->specular.z = (modelrt_q16)b;
}

static void x89_fill_material_emissive(void *u, xp_fx32 r, xp_fx32 g, xp_fx32 b)
{
    x89_state *s;
    x89_material *m;
    s = (x89_state *)u;
    if (s == 0 || s->current_material < 0) return;
    m = &s->materials[(modelrt_u32)s->current_material];
    m->emissive.x = (modelrt_q16)r;
    m->emissive.y = (modelrt_q16)g;
    m->emissive.z = (modelrt_q16)b;
}

static void x89_fill_texture(void *u, const char *filename)
{
    x89_state *s;
    x89_texture *t;
    s = (x89_state *)u;
    if (s == 0 || s->texture_cursor >= s->count.texture_count) {
        if (s != 0) s->parse_bad = 1;
        return;
    }
    t = &s->textures[s->texture_cursor];
    modelrt_copy_string(t->path, sizeof(t->path), filename);
    if (s->current_material >= 0)
        s->materials[(modelrt_u32)s->current_material].texture_index = (modelrt_i32)s->texture_cursor;
    ++s->texture_cursor;
}

static void x89_fill_material_end(void *u, const char *name)
{
    x89_state *s;
    (void)name;
    s = (x89_state *)u;
    if (s != 0) s->current_material = -1;
}

static void x89_fill_reference(void *u, const char *name)
{
    x89_state *s;
    x89_mesh *mesh;
    x89_material_slot *slot;
    s = (x89_state *)u;
    if (s == 0 || s->material_list_depth <= 0 || s->current_mesh < 0) return;
    mesh = &s->meshes[(modelrt_u32)s->current_mesh];
    if (s->current_slot_next >= mesh->material_slot_count) return;
    slot = &s->material_slots[mesh->material_slot_base + s->current_slot_next];
    slot->material_index = -1;
    modelrt_copy_string(slot->ref_name, sizeof(slot->ref_name), name);
    ++s->current_slot_next;
}

static void x89_fill_frame_node_begin(void *u, unsigned int frame_id,
                                      unsigned int parent_frame_id, const char *name,
                                      unsigned int frame_depth)
{
    x89_state *s;
    x89_frame *f;
    (void)frame_depth;
    s = (x89_state *)u;
    if (s == 0 || frame_id >= s->count.frame_count) {
        if (s != 0) s->parse_bad = 1;
        return;
    }
    f = &s->frames[frame_id];
    modelrt_zero(f, sizeof(*f));
    f->node_id = (modelrt_u32)frame_id;
    modelrt_copy_string(f->name, sizeof(f->name), name);
    if (parent_frame_id == XP_FRAME_ID_NONE) f->parent = -1;
    else if (parent_frame_id < s->count.frame_count) f->parent = (modelrt_i32)parent_frame_id;
    else { f->parent = -1; s->parse_bad = 1; }
    x89_mat_identity(f->local);
    x89_mat_identity(f->world);
    if (frame_id + 1u > s->frame_cursor) s->frame_cursor = frame_id + 1u;
}

static void x89_fill_frame_node_transform(void *u, unsigned int frame_id,
                                          const xp_fx32 m16[16])
{
    x89_state *s;
    x89_frame *f;
    int i;
    s = (x89_state *)u;
    if (s == 0 || m16 == 0 || frame_id >= s->count.frame_count) return;
    f = &s->frames[frame_id];
    for (i = 0; i < 16; ++i) f->local[i] = (modelrt_q16)m16[i];
}

static void x89_fill_frame_node_end(void *u, unsigned int frame_id)
{
    (void)u;
    (void)frame_id;
}

static void x89_fill_skin_begin(void *u, const char *bone_name, int nWeights)
{
    x89_state *s;
    x89_cluster *c;
    s = (x89_state *)u;
    if (s == 0 || s->current_mesh < 0 || nWeights < 0 ||
        s->cluster_cursor >= s->count.cluster_count ||
        (modelrt_u32)nWeights > s->count.max_cluster_weights) {
        if (s != 0) s->parse_bad = 1;
        return;
    }
    c = &s->clusters[s->cluster_cursor];
    modelrt_zero(c, sizeof(*c));
    modelrt_copy_string(c->bone_name, sizeof(c->bone_name), bone_name);
    c->frame_index = -1;
    c->mesh_index = (modelrt_u32)s->current_mesh;
    x89_mat_identity(c->offset);
    s->current_cluster = (modelrt_i32)s->cluster_cursor;
    ++s->cluster_cursor;
}

static void x89_fill_skin_index(void *u, int i, int vertex_index)
{
    x89_state *s;
    s = (x89_state *)u;
    if (s == 0 || i < 0 || vertex_index < 0 ||
        (modelrt_u32)i >= s->count.max_cluster_weights) return;
    s->skin_index_scratch[(modelrt_u32)i] = (modelrt_u32)vertex_index;
}

static void x89_fill_skin_value(void *u, int i, xp_fx32 weight)
{
    x89_state *s;
    x89_cluster *c;
    x89_mesh *m;
    x89_vertex *v;
    modelrt_u32 local;
    modelrt_u32 k;
    s = (x89_state *)u;
    if (s == 0 || s->current_cluster < 0 || i < 0 ||
        (modelrt_u32)i >= s->count.max_cluster_weights) return;
    c = &s->clusters[(modelrt_u32)s->current_cluster];
    if (c->mesh_index >= s->count.mesh_count) { s->parse_bad = 1; return; }
    m = &s->meshes[c->mesh_index];
    local = s->skin_index_scratch[(modelrt_u32)i];
    if (local >= m->vertex_count) { s->parse_bad = 1; return; }
    v = &s->vertices[m->first_vertex + local];
    k = (modelrt_u32)v->weight_count;
    if (k < 4u) {
        v->cluster[k] = s->current_cluster;
        v->weight[k] = (modelrt_q16)weight;
        v->weight_count = (modelrt_u8)(k + 1u);
    }
}

static void x89_fill_skin_offset(void *u, const xp_fx32 m16[16])
{
    x89_state *s;
    x89_cluster *c;
    int i;
    s = (x89_state *)u;
    if (s == 0 || s->current_cluster < 0 || m16 == 0) return;
    c = &s->clusters[(modelrt_u32)s->current_cluster];
    for (i = 0; i < 16; ++i) c->offset[i] = (modelrt_q16)m16[i];
}

static void x89_fill_skin_end(void *u, const char *bone_name)
{
    x89_state *s;
    (void)bone_name;
    s = (x89_state *)u;
    if (s != 0) s->current_cluster = -1;
}

static void x89_fill_anim_set_begin(void *u, const char *name)
{
    x89_state *s;
    x89_anim_set *a;
    s = (x89_state *)u;
    if (s == 0 || s->animation_cursor >= s->count.animation_sets) {
        if (s != 0) s->parse_bad = 1;
        return;
    }
    a = &s->animations[s->animation_cursor];
    modelrt_zero(a, sizeof(*a));
    modelrt_copy_string(a->name, sizeof(a->name), name);
    a->min_tick = INT_MAX;
    a->max_tick = 0;
    s->current_animation = (modelrt_i32)s->animation_cursor;
    ++s->animation_cursor;
}

static void x89_fill_anim_key(void *u, int key_type, int key_index, int time,
                              int nValues, const xp_fx32 *values)
{
    x89_state *s;
    x89_anim_set *a;
    (void)key_type;
    (void)key_index;
    (void)nValues;
    (void)values;
    s = (x89_state *)u;
    if (s == 0 || s->current_animation < 0) return;
    a = &s->animations[(modelrt_u32)s->current_animation];
    if (time < a->min_tick) a->min_tick = time;
    if (time > a->max_tick) a->max_tick = time;
}

static void x89_fill_anim_summary(void *u, const char *name, unsigned int track_count,
                                  int first_tick, int last_tick,
                                  unsigned int ticks_per_second,
                                  int ticks_per_second_known)
{
    x89_state *s;
    x89_anim_set *a;
    (void)name;
    (void)track_count;
    s = (x89_state *)u;
    if (s == 0 || s->current_animation < 0) return;
    a = &s->animations[(modelrt_u32)s->current_animation];
    a->min_tick = first_tick;
    a->max_tick = last_tick;
    if (ticks_per_second_known && ticks_per_second != 0u)
        s->ticks_per_second = (modelrt_u32)ticks_per_second;
}

static void x89_fill_anim_set_end(void *u, const char *name)
{
    x89_state *s;
    (void)name;
    s = (x89_state *)u;
    if (s != 0) s->current_animation = -1;
}

static void x89_fill_ticks(void *u, unsigned int ticks)
{
    x89_state *s;
    s = (x89_state *)u;
    if (s != 0) s->ticks_per_second = (modelrt_u32)ticks;
}

static int x89_find_material(const x89_state *s, const char *name)
{
    modelrt_u32 i;
    if (name == 0 || name[0] == '\0') return -1;
    for (i = 0u; i < s->material_count; ++i) {
        if (x89_str_ieq(s->materials[i].name, name)) return (int)i;
    }
    return -1;
}

static int x89_find_frame(const x89_state *s, const char *name)
{
    modelrt_u32 i;
    int found;
    if (name == 0 || name[0] == '\0') return -1;
    found = -1;
    for (i = 0u; i < s->count.frame_count; ++i) {
        if (x89_str_ieq(s->frames[i].name, name)) {
            if (found >= 0) return -1; /* duplicate-name target is ambiguous */
            found = (int)i;
        }
    }
    return found;
}

static void x89_finalize(x89_state *s)
{
    modelrt_u32 i;
    x89_frame *f;
    x89_material_slot *slot;
    int found;
    s->material_count = s->material_cursor;
    s->texture_count = s->texture_cursor;
    s->animation_count = s->animation_cursor;
    if (s->ticks_per_second == 0u) s->ticks_per_second = s->count.ticks_per_second;

    for (i = 0u; i < s->count.frame_count; ++i) {
        f = &s->frames[i];
        if (f->parent >= 0 && (modelrt_u32)f->parent < i)
            x89_mat_mul(f->local, s->frames[(modelrt_u32)f->parent].world, f->world);
        else
            memcpy(f->world, f->local, sizeof(f->world));
    }

    for (i = 0u; i < s->count.cluster_count; ++i)
        s->clusters[i].frame_index = (modelrt_i32)x89_find_frame(s, s->clusters[i].bone_name);

    for (i = 0u; i < s->count.material_slots; ++i) {
        slot = &s->material_slots[i];
        if (slot->material_index < 0 && slot->ref_name[0] != '\0') {
            found = x89_find_material(s, slot->ref_name);
            if (found >= 0) slot->material_index = (modelrt_i32)found;
            else if (s->material_count < s->material_capacity) {
                x89_default_material(&s->materials[s->material_count]);
                modelrt_copy_string(s->materials[s->material_count].name,
                                    sizeof(s->materials[s->material_count].name), slot->ref_name);
                slot->material_index = (modelrt_i32)s->material_count;
                ++s->material_count;
            }
        }
    }
}

static int x89_fill_file(const char *path, x89_state *s)
{
    xp_callbacks cb;
    int rc;
    modelrt_zero(&cb, sizeof(cb));
    s->current_mesh = -1;
    s->current_material = -1;
    s->current_cluster = -1;
    s->current_animation = -1;
    s->pending_mesh_frame = -1;
    cb.on_begin_object = x89_fill_object_begin;
    cb.on_end_object = x89_fill_object_end;
    cb.on_data_reference = x89_fill_reference;
    cb.on_mesh_frame_node_binding = x89_fill_mesh_frame_node;
    cb.on_mesh_begin = x89_fill_mesh;
    cb.on_mesh_face_count = x89_fill_face_count;
    cb.on_vertex = x89_fill_vertex;
    cb.on_face = x89_fill_face;
    cb.on_mesh_normals_begin = x89_fill_normals_begin;
    cb.on_normal = x89_fill_normal;
    cb.on_face_normals = x89_fill_face_normals;
    cb.on_mesh_texcoords_begin = x89_fill_texcoords_begin;
    cb.on_texcoord = x89_fill_texcoord;
    cb.on_vertex_color = x89_fill_color;
    cb.on_mesh_material_list_begin = x89_fill_matlist;
    cb.on_face_material = x89_fill_face_material;
    cb.on_material_begin = x89_fill_material_begin;
    cb.on_material_color = x89_fill_material_color;
    cb.on_material_power = x89_fill_material_power;
    cb.on_material_specular = x89_fill_material_specular;
    cb.on_material_emissive = x89_fill_material_emissive;
    cb.on_material_texture_filename = x89_fill_texture;
    cb.on_material_end = x89_fill_material_end;
    cb.on_frame_node_begin = x89_fill_frame_node_begin;
    cb.on_frame_node_transform = x89_fill_frame_node_transform;
    cb.on_frame_node_end = x89_fill_frame_node_end;
    cb.on_skin_weights_begin = x89_fill_skin_begin;
    cb.on_skin_weight_index = x89_fill_skin_index;
    cb.on_skin_weight_value = x89_fill_skin_value;
    cb.on_skin_weights_offset_matrix = x89_fill_skin_offset;
    cb.on_skin_weights_end = x89_fill_skin_end;
    cb.on_animation_set_begin = x89_fill_anim_set_begin;
    cb.on_animation_set_summary = x89_fill_anim_summary;
    cb.on_animation_set_end = x89_fill_anim_set_end;
    cb.on_animation_key = x89_fill_anim_key;
    cb.on_anim_ticks_per_second = x89_fill_ticks;
    cb.on_error = x89_fill_error;
    rc = x89_parse_file(path, &cb, s, s->parse_error, sizeof(s->parse_error));
    if (rc != MODELRT_OK) return rc;
    if (s->parse_bad) return MODELRT_ERR_LIMIT;
    x89_finalize(s);
    return MODELRT_OK;
}

static int x89_fill_animation_file(const char *path, x89_state *s)
{
    xp_callbacks cb;
    xp_result xr;
    int rc;
    if (s == 0) return MODELRT_ERR_BAD_ARG;
    xp_anim_capture_init(&s->anim_capture,
                         s->anim_nodes, s->anim_req.nodes,
                         s->anim_clips, s->anim_req.clips,
                         s->anim_tracks, s->anim_req.tracks,
                         s->anim_keys, s->anim_req.keys);
    xp_anim_capture_callbacks(&cb);
    s->parse_error[0] = '\0';
    rc = x89_parse_file(path, &cb, &s->anim_capture, s->parse_error, sizeof(s->parse_error));
    if (rc != MODELRT_OK) return rc;
    xr = xp_anim_capture_finalize(&s->anim_capture);
    if (xr == XP_OK) {
        if (s->anim_req.clips > 0xfffffffful) return MODELRT_ERR_LIMIT;
        s->animation_count = (modelrt_u32)s->anim_req.clips;
        return MODELRT_OK;
    }
    if (xr == XP_ELIMIT) return MODELRT_ERR_LIMIT;
    if (xr == XP_ENOTSUP) return MODELRT_ERR_UNSUPPORTED;
    if (xr == XP_EFORMAT) return MODELRT_ERR_FORMAT;
    return MODELRT_ERR_CODEC;
}

static int x89_probe(const char *path, const modelrt_u8 *head, size_t head_size)
{
    if (!modelrt_path_has_suffix(path, ".x")) return MODELRT_PROBE_NO;
    if (head != 0 && head_size >= 4u && head[0] == 'x' && head[1] == 'o' &&
        head[2] == 'f' && head[3] == ' ') return MODELRT_PROBE_YES;
    return MODELRT_PROBE_MAYBE;
}

static int x89_open(modelrt_runtime *rt, modelrt_handle *handle, const char *path)
{
    x89_state *s;
    x89_counts c;
    xp_anim_requirements areq;
    modelrt_u32 material_cap;
    int rc;
    modelrt_zero(&areq, sizeof(areq));
    rc = x89_count_file(path, &c);
    if (rc != MODELRT_OK) {
        modelrt_set_error(rt, "DirectX .x", rc, c.error[0] ? c.error : "count pass failed");
        return rc;
    }
    rc = x89_anim_requirements_file(path, &areq, c.error, sizeof(c.error));
    if (rc != MODELRT_OK) {
        modelrt_set_error(rt, "DirectX .x", rc, c.error[0] ? c.error : "animation inventory pass failed");
        return rc;
    }
    s = (x89_state *)modelrt_arena_alloc(handle, sizeof(*s), 8u);
    if (s == 0) return MODELRT_ERR_NO_SPACE;
    modelrt_zero(s, sizeof(*s));
    s->count = c;
    s->anim_req = areq;
    material_cap = c.material_defs + c.material_slots;
    if (material_cap < c.material_defs) return MODELRT_ERR_LIMIT;
    s->material_capacity = material_cap;
    s->meshes = (x89_mesh *)x89_array(handle, sizeof(x89_mesh), c.mesh_count);
    s->vertices = (x89_vertex *)x89_array(handle, sizeof(x89_vertex), c.vertex_count);
    s->faces = (x89_face *)x89_array(handle, sizeof(x89_face), c.face_count);
    s->indices = (modelrt_u32 *)x89_array(handle, sizeof(modelrt_u32), c.face_index_count);
    s->normals = (modelrt_vec3 *)x89_array(handle, sizeof(modelrt_vec3), c.normal_count);
    s->normal_indices = (modelrt_u32 *)x89_array(handle, sizeof(modelrt_u32), c.normal_index_count);
    s->materials = (x89_material *)x89_array(handle, sizeof(x89_material), material_cap);
    s->textures = (x89_texture *)x89_array(handle, sizeof(x89_texture), c.texture_count);
    s->material_slots = (x89_material_slot *)x89_array(handle, sizeof(x89_material_slot), c.material_slots);
    s->frames = (x89_frame *)x89_array(handle, sizeof(x89_frame), c.frame_count);
    s->clusters = (x89_cluster *)x89_array(handle, sizeof(x89_cluster), c.cluster_count);
    s->skin_index_scratch = (modelrt_u32 *)x89_array(handle, sizeof(modelrt_u32), c.max_cluster_weights);
    s->animations = (x89_anim_set *)x89_array(handle, sizeof(x89_anim_set), c.animation_sets);
    s->anim_nodes = (xp_anim_node_record *)x89_array_ul(handle, sizeof(xp_anim_node_record), areq.nodes);
    s->anim_clips = (xp_anim_clip_record *)x89_array_ul(handle, sizeof(xp_anim_clip_record), areq.clips);
    s->anim_tracks = (xp_anim_track_record *)x89_array_ul(handle, sizeof(xp_anim_track_record), areq.tracks);
    s->anim_keys = (xp_anim_key_record *)x89_array_ul(handle, sizeof(xp_anim_key_record), areq.keys);
    s->anim_pose_scratch = (xp_anim_local_pose *)x89_array_ul(handle, sizeof(xp_anim_local_pose), areq.nodes);
    s->anim_world = (modelrt_q16 *)x89_array(handle, sizeof(modelrt_q16) * 16u, c.frame_count);
    s->anim_world_state = (modelrt_u8 *)x89_array(handle, sizeof(modelrt_u8), c.frame_count);
    if ((c.mesh_count && s->meshes == 0) || (c.vertex_count && s->vertices == 0) ||
        (c.face_count && s->faces == 0) || (c.face_index_count && s->indices == 0) ||
        (c.normal_count && s->normals == 0) || (c.normal_index_count && s->normal_indices == 0) ||
        (material_cap && s->materials == 0) || (c.texture_count && s->textures == 0) ||
        (c.material_slots && s->material_slots == 0) || (c.frame_count && s->frames == 0) ||
        (c.cluster_count && s->clusters == 0) ||
        (c.max_cluster_weights && s->skin_index_scratch == 0) ||
        (c.animation_sets && s->animations == 0) ||
        (areq.nodes && (s->anim_nodes == 0 || s->anim_pose_scratch == 0)) ||
        (areq.clips && s->anim_clips == 0) || (areq.tracks && s->anim_tracks == 0) ||
        (areq.keys && s->anim_keys == 0) ||
        (c.frame_count && (s->anim_world == 0 || s->anim_world_state == 0)))
        return MODELRT_ERR_NO_SPACE;

    if (c.vertex_count) modelrt_zero(s->vertices, sizeof(x89_vertex) * (size_t)c.vertex_count);
    if (c.face_count) modelrt_zero(s->faces, sizeof(x89_face) * (size_t)c.face_count);
    if (material_cap) modelrt_zero(s->materials, sizeof(x89_material) * (size_t)material_cap);
    if (c.material_slots) modelrt_zero(s->material_slots, sizeof(x89_material_slot) * (size_t)c.material_slots);
    rc = x89_fill_file(path, s);
    if (rc != MODELRT_OK) {
        modelrt_set_error(rt, "DirectX .x", rc, s->parse_error[0] ? s->parse_error : "fill pass failed");
        return rc;
    }
    rc = x89_fill_animation_file(path, s);
    if (rc != MODELRT_OK) {
        modelrt_set_error(rt, "DirectX .x", rc, s->parse_error[0] ? s->parse_error : "animation capture pass failed");
        return rc;
    }

    handle->native_state = s;
    modelrt_copy_string(handle->info.name, sizeof(handle->info.name), path);
    handle->info.mesh_count = c.mesh_count;
    handle->info.material_count = s->material_count;
    handle->info.texture_count = s->texture_count;
    handle->info.animation_count = s->animation_count;
    handle->info.capabilities = MODELRT_CAP_NATIVE_SCENE;
    if (c.mesh_count != 0u) handle->info.capabilities |= MODELRT_CAP_GEOMETRY;
    if (c.normal_count != 0u) handle->info.capabilities |= MODELRT_CAP_NORMALS;
    if (s->material_count != 0u) handle->info.capabilities |= MODELRT_CAP_MATERIALS;
    if (s->texture_count != 0u) handle->info.capabilities |= MODELRT_CAP_TEXTURES | MODELRT_CAP_EXTERNAL_TEX;
    if (c.uv_count != 0u) handle->info.capabilities |= MODELRT_CAP_UV0;
    if (c.color_count != 0u) handle->info.capabilities |= MODELRT_CAP_VERTEX_COLOR;
    if (c.cluster_count != 0u && c.frame_count != 0u) {
        handle->info.capabilities |= MODELRT_CAP_SKELETON | MODELRT_CAP_SKINNING;
        handle->info.bone_count = c.frame_count;
    } else if (c.animation_sets != 0u && c.frame_count != 0u) {
        handle->info.capabilities |= MODELRT_CAP_SKELETON;
        handle->info.bone_count = c.frame_count;
    }
    if (s->animation_count != 0u) handle->info.capabilities |= MODELRT_CAP_ANIMATION;
    return MODELRT_OK;
}

static void x89_close(modelrt_handle *handle)
{
    (void)handle;
}

static modelrt_i32 x89_face_material(const x89_state *s, const x89_mesh *m, const x89_face *f)
{
    modelrt_u32 slot_index;
    if (f->material_slot < 0 || (modelrt_u32)f->material_slot >= m->material_slot_count) return -1;
    slot_index = m->material_slot_base + (modelrt_u32)f->material_slot;
    if (slot_index >= s->count.material_slots) return -1;
    return s->material_slots[slot_index].material_index;
}

static const modelrt_q16 *x89_frame_world(const x89_state *s, modelrt_u32 frame_index)
{
    if (s == 0 || frame_index >= s->count.frame_count) return 0;
    if (s->anim_pose_active && s->anim_world != 0)
        return s->anim_world + ((size_t)frame_index * 16u);
    return s->frames[frame_index].world;
}

static const xp_anim_local_pose *x89_pose_for_frame_id(const x89_state *s,
                                                        modelrt_u32 frame_id)
{
    unsigned long i;
    if (s == 0 || s->anim_pose_scratch == 0) return 0;
    for (i = 0ul; i < s->anim_req.nodes; ++i) {
        if (s->anim_pose_scratch[i].frame_id == (unsigned int)frame_id)
            return &s->anim_pose_scratch[i];
    }
    return 0;
}

static int x89_build_anim_world_frame(x89_state *s, modelrt_u32 frame_index,
                                      modelrt_u32 depth)
{
    const xp_anim_local_pose *pose;
    const modelrt_q16 *local;
    modelrt_q16 *dst;
    modelrt_i32 parent;
    int i;
    if (s == 0 || frame_index >= s->count.frame_count || s->anim_world == 0 ||
        s->anim_world_state == 0) return MODELRT_ERR_BAD_ARG;
    if (depth > s->count.frame_count) return MODELRT_ERR_FORMAT;
    if (s->anim_world_state[frame_index] == 2u) return MODELRT_OK;
    if (s->anim_world_state[frame_index] == 1u) return MODELRT_ERR_FORMAT;
    s->anim_world_state[frame_index] = 1u;
    pose = x89_pose_for_frame_id(s, frame_index);
    local = pose != 0 ? (const modelrt_q16 *)pose->local_matrix : s->frames[frame_index].local;
    dst = s->anim_world + ((size_t)frame_index * 16u);
    parent = s->frames[frame_index].parent;
    if (parent >= 0) {
        int rc;
        const modelrt_q16 *pw;
        if ((modelrt_u32)parent >= s->count.frame_count) return MODELRT_ERR_FORMAT;
        rc = x89_build_anim_world_frame(s, (modelrt_u32)parent, depth + 1u);
        if (rc != MODELRT_OK) return rc;
        pw = s->anim_world + ((size_t)(modelrt_u32)parent * 16u);
        x89_mat_mul(local, pw, dst);
    } else {
        for (i = 0; i < 16; ++i) dst[i] = local[i];
    }
    s->anim_world_state[frame_index] = 2u;
    return MODELRT_OK;
}

static int x89_clip_is_resolved(const x89_state *s, const xp_anim_clip_record *clip)
{
    unsigned long i;
    unsigned long end;
    if (s == 0 || clip == 0) return 0;
    end = clip->first_track + clip->track_count;
    if (end < clip->first_track || end > s->anim_req.tracks) return 0;
    for (i = clip->first_track; i < end; ++i) {
        if ((s->anim_tracks[i].flags &
             (XP_ANIM_TRACK_TARGET_UNRESOLVED | XP_ANIM_TRACK_TARGET_AMBIGUOUS)) != 0u)
            return 0;
    }
    return 1;
}

static int x89_prepare_animation_pose_frame(x89_state *s, int animation_index,
                                            modelrt_u32 frame_index)
{
    const xp_anim_clip_record *clip;
    xp_result xr;
    modelrt_u32 i;
    int tick;
    if (s == 0) return MODELRT_ERR_BAD_ARG;
    s->anim_pose_active = 0;
    if (animation_index < 0) return MODELRT_OK;
    if ((unsigned long)animation_index >= s->anim_req.clips || s->anim_clips == 0)
        return MODELRT_ERR_BAD_ARG;
    clip = &s->anim_clips[(unsigned long)animation_index];
    if (clip->frame_count == 0ul || !x89_clip_is_resolved(s, clip)) return MODELRT_OK;
    if ((unsigned long)frame_index >= clip->frame_count) return MODELRT_ERR_BAD_ARG;

    xr = xp_anim_sample_pose_frame(&s->anim_capture, (unsigned long)animation_index,
                                   (unsigned long)frame_index, s->anim_pose_scratch,
                                   s->anim_req.nodes, &tick);
    (void)tick;
    if (xr == XP_ENOTSUP) return MODELRT_OK;
    if (xr == XP_ELIMIT) return MODELRT_ERR_LIMIT;
    if (xr != XP_OK) return MODELRT_ERR_FORMAT;
    if (s->anim_world_state != 0 && s->count.frame_count != 0u)
        modelrt_zero(s->anim_world_state, (size_t)s->count.frame_count * sizeof(modelrt_u8));
    for (i = 0u; i < s->count.frame_count; ++i) {
        int rc;
        rc = x89_build_anim_world_frame(s, i, 0u);
        if (rc != MODELRT_OK) return rc;
    }
    s->anim_pose_active = 1;
    return MODELRT_OK;
}

static int x89_prepare_animation_pose(x89_state *s, int animation_index,
                                      modelrt_q16 time)
{
    const xp_anim_clip_record *clip;
    unsigned long frame_index;
    if (s == 0) return MODELRT_ERR_BAD_ARG;
    s->anim_pose_active = 0;
    if (animation_index < 0) return MODELRT_OK;
    if ((unsigned long)animation_index >= s->anim_req.clips || s->anim_clips == 0)
        return MODELRT_ERR_BAD_ARG;
    clip = &s->anim_clips[(unsigned long)animation_index];
    if (clip->frame_count == 0ul || !x89_clip_is_resolved(s, clip)) return MODELRT_OK;

    frame_index = 0ul;
    if (clip->ticks_per_second_known && clip->ticks_per_second != 0u &&
        clip->ticks_per_second <= 32767u) {
        modelrt_q16 tq;
        modelrt_i32 current_tick;
        tq = modelrt_q16_mul(time,
                             modelrt_q16_from_int((modelrt_i32)clip->ticks_per_second));
        current_tick = modelrt_q16_to_int(tq);
        if (current_tick <= clip->first_tick) frame_index = 0ul;
        else if (current_tick >= clip->last_tick) frame_index = clip->frame_count - 1ul;
        else frame_index = (unsigned long)(current_tick - clip->first_tick);
    }
    return x89_prepare_animation_pose_frame(s, animation_index, (modelrt_u32)frame_index);
}

static void x89_emit_vertex(const x89_state *s, const x89_mesh *m, const x89_face *f,
                            modelrt_u32 corner, modelrt_u32 vertex_index,
                            modelrt_vertex_view *out)
{
    const x89_vertex *v;
    const modelrt_q16 *owner_world;
    modelrt_vec3 n;
    modelrt_u32 ni;
    modelrt_u32 k;
    modelrt_u32 valid_skin;
    modelrt_vec3 skin_pos;
    modelrt_vec3 skin_nrm;
    modelrt_zero(out, sizeof(*out));
    if (vertex_index >= m->vertex_count) return;
    v = &s->vertices[m->first_vertex + vertex_index];
    owner_world = 0;
    if (m->frame_index >= 0 && (modelrt_u32)m->frame_index < s->count.frame_count)
        owner_world = x89_frame_world(s, (modelrt_u32)m->frame_index);

    n.x = n.y = n.z = 0;
    if (f->normal_degree == f->degree && corner < f->normal_degree) {
        ni = s->normal_indices[f->first_normal_index + corner];
        if (ni < m->normal_count) n = s->normals[m->normal_base + ni];
    } else if (vertex_index < m->normal_count) {
        n = s->normals[m->normal_base + vertex_index];
    }

    /* v0.5 runtime rule: for a skinned vertex use
       matrixOffset(inverse bind) * boneCombined for every influence and do
       NOT apply the mesh owner Frame a second time afterwards. */
    valid_skin = 0u;
    skin_pos.x = skin_pos.y = skin_pos.z = 0;
    skin_nrm.x = skin_nrm.y = skin_nrm.z = 0;
    for (k = 0u; k < (modelrt_u32)v->weight_count && k < 4u; ++k) {
        modelrt_i32 ci;
        const x89_cluster *cl;
        modelrt_q16 final_m[16];
        modelrt_vec3 tp;
        modelrt_vec3 tn;
        ci = v->cluster[k];
        out->weight[k] = v->weight[k];
        out->bone[k] = (modelrt_i16)-1;
        if (ci < 0 || (modelrt_u32)ci >= s->count.cluster_count) continue;
        cl = &s->clusters[(modelrt_u32)ci];
        if (cl->frame_index < 0 || (modelrt_u32)cl->frame_index >= s->count.frame_count) continue;
        out->bone[k] = (modelrt_i16)cl->frame_index;
        {
            const modelrt_q16 *bone_world;
            bone_world = x89_frame_world(s, (modelrt_u32)cl->frame_index);
            if (bone_world == 0) continue;
            x89_mat_mul(cl->offset, bone_world, final_m);
        }
        tp = x89_transform_point(final_m, v->position);
        skin_pos.x = x89_sat_add(skin_pos.x, modelrt_q16_mul(tp.x, v->weight[k]));
        skin_pos.y = x89_sat_add(skin_pos.y, modelrt_q16_mul(tp.y, v->weight[k]));
        skin_pos.z = x89_sat_add(skin_pos.z, modelrt_q16_mul(tp.z, v->weight[k]));
        if (n.x != 0 || n.y != 0 || n.z != 0) {
            tn = x89_transform_vector(final_m, n);
            skin_nrm.x = x89_sat_add(skin_nrm.x, modelrt_q16_mul(tn.x, v->weight[k]));
            skin_nrm.y = x89_sat_add(skin_nrm.y, modelrt_q16_mul(tn.y, v->weight[k]));
            skin_nrm.z = x89_sat_add(skin_nrm.z, modelrt_q16_mul(tn.z, v->weight[k]));
        }
        ++valid_skin;
    }

    if (valid_skin != 0u) {
        out->position = skin_pos;
        if (n.x != 0 || n.y != 0 || n.z != 0) {
            out->normal = skin_nrm;
            out->flags |= MODELRT_VTX_NORMAL;
        }
    } else {
        out->position = owner_world != 0 ? x89_transform_point(owner_world, v->position) : v->position;
        if (n.x != 0 || n.y != 0 || n.z != 0) {
            out->normal = owner_world != 0 ? x89_transform_vector(owner_world, n) : n;
            out->flags |= MODELRT_VTX_NORMAL;
        }
    }

    out->uv0 = v->uv;
    out->rgba[0] = v->rgba[0];
    out->rgba[1] = v->rgba[1];
    out->rgba[2] = v->rgba[2];
    out->rgba[3] = v->rgba[3];
    if ((v->flags & MODELRT_VTX_UV0) != 0u) out->flags |= MODELRT_VTX_UV0;
    if ((v->flags & MODELRT_VTX_COLOR) != 0u) out->flags |= MODELRT_VTX_COLOR;
    out->weight_count = v->weight_count;
    if (valid_skin != 0u) out->flags |= MODELRT_VTX_SKIN;
}

static int x89_emit_prepared(modelrt_handle *handle, x89_state *s,
                             const modelrt_visit_sink *sink, void *user)
{
    modelrt_u32 i;
    int cb;
    if (sink->on_model_begin) {
        cb = sink->on_model_begin(user, &handle->info);
        if (cb) return MODELRT_ERR_CALLBACK;
    }
    if (sink->on_material) {
        for (i = 0u; i < s->material_count; ++i) {
            modelrt_material_view mv;
            modelrt_zero(&mv, sizeof(mv));
            mv.index = i;
            modelrt_copy_string(mv.name, sizeof(mv.name), s->materials[i].name);
            mv.rgba[0] = s->materials[i].rgba[0];
            mv.rgba[1] = s->materials[i].rgba[1];
            mv.rgba[2] = s->materials[i].rgba[2];
            mv.rgba[3] = s->materials[i].rgba[3];
            mv.texture_index = s->materials[i].texture_index;
            cb = sink->on_material(user, &mv);
            if (cb) return MODELRT_ERR_CALLBACK;
        }
    }
    for (i = 0u; i < s->count.mesh_count; ++i) {
        const x89_mesh *m;
        modelrt_mesh_info mi;
        modelrt_u32 fi;
        m = &s->meshes[i];
        modelrt_zero(&mi, sizeof(mi));
        mi.index = i;
        modelrt_copy_string(mi.name, sizeof(mi.name), m->name);
        mi.vertex_count = m->vertex_count;
        mi.triangle_count = m->triangle_count;
        mi.material_count = m->material_slot_count;
        if (sink->on_mesh_begin) {
            cb = sink->on_mesh_begin(user, &mi);
            if (cb) return MODELRT_ERR_CALLBACK;
        }
        if (sink->on_triangle) {
            for (fi = 0u; fi < m->face_count; ++fi) {
                const x89_face *f;
                modelrt_u32 fan;
                f = &s->faces[m->first_face + fi];
                if (f->degree < 3u) continue;
                for (fan = 1u; fan + 1u < f->degree; ++fan) {
                    modelrt_triangle_view tv;
                    modelrt_u32 corner[3];
                    modelrt_u32 vi[3];
                    int k;
                    corner[0] = 0u;
                    corner[1] = fan;
                    corner[2] = fan + 1u;
                    vi[0] = s->indices[f->first_index + corner[0]];
                    vi[1] = s->indices[f->first_index + corner[1]];
                    vi[2] = s->indices[f->first_index + corner[2]];
                    if (vi[0] >= m->vertex_count || vi[1] >= m->vertex_count || vi[2] >= m->vertex_count)
                        return MODELRT_ERR_FORMAT;
                    modelrt_zero(&tv, sizeof(tv));
                    tv.mesh_index = i;
                    tv.material_index = x89_face_material(s, m, f);
                    for (k = 0; k < 3; ++k)
                        x89_emit_vertex(s, m, f, corner[k], vi[k], &tv.v[k]);
                    cb = sink->on_triangle(user, &tv);
                    if (cb) return MODELRT_ERR_CALLBACK;
                }
            }
        }
        if (sink->on_mesh_end) {
            cb = sink->on_mesh_end(user, &mi);
            if (cb) return MODELRT_ERR_CALLBACK;
        }
    }
    if (sink->on_bone && handle->info.bone_count != 0u) {
        for (i = 0u; i < s->count.frame_count; ++i) {
            modelrt_bone_view bv;
            modelrt_vec3 origin;
            modelrt_zero(&bv, sizeof(bv));
            bv.index = i;
            bv.parent = s->frames[i].parent;
            modelrt_copy_string(bv.name, sizeof(bv.name), s->frames[i].name);
            origin.x = origin.y = origin.z = 0;
            {
                const modelrt_q16 *bw;
                bw = x89_frame_world(s, i);
                bv.position = bw != 0 ? x89_transform_point(bw, origin) : origin;
            }
            cb = sink->on_bone(user, &bv);
            if (cb) return MODELRT_ERR_CALLBACK;
        }
    }
    if (sink->on_model_end) {
        cb = sink->on_model_end(user, &handle->info);
        if (cb) return MODELRT_ERR_CALLBACK;
    }
    return MODELRT_OK;
}

static int x89_visit(modelrt_handle *handle, int animation_index, modelrt_q16 time,
                     const modelrt_visit_sink *sink, void *user)
{
    x89_state *s;
    int rc;
    if (handle == 0 || sink == 0) return MODELRT_ERR_BAD_ARG;
    s = (x89_state *)handle->native_state;
    if (s == 0) return MODELRT_ERR_BAD_ARG;
    rc = x89_prepare_animation_pose(s, animation_index, time);
    if (rc != MODELRT_OK) return rc;
    return x89_emit_prepared(handle, s, sink, user);
}

static int x89_visit_animation_frame(modelrt_handle *handle, int animation_index,
                                     modelrt_u32 frame_index,
                                     const modelrt_visit_sink *sink, void *user)
{
    x89_state *s;
    int rc;
    if (handle == 0 || sink == 0 || animation_index < 0) return MODELRT_ERR_BAD_ARG;
    s = (x89_state *)handle->native_state;
    if (s == 0) return MODELRT_ERR_BAD_ARG;
    rc = x89_prepare_animation_pose_frame(s, animation_index, frame_index);
    if (rc != MODELRT_OK) return rc;
    return x89_emit_prepared(handle, s, sink, user);
}

static int x89_animation_count(const modelrt_handle *handle)
{
    const x89_state *s;
    if (handle == 0 || handle->native_state == 0) return 0;
    s = (const x89_state *)handle->native_state;
    return (int)s->animation_count;
}

static int x89_animation_info(modelrt_handle *handle, int animation_index,
                              modelrt_animation_info *out_info)
{
    x89_state *s;
    const xp_anim_clip_record *clip;
    modelrt_q16 start_time;
    modelrt_q16 end_time;
    if (handle == 0 || out_info == 0 || animation_index < 0) return MODELRT_ERR_BAD_ARG;
    s = (x89_state *)handle->native_state;
    if (s == 0 || (unsigned long)animation_index >= s->anim_req.clips || s->anim_clips == 0)
        return MODELRT_ERR_BAD_ARG;
    clip = &s->anim_clips[(unsigned long)animation_index];
    if (clip->frame_count > 0xfffffffful || clip->source_key_count > 0xfffffffful)
        return MODELRT_ERR_LIMIT;

    modelrt_zero(out_info, sizeof(*out_info));
    out_info->index = (modelrt_u32)animation_index;
    if (clip->name[0] != '\0') modelrt_copy_string(out_info->name, sizeof(out_info->name), clip->name);
    else modelrt_copy_string(out_info->name, sizeof(out_info->name), "DirectX AnimationSet");
    out_info->frame_count = (modelrt_u32)clip->frame_count;
    out_info->source_key_count = (modelrt_u32)clip->source_key_count;
    out_info->first_tick = (modelrt_i32)clip->first_tick;
    out_info->last_tick = (modelrt_i32)clip->last_tick;
    out_info->ticks_per_second = (modelrt_u32)clip->ticks_per_second;
    if (clip->frame_count != 0ul) out_info->flags |= MODELRT_ANIM_DENSE_FRAMES;
    if (x89_clip_is_resolved(s, clip) && clip->frame_count != 0ul)
        out_info->flags |= MODELRT_ANIM_POSE_SAMPLER;
    else
        out_info->flags |= MODELRT_ANIM_UNBOUND;

    start_time = 0;
    end_time = 0;
    if (clip->ticks_per_second_known && clip->ticks_per_second != 0u) {
        out_info->flags |= MODELRT_ANIM_TICK_RATE_KNOWN;
        if (clip->ticks_per_second <= 32767u && clip->first_tick >= -32768 &&
            clip->first_tick <= 32767 && clip->last_tick >= -32768 && clip->last_tick <= 32767) {
            modelrt_q16 rate;
            rate = modelrt_q16_from_int((modelrt_i32)clip->ticks_per_second);
            start_time = modelrt_q16_div(modelrt_q16_from_int((modelrt_i32)clip->first_tick), rate);
            end_time = modelrt_q16_div(modelrt_q16_from_int((modelrt_i32)clip->last_tick), rate);
        }
    }
    out_info->start_time = start_time;
    out_info->end_time = end_time;
    return MODELRT_OK;
}

static int x89_texture_count(const modelrt_handle *handle)
{
    const x89_state *s;
    if (handle == 0 || handle->native_state == 0) return 0;
    s = (const x89_state *)handle->native_state;
    return (int)s->texture_count;
}

static int x89_texture_info(modelrt_handle *handle, int texture_index,
                            modelrt_texture_info *out_info)
{
    x89_state *s;
    const char *path;
    const char *base;
    const char *p;
    if (handle == 0 || out_info == 0 || texture_index < 0) return MODELRT_ERR_BAD_ARG;
    s = (x89_state *)handle->native_state;
    if (s == 0 || (modelrt_u32)texture_index >= s->texture_count) return MODELRT_ERR_BAD_ARG;
    path = s->textures[(modelrt_u32)texture_index].path;
    base = path;
    for (p = path; *p != '\0'; ++p) if (*p == '/' || *p == '\\') base = p + 1;
    modelrt_zero(out_info, sizeof(*out_info));
    out_info->index = (modelrt_u32)texture_index;
    out_info->kind = MODELRT_TEXTURE_EXTERNAL;
    modelrt_copy_string(out_info->name, sizeof(out_info->name), base);
    modelrt_copy_string(out_info->path, sizeof(out_info->path), path);
    return MODELRT_OK;
}

const modelrt_provider modelrt_provider_x89 = {
    MODELRT_CODEC_DIRECTX_X,
    "DirectX .x / xparsermodget98 v0.5",
    x89_probe,
    x89_open,
    x89_close,
    x89_visit,
    x89_animation_count,
    x89_animation_info,
    x89_texture_count,
    x89_texture_info,
    0,
    0,
    0,
    0,
    x89_visit_animation_frame
};
