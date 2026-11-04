#include "giffy_recodec.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#define GRC_GFM1_MAGIC 0x314D4647u /* 'GFM1' little endian */
#define GRC_GFM2_MAGIC 0x324D4647u /* 'GFM2' little endian */
#define GRC_TMD_ID_A  0x00000041u
#define GRC_TMD_ID_B  0x41000000u
#define GRC_MAX_SANE_COUNT 1000000u

static grc_u16 grc_rd16(const grc_u8 *p)
{
    return (grc_u16)((grc_u16)p[0] | ((grc_u16)p[1] << 8));
}

static grc_i16 grc_rds16(const grc_u8 *p)
{
    return (grc_i16)grc_rd16(p);
}

static grc_u32 grc_rd32(const grc_u8 *p)
{
    return (grc_u32)((grc_u32)p[0] | ((grc_u32)p[1] << 8) | ((grc_u32)p[2] << 16) | ((grc_u32)p[3] << 24));
}

static void grc_wr16(FILE *f, grc_u16 v)
{
    grc_u8 b[2];
    b[0] = (grc_u8)(v & 255u);
    b[1] = (grc_u8)((v >> 8) & 255u);
    fwrite(b, 1u, 2u, f);
}

static void grc_wr32(FILE *f, grc_u32 v)
{
    grc_u8 b[4];
    b[0] = (grc_u8)(v & 255u);
    b[1] = (grc_u8)((v >> 8) & 255u);
    b[2] = (grc_u8)((v >> 16) & 255u);
    b[3] = (grc_u8)((v >> 24) & 255u);
    fwrite(b, 1u, 4u, f);
}

static void grc_wrfloat(FILE *f, float v)
{
    fwrite(&v, sizeof(float), 1u, f);
}

static float grc_rdfloat_file(FILE *f)
{
    float v;
    v = 0.0f;
    fread(&v, sizeof(float), 1u, f);
    return v;
}

int grc_check_host(void)
{
    if (sizeof(grc_u8) != 1u) return 0;
    if (sizeof(grc_u16) != 2u) return 0;
    if (sizeof(grc_u32) != 4u) return 0;
    if (sizeof(float) != 4u) return 0;
    return 1;
}

const char *grc_result_string(int code)
{
    switch (code) {
    case GRC_OK: return "ok";
    case GRC_ERR_BAD_ARGUMENT: return "bad argument";
    case GRC_ERR_IO: return "i/o error";
    case GRC_ERR_MEMORY: return "out of memory";
    case GRC_ERR_BAD_FORMAT: return "bad format";
    case GRC_ERR_UNSUPPORTED: return "unsupported feature";
    case GRC_ERR_OVERFLOW: return "overflow/range error";
    default: return "unknown error";
    }
}

const char *grc_format_string(grc_u32 fmt)
{
    switch (fmt) {
    case GRC_FORMAT_GFM: return "GFM";
    case GRC_FORMAT_OBJ: return "OBJ";
    case GRC_FORMAT_TMD: return "TMD";
    case GRC_FORMAT_TIM: return "TIM";
    case GRC_FORMAT_EMD: return "EMD";
    case GRC_FORMAT_PLD: return "PLD";
    case GRC_FORMAT_PLW: return "PLW";
    case GRC_FORMAT_MD1: return "MD1";
    case GRC_FORMAT_MD2: return "MD2";
    case GRC_FORMAT_AUTO: return "AUTO";
    default: return "UNKNOWN";
    }
}

const char *grc_game_string(grc_u32 game)
{
    switch (game) {
    case GRC_GAME_RE1: return "RE1";
    case GRC_GAME_RE2: return "RE2";
    case GRC_GAME_RE3: return "RE3";
    case GRC_GAME_UNKNOWN: return "UNKNOWN";
    default: return "UNKNOWN";
    }
}

void grc_options_default(grc_options *opt)
{
    if (opt != NULL) {
        opt->scale = 1.0f;
        opt->native_game_hint = GRC_GAME_UNKNOWN;
        opt->native_format_hint = GRC_FORMAT_AUTO;
        opt->flags = 0u;
        opt->palette_index = 0;
        opt->weld_epsilon = 0.001f;
    }
}

void grc_model_init(grc_model *model)
{
    if (model != NULL) {
        memset(model, 0, sizeof(*model));
        strcpy(model->name, "model");
    }
}

void grc_texture_init(grc_texture *texture)
{
    if (texture != NULL) {
        memset(texture, 0, sizeof(*texture));
        strcpy(texture->name, "texture");
    }
}

void grc_texture_free(grc_texture *texture)
{
    if (texture != NULL) {
        if (texture->rgba != NULL) free(texture->rgba);
        grc_texture_init(texture);
    }
}

void grc_model_free(grc_model *model)
{
    grc_u32 i;
    if (model == NULL) return;
    if (model->vertices != NULL) free(model->vertices);
    if (model->normals != NULL) free(model->normals);
    if (model->uvs != NULL) free(model->uvs);
    if (model->faces != NULL) free(model->faces);
    if (model->materials != NULL) free(model->materials);
    if (model->textures != NULL) {
        for (i = 0u; i < model->texture_count; ++i) {
            if (model->textures[i].rgba != NULL) free(model->textures[i].rgba);
        }
        free(model->textures);
    }
    if (model->pieces != NULL) free(model->pieces);
    grc_model_init(model);
}

void grc_section_table_free(grc_section_table *table)
{
    if (table != NULL) {
        if (table->sections != NULL) free(table->sections);
        table->sections = NULL;
        table->section_count = 0u;
    }
}

static int grc_reserve(void **ptr, grc_u32 *cap, grc_u32 want, size_t item_size)
{
    void *new_ptr;
    grc_u32 new_cap;
    size_t bytes;
    if (want <= *cap) return GRC_OK;
    new_cap = (*cap == 0u) ? 16u : *cap;
    while (new_cap < want) {
        if (new_cap > 0x7fffffffu / 2u) return GRC_ERR_OVERFLOW;
        new_cap *= 2u;
    }
    bytes = ((size_t)new_cap) * item_size;
    new_ptr = realloc(*ptr, bytes);
    if (new_ptr == NULL) return GRC_ERR_MEMORY;
    *ptr = new_ptr;
    *cap = new_cap;
    return GRC_OK;
}

int grc_model_add_vertex(grc_model *model, float x, float y, float z)
{
    int rc;
    grc_vec3 *v;
    if (model == NULL) return GRC_ERR_BAD_ARGUMENT;
    rc = grc_reserve((void **)&model->vertices, &model->vertex_capacity, model->vertex_count + 1u, sizeof(grc_vec3));
    if (rc != GRC_OK) return rc;
    v = &model->vertices[model->vertex_count];
    v->x = x;
    v->y = y;
    v->z = z;
    model->vertex_count++;
    return (int)(model->vertex_count - 1u);
}

int grc_model_add_normal(grc_model *model, float x, float y, float z)
{
    int rc;
    grc_vec3 *v;
    if (model == NULL) return GRC_ERR_BAD_ARGUMENT;
    rc = grc_reserve((void **)&model->normals, &model->normal_capacity, model->normal_count + 1u, sizeof(grc_vec3));
    if (rc != GRC_OK) return rc;
    v = &model->normals[model->normal_count];
    v->x = x;
    v->y = y;
    v->z = z;
    model->normal_count++;
    return (int)(model->normal_count - 1u);
}

int grc_model_add_uv(grc_model *model, float u, float v)
{
    int rc;
    grc_vec2 *t;
    if (model == NULL) return GRC_ERR_BAD_ARGUMENT;
    rc = grc_reserve((void **)&model->uvs, &model->uv_capacity, model->uv_count + 1u, sizeof(grc_vec2));
    if (rc != GRC_OK) return rc;
    t = &model->uvs[model->uv_count];
    t->u = u;
    t->v = v;
    model->uv_count++;
    return (int)(model->uv_count - 1u);
}

int grc_model_add_face(grc_model *model, const grc_face *face)
{
    int rc;
    if (model == NULL || face == NULL) return GRC_ERR_BAD_ARGUMENT;
    if (face->vertex_count != GRC_FACE_TRI && face->vertex_count != GRC_FACE_QUAD) return GRC_ERR_BAD_ARGUMENT;
    rc = grc_reserve((void **)&model->faces, &model->face_capacity, model->face_count + 1u, sizeof(grc_face));
    if (rc != GRC_OK) return rc;
    model->faces[model->face_count] = *face;
    model->face_count++;
    return (int)(model->face_count - 1u);
}

int grc_model_add_material(grc_model *model, const grc_material *mat)
{
    int rc;
    if (model == NULL || mat == NULL) return GRC_ERR_BAD_ARGUMENT;
    rc = grc_reserve((void **)&model->materials, &model->material_capacity, model->material_count + 1u, sizeof(grc_material));
    if (rc != GRC_OK) return rc;
    model->materials[model->material_count] = *mat;
    model->material_count++;
    return (int)(model->material_count - 1u);
}

int grc_model_add_texture(grc_model *model, const grc_texture *texture)
{
    int rc;
    grc_texture *dst;
    size_t bytes;
    if (model == NULL || texture == NULL) return GRC_ERR_BAD_ARGUMENT;
    rc = grc_reserve((void **)&model->textures, &model->texture_capacity, model->texture_count + 1u, sizeof(grc_texture));
    if (rc != GRC_OK) return rc;
    dst = &model->textures[model->texture_count];
    *dst = *texture;
    dst->rgba = NULL;
    if (texture->rgba != NULL && texture->width > 0u && texture->height > 0u) {
        bytes = ((size_t)texture->width) * ((size_t)texture->height) * 4u;
        dst->rgba = (grc_u8 *)malloc(bytes);
        if (dst->rgba == NULL) return GRC_ERR_MEMORY;
        memcpy(dst->rgba, texture->rgba, bytes);
    }
    model->texture_count++;
    return (int)(model->texture_count - 1u);
}

int grc_model_add_piece(grc_model *model, const grc_piece *piece)
{
    int rc;
    if (model == NULL || piece == NULL) return GRC_ERR_BAD_ARGUMENT;
    rc = grc_reserve((void **)&model->pieces, &model->piece_capacity, model->piece_count + 1u, sizeof(grc_piece));
    if (rc != GRC_OK) return rc;
    model->pieces[model->piece_count] = *piece;
    model->piece_count++;
    model->flags |= GRC_MODEL_FLAG_HAS_PIECES;
    return (int)(model->piece_count - 1u);
}

static void grc_piece_defaults(grc_piece *p, const char *name)
{
    size_t i;
    if (p == NULL) return;
    memset(p, 0, sizeof(*p));
    p->parent_index = -1;
    if (name != NULL && name[0] != '\0') {
        for (i = 0u; i < 63u && name[i] != '\0'; ++i) p->name[i] = name[i];
        p->name[i] = '\0';
    } else {
        strcpy(p->name, "piece");
    }
}

static int grc_add_piece_range(grc_model *model, const char *prefix, grc_u32 native_id,
                               grc_u32 first_vertex, grc_u32 vertex_count,
                               grc_u32 first_normal, grc_u32 normal_count,
                               grc_u32 first_uv, grc_u32 uv_count,
                               grc_u32 first_face, grc_u32 face_count)
{
    grc_piece piece;
    char nm[64];
    if (model == NULL || face_count == 0u) return GRC_OK;
    sprintf(nm, "%s_%u", (prefix != NULL) ? prefix : "piece", (unsigned int)native_id);
    grc_piece_defaults(&piece, nm);
    piece.native_id = native_id;
    piece.first_vertex = first_vertex;
    piece.vertex_count = vertex_count;
    piece.first_normal = first_normal;
    piece.normal_count = normal_count;
    piece.first_uv = first_uv;
    piece.uv_count = uv_count;
    piece.first_face = first_face;
    piece.face_count = face_count;
    return grc_model_add_piece(model, &piece);
}

static const char *grc_ext(const char *path)
{
    const char *p;
    const char *dot;
    if (path == NULL) return "";
    dot = NULL;
    p = path;
    while (*p != '\0') {
        if (*p == '.' || *p == '/' || *p == '\\') {
            if (*p == '.') dot = p;
            if (*p == '/' || *p == '\\') dot = NULL;
        }
        p++;
    }
    return (dot != NULL) ? dot + 1 : "";
}

static int grc_eqnocase(const char *a, const char *b)
{
    int ca;
    int cb;
    while (*a != '\0' && *b != '\0') {
        ca = tolower((unsigned char)*a);
        cb = tolower((unsigned char)*b);
        if (ca != cb) return 0;
        a++;
        b++;
    }
    return *a == '\0' && *b == '\0';
}

static grc_u32 grc_format_from_extension(const char *path)
{
    const char *e;
    e = grc_ext(path);
    if (grc_eqnocase(e, "gfm")) return GRC_FORMAT_GFM;
    if (grc_eqnocase(e, "obj")) return GRC_FORMAT_OBJ;
    if (grc_eqnocase(e, "tmd")) return GRC_FORMAT_TMD;
    if (grc_eqnocase(e, "tim")) return GRC_FORMAT_TIM;
    if (grc_eqnocase(e, "emd")) return GRC_FORMAT_EMD;
    if (grc_eqnocase(e, "pld")) return GRC_FORMAT_PLD;
    if (grc_eqnocase(e, "plw")) return GRC_FORMAT_PLW;
    if (grc_eqnocase(e, "md1")) return GRC_FORMAT_MD1;
    if (grc_eqnocase(e, "md2")) return GRC_FORMAT_MD2;
    return GRC_FORMAT_UNKNOWN;
}

static int grc_load_file_bytes(const char *path, grc_u8 **out_data, size_t *out_size)
{
    FILE *f;
    long len;
    grc_u8 *data;
    size_t got;
    if (path == NULL || out_data == NULL || out_size == NULL) return GRC_ERR_BAD_ARGUMENT;
    *out_data = NULL;
    *out_size = 0u;
    f = fopen(path, "rb");
    if (f == NULL) return GRC_ERR_IO;
    if (fseek(f, 0L, SEEK_END) != 0) {
        fclose(f);
        return GRC_ERR_IO;
    }
    len = ftell(f);
    if (len < 0L) {
        fclose(f);
        return GRC_ERR_IO;
    }
    if (fseek(f, 0L, SEEK_SET) != 0) {
        fclose(f);
        return GRC_ERR_IO;
    }
    data = (grc_u8 *)malloc((size_t)len + 1u);
    if (data == NULL) {
        fclose(f);
        return GRC_ERR_MEMORY;
    }
    got = fread(data, 1u, (size_t)len, f);
    fclose(f);
    if (got != (size_t)len) {
        free(data);
        return GRC_ERR_IO;
    }
    data[(size_t)len] = 0u;
    *out_data = data;
    *out_size = (size_t)len;
    return GRC_OK;
}

static int grc_write_bytes_file(const char *path, const grc_u8 *data, size_t size)
{
    FILE *f;
    if (path == NULL || data == NULL) return GRC_ERR_BAD_ARGUMENT;
    f = fopen(path, "wb");
    if (f == NULL) return GRC_ERR_IO;
    if (fwrite(data, 1u, size, f) != size) {
        fclose(f);
        return GRC_ERR_IO;
    }
    fclose(f);
    return GRC_OK;
}

int grc_probe_file(const char *path, grc_u32 *format_out)
{
    FILE *f;
    grc_u8 h[16];
    size_t n;
    grc_u32 fmt;
    if (path == NULL || format_out == NULL) return GRC_ERR_BAD_ARGUMENT;
    fmt = grc_format_from_extension(path);
    f = fopen(path, "rb");
    if (f == NULL) return GRC_ERR_IO;
    n = fread(h, 1u, sizeof(h), f);
    fclose(f);
    if (n >= 8u) {
        if (h[0] == 'G' && h[1] == 'F' && h[2] == 'M' && (h[3] == '1' || h[3] == '2')) fmt = GRC_FORMAT_GFM;
        else if (h[0] == 0x10u && h[1] == 0u) fmt = GRC_FORMAT_TIM;
        else {
            grc_u32 id;
            id = grc_rd32(h);
            if (id == GRC_TMD_ID_A || id == GRC_TMD_ID_B) fmt = GRC_FORMAT_TMD;
        }
    }
    *format_out = fmt;
    return GRC_OK;
}

static void grc_face_defaults(grc_face *f)
{
    int i;
    memset(f, 0, sizeof(*f));
    for (i = 0; i < 4; ++i) {
        f->v[i] = -1;
        f->vt[i] = -1;
        f->vn[i] = -1;
    }
    f->r = 200u;
    f->g = 200u;
    f->b = 200u;
    f->a = 255u;
    f->material_index = -1;
}

void grc_model_compute_bounds(const grc_model *model, grc_vec3 *min_out, grc_vec3 *max_out)
{
    grc_vec3 mn;
    grc_vec3 mx;
    grc_u32 i;
    if (min_out == NULL || max_out == NULL) return;
    mn.x = mn.y = mn.z = 0.0f;
    mx.x = mx.y = mx.z = 0.0f;
    if (model != NULL && model->vertex_count > 0u) {
        mn = model->vertices[0];
        mx = model->vertices[0];
        for (i = 1u; i < model->vertex_count; ++i) {
            if (model->vertices[i].x < mn.x) mn.x = model->vertices[i].x;
            if (model->vertices[i].y < mn.y) mn.y = model->vertices[i].y;
            if (model->vertices[i].z < mn.z) mn.z = model->vertices[i].z;
            if (model->vertices[i].x > mx.x) mx.x = model->vertices[i].x;
            if (model->vertices[i].y > mx.y) mx.y = model->vertices[i].y;
            if (model->vertices[i].z > mx.z) mx.z = model->vertices[i].z;
        }
    }
    *min_out = mn;
    *max_out = mx;
}


/* ------------------------------------------------------------------------- */
/* Model validation, cleanup and preparation                                  */
/* ------------------------------------------------------------------------- */

static float grc_absf(float v)
{
    return (v < 0.0f) ? -v : v;
}

static grc_i16 grc_model_clamp_i16(float x)
{
    long v;
    if (x < -32768.0f) return (grc_i16)-32768;
    if (x > 32767.0f) return (grc_i16)32767;
    v = (long)((x >= 0.0f) ? x + 0.5f : x - 0.5f);
    return (grc_i16)v;
}

static int grc_face_indices_valid(const grc_model *model, const grc_face *fa)
{
    grc_u32 i;
    if (model == NULL || fa == NULL) return 0;
    if (fa->vertex_count != GRC_FACE_TRI && fa->vertex_count != GRC_FACE_QUAD) return 0;
    for (i = 0u; i < fa->vertex_count; ++i) {
        if (fa->v[i] < 0 || (grc_u32)fa->v[i] >= model->vertex_count) return 0;
        if (fa->vt[i] >= 0 && (grc_u32)fa->vt[i] >= model->uv_count) return 0;
        if (fa->vn[i] >= 0 && (grc_u32)fa->vn[i] >= model->normal_count) return 0;
    }
    return 1;
}

static grc_vec3 grc_vec3_sub(grc_vec3 a, grc_vec3 b)
{
    grc_vec3 r;
    r.x = a.x - b.x;
    r.y = a.y - b.y;
    r.z = a.z - b.z;
    return r;
}

static grc_vec3 grc_vec3_cross(grc_vec3 a, grc_vec3 b)
{
    grc_vec3 r;
    r.x = a.y * b.z - a.z * b.y;
    r.y = a.z * b.x - a.x * b.z;
    r.z = a.x * b.y - a.y * b.x;
    return r;
}

static float grc_vec3_len2(grc_vec3 a)
{
    return a.x * a.x + a.y * a.y + a.z * a.z;
}

static grc_vec3 grc_vec3_normalize(grc_vec3 a)
{
    float l;
    l = (float)sqrt((double)grc_vec3_len2(a));
    if (l <= 0.0000001f) {
        a.x = 0.0f;
        a.y = 0.0f;
        a.z = 1.0f;
        return a;
    }
    a.x /= l;
    a.y /= l;
    a.z /= l;
    return a;
}

static int grc_face_degenerate(const grc_model *model, const grc_face *fa)
{
    grc_u32 i;
    grc_u32 j;
    grc_vec3 a;
    grc_vec3 b;
    grc_vec3 c;
    grc_vec3 n;
    if (!grc_face_indices_valid(model, fa)) return 1;
    for (i = 0u; i < fa->vertex_count; ++i) {
        for (j = i + 1u; j < fa->vertex_count; ++j) {
            if (fa->v[i] == fa->v[j]) return 1;
        }
    }
    a = model->vertices[fa->v[0]];
    b = model->vertices[fa->v[1]];
    c = model->vertices[fa->v[2]];
    n = grc_vec3_cross(grc_vec3_sub(b, a), grc_vec3_sub(c, a));
    if (grc_vec3_len2(n) <= 0.00000001f) return 1;
    return 0;
}

int grc_model_analyze(const grc_model *model, grc_model_stats *out_stats)
{
    grc_u32 i;
    grc_u8 *used;
    if (model == NULL || out_stats == NULL) return GRC_ERR_BAD_ARGUMENT;
    memset(out_stats, 0, sizeof(*out_stats));
    out_stats->vertex_count = model->vertex_count;
    out_stats->normal_count = model->normal_count;
    out_stats->uv_count = model->uv_count;
    out_stats->face_count = model->face_count;
    out_stats->piece_count = model->piece_count;
    grc_model_compute_bounds(model, &out_stats->bounds_min, &out_stats->bounds_max);
    used = NULL;
    if (model->vertex_count > 0u) {
        used = (grc_u8 *)calloc(model->vertex_count, 1u);
        if (used == NULL) return GRC_ERR_MEMORY;
    }
    for (i = 0u; i < model->face_count; ++i) {
        const grc_face *fa;
        grc_u32 k;
        fa = &model->faces[i];
        if (fa->vertex_count == GRC_FACE_TRI) out_stats->triangle_count++;
        else if (fa->vertex_count == GRC_FACE_QUAD) out_stats->quad_count++;
        if ((fa->flags & GRC_FACE_FLAG_TEXTURED) != 0u) out_stats->textured_face_count++;
        if (!grc_face_indices_valid(model, fa)) {
            out_stats->invalid_face_count++;
            continue;
        }
        if (grc_face_degenerate(model, fa)) out_stats->degenerate_face_count++;
        for (k = 0u; k < fa->vertex_count; ++k) used[fa->v[k]] = 1u;
    }
    if (used != NULL) {
        for (i = 0u; i < model->vertex_count; ++i) if (used[i] == 0u) out_stats->unused_vertex_count++;
        free(used);
    }
    return GRC_OK;
}

static void grc_append_report(char *report, size_t report_size, const char *msg)
{
    size_t len;
    size_t add;
    if (report == NULL || report_size == 0u || msg == NULL) return;
    len = strlen(report);
    if (len >= report_size - 1u) return;
    add = strlen(msg);
    if (add > report_size - 1u - len) add = report_size - 1u - len;
    memcpy(report + len, msg, add);
    report[len + add] = '\0';
}

int grc_model_validate(const grc_model *model, grc_model_stats *out_stats, char *report, size_t report_size)
{
    grc_model_stats st;
    int rc;
    char line[160];
    if (report != NULL && report_size > 0u) report[0] = '\0';
    if (model == NULL) return GRC_ERR_BAD_ARGUMENT;
    rc = grc_model_analyze(model, &st);
    if (rc != GRC_OK) return rc;
    if (out_stats != NULL) *out_stats = st;
    if (report != NULL && report_size > 0u) {
        sprintf(line, "verts=%u normals=%u uvs=%u faces=%u pieces=%u\n",
                (unsigned int)st.vertex_count, (unsigned int)st.normal_count,
                (unsigned int)st.uv_count, (unsigned int)st.face_count,
                (unsigned int)st.piece_count);
        grc_append_report(report, report_size, line);
        sprintf(line, "tris=%u quads=%u textured=%u invalid=%u degenerate=%u unused_vertices=%u\n",
                (unsigned int)st.triangle_count, (unsigned int)st.quad_count,
                (unsigned int)st.textured_face_count, (unsigned int)st.invalid_face_count,
                (unsigned int)st.degenerate_face_count, (unsigned int)st.unused_vertex_count);
        grc_append_report(report, report_size, line);
        if (st.invalid_face_count == 0u && st.degenerate_face_count == 0u) grc_append_report(report, report_size, "status=ok\n");
        else grc_append_report(report, report_size, "status=needs-clean\n");
    }
    return (st.invalid_face_count == 0u) ? GRC_OK : GRC_ERR_BAD_FORMAT;
}

static void grc_model_reset_single_piece(grc_model *model, const char *name)
{
    grc_piece p;
    if (model == NULL) return;
    if (model->pieces != NULL) free(model->pieces);
    model->pieces = NULL;
    model->piece_count = 0u;
    model->piece_capacity = 0u;
    if (model->face_count == 0u) return;
    grc_piece_defaults(&p, name);
    p.first_vertex = 0u;
    p.vertex_count = model->vertex_count;
    p.first_normal = 0u;
    p.normal_count = model->normal_count;
    p.first_uv = 0u;
    p.uv_count = model->uv_count;
    p.first_face = 0u;
    p.face_count = model->face_count;
    grc_model_add_piece(model, &p);
}

int grc_model_drop_invalid_faces(grc_model *model, grc_u32 drop_flags)
{
    grc_face *new_faces;
    grc_u32 i;
    grc_u32 n;
    if (model == NULL) return GRC_ERR_BAD_ARGUMENT;
    if (model->face_count == 0u) return GRC_OK;
    new_faces = (grc_face *)malloc((size_t)model->face_count * sizeof(grc_face));
    if (new_faces == NULL) return GRC_ERR_MEMORY;
    n = 0u;
    for (i = 0u; i < model->face_count; ++i) {
        int keep;
        keep = 1;
        if (!grc_face_indices_valid(model, &model->faces[i])) keep = 0;
        if (keep && (drop_flags & GRC_CLEAN_DROP_DEGENERATE) != 0u && grc_face_degenerate(model, &model->faces[i])) keep = 0;
        if (keep) new_faces[n++] = model->faces[i];
    }
    free(model->faces);
    model->faces = new_faces;
    model->face_count = n;
    model->face_capacity = n;
    grc_model_reset_single_piece(model, "cleaned");
    return GRC_OK;
}

int grc_model_triangulate_quads(grc_model *model)
{
    grc_u32 i;
    grc_u32 q;
    grc_u32 n;
    grc_face *new_faces;
    if (model == NULL) return GRC_ERR_BAD_ARGUMENT;
    q = 0u;
    for (i = 0u; i < model->face_count; ++i) if (model->faces[i].vertex_count == GRC_FACE_QUAD) q++;
    if (q == 0u) return GRC_OK;
    new_faces = (grc_face *)malloc(((size_t)model->face_count + (size_t)q) * sizeof(grc_face));
    if (new_faces == NULL) return GRC_ERR_MEMORY;
    n = 0u;
    for (i = 0u; i < model->face_count; ++i) {
        grc_face a;
        grc_face b;
        if (model->faces[i].vertex_count != GRC_FACE_QUAD) {
            new_faces[n++] = model->faces[i];
        } else {
            a = model->faces[i];
            b = model->faces[i];
            a.vertex_count = GRC_FACE_TRI;
            a.v[0] = model->faces[i].v[0]; a.v[1] = model->faces[i].v[1]; a.v[2] = model->faces[i].v[2]; a.v[3] = -1;
            a.vt[0] = model->faces[i].vt[0]; a.vt[1] = model->faces[i].vt[1]; a.vt[2] = model->faces[i].vt[2]; a.vt[3] = -1;
            a.vn[0] = model->faces[i].vn[0]; a.vn[1] = model->faces[i].vn[1]; a.vn[2] = model->faces[i].vn[2]; a.vn[3] = -1;
            b.vertex_count = GRC_FACE_TRI;
            b.v[0] = model->faces[i].v[0]; b.v[1] = model->faces[i].v[2]; b.v[2] = model->faces[i].v[3]; b.v[3] = -1;
            b.vt[0] = model->faces[i].vt[0]; b.vt[1] = model->faces[i].vt[2]; b.vt[2] = model->faces[i].vt[3]; b.vt[3] = -1;
            b.vn[0] = model->faces[i].vn[0]; b.vn[1] = model->faces[i].vn[2]; b.vn[2] = model->faces[i].vn[3]; b.vn[3] = -1;
            new_faces[n++] = a;
            new_faces[n++] = b;
        }
    }
    free(model->faces);
    model->faces = new_faces;
    model->face_count = n;
    model->face_capacity = n;
    grc_model_reset_single_piece(model, "triangulated");
    return GRC_OK;
}

int grc_model_generate_flat_normals(grc_model *model)
{
    grc_u32 i;
    if (model == NULL) return GRC_ERR_BAD_ARGUMENT;
    if (model->normals != NULL) free(model->normals);
    model->normals = NULL;
    model->normal_count = 0u;
    model->normal_capacity = 0u;
    for (i = 0u; i < model->face_count; ++i) {
        grc_face *fa;
        grc_vec3 a;
        grc_vec3 b;
        grc_vec3 c;
        grc_vec3 n;
        int ni;
        grc_u32 k;
        fa = &model->faces[i];
        if (!grc_face_indices_valid(model, fa)) continue;
        a = model->vertices[fa->v[0]];
        b = model->vertices[fa->v[1]];
        c = model->vertices[fa->v[2]];
        n = grc_vec3_normalize(grc_vec3_cross(grc_vec3_sub(b, a), grc_vec3_sub(c, a)));
        ni = grc_model_add_normal(model, n.x, n.y, n.z);
        if (ni < 0) return ni;
        for (k = 0u; k < fa->vertex_count; ++k) fa->vn[k] = ni;
    }
    model->flags |= GRC_MODEL_FLAG_GENERATED_NORMALS;
    if (model->piece_count > 0u) {
        grc_u32 pi;
        for (pi = 0u; pi < model->piece_count; ++pi) {
            if (model->pieces[pi].first_face < model->normal_count) {
                model->pieces[pi].first_normal = model->pieces[pi].first_face;
                model->pieces[pi].normal_count = model->pieces[pi].face_count;
            }
        }
    }
    return GRC_OK;
}

int grc_model_remove_unused_vertices(grc_model *model)
{
    grc_u8 *used;
    grc_i32 *remap;
    grc_vec3 *new_vertices;
    grc_u32 i;
    grc_u32 n;
    if (model == NULL) return GRC_ERR_BAD_ARGUMENT;
    if (model->vertex_count == 0u) return GRC_OK;
    used = (grc_u8 *)calloc(model->vertex_count, 1u);
    remap = (grc_i32 *)malloc((size_t)model->vertex_count * sizeof(grc_i32));
    if (used == NULL || remap == NULL) { if (used) free(used); if (remap) free(remap); return GRC_ERR_MEMORY; }
    for (i = 0u; i < model->vertex_count; ++i) remap[i] = -1;
    for (i = 0u; i < model->face_count; ++i) {
        grc_u32 k;
        for (k = 0u; k < model->faces[i].vertex_count; ++k) {
            if (model->faces[i].v[k] >= 0 && (grc_u32)model->faces[i].v[k] < model->vertex_count) used[model->faces[i].v[k]] = 1u;
        }
    }
    n = 0u;
    for (i = 0u; i < model->vertex_count; ++i) if (used[i] != 0u) remap[i] = (grc_i32)n++;
    new_vertices = (grc_vec3 *)malloc((size_t)((n == 0u) ? 1u : n) * sizeof(grc_vec3));
    if (new_vertices == NULL) { free(used); free(remap); return GRC_ERR_MEMORY; }
    for (i = 0u; i < model->vertex_count; ++i) if (remap[i] >= 0) new_vertices[remap[i]] = model->vertices[i];
    for (i = 0u; i < model->face_count; ++i) {
        grc_u32 k;
        for (k = 0u; k < model->faces[i].vertex_count; ++k) {
            if (model->faces[i].v[k] >= 0 && (grc_u32)model->faces[i].v[k] < model->vertex_count) model->faces[i].v[k] = remap[model->faces[i].v[k]];
        }
    }
    free(model->vertices);
    model->vertices = new_vertices;
    model->vertex_count = n;
    model->vertex_capacity = n;
    free(used);
    free(remap);
    grc_model_reset_single_piece(model, "packed");
    return GRC_OK;
}

int grc_model_weld_vertices(grc_model *model, float epsilon)
{
    grc_i32 *remap;
    grc_vec3 *new_vertices;
    grc_u32 i;
    grc_u32 j;
    grc_u32 n;
    float e;
    if (model == NULL) return GRC_ERR_BAD_ARGUMENT;
    if (model->vertex_count == 0u) return GRC_OK;
    e = (epsilon <= 0.0f) ? 0.0001f : epsilon;
    remap = (grc_i32 *)malloc((size_t)model->vertex_count * sizeof(grc_i32));
    new_vertices = (grc_vec3 *)malloc((size_t)model->vertex_count * sizeof(grc_vec3));
    if (remap == NULL || new_vertices == NULL) { if (remap) free(remap); if (new_vertices) free(new_vertices); return GRC_ERR_MEMORY; }
    n = 0u;
    for (i = 0u; i < model->vertex_count; ++i) {
        grc_i32 found;
        found = -1;
        for (j = 0u; j < n; ++j) {
            if (grc_absf(model->vertices[i].x - new_vertices[j].x) <= e &&
                grc_absf(model->vertices[i].y - new_vertices[j].y) <= e &&
                grc_absf(model->vertices[i].z - new_vertices[j].z) <= e) {
                found = (grc_i32)j;
                break;
            }
        }
        if (found < 0) {
            new_vertices[n] = model->vertices[i];
            found = (grc_i32)n;
            n++;
        }
        remap[i] = found;
    }
    for (i = 0u; i < model->face_count; ++i) {
        grc_u32 k;
        for (k = 0u; k < model->faces[i].vertex_count; ++k) {
            if (model->faces[i].v[k] >= 0 && (grc_u32)model->faces[i].v[k] < model->vertex_count) model->faces[i].v[k] = remap[model->faces[i].v[k]];
        }
    }
    free(model->vertices);
    model->vertices = new_vertices;
    model->vertex_count = n;
    model->vertex_capacity = model->vertex_count;
    free(remap);
    grc_model_reset_single_piece(model, "welded");
    return GRC_OK;
}

void grc_model_apply_scale(grc_model *model, float sx, float sy, float sz)
{
    grc_u32 i;
    if (model == NULL) return;
    for (i = 0u; i < model->vertex_count; ++i) {
        model->vertices[i].x *= sx;
        model->vertices[i].y *= sy;
        model->vertices[i].z *= sz;
    }
}

void grc_model_apply_translate(grc_model *model, float tx, float ty, float tz)
{
    grc_u32 i;
    if (model == NULL) return;
    for (i = 0u; i < model->vertex_count; ++i) {
        model->vertices[i].x += tx;
        model->vertices[i].y += ty;
        model->vertices[i].z += tz;
    }
}

void grc_model_flip_v(grc_model *model)
{
    grc_u32 i;
    if (model == NULL) return;
    for (i = 0u; i < model->uv_count; ++i) model->uvs[i].v = 1.0f - model->uvs[i].v;
}

static void grc_model_quantize_psx(grc_model *model, const grc_options *opt)
{
    grc_u32 i;
    float inv_scale;
    if (model == NULL) return;
    inv_scale = (opt != NULL && opt->scale != 0.0f) ? (1.0f / opt->scale) : 1.0f;
    for (i = 0u; i < model->vertex_count; ++i) {
        grc_i16 x;
        grc_i16 y;
        grc_i16 z;
        x = grc_model_clamp_i16(model->vertices[i].x * inv_scale);
        y = grc_model_clamp_i16(model->vertices[i].y * inv_scale);
        z = grc_model_clamp_i16(model->vertices[i].z * inv_scale);
        model->vertices[i].x = (float)x / inv_scale;
        model->vertices[i].y = (float)y / inv_scale;
        model->vertices[i].z = (float)z / inv_scale;
    }
    model->flags |= GRC_MODEL_FLAG_PSX_QUANTIZED;
}

int grc_model_prepare(grc_model *model, const grc_options *opt)
{
    grc_u32 flags;
    int rc;
    if (model == NULL) return GRC_ERR_BAD_ARGUMENT;
    flags = (opt != NULL) ? opt->flags : 0u;
    if ((flags & GRC_CLEAN_WELD_VERTICES) != 0u) {
        rc = grc_model_weld_vertices(model, (opt != NULL) ? opt->weld_epsilon : 0.0001f);
        if (rc != GRC_OK) return rc;
    }
    if ((flags & GRC_CLEAN_DROP_INVALID) != 0u || (flags & GRC_CLEAN_DROP_DEGENERATE) != 0u) {
        rc = grc_model_drop_invalid_faces(model, flags);
        if (rc != GRC_OK) return rc;
    }
    if ((flags & GRC_CLEAN_TRIANGULATE_QUADS) != 0u) {
        rc = grc_model_triangulate_quads(model);
        if (rc != GRC_OK) return rc;
    }
    if ((flags & GRC_CLEAN_REMOVE_UNUSED_VERTICES) != 0u) {
        rc = grc_model_remove_unused_vertices(model);
        if (rc != GRC_OK) return rc;
    }
    if ((flags & GRC_CLEAN_GENERATE_NORMALS) != 0u) {
        rc = grc_model_generate_flat_normals(model);
        if (rc != GRC_OK) return rc;
    }
    if ((flags & GRC_CLEAN_PSX_QUANTIZE) != 0u) grc_model_quantize_psx(model, opt);
    return GRC_OK;
}

/* ------------------------------------------------------------------------- */
/* GFM native interchange format                                             */
/* ------------------------------------------------------------------------- */

static void grc_write_piece(FILE *f, const grc_piece *p)
{
    fwrite(p->name, 1u, 64u, f);
    grc_wr32(f, p->first_vertex);
    grc_wr32(f, p->vertex_count);
    grc_wr32(f, p->first_normal);
    grc_wr32(f, p->normal_count);
    grc_wr32(f, p->first_uv);
    grc_wr32(f, p->uv_count);
    grc_wr32(f, p->first_face);
    grc_wr32(f, p->face_count);
    grc_wr32(f, (grc_u32)p->parent_index);
    grc_wrfloat(f, p->pivot.x);
    grc_wrfloat(f, p->pivot.y);
    grc_wrfloat(f, p->pivot.z);
    grc_wr32(f, p->native_id);
    grc_wr32(f, p->flags);
}

static int grc_read_piece(FILE *f, grc_piece *p)
{
    grc_u8 b[56];
    if (fread(p->name, 1u, 64u, f) != 64u) return GRC_ERR_IO;
    p->name[63] = '\0';
    if (fread(b, 1u, 56u, f) != 56u) return GRC_ERR_IO;
    p->first_vertex = grc_rd32(b + 0u);
    p->vertex_count = grc_rd32(b + 4u);
    p->first_normal = grc_rd32(b + 8u);
    p->normal_count = grc_rd32(b + 12u);
    p->first_uv = grc_rd32(b + 16u);
    p->uv_count = grc_rd32(b + 20u);
    p->first_face = grc_rd32(b + 24u);
    p->face_count = grc_rd32(b + 28u);
    p->parent_index = (grc_i32)grc_rd32(b + 32u);
    memcpy(&p->pivot.x, b + 36u, sizeof(float));
    memcpy(&p->pivot.y, b + 40u, sizeof(float));
    memcpy(&p->pivot.z, b + 44u, sizeof(float));
    p->native_id = grc_rd32(b + 48u);
    p->flags = grc_rd32(b + 52u);
    return GRC_OK;
}

int grc_save_gfm(const char *path, const grc_model *model)
{
    FILE *f;
    grc_u32 i;
    grc_u32 j;
    const grc_face *fa;
    const grc_texture *tx;
    size_t bytes;
    if (path == NULL || model == NULL) return GRC_ERR_BAD_ARGUMENT;
    f = fopen(path, "wb");
    if (f == NULL) return GRC_ERR_IO;
    fwrite("GFM2", 1u, 4u, f);
    grc_wr32(f, 2u);
    grc_wr32(f, model->flags);
    grc_wr32(f, model->source_game);
    grc_wr32(f, model->source_format);
    grc_wr32(f, model->vertex_count);
    grc_wr32(f, model->normal_count);
    grc_wr32(f, model->uv_count);
    grc_wr32(f, model->face_count);
    grc_wr32(f, model->material_count);
    grc_wr32(f, model->texture_count);
    grc_wr32(f, model->piece_count);
    fwrite(model->name, 1u, 64u, f);
    for (i = 0u; i < model->vertex_count; ++i) {
        grc_wrfloat(f, model->vertices[i].x);
        grc_wrfloat(f, model->vertices[i].y);
        grc_wrfloat(f, model->vertices[i].z);
    }
    for (i = 0u; i < model->normal_count; ++i) {
        grc_wrfloat(f, model->normals[i].x);
        grc_wrfloat(f, model->normals[i].y);
        grc_wrfloat(f, model->normals[i].z);
    }
    for (i = 0u; i < model->uv_count; ++i) {
        grc_wrfloat(f, model->uvs[i].u);
        grc_wrfloat(f, model->uvs[i].v);
    }
    for (i = 0u; i < model->face_count; ++i) {
        fa = &model->faces[i];
        grc_wr32(f, fa->vertex_count);
        for (j = 0u; j < 4u; ++j) grc_wr32(f, (grc_u32)fa->v[j]);
        for (j = 0u; j < 4u; ++j) grc_wr32(f, (grc_u32)fa->vt[j]);
        for (j = 0u; j < 4u; ++j) grc_wr32(f, (grc_u32)fa->vn[j]);
        fputc(fa->r, f);
        fputc(fa->g, f);
        fputc(fa->b, f);
        fputc(fa->a, f);
        grc_wr32(f, (grc_u32)fa->material_index);
        grc_wr32(f, fa->flags);
    }
    for (i = 0u; i < model->material_count; ++i) {
        fwrite(model->materials[i].name, 1u, 64u, f);
        fputc(model->materials[i].r, f);
        fputc(model->materials[i].g, f);
        fputc(model->materials[i].b, f);
        fputc(model->materials[i].a, f);
        grc_wr32(f, (grc_u32)model->materials[i].texture_index);
    }
    for (i = 0u; i < model->texture_count; ++i) {
        tx = &model->textures[i];
        fwrite(tx->name, 1u, 64u, f);
        grc_wr32(f, tx->width);
        grc_wr32(f, tx->height);
        bytes = ((size_t)tx->width) * ((size_t)tx->height) * 4u;
        if (bytes > 0u && tx->rgba != NULL) fwrite(tx->rgba, 1u, bytes, f);
    }
    for (i = 0u; i < model->piece_count; ++i) grc_write_piece(f, &model->pieces[i]);
    if (ferror(f)) {
        fclose(f);
        return GRC_ERR_IO;
    }
    fclose(f);
    return GRC_OK;
}

int grc_load_gfm(const char *path, grc_model *out_model)
{
    FILE *f;
    char magic[4];
    grc_u32 version;
    grc_u32 flags;
    grc_u32 source_game;
    grc_u32 source_format;
    grc_u32 vc;
    grc_u32 nc;
    grc_u32 tc;
    grc_u32 fc;
    grc_u32 mc;
    grc_u32 xc;
    grc_u32 pc;
    grc_u32 i;
    grc_u32 j;
    int rc;
    grc_face fa;
    grc_material mat;
    grc_texture tex;
    grc_piece piece;
    size_t bytes;
    if (path == NULL || out_model == NULL) return GRC_ERR_BAD_ARGUMENT;
    grc_model_init(out_model);
    f = fopen(path, "rb");
    if (f == NULL) return GRC_ERR_IO;
    if (fread(magic, 1u, 4u, f) != 4u || (memcmp(magic, "GFM1", 4u) != 0 && memcmp(magic, "GFM2", 4u) != 0)) {
        fclose(f);
        return GRC_ERR_BAD_FORMAT;
    }
    pc = 0u;
    if (memcmp(magic, "GFM1", 4u) == 0) {
        grc_u8 b[40];
        if (fread(b, 1u, 40u, f) != 40u) { fclose(f); return GRC_ERR_IO; }
        version = grc_rd32(b + 0u);
        flags = grc_rd32(b + 4u);
        source_game = grc_rd32(b + 8u);
        source_format = grc_rd32(b + 12u);
        vc = grc_rd32(b + 16u);
        nc = grc_rd32(b + 20u);
        tc = grc_rd32(b + 24u);
        fc = grc_rd32(b + 28u);
        mc = grc_rd32(b + 32u);
        xc = grc_rd32(b + 36u);
    } else {
        grc_u8 b[44];
        if (fread(b, 1u, 44u, f) != 44u) { fclose(f); return GRC_ERR_IO; }
        version = grc_rd32(b + 0u);
        flags = grc_rd32(b + 4u);
        source_game = grc_rd32(b + 8u);
        source_format = grc_rd32(b + 12u);
        vc = grc_rd32(b + 16u);
        nc = grc_rd32(b + 20u);
        tc = grc_rd32(b + 24u);
        fc = grc_rd32(b + 28u);
        mc = grc_rd32(b + 32u);
        xc = grc_rd32(b + 36u);
        pc = grc_rd32(b + 40u);
    }
    if ((version != 1u && version != 2u) || vc > GRC_MAX_SANE_COUNT || nc > GRC_MAX_SANE_COUNT || tc > GRC_MAX_SANE_COUNT || fc > GRC_MAX_SANE_COUNT || pc > 65535u) {
        fclose(f);
        return GRC_ERR_BAD_FORMAT;
    }
    if (fread(out_model->name, 1u, 64u, f) != 64u) { fclose(f); return GRC_ERR_IO; }
    out_model->name[63] = '\0';
    out_model->flags = flags;
    out_model->source_game = source_game;
    out_model->source_format = source_format;
    for (i = 0u; i < vc; ++i) {
        float x;
        float y;
        float z;
        x = grc_rdfloat_file(f); y = grc_rdfloat_file(f); z = grc_rdfloat_file(f);
        rc = grc_model_add_vertex(out_model, x, y, z);
        if (rc < 0) { fclose(f); return rc; }
    }
    for (i = 0u; i < nc; ++i) {
        float x;
        float y;
        float z;
        x = grc_rdfloat_file(f); y = grc_rdfloat_file(f); z = grc_rdfloat_file(f);
        rc = grc_model_add_normal(out_model, x, y, z);
        if (rc < 0) { fclose(f); return rc; }
    }
    for (i = 0u; i < tc; ++i) {
        float u;
        float v;
        u = grc_rdfloat_file(f); v = grc_rdfloat_file(f);
        rc = grc_model_add_uv(out_model, u, v);
        if (rc < 0) { fclose(f); return rc; }
    }
    for (i = 0u; i < fc; ++i) {
        grc_u8 b[64];
        if (fread(b, 1u, 64u, f) != 64u) { fclose(f); return GRC_ERR_IO; }
        grc_face_defaults(&fa);
        fa.vertex_count = grc_rd32(b + 0u);
        for (j = 0u; j < 4u; ++j) fa.v[j] = (grc_i32)grc_rd32(b + 4u + j * 4u);
        for (j = 0u; j < 4u; ++j) fa.vt[j] = (grc_i32)grc_rd32(b + 20u + j * 4u);
        for (j = 0u; j < 4u; ++j) fa.vn[j] = (grc_i32)grc_rd32(b + 36u + j * 4u);
        fa.r = b[52]; fa.g = b[53]; fa.b = b[54]; fa.a = b[55];
        fa.material_index = (grc_i32)grc_rd32(b + 56u);
        fa.flags = grc_rd32(b + 60u);
        rc = grc_model_add_face(out_model, &fa);
        if (rc < 0) { fclose(f); return rc; }
    }
    for (i = 0u; i < mc; ++i) {
        grc_u8 b[8];
        memset(&mat, 0, sizeof(mat));
        if (fread(mat.name, 1u, 64u, f) != 64u) { fclose(f); return GRC_ERR_IO; }
        mat.name[63] = '\0';
        if (fread(b, 1u, 8u, f) != 8u) { fclose(f); return GRC_ERR_IO; }
        mat.r = b[0]; mat.g = b[1]; mat.b = b[2]; mat.a = b[3];
        mat.texture_index = (grc_i32)grc_rd32(b + 4u);
        rc = grc_model_add_material(out_model, &mat);
        if (rc < 0) { fclose(f); return rc; }
    }
    for (i = 0u; i < xc; ++i) {
        grc_u8 b[8];
        grc_texture_init(&tex);
        if (fread(tex.name, 1u, 64u, f) != 64u) { fclose(f); return GRC_ERR_IO; }
        tex.name[63] = '\0';
        if (fread(b, 1u, 8u, f) != 8u) { fclose(f); return GRC_ERR_IO; }
        tex.width = grc_rd32(b + 0u);
        tex.height = grc_rd32(b + 4u);
        bytes = ((size_t)tex.width) * ((size_t)tex.height) * 4u;
        if (bytes > 0u) {
            tex.rgba = (grc_u8 *)malloc(bytes);
            if (tex.rgba == NULL) { fclose(f); return GRC_ERR_MEMORY; }
            if (fread(tex.rgba, 1u, bytes, f) != bytes) { grc_texture_free(&tex); fclose(f); return GRC_ERR_IO; }
        }
        rc = grc_model_add_texture(out_model, &tex);
        grc_texture_free(&tex);
        if (rc < 0) { fclose(f); return rc; }
    }
    for (i = 0u; i < pc; ++i) {
        rc = grc_read_piece(f, &piece);
        if (rc != GRC_OK) { fclose(f); return rc; }
        rc = grc_model_add_piece(out_model, &piece);
        if (rc < 0) { fclose(f); return rc; }
    }
    fclose(f);
    return GRC_OK;
}

/* ------------------------------------------------------------------------- */
/* OBJ import/export                                                         */
/* ------------------------------------------------------------------------- */

static char *grc_trim(char *s)
{
    char *e;
    while (*s != '\0' && isspace((unsigned char)*s)) s++;
    e = s + strlen(s);
    while (e > s && isspace((unsigned char)e[-1])) { e--; *e = '\0'; }
    return s;
}

static int grc_parse_obj_index(const char *token, grc_i32 *vi, grc_i32 *ti, grc_i32 *ni, grc_u32 vc, grc_u32 tc, grc_u32 nc)
{
    char buf[128];
    char *a;
    char *b;
    char *c;
    long v;
    long t;
    long n;
    if (token == NULL || vi == NULL || ti == NULL || ni == NULL) return GRC_ERR_BAD_ARGUMENT;
    strncpy(buf, token, sizeof(buf) - 1u);
    buf[sizeof(buf) - 1u] = '\0';
    a = buf;
    b = strchr(a, '/');
    c = NULL;
    if (b != NULL) {
        *b = '\0';
        b++;
        c = strchr(b, '/');
        if (c != NULL) {
            *c = '\0';
            c++;
        }
    }
    v = atol(a);
    t = (b != NULL && *b != '\0') ? atol(b) : 0L;
    n = (c != NULL && *c != '\0') ? atol(c) : 0L;
    if (v == 0L) return GRC_ERR_BAD_FORMAT;
    if (v < 0L) v = (long)vc + v + 1L;
    if (t < 0L) t = (long)tc + t + 1L;
    if (n < 0L) n = (long)nc + n + 1L;
    *vi = (grc_i32)(v - 1L);
    *ti = (t == 0L) ? -1 : (grc_i32)(t - 1L);
    *ni = (n == 0L) ? -1 : (grc_i32)(n - 1L);
    return GRC_OK;
}

static int grc_find_material(const grc_model *model, const char *name)
{
    grc_u32 i;
    if (model == NULL || name == NULL) return -1;
    for (i = 0u; i < model->material_count; ++i) {
        if (strcmp(model->materials[i].name, name) == 0) return (int)i;
    }
    return -1;
}

static int grc_get_or_add_material(grc_model *model, const char *name)
{
    grc_material mat;
    int idx;
    idx = grc_find_material(model, name);
    if (idx >= 0) return idx;
    memset(&mat, 0, sizeof(mat));
    if (name != NULL && name[0] != '\0') {
        strncpy(mat.name, name, 63u);
        mat.name[63] = '\0';
    } else {
        strcpy(mat.name, "material");
    }
    mat.r = 200u; mat.g = 200u; mat.b = 200u; mat.a = 255u; mat.texture_index = -1;
    return grc_model_add_material(model, &mat);
}

static void grc_dirname_join(char *out, size_t out_size, const char *obj_path, const char *rel)
{
    const char *slash;
    const char *p;
    size_t n;
    if (out == NULL || out_size == 0u) return;
    out[0] = '\0';
    if (rel == NULL) return;
    if (rel[0] == '/' || (strlen(rel) > 2u && rel[1] == ':')) {
        strncpy(out, rel, out_size - 1u);
        out[out_size - 1u] = '\0';
        return;
    }
    slash = NULL;
    if (obj_path != NULL) {
        for (p = obj_path; *p != '\0'; ++p) if (*p == '/' || *p == '\\') slash = p;
    }
    if (slash == NULL) {
        strncpy(out, rel, out_size - 1u);
        out[out_size - 1u] = '\0';
        return;
    }
    n = (size_t)(slash - obj_path + 1);
    if (n >= out_size) n = out_size - 1u;
    memcpy(out, obj_path, n);
    out[n] = '\0';
    strncat(out, rel, out_size - strlen(out) - 1u);
}

static void grc_load_mtl_file(const char *obj_path, const char *mtl_name, grc_model *model)
{
    char path[512];
    FILE *f;
    char line[512];
    int cur;
    grc_dirname_join(path, sizeof(path), obj_path, mtl_name);
    f = fopen(path, "r");
    if (f == NULL) return;
    cur = -1;
    while (fgets(line, sizeof(line), f) != NULL) {
        char *s;
        s = grc_trim(line);
        if (strncmp(s, "newmtl ", 7u) == 0) {
            cur = grc_get_or_add_material(model, grc_trim(s + 7));
        } else if (strncmp(s, "Kd ", 3u) == 0 && cur >= 0) {
            float r;
            float g;
            float b;
            if (sscanf(s + 3, "%f %f %f", &r, &g, &b) == 3) {
                if (r < 0.0f) r = 0.0f;
                if (r > 1.0f) r = 1.0f;
                if (g < 0.0f) g = 0.0f;
                if (g > 1.0f) g = 1.0f;
                if (b < 0.0f) b = 0.0f;
                if (b > 1.0f) b = 1.0f;
                model->materials[cur].r = (grc_u8)(r * 255.0f + 0.5f);
                model->materials[cur].g = (grc_u8)(g * 255.0f + 0.5f);
                model->materials[cur].b = (grc_u8)(b * 255.0f + 0.5f);
            }
        } else if ((strncmp(s, "d ", 2u) == 0 || strncmp(s, "Tr ", 3u) == 0) && cur >= 0) {
            float a;
            char *pval;
            pval = (s[0] == 'd') ? s + 2 : s + 3;
            if (sscanf(pval, "%f", &a) == 1) {
                if (a < 0.0f) a = 0.0f;
                if (a > 1.0f) a = 1.0f;
                model->materials[cur].a = (grc_u8)(a * 255.0f + 0.5f);
            }
        }
    }
    fclose(f);
}

static void grc_finalize_obj_piece(grc_model *model, const char *name, grc_u32 first_face, grc_u32 first_vertex)
{
    grc_piece piece;
    if (model == NULL || first_face >= model->face_count) return;
    grc_piece_defaults(&piece, (name != NULL && name[0] != '\0') ? name : "obj_group");
    piece.first_vertex = first_vertex;
    piece.vertex_count = (first_vertex <= model->vertex_count) ? (model->vertex_count - first_vertex) : model->vertex_count;
    piece.first_normal = 0u;
    piece.normal_count = model->normal_count;
    piece.first_uv = 0u;
    piece.uv_count = model->uv_count;
    piece.first_face = first_face;
    piece.face_count = model->face_count - first_face;
    grc_model_add_piece(model, &piece);
}

int grc_load_obj(const char *path, grc_model *out_model, const grc_options *opt)
{
    FILE *f;
    char line[1024];
    int rc;
    int current_mat;
    char group_name[64];
    grc_u32 group_first_face;
    grc_u32 group_first_vertex;
    (void)opt;
    if (path == NULL || out_model == NULL) return GRC_ERR_BAD_ARGUMENT;
    grc_model_init(out_model);
    out_model->source_format = GRC_FORMAT_OBJ;
    f = fopen(path, "r");
    if (f == NULL) return GRC_ERR_IO;
    current_mat = -1;
    strcpy(group_name, "obj_group");
    group_first_face = 0u;
    group_first_vertex = 0u;
    while (fgets(line, sizeof(line), f) != NULL) {
        char *s;
        s = grc_trim(line);
        if (*s == '\0' || *s == '#') continue;
        if (strncmp(s, "v ", 2u) == 0) {
            float x;
            float y;
            float z;
            if (sscanf(s + 2, "%f %f %f", &x, &y, &z) == 3) {
                rc = grc_model_add_vertex(out_model, x, y, z);
                if (rc < 0) { fclose(f); return rc; }
            }
        } else if (strncmp(s, "vn ", 3u) == 0) {
            float x;
            float y;
            float z;
            if (sscanf(s + 3, "%f %f %f", &x, &y, &z) == 3) {
                rc = grc_model_add_normal(out_model, x, y, z);
                if (rc < 0) { fclose(f); return rc; }
            }
        } else if (strncmp(s, "vt ", 3u) == 0) {
            float u;
            float v;
            if (sscanf(s + 3, "%f %f", &u, &v) >= 2) {
                rc = grc_model_add_uv(out_model, u, v);
                if (rc < 0) { fclose(f); return rc; }
            }
        } else if (strncmp(s, "f ", 2u) == 0) {
            char *tok;
            grc_i32 vi[64];
            grc_i32 ti[64];
            grc_i32 ni[64];
            grc_u32 n;
            grc_u32 i;
            n = 0u;
            tok = strtok(s + 2, " \t\r\n");
            while (tok != NULL && n < 64u) {
                if (grc_parse_obj_index(tok, &vi[n], &ti[n], &ni[n], out_model->vertex_count, out_model->uv_count, out_model->normal_count) == GRC_OK) n++;
                tok = strtok(NULL, " \t\r\n");
            }
            if (n >= 3u) {
                for (i = 1u; i + 1u < n; ++i) {
                    grc_face fa;
                    grc_face_defaults(&fa);
                    fa.vertex_count = GRC_FACE_TRI;
                    fa.v[0] = vi[0]; fa.v[1] = vi[i]; fa.v[2] = vi[i + 1u];
                    fa.vt[0] = ti[0]; fa.vt[1] = ti[i]; fa.vt[2] = ti[i + 1u];
                    fa.vn[0] = ni[0]; fa.vn[1] = ni[i]; fa.vn[2] = ni[i + 1u];
                    fa.material_index = current_mat;
                    if (current_mat >= 0 && (grc_u32)current_mat < out_model->material_count) {
                        fa.r = out_model->materials[current_mat].r;
                        fa.g = out_model->materials[current_mat].g;
                        fa.b = out_model->materials[current_mat].b;
                        fa.a = out_model->materials[current_mat].a;
                    }
                    if (fa.vt[0] >= 0 || fa.vt[1] >= 0 || fa.vt[2] >= 0) fa.flags |= GRC_FACE_FLAG_TEXTURED;
                    rc = grc_model_add_face(out_model, &fa);
                    if (rc < 0) { fclose(f); return rc; }
                }
            }
        } else if (strncmp(s, "o ", 2u) == 0 || strncmp(s, "g ", 2u) == 0) {
            char *name;
            if (out_model->face_count > group_first_face) grc_finalize_obj_piece(out_model, group_name, group_first_face, group_first_vertex);
            group_first_face = out_model->face_count;
            group_first_vertex = out_model->vertex_count;
            name = grc_trim(s + 1);
            strncpy(group_name, name, 63u);
            group_name[63] = '\0';
            strncpy(out_model->name, name, 63u);
            out_model->name[63] = '\0';
        } else if (strncmp(s, "usemtl ", 7u) == 0) {
            current_mat = grc_get_or_add_material(out_model, grc_trim(s + 7));
            if (current_mat < 0) { fclose(f); return current_mat; }
        } else if (strncmp(s, "mtllib ", 7u) == 0) {
            grc_load_mtl_file(path, grc_trim(s + 7), out_model);
        }
    }
    fclose(f);
    if (out_model->face_count > group_first_face) grc_finalize_obj_piece(out_model, group_name, group_first_face, group_first_vertex);
    return GRC_OK;
}

static void grc_make_sidecar_path(char *out, size_t out_size, const char *path, const char *ext)
{
    size_t len;
    size_t dot;
    if (out == NULL || out_size == 0u) return;
    out[0] = '\0';
    if (path == NULL || ext == NULL) return;
    strncpy(out, path, out_size - 1u);
    out[out_size - 1u] = '\0';
    len = strlen(out);
    dot = len;
    while (dot > 0u) {
        if (out[dot - 1u] == '.') break;
        if (out[dot - 1u] == '/' || out[dot - 1u] == '\\') { dot = len + 1u; break; }
        dot--;
    }
    if (dot == 0u || dot > len) dot = len;
    if (dot < out_size) out[dot] = '\0';
    strncat(out, ext, out_size - strlen(out) - 1u);
}

static int grc_save_mtl_sidecar(const char *path, const grc_model *model, char *mtl_base, size_t mtl_base_size)
{
    FILE *f;
    char mtl_path[512];
    const char *base;
    const char *p;
    grc_u32 i;
    if (path == NULL || model == NULL) return GRC_ERR_BAD_ARGUMENT;
    grc_make_sidecar_path(mtl_path, sizeof(mtl_path), path, "mtl");
    base = mtl_path;
    for (p = mtl_path; *p != '\0'; ++p) if (*p == '/' || *p == '\\') base = p + 1;
    if (mtl_base != NULL && mtl_base_size > 0u) {
        size_t bi;
        for (bi = 0u; bi + 1u < mtl_base_size && base[bi] != '\0'; ++bi) mtl_base[bi] = base[bi];
        mtl_base[bi] = '\0';
    }
    f = fopen(mtl_path, "w");
    if (f == NULL) return GRC_ERR_IO;
    fprintf(f, "# Written by Giffy RE Codec C89\n");
    for (i = 0u; i < model->material_count; ++i) {
        fprintf(f, "newmtl %s\n", model->materials[i].name);
        fprintf(f, "Kd %.9g %.9g %.9g\n", model->materials[i].r / 255.0, model->materials[i].g / 255.0, model->materials[i].b / 255.0);
        fprintf(f, "d %.9g\n", model->materials[i].a / 255.0);
        fprintf(f, "\n");
    }
    if (ferror(f)) { fclose(f); return GRC_ERR_IO; }
    fclose(f);
    return GRC_OK;
}


static void grc_write_obj_face(FILE *f, const grc_model *model, const grc_face *fa, grc_u32 *last_mat)
{
    grc_u32 j;
    if (fa->material_index >= 0 && (grc_u32)fa->material_index < model->material_count && (grc_u32)fa->material_index != *last_mat) {
        fprintf(f, "usemtl %s\n", model->materials[fa->material_index].name);
        *last_mat = (grc_u32)fa->material_index;
    }
    fprintf(f, "f");
    for (j = 0u; j < fa->vertex_count; ++j) {
        int v;
        int vt;
        int vn;
        v = fa->v[j] + 1;
        vt = fa->vt[j] + 1;
        vn = fa->vn[j] + 1;
        if (fa->vt[j] >= 0 && fa->vn[j] >= 0) fprintf(f, " %d/%d/%d", v, vt, vn);
        else if (fa->vt[j] >= 0) fprintf(f, " %d/%d", v, vt);
        else if (fa->vn[j] >= 0) fprintf(f, " %d//%d", v, vn);
        else fprintf(f, " %d", v);
    }
    fprintf(f, "\n");
}

int grc_save_obj_ex(const char *path, const grc_model *model, int write_mtl)
{
    FILE *f;
    grc_u32 i;
    grc_u32 last_mat;
    char mtl_base[256];
    if (path == NULL || model == NULL) return GRC_ERR_BAD_ARGUMENT;
    mtl_base[0] = '\0';
    if (write_mtl && model->material_count > 0u) {
        int rc;
        rc = grc_save_mtl_sidecar(path, model, mtl_base, sizeof(mtl_base));
        if (rc != GRC_OK) return rc;
    }
    f = fopen(path, "w");
    if (f == NULL) return GRC_ERR_IO;
    fprintf(f, "# Written by Giffy RE Codec C89\n");
    if (mtl_base[0] != '\0') fprintf(f, "mtllib %s\n", mtl_base);
    fprintf(f, "o %s\n", model->name);
    for (i = 0u; i < model->vertex_count; ++i) {
        fprintf(f, "v %.9g %.9g %.9g\n", model->vertices[i].x, model->vertices[i].y, model->vertices[i].z);
    }
    for (i = 0u; i < model->uv_count; ++i) {
        fprintf(f, "vt %.9g %.9g\n", model->uvs[i].u, model->uvs[i].v);
    }
    for (i = 0u; i < model->normal_count; ++i) {
        fprintf(f, "vn %.9g %.9g %.9g\n", model->normals[i].x, model->normals[i].y, model->normals[i].z);
    }
    last_mat = 0xffffffffu;
    if (model->piece_count > 0u) {
        grc_u32 pi;
        for (pi = 0u; pi < model->piece_count; ++pi) {
            grc_u32 first;
            grc_u32 end;
            fprintf(f, "g %s\n", model->pieces[pi].name);
            first = model->pieces[pi].first_face;
            end = first + model->pieces[pi].face_count;
            if (first > model->face_count) first = model->face_count;
            if (end > model->face_count) end = model->face_count;
            for (i = first; i < end; ++i) grc_write_obj_face(f, model, &model->faces[i], &last_mat);
        }
    } else {
        for (i = 0u; i < model->face_count; ++i) grc_write_obj_face(f, model, &model->faces[i], &last_mat);
    }
    if (ferror(f)) { fclose(f); return GRC_ERR_IO; }
    fclose(f);
    return GRC_OK;
}

int grc_save_obj(const char *path, const grc_model *model)
{
    return grc_save_obj_ex(path, model, 1);
}

/* ------------------------------------------------------------------------- */
/* TMD import/export                                                         */
/* ------------------------------------------------------------------------- */

static int grc_range_ok(size_t pos, size_t len, size_t size)
{
    if (pos > size) return 0;
    if (len > size - pos) return 0;
    return 1;
}

static size_t grc_tmd_resolve_offset(grc_u32 off, grc_u32 count, grc_u32 item_size, grc_u32 flags, size_t object_entry_pos, size_t data_start, size_t size)
{
    size_t len;
    size_t candidates[4];
    int i;
    len = (size_t)count * (size_t)item_size;
    candidates[0] = (size_t)off;
    candidates[1] = (size_t)off + 12u;
    candidates[2] = (size_t)off + object_entry_pos;
    candidates[3] = (size_t)off + data_start;
    if ((flags & 1u) != 0u) {
        if (grc_range_ok(candidates[0], len, size) && candidates[0] >= data_start) return candidates[0];
    }
    for (i = 0; i < 4; ++i) {
        if (grc_range_ok(candidates[i], len, size) && (count == 0u || candidates[i] >= data_start || candidates[i] == 0u)) return candidates[i];
    }
    return (size_t)0u;
}

static int grc_tmd_extract_indices(const grc_u8 *packet, grc_u32 bytes, grc_u32 vertex_count, grc_u32 want, grc_u16 *out)
{
    int start;
    int i;
    int ok;
    if (bytes < want * 2u) return 0;
    /* Fast path for the flat/untextured packets written by this codec:
       colour RGBA/CODE dword followed by vertex indices. */
    if (bytes >= 4u + want * 2u) {
        ok = 1;
        for (i = 0; i < (int)want; ++i) {
            grc_u16 idx;
            idx = grc_rd16(packet + 4u + i * 2);
            if ((grc_u32)idx >= vertex_count) { ok = 0; break; }
        }
        if (ok) {
            for (i = 0; i < (int)want; ++i) out[i] = grc_rd16(packet + 4u + i * 2);
            return 1;
        }
    }
    start = (int)bytes - (int)(want * 2u);
    while (start >= 0) {
        ok = 1;
        for (i = 0; i < (int)want; ++i) {
            grc_u16 idx;
            idx = grc_rd16(packet + start + i * 2);
            if ((grc_u32)idx >= vertex_count) { ok = 0; break; }
        }
        if (ok) {
            for (i = 0; i < (int)want; ++i) out[i] = grc_rd16(packet + start + i * 2);
            return 1;
        }
        start -= 2;
    }
    return 0;
}

static int grc_load_tmd_from_memory(const grc_u8 *data, size_t size, grc_model *out_model, const grc_options *opt)
{
    grc_u32 id;
    grc_u32 flags;
    grc_u32 obj_count;
    grc_u32 oi;
    float scale;
    int rc;
    if (data == NULL || out_model == NULL) return GRC_ERR_BAD_ARGUMENT;
    if (size < 12u) return GRC_ERR_BAD_FORMAT;
    id = grc_rd32(data);
    if (id != GRC_TMD_ID_A && id != GRC_TMD_ID_B) return GRC_ERR_BAD_FORMAT;
    flags = grc_rd32(data + 4);
    obj_count = grc_rd32(data + 8);
    if (obj_count > 2048u || !grc_range_ok(12u, (size_t)obj_count * 28u, size)) return GRC_ERR_BAD_FORMAT;
    scale = (opt != NULL && opt->scale != 0.0f) ? opt->scale : 1.0f;
    out_model->source_format = GRC_FORMAT_TMD;
    for (oi = 0u; oi < obj_count; ++oi) {
        size_t op;
        grc_u32 off_v;
        grc_u32 num_v;
        grc_u32 off_n;
        grc_u32 num_n;
        grc_u32 off_p;
        grc_u32 num_p;
        size_t data_start;
        size_t vp;
        size_t np;
        size_t pp;
        grc_u32 base_v;
        grc_u32 base_n;
        grc_u32 piece_uv_start;
        grc_u32 piece_face_start;
        grc_u32 pi;
        op = 12u + (size_t)oi * 28u;
        off_v = grc_rd32(data + op + 0u);
        num_v = grc_rd32(data + op + 4u);
        off_n = grc_rd32(data + op + 8u);
        num_n = grc_rd32(data + op + 12u);
        off_p = grc_rd32(data + op + 16u);
        num_p = grc_rd32(data + op + 20u);
        data_start = 12u + (size_t)obj_count * 28u;
        vp = grc_tmd_resolve_offset(off_v, num_v, 8u, flags, op, data_start, size);
        np = grc_tmd_resolve_offset(off_n, num_n, 8u, flags, op, data_start, size);
        pp = grc_tmd_resolve_offset(off_p, 1u, 4u, flags, op, data_start, size);
        if (num_v > 0u && vp == 0u) return GRC_ERR_BAD_FORMAT;
        if (num_p > 0u && pp == 0u) return GRC_ERR_BAD_FORMAT;
        base_v = out_model->vertex_count;
        for (pi = 0u; pi < num_v; ++pi) {
            const grc_u8 *v;
            v = data + vp + (size_t)pi * 8u;
            rc = grc_model_add_vertex(out_model, (float)grc_rds16(v) * scale, (float)grc_rds16(v + 2) * scale, (float)grc_rds16(v + 4) * scale);
            if (rc < 0) return rc;
        }
        base_n = out_model->normal_count;
        if (np != 0u) {
            for (pi = 0u; pi < num_n; ++pi) {
                const grc_u8 *n;
                n = data + np + (size_t)pi * 8u;
                rc = grc_model_add_normal(out_model, (float)grc_rds16(n) / 4096.0f, (float)grc_rds16(n + 2) / 4096.0f, (float)grc_rds16(n + 4) / 4096.0f);
                if (rc < 0) return rc;
            }
        }
        piece_uv_start = out_model->uv_count;
        piece_face_start = out_model->face_count;
        for (pi = 0u; pi < num_p; ++pi) {
            grc_u8 olen;
            grc_u8 ilen;
            grc_u8 pflag;
            grc_u8 mode;
            grc_u32 bytes;
            grc_u32 vc;
            grc_u16 idx[4];
            grc_face fa;
            grc_u32 k;
            if (!grc_range_ok(pp, 4u, size)) break;
            olen = data[pp + 0u];
            ilen = data[pp + 1u];
            pflag = data[pp + 2u];
            mode = data[pp + 3u];
            (void)olen;
            (void)pflag;
            bytes = (grc_u32)ilen * 4u;
            if (bytes == 0u || !grc_range_ok(pp + 4u, bytes, size)) break;
            vc = ((mode & 0x08u) != 0u) ? 4u : 3u;
            if ((mode & 0x20u) == 0u) vc = 3u;
            if (grc_tmd_extract_indices(data + pp + 4u, bytes, num_v, vc, idx)) {
                grc_face_defaults(&fa);
                fa.vertex_count = vc;
                if (bytes >= 4u) {
                    fa.r = data[pp + 4u];
                    fa.g = data[pp + 5u];
                    fa.b = data[pp + 6u];
                    fa.a = 255u;
                }
                for (k = 0u; k < vc; ++k) {
                    fa.v[k] = (grc_i32)(base_v + idx[k]);
                    if (idx[k] < num_n) fa.vn[k] = (grc_i32)(base_n + idx[k]);
                }
                rc = grc_model_add_face(out_model, &fa);
                if (rc < 0) return rc;
            }
            pp += 4u + (size_t)bytes;
        }
        rc = grc_add_piece_range(out_model, "tmd_obj", oi, base_v, num_v, base_n, num_n, piece_uv_start, out_model->uv_count - piece_uv_start, piece_face_start, out_model->face_count - piece_face_start);
        if (rc < 0) return rc;
    }
    return GRC_OK;
}

int grc_load_tmd(const char *path, grc_model *out_model, const grc_options *opt)
{
    grc_u8 *data;
    size_t size;
    int rc;
    if (path == NULL || out_model == NULL) return GRC_ERR_BAD_ARGUMENT;
    grc_model_init(out_model);
    rc = grc_load_file_bytes(path, &data, &size);
    if (rc != GRC_OK) return rc;
    rc = grc_load_tmd_from_memory(data, size, out_model, opt);
    free(data);
    return rc;
}

static grc_i16 grc_clamp_i16(float x)
{
    long v;
    if (x < -32768.0f) return (grc_i16)-32768;
    if (x > 32767.0f) return (grc_i16)32767;
    v = (long)((x >= 0.0f) ? x + 0.5f : x - 0.5f);
    return (grc_i16)v;
}

static grc_u8 grc_face_mode(const grc_face *fa)
{
    return (fa->vertex_count == GRC_FACE_QUAD) ? 0x28u : 0x20u;
}

int grc_save_tmd(const char *path, const grc_model *model, const grc_options *opt)
{
    FILE *f;
    grc_u32 i;
    grc_u32 prim_bytes;
    grc_u32 prim_offset;
    grc_u32 vertex_offset;
    float inv_scale;
    const grc_face *fa;
    if (path == NULL || model == NULL) return GRC_ERR_BAD_ARGUMENT;
    if (model->vertex_count > 65535u) return GRC_ERR_OVERFLOW;
    inv_scale = (opt != NULL && opt->scale != 0.0f) ? (1.0f / opt->scale) : 1.0f;
    prim_bytes = 0u;
    for (i = 0u; i < model->face_count; ++i) prim_bytes += 16u;
    prim_offset = 12u + 28u;
    vertex_offset = prim_offset + prim_bytes;
    f = fopen(path, "wb");
    if (f == NULL) return GRC_ERR_IO;
    grc_wr32(f, GRC_TMD_ID_A);
    grc_wr32(f, 1u);
    grc_wr32(f, 1u);
    grc_wr32(f, vertex_offset);
    grc_wr32(f, model->vertex_count);
    grc_wr32(f, vertex_offset + model->vertex_count * 8u);
    grc_wr32(f, 0u);
    grc_wr32(f, prim_offset);
    grc_wr32(f, model->face_count);
    grc_wr32(f, 0u);
    for (i = 0u; i < model->face_count; ++i) {
        grc_u32 color;
        grc_u16 v0;
        grc_u16 v1;
        grc_u16 v2;
        grc_u16 v3;
        fa = &model->faces[i];
        if (fa->vertex_count != GRC_FACE_TRI && fa->vertex_count != GRC_FACE_QUAD) continue;
        fputc((fa->vertex_count == GRC_FACE_QUAD) ? 5 : 4, f);
        fputc(3, f);
        fputc(1, f);
        fputc(grc_face_mode(fa), f);
        color = ((grc_u32)grc_face_mode(fa) << 24) | ((grc_u32)fa->b << 16) | ((grc_u32)fa->g << 8) | (grc_u32)fa->r;
        grc_wr32(f, color);
        v0 = (grc_u16)((fa->v[0] < 0) ? 0 : fa->v[0]);
        v1 = (grc_u16)((fa->v[1] < 0) ? 0 : fa->v[1]);
        v2 = (grc_u16)((fa->v[2] < 0) ? 0 : fa->v[2]);
        v3 = (grc_u16)((fa->v[3] < 0) ? 0 : fa->v[3]);
        grc_wr16(f, v0); grc_wr16(f, v1); grc_wr16(f, v2);
        if (fa->vertex_count == GRC_FACE_QUAD) grc_wr16(f, v3);
        else grc_wr16(f, 0u);
    }
    for (i = 0u; i < model->vertex_count; ++i) {
        grc_wr16(f, (grc_u16)grc_clamp_i16(model->vertices[i].x * inv_scale));
        grc_wr16(f, (grc_u16)grc_clamp_i16(model->vertices[i].y * inv_scale));
        grc_wr16(f, (grc_u16)grc_clamp_i16(model->vertices[i].z * inv_scale));
        grc_wr16(f, 0u);
    }
    if (ferror(f)) { fclose(f); return GRC_ERR_IO; }
    fclose(f);
    return GRC_OK;
}

/* ------------------------------------------------------------------------- */
/* TIM decoder                                                               */
/* ------------------------------------------------------------------------- */

static void grc_psx1555_to_rgba(grc_u16 c, grc_u8 *out)
{
    grc_u8 r;
    grc_u8 g;
    grc_u8 b;
    r = (grc_u8)(((c & 31u) * 255u) / 31u);
    g = (grc_u8)((((c >> 5) & 31u) * 255u) / 31u);
    b = (grc_u8)((((c >> 10) & 31u) * 255u) / 31u);
    out[0] = r;
    out[1] = g;
    out[2] = b;
    out[3] = (c == 0u) ? 0u : 255u;
}

int grc_tim_decode_rgba(const grc_u8 *data, size_t size, grc_texture *out_texture, int palette_index)
{
    grc_u32 flags;
    grc_u32 type;
    grc_u32 has_clut;
    size_t pos;
    const grc_u8 *clut_data;
    grc_u32 clut_w;
    grc_u32 clut_h;
    grc_u32 pix_size;
    grc_u32 pix_w_hw;
    grc_u32 pix_h;
    const grc_u8 *pix;
    grc_u32 width;
    grc_u32 height;
    grc_u32 x;
    grc_u32 y;
    size_t out_bytes;
    if (data == NULL || out_texture == NULL) return GRC_ERR_BAD_ARGUMENT;
    grc_texture_init(out_texture);
    if (size < 20u || data[0] != 0x10u || data[1] != 0u) return GRC_ERR_BAD_FORMAT;
    flags = grc_rd32(data + 4);
    type = flags & 7u;
    has_clut = flags & 8u;
    pos = 8u;
    clut_data = NULL;
    clut_w = 0u;
    clut_h = 0u;
    if (has_clut != 0u) {
        grc_u32 sec_size;
        if (!grc_range_ok(pos, 12u, size)) return GRC_ERR_BAD_FORMAT;
        sec_size = grc_rd32(data + pos);
        clut_w = grc_rd16(data + pos + 8u);
        clut_h = grc_rd16(data + pos + 10u);
        if (sec_size < 12u || !grc_range_ok(pos, sec_size, size)) return GRC_ERR_BAD_FORMAT;
        clut_data = data + pos + 12u;
        pos += sec_size;
    }
    if (!grc_range_ok(pos, 12u, size)) return GRC_ERR_BAD_FORMAT;
    pix_size = grc_rd32(data + pos);
    pix_w_hw = grc_rd16(data + pos + 8u);
    pix_h = grc_rd16(data + pos + 10u);
    if (pix_size < 12u || !grc_range_ok(pos, pix_size, size)) return GRC_ERR_BAD_FORMAT;
    pix = data + pos + 12u;
    height = pix_h;
    if (type == 0u) width = pix_w_hw * 4u;
    else if (type == 1u) width = pix_w_hw * 2u;
    else if (type == 2u) width = pix_w_hw;
    else if (type == 3u) width = (pix_w_hw * 2u) / 3u;
    else return GRC_ERR_UNSUPPORTED;
    if (width == 0u || height == 0u) return GRC_ERR_BAD_FORMAT;
    out_bytes = ((size_t)width) * ((size_t)height) * 4u;
    out_texture->rgba = (grc_u8 *)malloc(out_bytes);
    if (out_texture->rgba == NULL) return GRC_ERR_MEMORY;
    out_texture->width = width;
    out_texture->height = height;
    strcpy(out_texture->name, "tim_texture");
    if (type == 0u || type == 1u) {
        grc_u32 colors_per_pal;
        const grc_u8 *pal;
        if (clut_data == NULL || clut_w == 0u || clut_h == 0u) return GRC_ERR_BAD_FORMAT;
        colors_per_pal = (type == 0u) ? 16u : 256u;
        if (palette_index < 0) palette_index = 0;
        if ((grc_u32)palette_index >= clut_h) palette_index = 0;
        pal = clut_data + (size_t)palette_index * (size_t)clut_w * 2u;
        for (y = 0u; y < height; ++y) {
            for (x = 0u; x < width; ++x) {
                grc_u32 idx;
                grc_u16 c;
                grc_u8 *dst;
                dst = out_texture->rgba + (((size_t)y * width + x) * 4u);
                if (type == 0u) {
                    grc_u8 packed;
                    packed = pix[y * pix_w_hw * 2u + x / 2u];
                    idx = ((x & 1u) == 0u) ? (packed & 15u) : ((packed >> 4) & 15u);
                } else {
                    idx = pix[y * pix_w_hw * 2u + x];
                }
                if (idx >= colors_per_pal || idx >= clut_w) idx = 0u;
                c = grc_rd16(pal + idx * 2u);
                grc_psx1555_to_rgba(c, dst);
            }
        }
    } else if (type == 2u) {
        for (y = 0u; y < height; ++y) {
            for (x = 0u; x < width; ++x) {
                grc_u8 *dst;
                grc_u16 c;
                dst = out_texture->rgba + (((size_t)y * width + x) * 4u);
                c = grc_rd16(pix + ((size_t)y * pix_w_hw + x) * 2u);
                grc_psx1555_to_rgba(c, dst);
            }
        }
    } else if (type == 3u) {
        for (y = 0u; y < height; ++y) {
            for (x = 0u; x < width; ++x) {
                const grc_u8 *src;
                grc_u8 *dst;
                src = pix + ((size_t)y * pix_w_hw * 2u) + (size_t)x * 3u;
                dst = out_texture->rgba + (((size_t)y * width + x) * 4u);
                dst[0] = src[0]; dst[1] = src[1]; dst[2] = src[2]; dst[3] = 255u;
            }
        }
    }
    return GRC_OK;
}

int grc_tim_load_rgba(const char *path, grc_texture *out_texture, int palette_index)
{
    grc_u8 *data;
    size_t size;
    int rc;
    if (path == NULL || out_texture == NULL) return GRC_ERR_BAD_ARGUMENT;
    rc = grc_load_file_bytes(path, &data, &size);
    if (rc != GRC_OK) return rc;
    rc = grc_tim_decode_rgba(data, size, out_texture, palette_index);
    free(data);
    return rc;
}

int grc_texture_save_ppm(const char *path, const grc_texture *texture)
{
    FILE *f;
    grc_u32 x;
    grc_u32 y;
    const grc_u8 *p;
    if (path == NULL || texture == NULL || texture->rgba == NULL) return GRC_ERR_BAD_ARGUMENT;
    f = fopen(path, "wb");
    if (f == NULL) return GRC_ERR_IO;
    fprintf(f, "P6\n%u %u\n255\n", texture->width, texture->height);
    for (y = 0u; y < texture->height; ++y) {
        for (x = 0u; x < texture->width; ++x) {
            p = texture->rgba + (((size_t)y * texture->width + x) * 4u);
            fputc(p[0], f); fputc(p[1], f); fputc(p[2], f);
        }
    }
    if (ferror(f)) { fclose(f); return GRC_ERR_IO; }
    fclose(f);
    return GRC_OK;
}

/* ------------------------------------------------------------------------- */
/* Resident Evil EMD/PLD/PLW/MD1/MD2 import/export                           */
/* ------------------------------------------------------------------------- */

static int grc_read_end_dir4(const grc_u8 *data, size_t size, grc_section_table *out_table)
{
    grc_u32 offsets[4];
    grc_u32 i;
    if (size < 16u) return GRC_ERR_BAD_FORMAT;
    for (i = 0u; i < 4u; ++i) offsets[i] = grc_rd32(data + size - 16u + (size_t)i * 4u);
    for (i = 0u; i < 4u; ++i) {
        if ((size_t)offsets[i] >= size) return GRC_ERR_BAD_FORMAT;
        if (i > 0u && offsets[i] < offsets[i - 1u]) return GRC_ERR_BAD_FORMAT;
    }
    out_table->sections = (grc_section *)calloc(4u, sizeof(grc_section));
    if (out_table->sections == NULL) return GRC_ERR_MEMORY;
    out_table->section_count = 4u;
    for (i = 0u; i < 4u; ++i) {
        out_table->sections[i].offset = offsets[i];
        if (i + 1u < 4u) out_table->sections[i].size = offsets[i + 1u] - offsets[i];
        else out_table->sections[i].size = (grc_u32)(size - 16u - offsets[i]);
        out_table->sections[i].kind = 0u;
    }
    return GRC_OK;
}


static int grc_read_end_dir5_re1_anim(const grc_u8 *data, size_t size, grc_section_table *out_table)
{
    grc_u32 offsets[5];
    grc_u32 starts[6];
    grc_u32 dir_start;
    grc_u32 i;
    (void)data;
    if (size < 20u) return GRC_ERR_BAD_FORMAT;
    dir_start = (grc_u32)(size - 20u);
    for (i = 0u; i < 5u; ++i) offsets[i] = grc_rd32(data + size - 20u + (size_t)i * 4u);
    if (offsets[0] == 0u) return GRC_ERR_BAD_FORMAT;
    for (i = 0u; i < 5u; ++i) {
        if (offsets[i] >= dir_start) return GRC_ERR_BAD_FORMAT;
        if (i > 0u && offsets[i] < offsets[i - 1u]) return GRC_ERR_BAD_FORMAT;
    }
    starts[0] = 0u;
    for (i = 0u; i < 5u; ++i) starts[i + 1u] = offsets[i];
    out_table->sections = (grc_section *)calloc(6u, sizeof(grc_section));
    if (out_table->sections == NULL) return GRC_ERR_MEMORY;
    out_table->section_count = 6u;
    for (i = 0u; i < 6u; ++i) {
        out_table->sections[i].offset = starts[i];
        if (i + 1u < 6u) out_table->sections[i].size = starts[i + 1u] - starts[i];
        else out_table->sections[i].size = dir_start - starts[i];
        out_table->sections[i].kind = 0u;
    }
    return GRC_OK;
}

static int grc_read_header_dir(const grc_u8 *data, size_t size, grc_section_table *out_table)
{
    grc_u32 dir_offset;
    grc_u32 dir_count;
    grc_u32 i;
    grc_u32 *offsets;
    if (size < 8u) return GRC_ERR_BAD_FORMAT;
    dir_offset = grc_rd32(data + 0u);
    dir_count = grc_rd32(data + 4u);
    if (dir_count == 0u || dir_count > 256u) return GRC_ERR_BAD_FORMAT;
    if (!grc_range_ok(dir_offset, (size_t)dir_count * 4u, size)) return GRC_ERR_BAD_FORMAT;
    offsets = (grc_u32 *)calloc(dir_count, sizeof(grc_u32));
    if (offsets == NULL) return GRC_ERR_MEMORY;
    for (i = 0u; i < dir_count; ++i) {
        offsets[i] = grc_rd32(data + dir_offset + (size_t)i * 4u);
        if ((size_t)offsets[i] >= size) { free(offsets); return GRC_ERR_BAD_FORMAT; }
        if (i > 0u && offsets[i] < offsets[i - 1u]) { free(offsets); return GRC_ERR_BAD_FORMAT; }
    }
    out_table->sections = (grc_section *)calloc(dir_count, sizeof(grc_section));
    if (out_table->sections == NULL) { free(offsets); return GRC_ERR_MEMORY; }
    out_table->section_count = dir_count;
    for (i = 0u; i < dir_count; ++i) {
        out_table->sections[i].offset = offsets[i];
        if (i + 1u < dir_count) out_table->sections[i].size = offsets[i + 1u] - offsets[i];
        else out_table->sections[i].size = dir_offset - offsets[i];
        out_table->sections[i].kind = 0u;
    }
    free(offsets);
    return GRC_OK;
}

int grc_re_read_section_table(const grc_u8 *data, size_t size, grc_section_table *out_table, grc_u32 format_hint)
{
    int rc;
    if (data == NULL || out_table == NULL) return GRC_ERR_BAD_ARGUMENT;
    out_table->sections = NULL;
    out_table->section_count = 0u;
    if (format_hint == GRC_FORMAT_EMD) {
        rc = grc_read_header_dir(data, size, out_table);
        if (rc == GRC_OK) return rc;
        rc = grc_read_end_dir5_re1_anim(data, size, out_table);
        if (rc == GRC_OK) return rc;
        return grc_read_end_dir4(data, size, out_table);
    }
    if (format_hint == GRC_FORMAT_PLD || format_hint == GRC_FORMAT_PLW) return grc_read_header_dir(data, size, out_table);
    rc = grc_read_header_dir(data, size, out_table);
    if (rc == GRC_OK) return rc;
    return grc_read_end_dir4(data, size, out_table);
}


static float grc_re1_calc_pos_word(grc_u16 v)
{
    /* RE1MV Calculate_Pos(): world/model words are sign-flipped and scaled.
       RE1MV then renders TMD vertices as (Z,Y,X), so the caller handles axis order. */
    if (v > 32767u) return (float)(65535u - (grc_u32)v) / 100.0f;
    return -((float)v) / 100.0f;
}

static int grc_add_re1_vertex_array_i16(const grc_u8 *sec, size_t sec_size, size_t pos, grc_u32 count, grc_model *model, float scale, grc_u32 *base_out)
{
    grc_u32 i;
    int rc;
    if (!grc_range_ok(pos, (size_t)count * 8u, sec_size)) return GRC_ERR_BAD_FORMAT;
    *base_out = model->vertex_count;
    for (i = 0u; i < count; ++i) {
        const grc_u8 *p;
        float fx;
        float fy;
        float fz;
        p = sec + pos + (size_t)i * 8u;
        /* RE1MV reads file X,Y,Z, but calls glVertex3f(Z,Y,X). */
        fx = grc_re1_calc_pos_word(grc_rd16(p + 4u)) * scale;
        fy = grc_re1_calc_pos_word(grc_rd16(p + 2u)) * scale;
        fz = grc_re1_calc_pos_word(grc_rd16(p + 0u)) * scale;
        rc = grc_model_add_vertex(model, fx, fy, fz);
        if (rc < 0) return rc;
    }
    return GRC_OK;
}

static int grc_add_re1_normal_array_i16(const grc_u8 *sec, size_t sec_size, size_t pos, grc_u32 count, grc_model *model, grc_u32 *base_out)
{
    grc_u32 i;
    int rc;
    if (count == 0u) { *base_out = model->normal_count; return GRC_OK; }
    if (!grc_range_ok(pos, (size_t)count * 8u, sec_size)) return GRC_ERR_BAD_FORMAT;
    *base_out = model->normal_count;
    for (i = 0u; i < count; ++i) {
        const grc_u8 *p;
        float fx;
        float fy;
        float fz;
        p = sec + pos + (size_t)i * 8u;
        /* Keep RE1MV axis order and normalize back from Calculate_Pos() units. */
        fx = grc_re1_calc_pos_word(grc_rd16(p + 4u)) / 100.0f;
        fy = grc_re1_calc_pos_word(grc_rd16(p + 2u)) / 100.0f;
        fz = grc_re1_calc_pos_word(grc_rd16(p + 0u)) / 100.0f;
        rc = grc_model_add_normal(model, fx, fy, fz);
        if (rc < 0) return rc;
    }
    return GRC_OK;
}

static int grc_add_vertex_array_i16(const grc_u8 *sec, size_t sec_size, size_t pos, grc_u32 count, grc_model *model, float scale, grc_u32 *base_out)
{
    grc_u32 i;
    int rc;
    if (!grc_range_ok(pos, (size_t)count * 8u, sec_size)) return GRC_ERR_BAD_FORMAT;
    *base_out = model->vertex_count;
    for (i = 0u; i < count; ++i) {
        const grc_u8 *p;
        p = sec + pos + (size_t)i * 8u;
        rc = grc_model_add_vertex(model, (float)grc_rds16(p) * scale, (float)grc_rds16(p + 2) * scale, (float)grc_rds16(p + 4) * scale);
        if (rc < 0) return rc;
    }
    return GRC_OK;
}

static int grc_add_normal_array_i16(const grc_u8 *sec, size_t sec_size, size_t pos, grc_u32 count, grc_model *model, grc_u32 *base_out)
{
    grc_u32 i;
    int rc;
    if (count == 0u) { *base_out = model->normal_count; return GRC_OK; }
    if (!grc_range_ok(pos, (size_t)count * 8u, sec_size)) return GRC_ERR_BAD_FORMAT;
    *base_out = model->normal_count;
    for (i = 0u; i < count; ++i) {
        const grc_u8 *p;
        p = sec + pos + (size_t)i * 8u;
        rc = grc_model_add_normal(model, (float)grc_rds16(p) / 4096.0f, (float)grc_rds16(p + 2) / 4096.0f, (float)grc_rds16(p + 4) / 4096.0f);
        if (rc < 0) return rc;
    }
    return GRC_OK;
}

static int grc_parse_re1_mesh_section(const grc_u8 *sec, size_t sec_size, grc_model *model, float scale)
{
    grc_u32 obj_count;
    grc_u32 oi;
    grc_u32 start_faces;
    if (sec_size < 12u) return GRC_ERR_BAD_FORMAT;
    start_faces = model->face_count;
    obj_count = grc_rd32(sec + 8u);
    if (obj_count == 0u || obj_count > 2048u) return GRC_ERR_BAD_FORMAT;
    if (!grc_range_ok(12u, (size_t)obj_count * 28u, sec_size)) return GRC_ERR_BAD_FORMAT;
    for (oi = 0u; oi < obj_count; ++oi) {
        size_t dpos;
        grc_u32 vertex_offset;
        grc_u32 vertex_count;
        grc_u32 normal_offset;
        grc_u32 normal_count;
        grc_u32 tri_offset;
        grc_u32 tri_count;
        grc_u32 base_v;
        grc_u32 base_n;
        grc_u32 piece_uv_start;
        grc_u32 piece_face_start;
        grc_u32 ti;
        int rc;
        dpos = 12u + (size_t)oi * 28u;
        vertex_offset = grc_rd32(sec + dpos + 0u);
        vertex_count = grc_rd32(sec + dpos + 4u);
        normal_offset = grc_rd32(sec + dpos + 8u);
        normal_count = grc_rd32(sec + dpos + 12u);
        tri_offset = grc_rd32(sec + dpos + 16u);
        tri_count = grc_rd32(sec + dpos + 20u);
        if (vertex_count > 65535u || tri_count > 65535u) return GRC_ERR_BAD_FORMAT;
        rc = grc_add_re1_vertex_array_i16(sec, sec_size, 12u + vertex_offset, vertex_count, model, scale, &base_v);
        if (rc != GRC_OK) return rc;
        rc = grc_add_re1_normal_array_i16(sec, sec_size, 12u + normal_offset, normal_count, model, &base_n);
        if (rc != GRC_OK) return rc;
        piece_uv_start = model->uv_count;
        piece_face_start = model->face_count;
        if (!grc_range_ok(12u + tri_offset, (size_t)tri_count * 28u, sec_size)) continue;
        for (ti = 0u; ti < tri_count; ++ti) {
            const grc_u8 *t;
            grc_face fa;
            grc_u16 n0;
            grc_u16 n1;
            grc_u16 n2;
            grc_u16 v0;
            grc_u16 v1;
            grc_u16 v2;
            t = sec + 12u + tri_offset + (size_t)ti * 28u;
            n0 = grc_rd16(t + 16u); v0 = grc_rd16(t + 18u);
            n1 = grc_rd16(t + 20u); v1 = grc_rd16(t + 22u);
            n2 = grc_rd16(t + 24u); v2 = grc_rd16(t + 26u);
            if ((grc_u32)v0 >= vertex_count || (grc_u32)v1 >= vertex_count || (grc_u32)v2 >= vertex_count) continue;
            grc_face_defaults(&fa);
            fa.vertex_count = GRC_FACE_TRI;
            fa.flags = GRC_FACE_FLAG_TEXTURED;
            fa.vt[0] = grc_model_add_uv(model, (float)t[4] / 255.0f, (float)t[5] / 255.0f);
            fa.vt[1] = grc_model_add_uv(model, (float)t[8] / 255.0f, (float)t[9] / 255.0f);
            fa.vt[2] = grc_model_add_uv(model, (float)t[12] / 255.0f, (float)t[13] / 255.0f);
            if ((grc_u32)n0 < normal_count) fa.vn[0] = (grc_i32)(base_n + n0);
            if ((grc_u32)n1 < normal_count) fa.vn[1] = (grc_i32)(base_n + n1);
            if ((grc_u32)n2 < normal_count) fa.vn[2] = (grc_i32)(base_n + n2);
            fa.v[0] = (grc_i32)(base_v + v0);
            fa.v[1] = (grc_i32)(base_v + v1);
            fa.v[2] = (grc_i32)(base_v + v2);
            rc = grc_model_add_face(model, &fa);
            if (rc < 0) return rc;
        }
        rc = grc_add_piece_range(model, "re1_limb", oi, base_v, vertex_count, base_n, normal_count, piece_uv_start, model->uv_count - piece_uv_start, piece_face_start, model->face_count - piece_face_start);
        if (rc < 0) return rc;
    }
    return (model->face_count > start_faces) ? GRC_OK : GRC_ERR_BAD_FORMAT;
}

static int grc_re2_clut_material_index(grc_model *model, grc_u16 clut_id)
{
    grc_u32 i;
    grc_material mat;
    char name[64];
    sprintf(name, "re2_clut_%u", (unsigned int)clut_id);
    for (i = 0u; i < model->material_count; ++i) {
        if (strcmp(model->materials[i].name, name) == 0) return (int)i;
    }
    memset(&mat, 0, sizeof(mat));
    strcpy(mat.name, name);
    mat.r = 200u;
    mat.g = 200u;
    mat.b = 200u;
    mat.a = 255u;
    mat.texture_index = 0;
    return grc_model_add_material(model, &mat);
}

static int grc_parse_re2_mesh_section(const grc_u8 *sec, size_t sec_size, grc_model *model, float scale)
{
    grc_u32 raw_mesh_count;
    grc_u32 mesh_count;
    grc_u32 mi;
    grc_u32 any_faces;
    size_t base_offset;

    if (sec == NULL || model == NULL) return GRC_ERR_BAD_ARGUMENT;
    if (sec_size < 12u) return GRC_ERR_BAD_FORMAT;

    raw_mesh_count = grc_rd32(sec + 8u);
    if (raw_mesh_count == 0u || raw_mesh_count > 8192u) return GRC_ERR_BAD_FORMAT;

    /*
       BioModels / RE2 MD1 layout:
       u32 length, u32 unknown, u32 meshCountTimesTwo;
       then meshCountTimesTwo / 2 descriptors, each 56 bytes:
       tri vertex/normal/index/tex tables + quad vertex/normal/index/tex tables.
       Older recodec builds accidentally treated every 28-byte half as its own
       object; this parser keeps the real BioModels limb granularity.
    */
    mesh_count = raw_mesh_count / 2u;
    if (mesh_count == 0u) mesh_count = raw_mesh_count;
    if (mesh_count > 4096u) return GRC_ERR_BAD_FORMAT;
    if (!grc_range_ok(12u, (size_t)mesh_count * 56u, sec_size)) return GRC_ERR_BAD_FORMAT;

    base_offset = 12u;
    any_faces = 0u;

    for (mi = 0u; mi < mesh_count; ++mi) {
        size_t dpos;
        grc_u32 tri_vertex_offset;
        grc_u32 tri_vertex_count;
        grc_u32 tri_normal_offset;
        grc_u32 tri_normal_count;
        grc_u32 tri_index_offset;
        grc_u32 tri_count;
        grc_u32 tri_texture_offset;
        grc_u32 quad_vertex_offset;
        grc_u32 quad_vertex_count;
        grc_u32 quad_normal_offset;
        grc_u32 quad_normal_count;
        grc_u32 quad_index_offset;
        grc_u32 quad_count;
        grc_u32 quad_texture_offset;
        grc_u32 vertex_offset;
        grc_u32 vertex_count;
        grc_u32 normal_offset;
        grc_u32 normal_count;
        grc_u32 base_v;
        grc_u32 base_n;
        grc_u32 piece_uv_start;
        grc_u32 piece_face_start;
        grc_u32 fi;
        int normal_ok;
        int rc;

        dpos = 12u + (size_t)mi * 56u;
        tri_vertex_offset = grc_rd32(sec + dpos + 0u);
        tri_vertex_count = grc_rd32(sec + dpos + 4u);
        tri_normal_offset = grc_rd32(sec + dpos + 8u);
        tri_normal_count = grc_rd32(sec + dpos + 12u);
        tri_index_offset = grc_rd32(sec + dpos + 16u);
        tri_count = grc_rd32(sec + dpos + 20u);
        tri_texture_offset = grc_rd32(sec + dpos + 24u);
        quad_vertex_offset = grc_rd32(sec + dpos + 28u);
        quad_vertex_count = grc_rd32(sec + dpos + 32u);
        quad_normal_offset = grc_rd32(sec + dpos + 36u);
        quad_normal_count = grc_rd32(sec + dpos + 40u);
        quad_index_offset = grc_rd32(sec + dpos + 44u);
        quad_count = grc_rd32(sec + dpos + 48u);
        quad_texture_offset = grc_rd32(sec + dpos + 52u);

        vertex_offset = tri_vertex_offset;
        vertex_count = tri_vertex_count;
        normal_offset = tri_normal_offset;
        normal_count = tri_normal_count;
        if (vertex_count == 0u && quad_vertex_count > 0u) {
            vertex_offset = quad_vertex_offset;
            vertex_count = quad_vertex_count;
        }
        if (normal_count == 0u && quad_normal_count > 0u) {
            normal_offset = quad_normal_offset;
            normal_count = quad_normal_count;
        }
        if (vertex_count == 0u || vertex_count > 65535u) continue;
        if (normal_count > 65535u) normal_count = 0u;

        rc = grc_add_vertex_array_i16(sec, sec_size, base_offset + vertex_offset, vertex_count, model, scale, &base_v);
        if (rc != GRC_OK) continue;

        base_n = model->normal_count;
        normal_ok = 0;
        if (normal_count > 0u) {
            rc = grc_add_normal_array_i16(sec, sec_size, base_offset + normal_offset, normal_count, model, &base_n);
            if (rc == GRC_OK) normal_ok = 1;
        }

        piece_uv_start = model->uv_count;
        piece_face_start = model->face_count;

        if (tri_count > 0u && tri_count <= 65535u && grc_range_ok(base_offset + tri_index_offset, (size_t)tri_count * 12u, sec_size)) {
            for (fi = 0u; fi < tri_count; ++fi) {
                const grc_u8 *idx;
                const grc_u8 *uv;
                grc_face fa;
                grc_u16 n0;
                grc_u16 n1;
                grc_u16 n2;
                grc_u16 v0;
                grc_u16 v1;
                grc_u16 v2;
                grc_u16 clut_id;
                int mi_idx;
                int uv0;
                int uv1;
                int uv2;

                idx = sec + base_offset + tri_index_offset + (size_t)fi * 12u;
                n0 = grc_rd16(idx + 0u); v0 = grc_rd16(idx + 2u);
                n1 = grc_rd16(idx + 4u); v1 = grc_rd16(idx + 6u);
                n2 = grc_rd16(idx + 8u); v2 = grc_rd16(idx + 10u);
                if ((grc_u32)v0 >= vertex_count || (grc_u32)v1 >= vertex_count || (grc_u32)v2 >= vertex_count) continue;

                grc_face_defaults(&fa);
                fa.vertex_count = GRC_FACE_TRI;
                fa.flags = GRC_FACE_FLAG_NATIVE_EXPERIMENTAL;
                fa.v[0] = (grc_i32)(base_v + v0);
                fa.v[1] = (grc_i32)(base_v + v1);
                fa.v[2] = (grc_i32)(base_v + v2);
                if (normal_ok && (grc_u32)n0 < normal_count) fa.vn[0] = (grc_i32)(base_n + n0);
                if (normal_ok && (grc_u32)n1 < normal_count) fa.vn[1] = (grc_i32)(base_n + n1);
                if (normal_ok && (grc_u32)n2 < normal_count) fa.vn[2] = (grc_i32)(base_n + n2);

                uv = NULL;
                if (tri_texture_offset != 0u && grc_range_ok(base_offset + tri_texture_offset + (size_t)fi * 12u, 12u, sec_size)) {
                    uv = sec + base_offset + tri_texture_offset + (size_t)fi * 12u;
                }
                if (uv != NULL) {
                    clut_id = grc_rd16(uv + 6u);
                    mi_idx = grc_re2_clut_material_index(model, clut_id);
                    if (mi_idx < 0) return mi_idx;
                    uv0 = grc_model_add_uv(model, (float)uv[0] / 255.0f, (float)uv[1] / 255.0f);
                    uv1 = grc_model_add_uv(model, (float)uv[4] / 255.0f, (float)uv[5] / 255.0f);
                    uv2 = grc_model_add_uv(model, (float)uv[8] / 255.0f, (float)uv[9] / 255.0f);
                    if (uv0 < 0 || uv1 < 0 || uv2 < 0) return GRC_ERR_MEMORY;
                    fa.flags |= GRC_FACE_FLAG_TEXTURED;
                    fa.material_index = mi_idx;
                    fa.vt[0] = uv0;
                    fa.vt[1] = uv1;
                    fa.vt[2] = uv2;
                }
                rc = grc_model_add_face(model, &fa);
                if (rc < 0) return rc;
                any_faces++;
            }
        }

        if (quad_count > 0u && quad_count <= 65535u && grc_range_ok(base_offset + quad_index_offset, (size_t)quad_count * 16u, sec_size)) {
            for (fi = 0u; fi < quad_count; ++fi) {
                const grc_u8 *idx;
                const grc_u8 *uv;
                grc_face fa;
                grc_u16 n0;
                grc_u16 n1;
                grc_u16 n2;
                grc_u16 n3;
                grc_u16 v0;
                grc_u16 v1;
                grc_u16 v2;
                grc_u16 v3;
                grc_u16 clut_id;
                int mi_idx;
                int uv0;
                int uv1;
                int uv2;
                int uv3;

                idx = sec + base_offset + quad_index_offset + (size_t)fi * 16u;
                n0 = grc_rd16(idx + 0u);  v0 = grc_rd16(idx + 2u);
                n1 = grc_rd16(idx + 4u);  v1 = grc_rd16(idx + 6u);
                n2 = grc_rd16(idx + 8u);  v2 = grc_rd16(idx + 10u);
                n3 = grc_rd16(idx + 12u); v3 = grc_rd16(idx + 14u);
                if ((grc_u32)v0 >= vertex_count || (grc_u32)v1 >= vertex_count || (grc_u32)v2 >= vertex_count || (grc_u32)v3 >= vertex_count) continue;

                grc_face_defaults(&fa);
                fa.vertex_count = GRC_FACE_QUAD;
                fa.flags = GRC_FACE_FLAG_NATIVE_EXPERIMENTAL;
                fa.v[0] = (grc_i32)(base_v + v0);
                fa.v[1] = (grc_i32)(base_v + v1);
                fa.v[2] = (grc_i32)(base_v + v3);
                fa.v[3] = (grc_i32)(base_v + v2);
                if (normal_ok && (grc_u32)n0 < normal_count) fa.vn[0] = (grc_i32)(base_n + n0);
                if (normal_ok && (grc_u32)n1 < normal_count) fa.vn[1] = (grc_i32)(base_n + n1);
                if (normal_ok && (grc_u32)n3 < normal_count) fa.vn[2] = (grc_i32)(base_n + n3);
                if (normal_ok && (grc_u32)n2 < normal_count) fa.vn[3] = (grc_i32)(base_n + n2);

                uv = NULL;
                if (quad_texture_offset != 0u && grc_range_ok(base_offset + quad_texture_offset + (size_t)fi * 16u, 16u, sec_size)) {
                    uv = sec + base_offset + quad_texture_offset + (size_t)fi * 16u;
                }
                if (uv != NULL) {
                    clut_id = grc_rd16(uv + 6u);
                    mi_idx = grc_re2_clut_material_index(model, clut_id);
                    if (mi_idx < 0) return mi_idx;
                    uv0 = grc_model_add_uv(model, (float)uv[0] / 255.0f, (float)uv[1] / 255.0f);
                    uv1 = grc_model_add_uv(model, (float)uv[4] / 255.0f, (float)uv[5] / 255.0f);
                    uv2 = grc_model_add_uv(model, (float)uv[12] / 255.0f, (float)uv[13] / 255.0f);
                    uv3 = grc_model_add_uv(model, (float)uv[8] / 255.0f, (float)uv[9] / 255.0f);
                    if (uv0 < 0 || uv1 < 0 || uv2 < 0 || uv3 < 0) return GRC_ERR_MEMORY;
                    fa.flags |= GRC_FACE_FLAG_TEXTURED;
                    fa.material_index = mi_idx;
                    fa.vt[0] = uv0;
                    fa.vt[1] = uv1;
                    fa.vt[2] = uv2;
                    fa.vt[3] = uv3;
                }
                rc = grc_model_add_face(model, &fa);
                if (rc < 0) return rc;
                any_faces++;
            }
        }

        rc = grc_add_piece_range(model, "re2_limb", mi, base_v, vertex_count, base_n, normal_ok ? normal_count : 0u,
                                 piece_uv_start, model->uv_count - piece_uv_start,
                                 piece_face_start, model->face_count - piece_face_start);
        if (rc < 0) return rc;
    }

    return (any_faces > 0u) ? GRC_OK : GRC_ERR_BAD_FORMAT;
}

static void grc_apply_re2_skeleton_section(const grc_u8 *sec, size_t sec_size, grc_model *model, float scale)
{
    enum { GRC_RE2_MAX_BONES_LOCAL = 256 };
    grc_u32 bones_offset;
    grc_u32 bone_count;
    grc_u32 i;
    grc_i32 parent[GRC_RE2_MAX_BONES_LOCAL];
    grc_vec3 pivot[GRC_RE2_MAX_BONES_LOCAL];

    if (sec == NULL || model == NULL) return;
    if (sec_size < 8u || model->piece_count == 0u) return;

    bones_offset = grc_rd16(sec + 0u);
    bone_count = grc_rd16(sec + 4u);
    if (bone_count == 0u || bone_count > GRC_RE2_MAX_BONES_LOCAL) return;
    if (!grc_range_ok(8u, (size_t)bone_count * 6u, sec_size)) return;
    if (!grc_range_ok(bones_offset, (size_t)bone_count * 4u, sec_size)) return;

    for (i = 0u; i < bone_count; ++i) {
        size_t rpos;
        parent[i] = -1;
        rpos = 8u + (size_t)i * 6u;
        pivot[i].x = (float)grc_rds16(sec + rpos + 0u) * scale;
        pivot[i].y = (float)grc_rds16(sec + rpos + 2u) * scale;
        pivot[i].z = (float)grc_rds16(sec + rpos + 4u) * scale;
    }

    for (i = 0u; i < bone_count; ++i) {
        size_t bpos;
        grc_u32 child_count;
        grc_u32 child_offset;
        grc_u32 j;
        bpos = (size_t)bones_offset + (size_t)i * 4u;
        child_count = grc_rd16(sec + bpos + 0u);
        child_offset = grc_rd16(sec + bpos + 2u);
        if (child_count > 128u) continue;
        if (!grc_range_ok((size_t)bones_offset + child_offset, child_count, sec_size)) continue;
        for (j = 0u; j < child_count; ++j) {
            grc_u32 child;
            child = sec[(size_t)bones_offset + child_offset + j];
            if (child < bone_count && child != i && parent[child] < 0) parent[child] = (grc_i32)i;
        }
    }

    for (i = 0u; i < model->piece_count; ++i) {
        grc_u32 id;
        id = model->pieces[i].native_id;
        if (id < bone_count) {
            model->pieces[i].parent_index = parent[id];
            model->pieces[i].pivot = pivot[id];
        }
    }
}

static int grc_parse_re3_mesh_section(const grc_u8 *sec, size_t sec_size, grc_model *model, float scale)
{
    grc_u32 obj_count;
    grc_u32 oi;
    if (sec_size < 8u) return GRC_ERR_BAD_FORMAT;
    obj_count = grc_rd32(sec + 4u);
    if (obj_count == 0u || obj_count > 4096u) return GRC_ERR_BAD_FORMAT;
    if (!grc_range_ok(8u, (size_t)obj_count * 24u, sec_size)) return GRC_ERR_BAD_FORMAT;
    for (oi = 0u; oi < obj_count; ++oi) {
        size_t dpos;
        grc_u32 vtx_offset;
        grc_u32 nor_offset;
        grc_u32 vtx_count;
        grc_u32 tri_offset;
        grc_u32 quad_offset;
        grc_u32 tri_count;
        grc_u32 quad_count;
        grc_u32 base_v;
        grc_u32 base_n;
        grc_u32 piece_uv_start;
        grc_u32 piece_face_start;
        grc_u32 fi;
        int rc;
        dpos = 8u + (size_t)oi * 24u;
        vtx_offset = grc_rd16(sec + dpos + 0u);
        nor_offset = grc_rd16(sec + dpos + 4u);
        vtx_count = grc_rd16(sec + dpos + 8u);
        tri_offset = grc_rd16(sec + dpos + 12u);
        quad_offset = grc_rd16(sec + dpos + 16u);
        tri_count = grc_rd16(sec + dpos + 20u);
        quad_count = grc_rd16(sec + dpos + 22u);
        if (tri_count == tri_offset || tri_count == quad_offset) tri_count = 0u;
        if (quad_count > 256u) quad_count = 0u;
        if (vtx_count == 0u || vtx_count > 256u) continue;
        rc = grc_add_vertex_array_i16(sec, sec_size, 8u + vtx_offset, vtx_count, model, scale, &base_v);
        if (rc != GRC_OK) continue;
        rc = grc_add_normal_array_i16(sec, sec_size, 8u + nor_offset, vtx_count, model, &base_n);
        if (rc != GRC_OK) base_n = 0u;
        piece_uv_start = model->uv_count;
        piece_face_start = model->face_count;
        if (tri_count > 0u && grc_range_ok(8u + tri_offset, (size_t)tri_count * 12u, sec_size)) {
            for (fi = 0u; fi < tri_count; ++fi) {
                const grc_u8 *t;
                grc_face fa;
                t = sec + 8u + tri_offset + (size_t)fi * 12u;
                grc_face_defaults(&fa);
                fa.vertex_count = GRC_FACE_TRI;
                fa.flags = GRC_FACE_FLAG_TEXTURED | GRC_FACE_FLAG_NATIVE_EXPERIMENTAL;
                fa.vt[0] = grc_model_add_uv(model, (float)t[0] / 255.0f, (float)t[1] / 255.0f);
                fa.vt[1] = grc_model_add_uv(model, (float)t[4] / 255.0f, (float)t[5] / 255.0f);
                fa.vt[2] = grc_model_add_uv(model, (float)t[8] / 255.0f, (float)t[9] / 255.0f);
                fa.v[0] = (grc_i32)(base_v + t[7]);
                fa.v[1] = (grc_i32)(base_v + t[10]);
                fa.v[2] = (grc_i32)(base_v + t[11]);
                fa.vn[0] = (grc_i32)(base_n + t[7]);
                fa.vn[1] = (grc_i32)(base_n + t[10]);
                fa.vn[2] = (grc_i32)(base_n + t[11]);
                rc = grc_model_add_face(model, &fa);
                if (rc < 0) return rc;
            }
        }
        if (quad_count > 0u && grc_range_ok(8u + quad_offset, (size_t)quad_count * 16u, sec_size)) {
            for (fi = 0u; fi < quad_count; ++fi) {
                const grc_u8 *q;
                grc_face fa;
                q = sec + 8u + quad_offset + (size_t)fi * 16u;
                grc_face_defaults(&fa);
                fa.vertex_count = GRC_FACE_QUAD;
                fa.flags = GRC_FACE_FLAG_TEXTURED | GRC_FACE_FLAG_NATIVE_EXPERIMENTAL;
                fa.vt[0] = grc_model_add_uv(model, (float)q[0] / 255.0f, (float)q[1] / 255.0f);
                fa.vt[1] = grc_model_add_uv(model, (float)q[4] / 255.0f, (float)q[5] / 255.0f);
                fa.vt[2] = grc_model_add_uv(model, (float)q[12] / 255.0f, (float)q[13] / 255.0f);
                fa.vt[3] = grc_model_add_uv(model, (float)q[8] / 255.0f, (float)q[9] / 255.0f);
                fa.v[0] = (grc_i32)(base_v + q[10]);
                fa.v[1] = (grc_i32)(base_v + q[11]);
                fa.v[2] = (grc_i32)(base_v + q[15]);
                fa.v[3] = (grc_i32)(base_v + q[14]);
                fa.vn[0] = (grc_i32)(base_n + q[10]);
                fa.vn[1] = (grc_i32)(base_n + q[11]);
                fa.vn[2] = (grc_i32)(base_n + q[15]);
                fa.vn[3] = (grc_i32)(base_n + q[14]);
                rc = grc_model_add_face(model, &fa);
                if (rc < 0) return rc;
            }
        }
        rc = grc_add_piece_range(model, "re3_limb", oi, base_v, vtx_count, base_n, vtx_count, piece_uv_start, model->uv_count - piece_uv_start, piece_face_start, model->face_count - piece_face_start);
        if (rc < 0) return rc;
    }
    return (model->face_count > 0u) ? GRC_OK : GRC_ERR_BAD_FORMAT;
}

static int grc_load_emd_from_memory(const grc_u8 *data, size_t size, grc_model *out_model, grc_u32 hint, const grc_options *opt)
{
    grc_section_table table;
    float scale;
    int rc;
    grc_u32 fmt;
    grc_u32 mesh_index;

    if (data == NULL || out_model == NULL) return GRC_ERR_BAD_ARGUMENT;
    scale = (opt != NULL && opt->scale != 0.0f) ? opt->scale : 1.0f;
    grc_model_init(out_model);
    fmt = hint;
    if (fmt == GRC_FORMAT_UNKNOWN || fmt == GRC_FORMAT_AUTO) fmt = GRC_FORMAT_EMD;

    table.sections = NULL;
    table.section_count = 0u;
    rc = grc_re_read_section_table(data, size, &table, fmt);
    if (rc != GRC_OK) return rc;

    /* Real RE2 PLD/PLW: BioModels header has 4 offsets: EDD, EMR, MD1, TIM. */
    if ((fmt == GRC_FORMAT_PLD || fmt == GRC_FORMAT_PLW) && table.section_count == 4u) {
        mesh_index = 2u;
        if (table.section_count > mesh_index) {
            rc = grc_parse_re2_mesh_section(data + table.sections[mesh_index].offset, table.sections[mesh_index].size, out_model, scale);
            if (rc == GRC_OK) {
                if (table.section_count > 1u) {
                    grc_apply_re2_skeleton_section(data + table.sections[1u].offset, table.sections[1u].size, out_model, scale);
                }
                out_model->source_game = GRC_GAME_RE2;
                out_model->source_format = fmt;
                grc_section_table_free(&table);
                return GRC_OK;
            }
            grc_model_free(out_model);
            grc_model_init(out_model);
        }
    }

    /* Legacy recodec PLD/PLW writer used 5 sections; keep it loadable. */
    if ((fmt == GRC_FORMAT_PLD || fmt == GRC_FORMAT_PLW) && table.section_count != 4u) {
        mesh_index = 2u;
        if (table.section_count > mesh_index) {
            rc = grc_parse_re2_mesh_section(data + table.sections[mesh_index].offset, table.sections[mesh_index].size, out_model, scale);
            if (rc == GRC_OK) {
                out_model->source_game = GRC_GAME_RE3;
                out_model->source_format = fmt;
                grc_section_table_free(&table);
                return GRC_OK;
            }
            grc_model_free(out_model);
            grc_model_init(out_model);
        }
    }

    /* RE1 PSX EMD with trailing 5-offset directory: EMR_A, EDD_A, EMR_B, EDD_B, TMD, TIM. */
    if (fmt == GRC_FORMAT_EMD && table.section_count == 6u) {
        mesh_index = 4u;
        rc = grc_parse_re1_mesh_section(data + table.sections[mesh_index].offset, table.sections[mesh_index].size, out_model, scale);
        if (rc == GRC_OK) {
            out_model->source_game = GRC_GAME_RE1;
            out_model->source_format = GRC_FORMAT_EMD;
            grc_section_table_free(&table);
            return GRC_OK;
        }
        grc_model_free(out_model);
        grc_model_init(out_model);
    }

    /* RE3 EMD commonly exposes a larger table; keep this before RE2 EMD probing. */
    if (table.section_count >= 15u) {
        mesh_index = 14u;
        rc = grc_parse_re3_mesh_section(data + table.sections[mesh_index].offset, table.sections[mesh_index].size, out_model, scale);
        if (rc != GRC_OK) {
            grc_model_free(out_model);
            grc_model_init(out_model);
            if (table.section_count > 13u) rc = grc_parse_re3_mesh_section(data + table.sections[13u].offset, table.sections[13u].size, out_model, scale);
        }
        if (rc == GRC_OK) {
            out_model->source_game = GRC_GAME_RE3;
            out_model->source_format = GRC_FORMAT_EMD;
            grc_section_table_free(&table);
            return GRC_OK;
        }
        grc_model_free(out_model);
        grc_model_init(out_model);
    }

    /* RE2 EMD: 8 offsets. Entry 7 is MD1; entries 2/4/6 are candidate skeletons. */
    if (table.section_count >= 8u) {
        mesh_index = 7u;
        rc = grc_parse_re2_mesh_section(data + table.sections[mesh_index].offset, table.sections[mesh_index].size, out_model, scale);
        if (rc == GRC_OK) {
            if (table.section_count > 2u) {
                grc_apply_re2_skeleton_section(data + table.sections[2u].offset, table.sections[2u].size, out_model, scale);
            }
            if (table.section_count > 4u) {
                grc_apply_re2_skeleton_section(data + table.sections[4u].offset, table.sections[4u].size, out_model, scale);
            }
            if (table.section_count > 6u) {
                grc_apply_re2_skeleton_section(data + table.sections[6u].offset, table.sections[6u].size, out_model, scale);
            }
            out_model->source_game = GRC_GAME_RE2;
            out_model->source_format = GRC_FORMAT_EMD;
            grc_section_table_free(&table);
            return GRC_OK;
        }
        grc_model_free(out_model);
        grc_model_init(out_model);
    }

    /* Legacy / RE1-style 4-section EMD fallback. */
    if (table.section_count == 4u) {
        mesh_index = 2u;
        if (mesh_index < table.section_count) {
            rc = grc_parse_re1_mesh_section(data + table.sections[mesh_index].offset, table.sections[mesh_index].size, out_model, scale);
            if (rc == GRC_OK) {
                out_model->source_game = GRC_GAME_RE1;
                out_model->source_format = GRC_FORMAT_EMD;
                grc_section_table_free(&table);
                return GRC_OK;
            }
            grc_model_free(out_model);
            grc_model_init(out_model);

            rc = grc_parse_re2_mesh_section(data + table.sections[mesh_index].offset, table.sections[mesh_index].size, out_model, scale);
            if (rc == GRC_OK) {
                if (table.section_count > 1u) {
                    grc_apply_re2_skeleton_section(data + table.sections[1u].offset, table.sections[1u].size, out_model, scale);
                }
                out_model->source_game = GRC_GAME_RE2;
                out_model->source_format = GRC_FORMAT_EMD;
                grc_section_table_free(&table);
                return GRC_OK;
            }
            grc_model_free(out_model);
            grc_model_init(out_model);
        }
    }

    grc_section_table_free(&table);
    return GRC_ERR_UNSUPPORTED;
}

static int grc_attach_tim_blob(grc_model *model, const grc_u8 *data, size_t size, int palette_index)
{
    grc_texture tex;
    int rc;
    if (model == NULL || data == NULL || size < 8u) return GRC_ERR_BAD_ARGUMENT;
    if (data[0] != 0x10u || data[1] != 0u) return GRC_ERR_BAD_FORMAT;
    grc_texture_init(&tex);
    rc = grc_tim_decode_rgba(data, size, &tex, palette_index);
    if (rc == GRC_OK) {
        rc = grc_model_add_texture(model, &tex);
    }
    grc_texture_free(&tex);
    return rc;
}

static void grc_attach_tim_from_container(const grc_u8 *data, size_t size, grc_model *model, grc_u32 fmt, const grc_options *opt)
{
    grc_section_table table;
    int rc;
    grc_u32 i;
    int palette_index;
    if (data == NULL || model == NULL || model->texture_count > 0u) return;
    palette_index = (opt != NULL) ? opt->palette_index : 0;
    table.sections = NULL;
    table.section_count = 0u;
    rc = grc_re_read_section_table(data, size, &table, fmt);
    if (rc != GRC_OK) return;

    if ((fmt == GRC_FORMAT_PLD || fmt == GRC_FORMAT_PLW) && table.section_count > 3u) {
        grc_section *s;
        s = &table.sections[3u];
        if (s->size >= 8u && grc_range_ok(s->offset, s->size, size)) {
            rc = grc_attach_tim_blob(model, data + s->offset, s->size, palette_index);
            if (rc == GRC_OK) { grc_section_table_free(&table); return; }
        }
    }

    for (i = 0u; i < table.section_count; ++i) {
        grc_section *s;
        s = &table.sections[i];
        if (s->size >= 8u && grc_range_ok(s->offset, s->size, size) && data[s->offset] == 0x10u && data[s->offset + 1u] == 0u) {
            rc = grc_attach_tim_blob(model, data + s->offset, s->size, palette_index);
            if (rc == GRC_OK) break;
        }
    }
    grc_section_table_free(&table);
}

static void grc_attach_sidecar_tim(const char *path, grc_model *model, const grc_options *opt)
{
    const char *tries[3];
    char candidate[512];
    grc_texture tex;
    int palette_index;
    int rc;
    int i;
    if (path == NULL || model == NULL || model->texture_count > 0u) return;
    palette_index = (opt != NULL) ? opt->palette_index : 0;
    tries[0] = "TIM";
    tries[1] = "tim";
    tries[2] = "Tim";
    for (i = 0; i < 3; ++i) {
        grc_make_sidecar_path(candidate, sizeof(candidate), path, tries[i]);
        if (candidate[0] == '\0') continue;
        grc_texture_init(&tex);
        rc = grc_tim_load_rgba(candidate, &tex, palette_index);
        if (rc == GRC_OK) {
            rc = grc_model_add_texture(model, &tex);
            grc_texture_free(&tex);
            if (rc == GRC_OK) return;
        } else {
            grc_texture_free(&tex);
        }
    }
}

int grc_load_emd(const char *path, grc_model *out_model, const grc_options *opt)
{
    grc_u8 *data;
    size_t size;
    int rc;
    grc_u32 fmt;
    if (path == NULL || out_model == NULL) return GRC_ERR_BAD_ARGUMENT;
    fmt = grc_format_from_extension(path);
    rc = grc_load_file_bytes(path, &data, &size);
    if (rc != GRC_OK) return rc;
    rc = grc_load_emd_from_memory(data, size, out_model, fmt, opt);
    if (rc == GRC_OK) {
        grc_attach_tim_from_container(data, size, out_model, fmt, opt);
        if (out_model->texture_count == 0u && fmt == GRC_FORMAT_EMD) {
            grc_attach_sidecar_tim(path, out_model, opt);
        }
    }
    free(data);
    return rc;
}

static int grc_load_md1_from_memory(const grc_u8 *data, size_t size, grc_model *out_model, const grc_options *opt)
{
    float scale;
    int rc;
    scale = (opt != NULL && opt->scale != 0.0f) ? opt->scale : 1.0f;
    grc_model_init(out_model);
    rc = grc_parse_re2_mesh_section(data, size, out_model, scale);
    if (rc == GRC_OK) { out_model->source_game = GRC_GAME_RE2; out_model->source_format = GRC_FORMAT_MD1; }
    return rc;
}

static int grc_load_md2_from_memory(const grc_u8 *data, size_t size, grc_model *out_model, const grc_options *opt)
{
    float scale;
    int rc;
    scale = (opt != NULL && opt->scale != 0.0f) ? opt->scale : 1.0f;
    grc_model_init(out_model);
    rc = grc_parse_re3_mesh_section(data, size, out_model, scale);
    if (rc == GRC_OK) { out_model->source_game = GRC_GAME_RE3; out_model->source_format = GRC_FORMAT_MD2; }
    return rc;
}

int grc_load_md1(const char *path, grc_model *out_model, const grc_options *opt)
{
    grc_u8 *data;
    size_t size;
    int rc;
    if (path == NULL || out_model == NULL) return GRC_ERR_BAD_ARGUMENT;
    rc = grc_load_file_bytes(path, &data, &size);
    if (rc != GRC_OK) return rc;
    rc = grc_load_md1_from_memory(data, size, out_model, opt);
    free(data);
    return rc;
}

int grc_load_md2(const char *path, grc_model *out_model, const grc_options *opt)
{
    grc_u8 *data;
    size_t size;
    int rc;
    if (path == NULL || out_model == NULL) return GRC_ERR_BAD_ARGUMENT;
    rc = grc_load_file_bytes(path, &data, &size);
    if (rc != GRC_OK) return rc;
    rc = grc_load_md2_from_memory(data, size, out_model, opt);
    free(data);
    return rc;
}

static int grc_count_tris_quads_for_re2(const grc_model *model, grc_u32 *tri_count, grc_u32 *quad_count)
{
    grc_u32 i;
    *tri_count = 0u;
    *quad_count = 0u;
    for (i = 0u; i < model->face_count; ++i) {
        if (model->faces[i].vertex_count == GRC_FACE_TRI) (*tri_count)++;
        else if (model->faces[i].vertex_count == GRC_FACE_QUAD) (*quad_count)++;
    }
    return GRC_OK;
}

static void grc_write_vertex_i16(FILE *f, const grc_vec3 *v, float inv_scale)
{
    grc_wr16(f, (grc_u16)grc_clamp_i16(v->x * inv_scale));
    grc_wr16(f, (grc_u16)grc_clamp_i16(v->y * inv_scale));
    grc_wr16(f, (grc_u16)grc_clamp_i16(v->z * inv_scale));
    grc_wr16(f, 0u);
}

static void grc_write_default_normal(FILE *f)
{
    grc_wr16(f, 0u);
    grc_wr16(f, 0u);
    grc_wr16(f, (grc_u16)4096u);
    grc_wr16(f, 0u);
}

static int grc_save_re2_mesh_section_file(FILE *f, const grc_model *model, const grc_options *opt)
{
    grc_u32 tri_count;
    grc_u32 quad_count;
    grc_u32 header_size;
    grc_u32 desc_start;
    grc_u32 vertex_offset;
    grc_u32 normal_offset;
    grc_u32 tri_offset;
    grc_u32 tritex_offset;
    grc_u32 quad_offset;
    grc_u32 quadtex_offset;
    grc_u32 total_len;
    grc_u32 i;
    float inv_scale;
    grc_u32 tri_written;
    grc_u32 quad_written;
    grc_count_tris_quads_for_re2(model, &tri_count, &quad_count);
    if (model->vertex_count > 65535u || tri_count > 65535u || quad_count > 65535u) return GRC_ERR_OVERFLOW;
    inv_scale = (opt != NULL && opt->scale != 0.0f) ? (1.0f / opt->scale) : 1.0f;
    header_size = 12u;
    desc_start = header_size;
    vertex_offset = 56u;
    normal_offset = vertex_offset + model->vertex_count * 8u;
    tri_offset = normal_offset + model->vertex_count * 8u;
    tritex_offset = tri_offset + tri_count * 12u;
    quad_offset = tritex_offset + tri_count * 12u;
    quadtex_offset = quad_offset + quad_count * 16u;
    total_len = header_size + quadtex_offset + quad_count * 16u;
    grc_wr32(f, total_len);
    grc_wr32(f, 0u);
    grc_wr32(f, 2u);
    grc_wr32(f, vertex_offset);
    grc_wr32(f, model->vertex_count);
    grc_wr32(f, normal_offset);
    grc_wr32(f, model->vertex_count);
    grc_wr32(f, tri_offset);
    grc_wr32(f, tri_count);
    grc_wr32(f, tritex_offset);
    grc_wr32(f, vertex_offset);
    grc_wr32(f, model->vertex_count);
    grc_wr32(f, normal_offset);
    grc_wr32(f, model->vertex_count);
    grc_wr32(f, quad_offset);
    grc_wr32(f, quad_count);
    grc_wr32(f, quadtex_offset);
    for (i = 0u; i < model->vertex_count; ++i) grc_write_vertex_i16(f, &model->vertices[i], inv_scale);
    for (i = 0u; i < model->vertex_count; ++i) grc_write_default_normal(f);
    tri_written = 0u;
    for (i = 0u; i < model->face_count; ++i) {
        const grc_face *fa;
        grc_u16 v0;
        grc_u16 v1;
        grc_u16 v2;
        fa = &model->faces[i];
        if (fa->vertex_count != GRC_FACE_TRI) continue;
        v0 = (grc_u16)((fa->v[0] < 0) ? 0 : fa->v[0]);
        v1 = (grc_u16)((fa->v[1] < 0) ? 0 : fa->v[1]);
        v2 = (grc_u16)((fa->v[2] < 0) ? 0 : fa->v[2]);
        grc_wr16(f, v0); grc_wr16(f, v0);
        grc_wr16(f, v1); grc_wr16(f, v1);
        grc_wr16(f, v2); grc_wr16(f, v2);
        tri_written++;
    }
    (void)tri_written;
    for (i = 0u; i < model->face_count; ++i) {
        const grc_face *fa;
        fa = &model->faces[i];
        if (fa->vertex_count != GRC_FACE_TRI) continue;
        fputc(0, f); fputc(0, f); grc_wr16(f, 0u);
        fputc(0, f); fputc(0, f); grc_wr16(f, 0u);
        fputc(0, f); fputc(0, f); grc_wr16(f, 0u);
    }
    quad_written = 0u;
    for (i = 0u; i < model->face_count; ++i) {
        const grc_face *fa;
        grc_u16 v0;
        grc_u16 v1;
        grc_u16 v2;
        grc_u16 v3;
        fa = &model->faces[i];
        if (fa->vertex_count != GRC_FACE_QUAD) continue;
        v0 = (grc_u16)((fa->v[0] < 0) ? 0 : fa->v[0]);
        v1 = (grc_u16)((fa->v[1] < 0) ? 0 : fa->v[1]);
        v2 = (grc_u16)((fa->v[2] < 0) ? 0 : fa->v[2]);
        v3 = (grc_u16)((fa->v[3] < 0) ? 0 : fa->v[3]);
        grc_wr16(f, v0); grc_wr16(f, v0);
        grc_wr16(f, v1); grc_wr16(f, v1);
        grc_wr16(f, v3); grc_wr16(f, v3);
        grc_wr16(f, v2); grc_wr16(f, v2);
        quad_written++;
    }
    (void)quad_written;
    for (i = 0u; i < model->face_count; ++i) {
        const grc_face *fa;
        fa = &model->faces[i];
        if (fa->vertex_count != GRC_FACE_QUAD) continue;
        fputc(0, f); fputc(0, f); grc_wr16(f, 0u);
        fputc(0, f); fputc(0, f); grc_wr16(f, 0u);
        fputc(0, f); fputc(0, f); grc_wr16(f, 0u);
        fputc(0, f); fputc(0, f); grc_wr16(f, 0u);
    }
    (void)desc_start;
    return GRC_OK;
}

int grc_save_md1(const char *path, const grc_model *model, const grc_options *opt)
{
    FILE *f;
    int rc;
    if (path == NULL || model == NULL) return GRC_ERR_BAD_ARGUMENT;
    f = fopen(path, "wb");
    if (f == NULL) return GRC_ERR_IO;
    rc = grc_save_re2_mesh_section_file(f, model, opt);
    if (ferror(f)) rc = GRC_ERR_IO;
    fclose(f);
    return rc;
}

static int grc_save_re3_mesh_section_file(FILE *f, const grc_model *model, const grc_options *opt)
{
    grc_u32 tri_count;
    grc_u32 quad_count;
    grc_u32 vtx_offset;
    grc_u32 nor_offset;
    grc_u32 tri_offset;
    grc_u32 quad_offset;
    grc_u32 length;
    grc_u32 i;
    float inv_scale;
    grc_count_tris_quads_for_re2(model, &tri_count, &quad_count);
    if (model->vertex_count > 255u || tri_count > 65535u || quad_count > 256u) return GRC_ERR_OVERFLOW;
    inv_scale = (opt != NULL && opt->scale != 0.0f) ? (1.0f / opt->scale) : 1.0f;
    vtx_offset = 24u;
    nor_offset = vtx_offset + model->vertex_count * 8u;
    tri_offset = nor_offset + model->vertex_count * 8u;
    quad_offset = tri_offset + tri_count * 12u;
    length = 8u + quad_offset + quad_count * 16u;
    grc_wr32(f, length);
    grc_wr32(f, 1u);
    grc_wr16(f, (grc_u16)vtx_offset); grc_wr16(f, 0u);
    grc_wr16(f, (grc_u16)nor_offset); grc_wr16(f, 0u);
    grc_wr16(f, (grc_u16)model->vertex_count); grc_wr16(f, 0u);
    grc_wr16(f, (grc_u16)tri_offset); grc_wr16(f, 0u);
    grc_wr16(f, (grc_u16)quad_offset); grc_wr16(f, 0u);
    grc_wr16(f, (grc_u16)tri_count);
    grc_wr16(f, (grc_u16)quad_count);
    for (i = 0u; i < model->vertex_count; ++i) grc_write_vertex_i16(f, &model->vertices[i], inv_scale);
    for (i = 0u; i < model->vertex_count; ++i) grc_write_default_normal(f);
    for (i = 0u; i < model->face_count; ++i) {
        const grc_face *fa;
        fa = &model->faces[i];
        if (fa->vertex_count != GRC_FACE_TRI) continue;
        fputc(0, f); fputc(0, f); fputc(0, f); fputc(0, f);
        fputc(0, f); fputc(0, f); fputc(0, f); fputc((grc_u8)fa->v[0], f);
        fputc(0, f); fputc(0, f); fputc((grc_u8)fa->v[1], f); fputc((grc_u8)fa->v[2], f);
    }
    for (i = 0u; i < model->face_count; ++i) {
        const grc_face *fa;
        fa = &model->faces[i];
        if (fa->vertex_count != GRC_FACE_QUAD) continue;
        fputc(0, f); fputc(0, f); fputc(0, f); fputc(0, f);
        fputc(0, f); fputc(0, f); fputc(0, f); fputc(0, f);
        fputc(0, f); fputc(0, f); fputc((grc_u8)fa->v[0], f); fputc((grc_u8)fa->v[1], f);
        fputc(0, f); fputc(0, f); fputc((grc_u8)fa->v[3], f); fputc((grc_u8)fa->v[2], f);
    }
    return GRC_OK;
}

int grc_save_md2(const char *path, const grc_model *model, const grc_options *opt)
{
    FILE *f;
    int rc;
    if (path == NULL || model == NULL) return GRC_ERR_BAD_ARGUMENT;
    f = fopen(path, "wb");
    if (f == NULL) return GRC_ERR_IO;
    rc = grc_save_re3_mesh_section_file(f, model, opt);
    if (ferror(f)) rc = GRC_ERR_IO;
    fclose(f);
    return rc;
}

static int grc_save_container_with_mesh(const char *path, const grc_model *model, const grc_options *opt, grc_u32 section_count, grc_u32 mesh_index, int mesh_kind)
{
    FILE *f;
    long dir_offset_pos;
    grc_u32 *offsets;
    grc_u32 i;
    int rc;
    if (section_count == 0u || mesh_index >= section_count) return GRC_ERR_BAD_ARGUMENT;
    offsets = (grc_u32 *)calloc(section_count, sizeof(grc_u32));
    if (offsets == NULL) return GRC_ERR_MEMORY;
    f = fopen(path, "wb");
    if (f == NULL) { free(offsets); return GRC_ERR_IO; }
    grc_wr32(f, 0u);
    grc_wr32(f, section_count);
    dir_offset_pos = 0L;
    (void)dir_offset_pos;
    for (i = 0u; i < section_count; ++i) {
        offsets[i] = (grc_u32)ftell(f);
        if (i == mesh_index) {
            if (mesh_kind == 2) rc = grc_save_re3_mesh_section_file(f, model, opt);
            else rc = grc_save_re2_mesh_section_file(f, model, opt);
            if (rc != GRC_OK) { fclose(f); free(offsets); return rc; }
        } else {
            grc_wr32(f, 0u);
        }
    }
    {
        grc_u32 dir_offset;
        dir_offset = (grc_u32)ftell(f);
        for (i = 0u; i < section_count; ++i) grc_wr32(f, offsets[i]);
        fseek(f, 0L, SEEK_SET);
        grc_wr32(f, dir_offset);
        grc_wr32(f, section_count);
    }
    if (ferror(f)) { fclose(f); free(offsets); return GRC_ERR_IO; }
    fclose(f);
    free(offsets);
    return GRC_OK;
}

int grc_save_emd_re2(const char *path, const grc_model *model, const grc_options *opt)
{
    return grc_save_container_with_mesh(path, model, opt, 8u, 7u, 1);
}

int grc_save_emd_re3(const char *path, const grc_model *model, const grc_options *opt)
{
    return grc_save_container_with_mesh(path, model, opt, 15u, 14u, 2);
}

int grc_save_pld_re2(const char *path, const grc_model *model, const grc_options *opt)
{
    return grc_save_container_with_mesh(path, model, opt, 4u, 2u, 1);
}

int grc_save_pld_re3(const char *path, const grc_model *model, const grc_options *opt)
{
    return grc_save_container_with_mesh(path, model, opt, 5u, 2u, 1);
}

int grc_save_emd_re1(const char *path, const grc_model *model, const grc_options *opt)
{
    FILE *f;
    grc_u32 offsets[4];
    grc_u32 i;
    int rc;
    if (path == NULL || model == NULL) return GRC_ERR_BAD_ARGUMENT;
    f = fopen(path, "wb");
    if (f == NULL) return GRC_ERR_IO;
    offsets[0] = (grc_u32)ftell(f); grc_wr32(f, 0u);
    offsets[1] = (grc_u32)ftell(f); grc_wr32(f, 0u);
    offsets[2] = (grc_u32)ftell(f);
    rc = grc_save_re2_mesh_section_file(f, model, opt);
    if (rc != GRC_OK) { fclose(f); return rc; }
    offsets[3] = (grc_u32)ftell(f); grc_wr32(f, 0u);
    for (i = 0u; i < 4u; ++i) grc_wr32(f, offsets[i]);
    if (ferror(f)) { fclose(f); return GRC_ERR_IO; }
    fclose(f);
    return GRC_OK;
}

int grc_re_extract_sections(const char *path, const char *prefix, grc_u32 format_hint)
{
    grc_u8 *data;
    size_t size;
    grc_section_table table;
    int rc;
    grc_u32 i;
    char name[512];
    if (path == NULL || prefix == NULL) return GRC_ERR_BAD_ARGUMENT;
    rc = grc_load_file_bytes(path, &data, &size);
    if (rc != GRC_OK) return rc;
    table.sections = NULL;
    table.section_count = 0u;
    rc = grc_re_read_section_table(data, size, &table, format_hint);
    if (rc != GRC_OK) { free(data); return rc; }
    for (i = 0u; i < table.section_count; ++i) {
        const char *ext;
        ext = "bin";
        if (table.sections[i].size >= 8u) {
            grc_u32 id;
            id = grc_rd32(data + table.sections[i].offset);
            if (id == GRC_TMD_ID_A || id == GRC_TMD_ID_B) ext = "tmd";
            else if (data[table.sections[i].offset] == 0x10u) ext = "tim";
        }
        sprintf(name, "%s_%02u.%s", prefix, (unsigned int)i, ext);
        rc = grc_write_bytes_file(name, data + table.sections[i].offset, table.sections[i].size);
        if (rc != GRC_OK) { grc_section_table_free(&table); free(data); return rc; }
    }
    grc_section_table_free(&table);
    free(data);
    return GRC_OK;
}

/* ------------------------------------------------------------------------- */
/* Dispatch                                                                   */
/* ------------------------------------------------------------------------- */

int grc_load_model(const char *path, grc_model *out_model, const grc_options *opt)
{
    grc_u32 fmt;
    int rc;
    if (path == NULL || out_model == NULL) return GRC_ERR_BAD_ARGUMENT;
    rc = grc_probe_file(path, &fmt);
    if (rc != GRC_OK) return rc;
    if (fmt == GRC_FORMAT_GFM) return grc_load_gfm(path, out_model);
    if (fmt == GRC_FORMAT_OBJ) return grc_load_obj(path, out_model, opt);
    if (fmt == GRC_FORMAT_TMD) return grc_load_tmd(path, out_model, opt);
    if (fmt == GRC_FORMAT_EMD || fmt == GRC_FORMAT_PLD || fmt == GRC_FORMAT_PLW) return grc_load_emd(path, out_model, opt);
    if (fmt == GRC_FORMAT_MD1) return grc_load_md1(path, out_model, opt);
    if (fmt == GRC_FORMAT_MD2) return grc_load_md2(path, out_model, opt);
    return GRC_ERR_UNSUPPORTED;
}

int grc_save_model(const char *path, const grc_model *model, const grc_options *opt)
{
    grc_u32 fmt;
    if (path == NULL || model == NULL) return GRC_ERR_BAD_ARGUMENT;
    fmt = grc_format_from_extension(path);
    if (fmt == GRC_FORMAT_GFM) return grc_save_gfm(path, model);
    if (fmt == GRC_FORMAT_OBJ) return grc_save_obj(path, model);
    if (fmt == GRC_FORMAT_TMD) return grc_save_tmd(path, model, opt);
    if (fmt == GRC_FORMAT_MD1) return grc_save_md1(path, model, opt);
    if (fmt == GRC_FORMAT_MD2) return grc_save_md2(path, model, opt);
    if (fmt == GRC_FORMAT_PLD || fmt == GRC_FORMAT_PLW) {
        if (opt != NULL && opt->native_game_hint == GRC_GAME_RE2) return grc_save_pld_re2(path, model, opt);
        return grc_save_pld_re3(path, model, opt);
    }
    if (fmt == GRC_FORMAT_EMD) {
        if (opt != NULL && opt->native_game_hint == GRC_GAME_RE1) return grc_save_emd_re1(path, model, opt);
        if (opt != NULL && opt->native_game_hint == GRC_GAME_RE3) return grc_save_emd_re3(path, model, opt);
        return grc_save_emd_re2(path, model, opt);
    }
    return GRC_ERR_UNSUPPORTED;
}

