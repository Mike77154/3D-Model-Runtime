#include <stdio.h>
#include <string.h>
#include "mpk_reader.h"

mpk_fixed mpk_fixed_from_float_bits(mpk_u32 bits);
mpk_fixed mpk_fixed_from_half_bits(mpk_u32 bits);

#define MPK_OK 1
#define MPK_FAIL 0

typedef struct mpk_stream_s {
    FILE *f;
    mpk_model *model;
    int strict_tags;
} mpk_stream;

static void mpk_set_error(mpk_model *m, const char *msg)
{
    if (m == (mpk_model *)0) return;
    strncpy(m->last_error, msg, MPK_ERROR_MAX - 1);
    m->last_error[MPK_ERROR_MAX - 1] = '\0';
}

void mpk_model_init(mpk_model *m)
{
    if (m != (mpk_model *)0) {
        memset(m, 0, sizeof(*m));
    }
}

const char *mpk_chunk_name(mpk_u32 id)
{
    switch (id) {
    case MPK_ID_NODE: return "Node";
    case MPK_ID_CAMERA: return "Camera";
    case MPK_ID_POINT_LIGHT: return "PointLight";
    case MPK_ID_DIR_LIGHT: return "DirectionalLight";
    case MPK_ID_SPOT_LIGHT: return "SpotLight";
    case MPK_ID_MESH: return "Mesh";
    case MPK_ID_GEOMETRY: return "Geometry";
    case MPK_ID_POLYGONS: return "Polygons";
    case MPK_ID_POLYGON: return "Polygon";
    case MPK_ID_SMOOTHING: return "Smoothing";
    case MPK_ID_POLY_MAT: return "PolygonMaterial";
    case MPK_ID_UV_MAPPING: return "UVMapping";
    case MPK_ID_MATERIAL_LIST: return "MaterialList";
    case MPK_ID_MATERIAL: return "Material";
    case MPK_ID_TEXTURE: return "Texture";
    case MPK_ID_KEYFRAME_ANIM: return "KeyframeAnimation";
    case MPK_ID_ANIMATION: return "Animation";
    case MPK_ID_SKIN: return "Skin";
    case MPK_ID_ENVIRONMENT: return "Environment";
    case MPK_ID_HELPER: return "Helper";
    case MPK_ID_POINT_ANIM: return "PointLightAnimation";
    case MPK_ID_DIR_ANIM: return "DirectionalLightAnimation";
    case MPK_ID_SPOT_ANIM: return "SpotLightAnimation";
    case MPK_ID_REFERENCE: return "ReferenceToData";
    default: return "Unknown";
    }
}

static long mpk_tell(mpk_stream *s)
{
    return ftell(s->f);
}

static int mpk_seek_abs(mpk_stream *s, long pos)
{
    return fseek(s->f, pos, SEEK_SET) == 0 ? MPK_OK : MPK_FAIL;
}

static int mpk_read_u8_raw(mpk_stream *s, mpk_u8 *out)
{
    int c;
    c = fgetc(s->f);
    if (c == EOF) return MPK_FAIL;
    *out = (mpk_u8)(c & 255);
    return MPK_OK;
}

static int mpk_read_u16_raw(mpk_stream *s, mpk_u16 *out)
{
    mpk_u8 b0;
    mpk_u8 b1;
    if (!mpk_read_u8_raw(s, &b0)) return MPK_FAIL;
    if (!mpk_read_u8_raw(s, &b1)) return MPK_FAIL;
    *out = (mpk_u16)((mpk_u16)b0 | ((mpk_u16)b1 << 8));
    return MPK_OK;
}

static int mpk_read_u24_raw(mpk_stream *s, mpk_u32 *out)
{
    mpk_u8 b0;
    mpk_u8 b1;
    mpk_u8 b2;
    if (!mpk_read_u8_raw(s, &b0)) return MPK_FAIL;
    if (!mpk_read_u8_raw(s, &b1)) return MPK_FAIL;
    if (!mpk_read_u8_raw(s, &b2)) return MPK_FAIL;
    *out = (mpk_u32)b0 | ((mpk_u32)b1 << 8) | ((mpk_u32)b2 << 16);
    return MPK_OK;
}

static int mpk_read_u32_raw(mpk_stream *s, mpk_u32 *out)
{
    mpk_u8 b0;
    mpk_u8 b1;
    mpk_u8 b2;
    mpk_u8 b3;
    if (!mpk_read_u8_raw(s, &b0)) return MPK_FAIL;
    if (!mpk_read_u8_raw(s, &b1)) return MPK_FAIL;
    if (!mpk_read_u8_raw(s, &b2)) return MPK_FAIL;
    if (!mpk_read_u8_raw(s, &b3)) return MPK_FAIL;
    *out = (mpk_u32)b0 | ((mpk_u32)b1 << 8) | ((mpk_u32)b2 << 16) | ((mpk_u32)b3 << 24);
    return MPK_OK;
}

static mpk_i32 mpk_sign_extend(mpk_u32 v, int bits)
{
    mpk_u32 mask;
    mpk_u32 sign;
    if (bits >= 32) return (mpk_i32)v;
    sign = 1UL << (bits - 1);
    mask = (1UL << bits) - 1UL;
    v &= mask;
    if ((v & sign) != 0UL) {
        v |= ~mask;
    }
    return (mpk_i32)v;
}

static int mpk_read_chunk_header_after_tag(mpk_stream *s, mpk_chunk_header *h)
{
    if (!mpk_read_u32_raw(s, &h->id)) return MPK_FAIL;
    if (!mpk_read_u32_raw(s, &h->version)) return MPK_FAIL;
    if (!mpk_read_u32_raw(s, &h->size)) return MPK_FAIL;
    h->data_start = mpk_tell(s);
    h->data_end = h->data_start + (long)h->size;
    return MPK_OK;
}

static int mpk_read_chunk_header(mpk_stream *s, mpk_chunk_header *h)
{
    mpk_u8 tag;
    if (!mpk_read_u8_raw(s, &tag)) return MPK_FAIL;
    if (tag != MPK_CHUNK_TAG) {
        mpk_set_error(s->model, "bad chunk tag");
        return MPK_FAIL;
    }
    return mpk_read_chunk_header_after_tag(s, h);
}

static int mpk_expect_tag(mpk_stream *s, mpk_u8 expected)
{
    mpk_u8 tag;
    if (!mpk_read_u8_raw(s, &tag)) return MPK_FAIL;
    if (tag != expected) {
        if (s->strict_tags) {
            mpk_set_error(s->model, "unexpected typed data tag");
            return MPK_FAIL;
        }
        return MPK_FAIL;
    }
    return MPK_OK;
}

static int mpk_read_int_typed(mpk_stream *s, mpk_i32 *out)
{
    mpk_u8 tag;
    mpk_u32 u;
    mpk_u16 u16;
    mpk_u8 u8;
    if (!mpk_read_u8_raw(s, &tag)) return MPK_FAIL;
    if (tag == MPK_T_INT32 || tag == MPK_T_LONG) {
        if (!mpk_read_u32_raw(s, &u)) return MPK_FAIL;
        *out = (mpk_i32)u;
        return MPK_OK;
    }
    if (tag == MPK_T_INT24) {
        if (!mpk_read_u24_raw(s, &u)) return MPK_FAIL;
        *out = mpk_sign_extend(u, 24);
        return MPK_OK;
    }
    if (tag == MPK_T_INT16 || tag == MPK_T_SHORT) {
        if (!mpk_read_u16_raw(s, &u16)) return MPK_FAIL;
        *out = mpk_sign_extend((mpk_u32)u16, 16);
        return MPK_OK;
    }
    if (tag == MPK_T_INT8 || tag == MPK_T_SCHAR || tag == MPK_T_CHAR) {
        if (!mpk_read_u8_raw(s, &u8)) return MPK_FAIL;
        *out = mpk_sign_extend((mpk_u32)u8, 8);
        return MPK_OK;
    }
    mpk_set_error(s->model, "expected typed int");
    return MPK_FAIL;
}

static int mpk_read_uint_typed(mpk_stream *s, mpk_u32 *out)
{
    mpk_u8 tag;
    mpk_u16 u16;
    mpk_u8 u8;
    if (!mpk_read_u8_raw(s, &tag)) return MPK_FAIL;
    if (tag == MPK_T_UINT32 || tag == MPK_T_ULONG) {
        if (!mpk_read_u32_raw(s, out)) return MPK_FAIL;
        return MPK_OK;
    }
    if (tag == MPK_T_INT32 || tag == MPK_T_LONG) {
        if (!mpk_read_u32_raw(s, out)) return MPK_FAIL;
        return MPK_OK;
    }
    if (tag == MPK_T_UINT24 || tag == MPK_T_INT24) {
        if (!mpk_read_u24_raw(s, out)) return MPK_FAIL;
        return MPK_OK;
    }
    if (tag == MPK_T_UINT16 || tag == MPK_T_USHORT || tag == MPK_T_INT16 || tag == MPK_T_SHORT) {
        if (!mpk_read_u16_raw(s, &u16)) return MPK_FAIL;
        *out = (mpk_u32)u16;
        return MPK_OK;
    }
    if (tag == MPK_T_UINT8 || tag == MPK_T_UCHAR || tag == MPK_T_INT8 || tag == MPK_T_CHAR) {
        if (!mpk_read_u8_raw(s, &u8)) return MPK_FAIL;
        *out = (mpk_u32)u8;
        return MPK_OK;
    }
    mpk_set_error(s->model, "expected typed uint");
    return MPK_FAIL;
}

static int mpk_read_bool_typed(mpk_stream *s, int *out)
{
    mpk_u8 v;
    if (!mpk_expect_tag(s, MPK_T_BOOL)) return MPK_FAIL;
    if (!mpk_read_u8_raw(s, &v)) return MPK_FAIL;
    *out = v ? MPK_TRUE : MPK_FALSE;
    return MPK_OK;
}

static int mpk_read_uchar_typed(mpk_stream *s, mpk_u8 *out)
{
    mpk_u8 tag;
    if (!mpk_read_u8_raw(s, &tag)) return MPK_FAIL;
    if (tag != MPK_T_UCHAR && tag != MPK_T_UINT8 && tag != MPK_T_CHAR) return MPK_FAIL;
    return mpk_read_u8_raw(s, out);
}

static int mpk_read_fixed_typed(mpk_stream *s, mpk_fixed *out)
{
    mpk_u8 tag;
    mpk_u32 bits;
    mpk_u16 half;
    if (!mpk_read_u8_raw(s, &tag)) return MPK_FAIL;
    if (tag == MPK_T_FLOAT32) {
        if (!mpk_read_u32_raw(s, &bits)) return MPK_FAIL;
        *out = mpk_fixed_from_float_bits(bits);
        return MPK_OK;
    }
    if (tag == MPK_T_FLOAT16) {
        if (!mpk_read_u16_raw(s, &half)) return MPK_FAIL;
        *out = mpk_fixed_from_half_bits((mpk_u32)half);
        return MPK_OK;
    }
    if (tag == MPK_T_INT32 || tag == MPK_T_LONG) {
        if (!mpk_read_u32_raw(s, &bits)) return MPK_FAIL;
        *out = ((mpk_i32)bits) << 16;
        return MPK_OK;
    }
    mpk_set_error(s->model, "expected typed fixed float");
    return MPK_FAIL;
}

static int mpk_read_fixed_raw32(mpk_stream *s, mpk_fixed *out)
{
    mpk_u32 bits;
    if (!mpk_read_u32_raw(s, &bits)) return MPK_FAIL;
    *out = mpk_fixed_from_float_bits(bits);
    return MPK_OK;
}

static int mpk_read_vec3_typed(mpk_stream *s, mpk_vec3 *v)
{
    if (!mpk_expect_tag(s, MPK_T_VEC3)) return MPK_FAIL;
    if (!mpk_read_fixed_raw32(s, &v->x)) return MPK_FAIL;
    if (!mpk_read_fixed_raw32(s, &v->y)) return MPK_FAIL;
    if (!mpk_read_fixed_raw32(s, &v->z)) return MPK_FAIL;
    return MPK_OK;
}

static int mpk_read_mat4x3_typed(mpk_stream *s, mpk_matrix4x3 *m)
{
    int i;
    if (!mpk_expect_tag(s, MPK_T_MAT4X3)) return MPK_FAIL;
    for (i = 0; i < 12; ++i) {
        if (!mpk_read_fixed_raw32(s, &m->m[i])) return MPK_FAIL;
    }
    return MPK_OK;
}

static int mpk_read_string_typed(mpk_stream *s, char *dst, int dst_size)
{
    mpk_i32 slen_i;
    mpk_u32 slen;
    mpk_u32 i;
    int c;

    if (!mpk_expect_tag(s, MPK_T_STRING)) return MPK_FAIL;
    if (!mpk_read_int_typed(s, &slen_i)) return MPK_FAIL;
    if (slen_i < 0) return MPK_FAIL;
    slen = (mpk_u32)slen_i;
    if (dst_size > 0) dst[0] = '\0';
    for (i = 0; i < slen; ++i) {
        c = fgetc(s->f);
        if (c == EOF) return MPK_FAIL;
        if ((int)i < dst_size - 1) {
            dst[i] = (char)c;
            dst[i + 1] = '\0';
        }
    }
    return MPK_OK;
}

static int mpk_skip_to_end(mpk_stream *s, const mpk_chunk_header *h)
{
    if (mpk_tell(s) > h->data_end) return MPK_FAIL;
    return mpk_seek_abs(s, h->data_end);
}

static mpk_fixed mpk_fixed_lerp(mpk_fixed a, mpk_fixed b, mpk_u32 num, mpk_u32 den)
{
    mpk_i32 diff;
    if (den == 0UL) return a;
    diff = (mpk_i32)(b - a);
    return (mpk_fixed)(a + (mpk_fixed)((diff / (mpk_i32)den) * (mpk_i32)num));
}

static mpk_fixed mpk_fixed_mul(mpk_fixed a, mpk_fixed b)
{
    /* Q16.16 multiply without C99 long long: reduce both operands by 8 bits. */
    return (mpk_fixed)((a >> 8) * (b >> 8));
}

static void mpk_mat_identity(mpk_matrix4x3 *m)
{
    int i;
    for (i = 0; i < 12; ++i) m->m[i] = 0;
    m->m[0] = 65536L;
    m->m[4] = 65536L;
    m->m[8] = 65536L;
}

static void mpk_mat_copy(mpk_matrix4x3 *dst, const mpk_matrix4x3 *src)
{
    int i;
    for (i = 0; i < 12; ++i) dst->m[i] = src->m[i];
}

static void mpk_mat_transform_point(const mpk_matrix4x3 *m, const mpk_vec3 *v, mpk_vec3 *out)
{
    /* Max Payne matrices are exposed as M_Matrix4x3.  This decoder uses the
       common row-vector affine layout: 3x3 basis plus translation in m[9..11]. */
    out->x = (mpk_fixed)(mpk_fixed_mul(v->x, m->m[0]) + mpk_fixed_mul(v->y, m->m[3]) + mpk_fixed_mul(v->z, m->m[6]) + m->m[9]);
    out->y = (mpk_fixed)(mpk_fixed_mul(v->x, m->m[1]) + mpk_fixed_mul(v->y, m->m[4]) + mpk_fixed_mul(v->z, m->m[7]) + m->m[10]);
    out->z = (mpk_fixed)(mpk_fixed_mul(v->x, m->m[2]) + mpk_fixed_mul(v->y, m->m[5]) + mpk_fixed_mul(v->z, m->m[8]) + m->m[11]);
}

static void mpk_mat_mul(mpk_matrix4x3 *out, const mpk_matrix4x3 *a, const mpk_matrix4x3 *b)
{
    mpk_matrix4x3 r;
    int row;
    int col;
    int k;
    for (row = 0; row < 3; ++row) {
        for (col = 0; col < 3; ++col) {
            mpk_fixed sum;
            sum = 0;
            for (k = 0; k < 3; ++k) {
                sum = (mpk_fixed)(sum + mpk_fixed_mul(a->m[row * 3 + k], b->m[k * 3 + col]));
            }
            r.m[row * 3 + col] = sum;
        }
    }
    r.m[9] = (mpk_fixed)(mpk_fixed_mul(a->m[9], b->m[0]) + mpk_fixed_mul(a->m[10], b->m[3]) + mpk_fixed_mul(a->m[11], b->m[6]) + b->m[9]);
    r.m[10] = (mpk_fixed)(mpk_fixed_mul(a->m[9], b->m[1]) + mpk_fixed_mul(a->m[10], b->m[4]) + mpk_fixed_mul(a->m[11], b->m[7]) + b->m[10]);
    r.m[11] = (mpk_fixed)(mpk_fixed_mul(a->m[9], b->m[2]) + mpk_fixed_mul(a->m[10], b->m[5]) + mpk_fixed_mul(a->m[11], b->m[8]) + b->m[11]);
    mpk_mat_copy(out, &r);
}

static int mpk_names_equal(const char *a, const char *b)
{
    if (a == (const char *)0 || b == (const char *)0) return MPK_FALSE;
    return strcmp(a, b) == 0 ? MPK_TRUE : MPK_FALSE;
}

static int mpk_read_string_after_tag(mpk_stream *s, char *dst, int dst_size)
{
    mpk_i32 slen_i;
    mpk_u32 slen;
    mpk_u32 i;
    int c;
    if (!mpk_read_int_typed(s, &slen_i)) return MPK_FAIL;
    if (slen_i < 0) return MPK_FAIL;
    slen = (mpk_u32)slen_i;
    if (dst_size > 0) dst[0] = '\0';
    for (i = 0; i < slen; ++i) {
        c = fgetc(s->f);
        if (c == EOF) return MPK_FAIL;
        if ((int)i < dst_size - 1) {
            dst[i] = (char)c;
            dst[i + 1] = '\0';
        }
    }
    return MPK_OK;
}

static int mpk_skip_typed_value_after_tag(mpk_stream *s, mpk_u8 tag)
{
    mpk_u32 u;
    mpk_u16 u16;
    mpk_u8 u8;
    char temp[MPK_PATH_MAX];
    int i;
    if (tag == MPK_T_LONG || tag == MPK_T_ULONG || tag == MPK_T_INT32 || tag == MPK_T_UINT32 || tag == MPK_T_FLOAT32) {
        return mpk_read_u32_raw(s, &u);
    }
    if (tag == MPK_T_SHORT || tag == MPK_T_USHORT || tag == MPK_T_INT16 || tag == MPK_T_UINT16 || tag == MPK_T_FLOAT16) {
        return mpk_read_u16_raw(s, &u16);
    }
    if (tag == MPK_T_CHAR || tag == MPK_T_SCHAR || tag == MPK_T_UCHAR || tag == MPK_T_INT8 || tag == MPK_T_UINT8 || tag == MPK_T_BOOL) {
        return mpk_read_u8_raw(s, &u8);
    }
    if (tag == MPK_T_INT24 || tag == MPK_T_UINT24) {
        return mpk_read_u24_raw(s, &u);
    }
    if (tag == MPK_T_STRING) {
        return mpk_read_string_after_tag(s, temp, MPK_PATH_MAX);
    }
    if (tag == MPK_T_VEC2) {
        for (i = 0; i < 2; ++i) if (!mpk_read_u32_raw(s, &u)) return MPK_FAIL;
        return MPK_OK;
    }
    if (tag == MPK_T_VEC3) {
        for (i = 0; i < 3; ++i) if (!mpk_read_u32_raw(s, &u)) return MPK_FAIL;
        return MPK_OK;
    }
    if (tag == MPK_T_VEC4 || tag == MPK_T_MAT2) {
        for (i = 0; i < 4; ++i) if (!mpk_read_u32_raw(s, &u)) return MPK_FAIL;
        return MPK_OK;
    }
    if (tag == MPK_T_MAT3) {
        for (i = 0; i < 9; ++i) if (!mpk_read_u32_raw(s, &u)) return MPK_FAIL;
        return MPK_OK;
    }
    if (tag == MPK_T_MAT4X3) {
        for (i = 0; i < 12; ++i) if (!mpk_read_u32_raw(s, &u)) return MPK_FAIL;
        return MPK_OK;
    }
    if (tag == MPK_T_MAT4) {
        for (i = 0; i < 16; ++i) if (!mpk_read_u32_raw(s, &u)) return MPK_FAIL;
        return MPK_OK;
    }
    return MPK_FAIL;
}

static mpk_node *mpk_alloc_node(mpk_model *m)
{
    mpk_node *n;
    if (m->node_count >= MPK_MAX_NODES) return (mpk_node *)0;
    n = &m->nodes[m->node_count++];
    memset(n, 0, sizeof(*n));
    return n;
}

static int mpk_parse_node_body(mpk_stream *s, const mpk_chunk_header *h, mpk_node *out)
{
    memset(out, 0, sizeof(*out));
    if (!mpk_read_string_typed(s, out->name, MPK_NAME_MAX)) return MPK_FAIL;
    if (!mpk_read_string_typed(s, out->parent, MPK_NAME_MAX)) return MPK_FAIL;
    if (!mpk_read_mat4x3_typed(s, &out->object_to_parent)) return MPK_FAIL;
    if (!mpk_read_bool_typed(s, &out->has_parent)) return MPK_FAIL;
    if (h->version >= 1UL && mpk_tell(s) < h->data_end) {
        if (!mpk_read_string_typed(s, out->user, MPK_PATH_MAX)) return MPK_FAIL;
    }
    return mpk_skip_to_end(s, h);
}

static int mpk_parse_node_chunk(mpk_stream *s, const mpk_chunk_header *h, mpk_node *out)
{
    mpk_node tmp;
    mpk_node *dst;
    if (out == (mpk_node *)0) dst = &tmp;
    else dst = out;
    if (!mpk_parse_node_body(s, h, dst)) return MPK_FAIL;
    if (out == (mpk_node *)0) {
        mpk_node *slot;
        slot = mpk_alloc_node(s->model);
        if (slot != (mpk_node *)0) *slot = tmp;
    }
    return MPK_OK;
}

static int mpk_parse_texture_chunk(mpk_stream *s, const mpk_chunk_header *h, char *first_file, int first_file_size)
{
    char texture_name[MPK_NAME_MAX];
    mpk_i32 mipmaps;
    mpk_i32 filtering;
    mpk_i32 num_textures_i;
    mpk_u32 num_textures;
    mpk_u32 i;
    int b;
    char temp[MPK_PATH_MAX];
    mpk_i32 start_frame;
    mpk_i32 fps;
    mpk_i32 end_cond;

    if (first_file_size > 0) first_file[0] = '\0';
    if (!mpk_read_string_typed(s, texture_name, MPK_NAME_MAX)) return MPK_FAIL;
    if (!mpk_read_int_typed(s, &mipmaps)) return MPK_FAIL;
    if (!mpk_read_int_typed(s, &filtering)) return MPK_FAIL;
    if (!mpk_read_int_typed(s, &num_textures_i)) return MPK_FAIL;
    if (num_textures_i < 0) return MPK_FAIL;
    num_textures = (mpk_u32)num_textures_i;
    for (i = 0; i < num_textures; ++i) {
        if (!mpk_read_string_typed(s, temp, MPK_PATH_MAX)) return MPK_FAIL;
        if (i == 0 && first_file_size > 0) {
            strncpy(first_file, temp, first_file_size - 1);
            first_file[first_file_size - 1] = '\0';
        }
    }
    if (num_textures > 0UL && mpk_tell(s) < h->data_end) {
        if (!mpk_read_bool_typed(s, &b)) return MPK_FAIL;
        if (!mpk_read_bool_typed(s, &b)) return MPK_FAIL;
        if (!mpk_read_int_typed(s, &start_frame)) return MPK_FAIL;
        if (!mpk_read_int_typed(s, &fps)) return MPK_FAIL;
        if (!mpk_read_int_typed(s, &end_cond)) return MPK_FAIL;
    }
    return mpk_skip_to_end(s, h);
}

static int mpk_parse_material_body(mpk_stream *s, const mpk_chunk_header *h, mpk_material *mat)
{
    int b;
    mpk_i32 itemp;
    mpk_chunk_header th;

    memset(mat, 0, sizeof(*mat));
    if (!mpk_read_string_typed(s, mat->name, MPK_NAME_MAX)) return MPK_FAIL;
    if (!mpk_read_bool_typed(s, &mat->two_sided)) return MPK_FAIL;
    if (!mpk_read_bool_typed(s, &mat->fogging)) return MPK_FAIL;
    if (!mpk_read_bool_typed(s, &b)) return MPK_FAIL;
    if (!mpk_read_bool_typed(s, &mat->invisible_geometry)) return MPK_FAIL;
    if (!mpk_read_bool_typed(s, &mat->has_vertex_alpha)) return MPK_FAIL;
    if (!mpk_read_int_typed(s, &itemp)) return MPK_FAIL;
    if (!mpk_read_int_typed(s, &itemp)) return MPK_FAIL;
    if (!mpk_read_int_typed(s, &itemp)) return MPK_FAIL;
    if (!mpk_read_fixed_typed(s, &mat->ambient[0])) return MPK_FAIL;
    if (!mpk_read_fixed_typed(s, &mat->ambient[1])) return MPK_FAIL;
    if (!mpk_read_fixed_typed(s, &mat->ambient[2])) return MPK_FAIL;
    if (!mpk_read_fixed_typed(s, &mat->ambient[3])) return MPK_FAIL;
    if (!mpk_read_fixed_typed(s, &mat->diffuse[0])) return MPK_FAIL;
    if (!mpk_read_fixed_typed(s, &mat->diffuse[1])) return MPK_FAIL;
    if (!mpk_read_fixed_typed(s, &mat->diffuse[2])) return MPK_FAIL;
    if (!mpk_read_fixed_typed(s, &mat->diffuse[3])) return MPK_FAIL;
    if (!mpk_read_fixed_typed(s, &mat->specular[0])) return MPK_FAIL;
    if (!mpk_read_fixed_typed(s, &mat->specular[1])) return MPK_FAIL;
    if (!mpk_read_fixed_typed(s, &mat->specular[2])) return MPK_FAIL;
    if (!mpk_read_fixed_typed(s, &mat->specular[3])) return MPK_FAIL;
    if (!mpk_read_fixed_typed(s, &mat->vertex_alpha)) return MPK_FAIL;
    if (!mpk_read_fixed_typed(s, &mat->specular_exponent)) return MPK_FAIL;
    if (!mpk_read_int_typed(s, &itemp)) return MPK_FAIL;
    if (!mpk_read_int_typed(s, &itemp)) return MPK_FAIL;
    if (!mpk_read_fixed_typed(s, &mat->specular_exponent)) return MPK_FAIL;

    if (!mpk_read_bool_typed(s, &b)) return MPK_FAIL;
    if (b && mpk_tell(s) < h->data_end) {
        if (!mpk_read_chunk_header(s, &th)) return MPK_FAIL;
        if (!mpk_parse_texture_chunk(s, &th, mat->diffuse_texture, MPK_PATH_MAX)) return MPK_FAIL;
    }
    if (!mpk_read_bool_typed(s, &b)) return MPK_FAIL;
    if (b && mpk_tell(s) < h->data_end) {
        if (!mpk_read_chunk_header(s, &th)) return MPK_FAIL;
        if (!mpk_parse_texture_chunk(s, &th, mat->reflection_texture, MPK_PATH_MAX)) return MPK_FAIL;
    }
    if (!mpk_read_bool_typed(s, &b)) return MPK_FAIL;
    if (b && mpk_tell(s) < h->data_end) {
        if (!mpk_read_chunk_header(s, &th)) return MPK_FAIL;
        if (!mpk_parse_texture_chunk(s, &th, mat->bump_texture, MPK_PATH_MAX)) return MPK_FAIL;
    }
    if (!mpk_read_bool_typed(s, &b)) return MPK_FAIL;
    if (b && mpk_tell(s) < h->data_end) {
        if (!mpk_read_chunk_header(s, &th)) return MPK_FAIL;
        if (!mpk_parse_texture_chunk(s, &th, mat->opacity_texture, MPK_PATH_MAX)) return MPK_FAIL;
    }
    if (h->version >= 1UL && mpk_tell(s) < h->data_end) {
        if (!mpk_read_bool_typed(s, &b)) return MPK_FAIL;
        if (b && mpk_tell(s) < h->data_end) {
            if (!mpk_read_chunk_header(s, &th)) return MPK_FAIL;
            if (!mpk_parse_texture_chunk(s, &th, mat->mask_texture, MPK_PATH_MAX)) return MPK_FAIL;
        }
        if (mpk_tell(s) < h->data_end) {
            if (!mpk_read_int_typed(s, &itemp)) return MPK_FAIL;
        }
        if (mpk_tell(s) < h->data_end) {
            if (!mpk_read_bool_typed(s, &b)) return MPK_FAIL;
        }
    }
    if (h->version >= 2UL && mpk_tell(s) < h->data_end) {
        if (!mpk_read_bool_typed(s, &b)) return MPK_FAIL;
        if (!mpk_read_bool_typed(s, &b)) return MPK_FAIL;
        if (!mpk_read_int_typed(s, &itemp)) return MPK_FAIL;
    }
    return mpk_skip_to_end(s, h);
}

static int mpk_parse_material_chunk(mpk_stream *s, const mpk_chunk_header *h)
{
    mpk_material temp;
    if (!mpk_parse_material_body(s, h, &temp)) return MPK_FAIL;
    if (s->model->material_count < MPK_MAX_MATERIALS) {
        s->model->materials[s->model->material_count++] = temp;
    }
    return MPK_OK;
}

static int mpk_parse_material_list(mpk_stream *s, const mpk_chunk_header *h)
{
    mpk_i32 count_i;
    mpk_u32 count;
    mpk_u32 i;
    mpk_chunk_header mh;
    if (!mpk_read_string_typed(s, s->model->texture_dirs, MPK_PATH_MAX)) return MPK_FAIL;
    if (!mpk_read_int_typed(s, &count_i)) return MPK_FAIL;
    if (count_i < 0) return MPK_FAIL;
    count = (mpk_u32)count_i;
    for (i = 0; i < count && mpk_tell(s) < h->data_end; ++i) {
        if (!mpk_read_chunk_header(s, &mh)) return MPK_FAIL;
        if (mh.id == MPK_ID_MATERIAL) {
            if (!mpk_parse_material_chunk(s, &mh)) return MPK_FAIL;
        } else {
            s->model->skipped_chunk_count++;
            if (!mpk_skip_to_end(s, &mh)) return MPK_FAIL;
        }
    }
    return mpk_skip_to_end(s, h);
}

static mpk_mesh *mpk_alloc_mesh(mpk_model *m)
{
    mpk_mesh *mesh;
    if (m->mesh_count >= MPK_MAX_MESHES) return (mpk_mesh *)0;
    mesh = &m->meshes[m->mesh_count++];
    memset(mesh, 0, sizeof(*mesh));
    return mesh;
}

static int mpk_store_vertex(mpk_model *m, const mpk_vec3 *v)
{
    if (m->vertex_count >= MPK_MAX_VERTICES) return MPK_FAIL;
    m->vertices[m->vertex_count++] = *v;
    return MPK_OK;
}

static int mpk_store_normal(mpk_model *m, const mpk_vec3 *v)
{
    if (m->normal_count >= MPK_MAX_NORMALS) return MPK_FAIL;
    m->normals[m->normal_count++] = *v;
    return MPK_OK;
}

static int mpk_store_uv(mpk_model *m, const mpk_uv *uv)
{
    if (m->uv_count >= MPK_MAX_UVS) return MPK_FAIL;
    m->uvs[m->uv_count++] = *uv;
    return MPK_OK;
}

static int mpk_store_polygon(mpk_model *m, const mpk_polygon *p)
{
    if (m->polygon_count >= MPK_MAX_POLYGONS) return MPK_FAIL;
    m->polygons[m->polygon_count++] = *p;
    return MPK_OK;
}

static int mpk_parse_geometry(mpk_stream *s, const mpk_chunk_header *h, mpk_mesh *mesh)
{
    mpk_i32 count_i;
    mpk_u32 count;
    mpk_u32 i;
    mpk_vec3 v;
    mpk_u32 prims;
    mpk_u32 tmp;

    mesh->first_vertex = s->model->vertex_count;
    mesh->first_normal = s->model->normal_count;

    if (h->version == 0UL) {
        if (!mpk_read_int_typed(s, &count_i)) return MPK_FAIL;
        if (count_i < 0) return MPK_FAIL;
        count = (mpk_u32)count_i;
        for (i = 0; i < count; ++i) {
            if (!mpk_read_vec3_typed(s, &v)) return MPK_FAIL;
            if (!mpk_store_vertex(s->model, &v)) return MPK_FAIL;
        }
        mesh->vertex_count = s->model->vertex_count - mesh->first_vertex;
    } else {
        if (!mpk_read_uint_typed(s, &count)) return MPK_FAIL;
        for (i = 0; i < count; ++i) {
            if (!mpk_read_fixed_raw32(s, &v.x)) return MPK_FAIL;
            if (!mpk_read_fixed_raw32(s, &v.y)) return MPK_FAIL;
            if (!mpk_read_fixed_raw32(s, &v.z)) return MPK_FAIL;
            if (!mpk_store_vertex(s->model, &v)) return MPK_FAIL;
        }
        mesh->vertex_count = s->model->vertex_count - mesh->first_vertex;
        for (i = 0; i < count; ++i) {
            if (!mpk_read_fixed_raw32(s, &v.x)) return MPK_FAIL;
            if (!mpk_read_fixed_raw32(s, &v.y)) return MPK_FAIL;
            if (!mpk_read_fixed_raw32(s, &v.z)) return MPK_FAIL;
            if (!mpk_store_normal(s->model, &v)) return MPK_FAIL;
        }
        mesh->normal_count = s->model->normal_count - mesh->first_normal;
        if (mpk_tell(s) < h->data_end) {
            if (!mpk_read_uint_typed(s, &prims)) return MPK_FAIL;
            for (i = 0; i < prims && mpk_tell(s) < h->data_end; ++i) {
                if (!mpk_read_uint_typed(s, &tmp)) return MPK_FAIL;
            }
        }
    }
    return mpk_skip_to_end(s, h);
}

static int mpk_parse_one_polygon_body(mpk_stream *s, const mpk_chunk_header *h, mpk_polygon *p)
{
    mpk_i32 count_i;
    mpk_i32 vi;
    int i;
    memset(p, 0, sizeof(*p));
    if (!mpk_read_int_typed(s, &count_i)) return MPK_FAIL;
    if (count_i < 0) return MPK_FAIL;
    p->count = (int)count_i;
    if (p->count > MPK_MAX_POLY_VERTS) p->count = MPK_MAX_POLY_VERTS;
    for (i = 0; i < count_i; ++i) {
        if (!mpk_read_int_typed(s, &vi)) return MPK_FAIL;
        if (i < MPK_MAX_POLY_VERTS) p->v[i] = (mpk_u32)vi;
    }
    return mpk_skip_to_end(s, h);
}

static int mpk_parse_polygons(mpk_stream *s, const mpk_chunk_header *h, mpk_mesh *mesh)
{
    mpk_i32 count_i;
    mpk_u32 count;
    mpk_u32 i;
    mpk_chunk_header ph;
    mpk_polygon p;
    mpk_i32 unknown;
    mpk_u16 idx;
    mpk_u32 prims;
    mpk_u32 tmp;

    mesh->first_polygon = s->model->polygon_count;
    if (h->version == 0UL) {
        if (!mpk_read_int_typed(s, &count_i)) return MPK_FAIL;
        if (count_i < 0) return MPK_FAIL;
        count = (mpk_u32)count_i;
        for (i = 0; i < count && mpk_tell(s) < h->data_end; ++i) {
            if (!mpk_read_chunk_header(s, &ph)) return MPK_FAIL;
            if (ph.id == MPK_ID_POLYGON) {
                if (!mpk_parse_one_polygon_body(s, &ph, &p)) return MPK_FAIL;
                if (!mpk_store_polygon(s->model, &p)) return MPK_FAIL;
            } else {
                if (!mpk_skip_to_end(s, &ph)) return MPK_FAIL;
            }
        }
        if (mpk_tell(s) < h->data_end) {
            if (!mpk_read_int_typed(s, &unknown)) return MPK_FAIL;
        }
        if (mpk_tell(s) < h->data_end) {
            if (!mpk_read_int_typed(s, &unknown)) return MPK_FAIL;
        }
    } else {
        if (!mpk_read_uint_typed(s, &count)) return MPK_FAIL;
        memset(&p, 0, sizeof(p));
        p.count = 3;
        for (i = 0; i < count; ++i) {
            if (!mpk_read_u16_raw(s, &idx)) return MPK_FAIL;
            p.v[i % 3UL] = (mpk_u32)idx;
            if ((i % 3UL) == 2UL) {
                if (!mpk_store_polygon(s->model, &p)) return MPK_FAIL;
                memset(&p, 0, sizeof(p));
                p.count = 3;
            }
        }
        if (mpk_tell(s) < h->data_end) {
            if (!mpk_read_uint_typed(s, &prims)) return MPK_FAIL;
            for (i = 0; i < prims && mpk_tell(s) < h->data_end; ++i) {
                if (!mpk_read_uint_typed(s, &tmp)) return MPK_FAIL;
            }
        }
    }
    mesh->polygon_count = s->model->polygon_count - mesh->first_polygon;
    return mpk_skip_to_end(s, h);
}

static int mpk_parse_smoothing(mpk_stream *s, const mpk_chunk_header *h, mpk_mesh *mesh)
{
    mpk_i32 count_i;
    mpk_u32 count;
    mpk_u32 i;
    mpk_u32 sg;
    (void)mesh;
    if (!mpk_read_int_typed(s, &count_i)) return MPK_FAIL;
    if (count_i < 0) return MPK_FAIL;
    count = (mpk_u32)count_i;
    for (i = 0; i < count && mpk_tell(s) < h->data_end; ++i) {
        if (!mpk_read_uint_typed(s, &sg)) return MPK_FAIL;
        if (mesh != (mpk_mesh *)0 && i < mesh->polygon_count) {
            s->model->polygons[mesh->first_polygon + i].smoothing = sg;
        }
    }
    return mpk_skip_to_end(s, h);
}

static int mpk_parse_poly_material(mpk_stream *s, const mpk_chunk_header *h, mpk_mesh *mesh)
{
    mpk_i32 mats_i;
    mpk_i32 polys_i;
    mpk_i32 mi;
    mpk_u32 i;
    char name[MPK_NAME_MAX];
    if (!mpk_read_int_typed(s, &mats_i)) return MPK_FAIL;
    if (mats_i < 0) return MPK_FAIL;
    for (i = 0; i < (mpk_u32)mats_i && mpk_tell(s) < h->data_end; ++i) {
        if (!mpk_read_string_typed(s, name, MPK_NAME_MAX)) return MPK_FAIL;
    }
    if (h->version == 0UL && mpk_tell(s) < h->data_end) {
        if (!mpk_read_int_typed(s, &polys_i)) return MPK_FAIL;
        if (polys_i < 0) return MPK_FAIL;
        for (i = 0; i < (mpk_u32)polys_i && mpk_tell(s) < h->data_end; ++i) {
            if (!mpk_read_int_typed(s, &mi)) return MPK_FAIL;
            if (mesh != (mpk_mesh *)0 && i < mesh->polygon_count) {
                s->model->polygons[mesh->first_polygon + i].material = (mpk_u32)mi;
            }
        }
    }
    return mpk_skip_to_end(s, h);
}

static int mpk_parse_uv_mapping(mpk_stream *s, const mpk_chunk_header *h, mpk_mesh *mesh)
{
    mpk_i32 layer_i;
    mpk_i32 count_i;
    mpk_i32 reserved;
    mpk_u32 count;
    mpk_u32 i;
    mpk_chunk_header ph;
    mpk_polygon p;
    mpk_uv uv;
    mpk_u32 prims;
    mpk_u32 tmp;
    (void)layer_i;

    mesh->first_uv = s->model->uv_count;
    if (h->version == 0UL) {
        if (!mpk_read_int_typed(s, &layer_i)) return MPK_FAIL;
        if (!mpk_read_int_typed(s, &count_i)) return MPK_FAIL;
        if (count_i < 0) return MPK_FAIL;
        for (i = 0; i < (mpk_u32)count_i && mpk_tell(s) < h->data_end; ++i) {
            if (!mpk_read_chunk_header(s, &ph)) return MPK_FAIL;
            if (!mpk_parse_one_polygon_body(s, &ph, &p)) return MPK_FAIL;
            if (mesh != (mpk_mesh *)0 && i < mesh->polygon_count) {
                int k;
                for (k = 0; k < p.count && k < MPK_MAX_POLY_VERTS; ++k) {
                    s->model->polygons[mesh->first_polygon + i].uv[k] = p.v[k];
                }
            }
        }
        if (!mpk_read_int_typed(s, &reserved)) return MPK_FAIL;
        if (!mpk_read_int_typed(s, &reserved)) return MPK_FAIL;
        if (!mpk_read_int_typed(s, &count_i)) return MPK_FAIL;
        if (count_i < 0) return MPK_FAIL;
        count = (mpk_u32)count_i;
        for (i = 0; i < count && mpk_tell(s) < h->data_end; ++i) {
            mpk_vec3 v;
            if (!mpk_read_vec3_typed(s, &v)) return MPK_FAIL;
            uv.u = v.x;
            uv.v = v.y;
            uv.w = v.z;
            if (!mpk_store_uv(s->model, &uv)) return MPK_FAIL;
        }
    } else {
        if (!mpk_read_uint_typed(s, &tmp)) return MPK_FAIL;
        if (!mpk_read_uint_typed(s, &count)) return MPK_FAIL;
        for (i = 0; i < count && mpk_tell(s) < h->data_end; ++i) {
            if (!mpk_read_fixed_raw32(s, &uv.u)) return MPK_FAIL;
            if (!mpk_read_fixed_raw32(s, &uv.v)) return MPK_FAIL;
            if (!mpk_read_fixed_raw32(s, &uv.w)) return MPK_FAIL;
            if (!mpk_store_uv(s->model, &uv)) return MPK_FAIL;
        }
        if (mpk_tell(s) < h->data_end) {
            if (!mpk_read_uint_typed(s, &prims)) return MPK_FAIL;
            for (i = 0; i < prims && mpk_tell(s) < h->data_end; ++i) {
                if (!mpk_read_uint_typed(s, &tmp)) return MPK_FAIL;
            }
        }
    }
    mesh->uv_count = s->model->uv_count - mesh->first_uv;
    return mpk_skip_to_end(s, h);
}

static int mpk_parse_reference(mpk_stream *s, const mpk_chunk_header *h, mpk_mesh *mesh)
{
    if (!mpk_read_string_typed(s, mesh->reference, MPK_NAME_MAX)) return MPK_FAIL;
    return mpk_skip_to_end(s, h);
}

static int mpk_parse_mesh(mpk_stream *s, const mpk_chunk_header *h)
{
    mpk_mesh *mesh;
    mpk_chunk_header sh;
    mesh = mpk_alloc_mesh(s->model);
    if (mesh == (mpk_mesh *)0) {
        return mpk_skip_to_end(s, h);
    }
    while (mpk_tell(s) < h->data_end) {
        if (!mpk_read_chunk_header(s, &sh)) return MPK_FAIL;
        if (sh.id == MPK_ID_NODE) {
            if (!mpk_parse_node_chunk(s, &sh, &mesh->node)) return MPK_FAIL;
            if (s->model->node_count < MPK_MAX_NODES) {
                s->model->nodes[s->model->node_count++] = mesh->node;
            }
        } else if (sh.id == MPK_ID_GEOMETRY) {
            if (!mpk_parse_geometry(s, &sh, mesh)) return MPK_FAIL;
        } else if (sh.id == MPK_ID_POLYGONS) {
            if (!mpk_parse_polygons(s, &sh, mesh)) return MPK_FAIL;
        } else if (sh.id == MPK_ID_SMOOTHING) {
            if (!mpk_parse_smoothing(s, &sh, mesh)) return MPK_FAIL;
        } else if (sh.id == MPK_ID_POLY_MAT) {
            if (!mpk_parse_poly_material(s, &sh, mesh)) return MPK_FAIL;
        } else if (sh.id == MPK_ID_UV_MAPPING) {
            if (!mpk_parse_uv_mapping(s, &sh, mesh)) return MPK_FAIL;
        } else if (sh.id == MPK_ID_REFERENCE) {
            if (!mpk_parse_reference(s, &sh, mesh)) return MPK_FAIL;
        } else {
            s->model->skipped_chunk_count++;
            if (!mpk_skip_to_end(s, &sh)) return MPK_FAIL;
        }
    }
    return mpk_skip_to_end(s, h);
}

static int mpk_store_skin_weight(mpk_model *m, mpk_u32 vi, mpk_u32 bi, mpk_fixed w)
{
    mpk_skin_weight *sw;
    if (m->skin_weight_count >= MPK_MAX_SKIN_WEIGHTS) return MPK_FAIL;
    sw = &m->skin_weights[m->skin_weight_count++];
    sw->vertex_index = vi;
    sw->bone_index = bi;
    sw->weight = w;
    return MPK_OK;
}

static void mpk_copy_name(char *dst, int dst_size, const char *src)
{
    int i;
    if (dst_size <= 0) return;
    for (i = 0; i < dst_size - 1 && src[i] != '\0'; ++i) {
        dst[i] = src[i];
    }
    dst[i] = '\0';
}

static int mpk_store_bone_name(mpk_bone_name *arr, mpk_u32 *count, mpk_u32 cap, const char *name)
{
    if (*count >= cap) return MPK_OK;
    mpk_copy_name(arr[*count].name, MPK_NAME_MAX, name);
    (*count)++;
    return MPK_OK;
}

static int mpk_parse_skin_weight_block_v0(mpk_stream *s, const mpk_chunk_header *h)
{
    mpk_i32 n_i;
    mpk_i32 vi;
    mpk_i32 bi;
    mpk_fixed w;
    mpk_u32 i;
    if (!mpk_read_int_typed(s, &n_i)) return MPK_FAIL;
    if (n_i < 0) return MPK_FAIL;
    for (i = 0; i < (mpk_u32)n_i && mpk_tell(s) < h->data_end; ++i) {
        if (!mpk_read_int_typed(s, &vi)) return MPK_FAIL;
        if (!mpk_read_int_typed(s, &bi)) return MPK_FAIL;
        if (!mpk_read_fixed_typed(s, &w)) return MPK_FAIL;
        if (!mpk_store_skin_weight(s->model, (mpk_u32)vi, (mpk_u32)bi, w)) return MPK_FAIL;
    }
    return MPK_OK;
}

static int mpk_parse_skin(mpk_stream *s, const mpk_chunk_header *h)
{
    mpk_i32 n_i;
    mpk_u32 i;
    char name[MPK_NAME_MAX];
    mpk_u8 tag;
    mpk_u32 n;
    mpk_u32 tmp;
    mpk_fixed w;
    mpk_vec3 v;

    if (h->version == 0UL) {
        if (!mpk_parse_skin_weight_block_v0(s, h)) return MPK_FAIL;
        if (!mpk_parse_skin_weight_block_v0(s, h)) return MPK_FAIL;
        if (!mpk_parse_skin_weight_block_v0(s, h)) return MPK_FAIL;
        if (!mpk_read_int_typed(s, &n_i)) return MPK_FAIL;
        if (n_i < 0) return MPK_FAIL;
        for (i = 0; i < (mpk_u32)n_i && mpk_tell(s) < h->data_end; ++i) {
            if (!mpk_read_string_typed(s, name, MPK_NAME_MAX)) return MPK_FAIL;
            mpk_store_bone_name(s->model->skin_objects, &s->model->skin_object_count, MPK_MAX_BONES, name);
        }
        if (!mpk_read_int_typed(s, &n_i)) return MPK_FAIL;
        if (n_i < 0) return MPK_FAIL;
        for (i = 0; i < (mpk_u32)n_i && mpk_tell(s) < h->data_end; ++i) {
            if (!mpk_read_string_typed(s, name, MPK_NAME_MAX)) return MPK_FAIL;
            mpk_store_bone_name(s->model->bones, &s->model->bone_count, MPK_MAX_BONES, name);
        }
        if (mpk_tell(s) < h->data_end) {
            if (!mpk_read_int_typed(s, &n_i)) return MPK_FAIL;
            if (n_i < 0) return MPK_FAIL;
            for (i = 0; i < (mpk_u32)n_i && mpk_tell(s) < h->data_end; ++i) {
                if (!mpk_read_vec3_typed(s, &v)) return MPK_FAIL;
            }
        }
    } else {
        mpk_u32 cursor;
        mpk_u32 vertex_id;
        mpk_u32 bone_count_for_vertex;
        mpk_u32 bidx;

        /* SKD/KF2 skin v1, documented as typed groups:
           skin object names, skeleton object names, vertex_indices,
           bones_per_vertex, bones_list_per_vertex, weights, primitive tables. */
        if (!mpk_read_u8_raw(s, &tag)) return MPK_FAIL;
        (void)tag;
        if (!mpk_read_int_typed(s, &n_i)) return MPK_FAIL;
        if (n_i < 0) return MPK_FAIL;
        for (i = 0; i < (mpk_u32)n_i; ++i) {
            if (!mpk_read_string_typed(s, name, MPK_NAME_MAX)) return MPK_FAIL;
            mpk_store_bone_name(s->model->skin_objects, &s->model->skin_object_count, MPK_MAX_BONES, name);
        }

        if (!mpk_read_u8_raw(s, &tag)) return MPK_FAIL;
        (void)tag;
        if (!mpk_read_int_typed(s, &n_i)) return MPK_FAIL;
        if (n_i < 0) return MPK_FAIL;
        for (i = 0; i < (mpk_u32)n_i; ++i) {
            if (!mpk_read_string_typed(s, name, MPK_NAME_MAX)) return MPK_FAIL;
            mpk_store_bone_name(s->model->bones, &s->model->bone_count, MPK_MAX_BONES, name);
        }

        if (!mpk_read_u8_raw(s, &tag)) return MPK_FAIL;
        (void)tag;
        if (!mpk_read_uint_typed(s, &n)) return MPK_FAIL;
        s->model->skin_v1_vertex_index_count = 0UL;
        for (i = 0; i < n && mpk_tell(s) < h->data_end; ++i) {
            if (!mpk_read_uint_typed(s, &tmp)) return MPK_FAIL;
            if (s->model->skin_v1_vertex_index_count < MPK_MAX_VERTICES) {
                s->model->skin_v1_vertex_indices[s->model->skin_v1_vertex_index_count++] = tmp;
            }
        }

        if (!mpk_read_u8_raw(s, &tag)) return MPK_FAIL;
        (void)tag;
        if (!mpk_read_uint_typed(s, &n)) return MPK_FAIL;
        s->model->skin_v1_bones_per_vertex_count = 0UL;
        for (i = 0; i < n && mpk_tell(s) < h->data_end; ++i) {
            if (!mpk_read_uint_typed(s, &tmp)) return MPK_FAIL;
            if (s->model->skin_v1_bones_per_vertex_count < MPK_MAX_VERTICES) {
                s->model->skin_v1_bones_per_vertex[s->model->skin_v1_bones_per_vertex_count++] = tmp;
            }
        }

        if (!mpk_read_u8_raw(s, &tag)) return MPK_FAIL;
        (void)tag;
        if (!mpk_read_uint_typed(s, &n)) return MPK_FAIL;
        s->model->skin_v1_bones_list_count = 0UL;
        for (i = 0; i < n && mpk_tell(s) < h->data_end; ++i) {
            if (!mpk_read_uint_typed(s, &tmp)) return MPK_FAIL;
            if (s->model->skin_v1_bones_list_count < MPK_MAX_SKIN_WEIGHTS) {
                s->model->skin_v1_bones_list[s->model->skin_v1_bones_list_count++] = tmp;
            }
        }

        if (!mpk_read_u8_raw(s, &tag)) return MPK_FAIL;
        (void)tag;
        if (!mpk_read_uint_typed(s, &n)) return MPK_FAIL;
        s->model->skin_v1_weight_count = 0UL;
        for (i = 0; i < n && mpk_tell(s) < h->data_end; ++i) {
            if (!mpk_read_fixed_typed(s, &w)) return MPK_FAIL;
            if (s->model->skin_v1_weight_count < MPK_MAX_SKIN_WEIGHTS) {
                s->model->skin_v1_weights[s->model->skin_v1_weight_count++] = w;
            }
        }

        cursor = 0UL;
        s->model->skin_v1_reconstructed_count = 0UL;
        for (i = 0; i < s->model->skin_v1_vertex_index_count && i < s->model->skin_v1_bones_per_vertex_count; ++i) {
            vertex_id = s->model->skin_v1_vertex_indices[i];
            bone_count_for_vertex = s->model->skin_v1_bones_per_vertex[i];
            for (bidx = 0; bidx < bone_count_for_vertex; ++bidx) {
                if (cursor >= s->model->skin_v1_bones_list_count) break;
                if (cursor >= s->model->skin_v1_weight_count) break;
                if (mpk_store_skin_weight(s->model, vertex_id, s->model->skin_v1_bones_list[cursor], s->model->skin_v1_weights[cursor])) {
                    s->model->skin_v1_reconstructed_count++;
                }
                cursor++;
            }
        }

        /* Remaining primitive lookup tables are preserved structurally by skipping
           typed values. They are useful for exact MP2 primitive reconstruction but
           not needed for global vertex->bone weights. */
        while (mpk_tell(s) < h->data_end) {
            mpk_u32 group_count;
            mpk_u32 gi;
            if (!mpk_read_u8_raw(s, &tag)) return MPK_FAIL;
            (void)tag;
            if (!mpk_read_uint_typed(s, &group_count)) return MPK_FAIL;
            for (gi = 0; gi < group_count && mpk_tell(s) < h->data_end; ++gi) {
                if (!mpk_read_uint_typed(s, &tmp)) return MPK_FAIL;
            }
        }
    }
    return mpk_skip_to_end(s, h);
}

static int mpk_parse_animation_subchunk(mpk_stream *s, const mpk_chunk_header *h, mpk_animation *a)
{
    mpk_i32 fps;
    int looping;
    if (!mpk_read_string_typed(s, a->object_name, MPK_NAME_MAX)) return MPK_FAIL;
    if (!mpk_read_int_typed(s, &fps)) return MPK_FAIL;
    if (!mpk_read_bool_typed(s, &looping)) return MPK_FAIL;
    a->fps = fps;
    a->looping = looping;
    return mpk_skip_to_end(s, h);
}

static int mpk_parse_keyframe_anim(mpk_stream *s, const mpk_chunk_header *h)
{
    mpk_animation *a;
    mpk_chunk_header ah;
    mpk_u32 anim_index;
    mpk_u32 total;
    mpk_u32 key_count;
    mpk_u32 i;
    mpk_u32 frame_id;
    mpk_u32 vis_count;
    mpk_fixed vis;
    mpk_i32 method;
    int b;

    if (s->model->animation_count >= MPK_MAX_ANIMS) return mpk_skip_to_end(s, h);
    anim_index = s->model->animation_count;
    a = &s->model->animations[s->model->animation_count++];
    memset(a, 0, sizeof(*a));
    a->first_keyframe = s->model->keyframe_count;

    if (!mpk_read_chunk_header(s, &ah)) return MPK_FAIL;
    if (ah.id == MPK_ID_ANIMATION) {
        if (!mpk_parse_animation_subchunk(s, &ah, a)) return MPK_FAIL;
    } else {
        if (!mpk_skip_to_end(s, &ah)) return MPK_FAIL;
    }
    if (!mpk_read_string_typed(s, a->parent_name, MPK_NAME_MAX)) return MPK_FAIL;
    if (!mpk_read_bool_typed(s, &a->loop_interpolation)) return MPK_FAIL;
    if (!mpk_read_uint_typed(s, &total)) return MPK_FAIL;
    if (h->version < 5UL) total += 1UL;
    a->total_keyframes = total;
    if (!mpk_read_uint_typed(s, &key_count)) return MPK_FAIL;
    for (i = 0; i < key_count && mpk_tell(s) < h->data_end; ++i) {
        if (!mpk_read_uint_typed(s, &frame_id)) return MPK_FAIL;
        if (s->model->keyframe_count < MPK_MAX_KEYFRAMES) {
            mpk_keyframe *kf;
            kf = &s->model->keyframes[s->model->keyframe_count++];
            kf->anim_index = anim_index;
            kf->frame_id = frame_id;
            if (!mpk_read_mat4x3_typed(s, &kf->transform)) return MPK_FAIL;
        } else {
            mpk_matrix4x3 tmpm;
            if (!mpk_read_mat4x3_typed(s, &tmpm)) return MPK_FAIL;
        }
    }
    a->keyframe_count = s->model->keyframe_count - a->first_keyframe;
    if (h->version >= 1UL && mpk_tell(s) < h->data_end) {
        if (!mpk_read_uint_typed(s, &vis_count)) return MPK_FAIL;
        for (i = 0; i < vis_count && mpk_tell(s) < h->data_end; ++i) {
            if (!mpk_read_uint_typed(s, &frame_id)) return MPK_FAIL;
            if (!mpk_read_fixed_typed(s, &vis)) return MPK_FAIL;
        }
    }
    if (h->version >= 2UL && mpk_tell(s) < h->data_end) {
        if (!mpk_read_uint_typed(s, &frame_id)) return MPK_FAIL;
    }
    if (h->version >= 3UL && mpk_tell(s) < h->data_end) {
        if (!mpk_read_int_typed(s, &method)) return MPK_FAIL;
    }
    if (h->version >= 4UL && mpk_tell(s) < h->data_end) {
        if (!mpk_read_bool_typed(s, &b)) return MPK_FAIL;
    }
    return mpk_skip_to_end(s, h);
}

static int mpk_parse_camera(mpk_stream *s, const mpk_chunk_header *h)
{
    mpk_camera *c;
    mpk_chunk_header nh;
    mpk_node tmp;
    if (s->model->camera_count >= 32UL) return mpk_skip_to_end(s, h);
    c = &s->model->cameras[s->model->camera_count++];
    memset(c, 0, sizeof(*c));
    if (!mpk_read_chunk_header(s, &nh)) return MPK_FAIL;
    if (nh.id == MPK_ID_NODE) {
        if (!mpk_parse_node_chunk(s, &nh, &tmp)) return MPK_FAIL;
        c->node = tmp;
    } else {
        if (!mpk_skip_to_end(s, &nh)) return MPK_FAIL;
    }
    if (!mpk_read_fixed_typed(s, &c->fov)) return MPK_FAIL;
    if (!mpk_read_fixed_typed(s, &c->front_plane)) return MPK_FAIL;
    if (!mpk_read_fixed_typed(s, &c->back_plane)) return MPK_FAIL;
    return mpk_skip_to_end(s, h);
}

static int mpk_parse_environment(mpk_stream *s, const mpk_chunk_header *h)
{
    mpk_environment *e;
    int i;
    if (s->model->environment_count >= 8UL) return mpk_skip_to_end(s, h);
    e = &s->model->environments[s->model->environment_count++];
    memset(e, 0, sizeof(*e));
    for (i = 0; i < 4; ++i) {
        if (!mpk_read_uchar_typed(s, &e->ambient[i])) return MPK_FAIL;
    }
    for (i = 0; i < 4; ++i) {
        if (!mpk_read_uchar_typed(s, &e->clearing[i])) return MPK_FAIL;
    }
    return mpk_skip_to_end(s, h);
}

const char *mpk_light_type_name(int type)
{
    if (type == MPK_LIGHT_POINT) return "PointLight";
    if (type == MPK_LIGHT_DIRECTIONAL) return "DirectionalLight";
    if (type == MPK_LIGHT_SPOT) return "SpotLight";
    return "Light";
}

static int mpk_store_light_param(mpk_light *l, const mpk_light_param *p)
{
    if (l->param_count >= MPK_MAX_LIGHT_PARAMS) return MPK_OK;
    l->params[l->param_count++] = *p;
    return MPK_OK;
}

static int mpk_store_light_vec(mpk_light *l, const mpk_vec3 *v)
{
    if (l->vector_count >= MPK_MAX_LIGHT_VECS) return MPK_OK;
    l->vectors[l->vector_count++] = *v;
    return MPK_OK;
}

static void mpk_light_assign_semantic_defaults(mpk_light *l)
{
    mpk_u32 i;
    l->color[0] = 65536L;
    l->color[1] = 65536L;
    l->color[2] = 65536L;
    l->color[3] = 65536L;
    l->intensity = 65536L;
    l->range = 0;
    l->inner_cone = 0;
    l->outer_cone = 0;
    for (i = 0; i < l->param_count; ++i) {
        if (l->params[i].kind == MPK_LPARAM_FIXED) {
            if (l->intensity == 65536L) {
                l->intensity = l->params[i].fx;
            } else if (l->range == 0) {
                l->range = l->params[i].fx;
            } else if (l->inner_cone == 0) {
                l->inner_cone = l->params[i].fx;
            } else if (l->outer_cone == 0) {
                l->outer_cone = l->params[i].fx;
            }
        }
    }
    if (l->vector_count > 0UL) {
        l->color[0] = l->vectors[0].x;
        l->color[1] = l->vectors[0].y;
        l->color[2] = l->vectors[0].z;
        l->color[3] = 65536L;
    }
}

static int mpk_parse_light_typed_payload_after_tag(mpk_stream *s, mpk_light *l, mpk_u8 tag)
{
    mpk_light_param p;
    mpk_u32 u;
    mpk_u16 u16;
    mpk_u8 u8;
    mpk_vec3 v;
    char temp[MPK_PATH_MAX];
    int i;
    memset(&p, 0, sizeof(p));
    p.source_tag = tag;

    if (tag == MPK_T_FLOAT32) {
        if (!mpk_read_u32_raw(s, &u)) return MPK_FAIL;
        p.kind = MPK_LPARAM_FIXED;
        p.fx = mpk_fixed_from_float_bits(u);
        return mpk_store_light_param(l, &p);
    }
    if (tag == MPK_T_FLOAT16) {
        if (!mpk_read_u16_raw(s, &u16)) return MPK_FAIL;
        p.kind = MPK_LPARAM_FIXED;
        p.fx = mpk_fixed_from_half_bits((mpk_u32)u16);
        return mpk_store_light_param(l, &p);
    }
    if (tag == MPK_T_INT32 || tag == MPK_T_LONG) {
        if (!mpk_read_u32_raw(s, &u)) return MPK_FAIL;
        p.kind = MPK_LPARAM_INT;
        p.i = (mpk_i32)u;
        return mpk_store_light_param(l, &p);
    }
    if (tag == MPK_T_UINT32 || tag == MPK_T_ULONG) {
        if (!mpk_read_u32_raw(s, &u)) return MPK_FAIL;
        p.kind = MPK_LPARAM_UINT;
        p.u = u;
        return mpk_store_light_param(l, &p);
    }
    if (tag == MPK_T_INT24) {
        if (!mpk_read_u24_raw(s, &u)) return MPK_FAIL;
        p.kind = MPK_LPARAM_INT;
        p.i = mpk_sign_extend(u, 24);
        return mpk_store_light_param(l, &p);
    }
    if (tag == MPK_T_UINT24) {
        if (!mpk_read_u24_raw(s, &u)) return MPK_FAIL;
        p.kind = MPK_LPARAM_UINT;
        p.u = u;
        return mpk_store_light_param(l, &p);
    }
    if (tag == MPK_T_INT16 || tag == MPK_T_SHORT) {
        if (!mpk_read_u16_raw(s, &u16)) return MPK_FAIL;
        p.kind = MPK_LPARAM_INT;
        p.i = mpk_sign_extend((mpk_u32)u16, 16);
        return mpk_store_light_param(l, &p);
    }
    if (tag == MPK_T_UINT16 || tag == MPK_T_USHORT) {
        if (!mpk_read_u16_raw(s, &u16)) return MPK_FAIL;
        p.kind = MPK_LPARAM_UINT;
        p.u = (mpk_u32)u16;
        return mpk_store_light_param(l, &p);
    }
    if (tag == MPK_T_INT8 || tag == MPK_T_SCHAR || tag == MPK_T_CHAR) {
        if (!mpk_read_u8_raw(s, &u8)) return MPK_FAIL;
        p.kind = MPK_LPARAM_INT;
        p.i = mpk_sign_extend((mpk_u32)u8, 8);
        return mpk_store_light_param(l, &p);
    }
    if (tag == MPK_T_UINT8 || tag == MPK_T_UCHAR) {
        if (!mpk_read_u8_raw(s, &u8)) return MPK_FAIL;
        p.kind = MPK_LPARAM_UCHAR;
        p.u8 = u8;
        return mpk_store_light_param(l, &p);
    }
    if (tag == MPK_T_BOOL) {
        if (!mpk_read_u8_raw(s, &u8)) return MPK_FAIL;
        p.kind = MPK_LPARAM_BOOL;
        p.u8 = u8 ? 1U : 0U;
        return mpk_store_light_param(l, &p);
    }
    if (tag == MPK_T_STRING) {
        return mpk_read_string_after_tag(s, temp, MPK_PATH_MAX);
    }
    if (tag == MPK_T_VEC3) {
        if (!mpk_read_fixed_raw32(s, &v.x)) return MPK_FAIL;
        if (!mpk_read_fixed_raw32(s, &v.y)) return MPK_FAIL;
        if (!mpk_read_fixed_raw32(s, &v.z)) return MPK_FAIL;
        return mpk_store_light_vec(l, &v);
    }
    if (tag == MPK_T_VEC4) {
        if (!mpk_read_fixed_raw32(s, &l->color[0])) return MPK_FAIL;
        if (!mpk_read_fixed_raw32(s, &l->color[1])) return MPK_FAIL;
        if (!mpk_read_fixed_raw32(s, &l->color[2])) return MPK_FAIL;
        if (!mpk_read_fixed_raw32(s, &l->color[3])) return MPK_FAIL;
        return MPK_OK;
    }
    if (tag == MPK_T_MAT4X3) {
        /* Some tool exports put a transform matrix directly in the light body. */
        for (i = 0; i < 12; ++i) {
            if (!mpk_read_fixed_raw32(s, &l->node.object_to_parent.m[i])) return MPK_FAIL;
        }
        return MPK_OK;
    }
    return mpk_skip_typed_value_after_tag(s, tag);
}

static int mpk_parse_light(mpk_stream *s, const mpk_chunk_header *h, int light_type)
{
    mpk_light *l;
    mpk_u8 tag;
    mpk_chunk_header sh;
    mpk_node tmp;
    if (s->model->light_count >= MPK_MAX_LIGHTS) return mpk_skip_to_end(s, h);
    l = &s->model->lights[s->model->light_count++];
    memset(l, 0, sizeof(*l));
    l->type = light_type;
    l->version = h->version;
    mpk_mat_identity(&l->node.object_to_parent);
    while (mpk_tell(s) < h->data_end) {
        if (!mpk_read_u8_raw(s, &tag)) return MPK_FAIL;
        if (tag == MPK_CHUNK_TAG) {
            if (!mpk_read_chunk_header_after_tag(s, &sh)) return MPK_FAIL;
            if (sh.id == MPK_ID_NODE) {
                if (!mpk_parse_node_chunk(s, &sh, &tmp)) return MPK_FAIL;
                l->node = tmp;
            } else if (sh.id == MPK_ID_ANIMATION) {
                mpk_animation dummy;
                memset(&dummy, 0, sizeof(dummy));
                if (!mpk_parse_animation_subchunk(s, &sh, &dummy)) return MPK_FAIL;
            } else {
                s->model->skipped_chunk_count++;
                if (!mpk_skip_to_end(s, &sh)) return MPK_FAIL;
            }
        } else {
            if (!mpk_parse_light_typed_payload_after_tag(s, l, tag)) return MPK_FAIL;
        }
    }
    mpk_light_assign_semantic_defaults(l);
    return mpk_skip_to_end(s, h);
}

static int mpk_parse_helper(mpk_stream *s, const mpk_chunk_header *h)
{
    mpk_chunk_header sh;
    while (mpk_tell(s) < h->data_end) {
        if (!mpk_read_chunk_header(s, &sh)) return MPK_FAIL;
        if (sh.id == MPK_ID_NODE) {
            if (!mpk_parse_node_chunk(s, &sh, (mpk_node *)0)) return MPK_FAIL;
        } else {
            if (!mpk_skip_to_end(s, &sh)) return MPK_FAIL;
        }
    }
    return mpk_skip_to_end(s, h);
}

static int mpk_parse_top_chunk(mpk_stream *s, const mpk_chunk_header *h)
{
    if (h->id == MPK_ID_MATERIAL_LIST) return mpk_parse_material_list(s, h);
    if (h->id == MPK_ID_MESH) return mpk_parse_mesh(s, h);
    if (h->id == MPK_ID_SKIN) return mpk_parse_skin(s, h);
    if (h->id == MPK_ID_KEYFRAME_ANIM) return mpk_parse_keyframe_anim(s, h);
    if (h->id == MPK_ID_POINT_ANIM) return mpk_parse_keyframe_anim(s, h);
    if (h->id == MPK_ID_DIR_ANIM) return mpk_parse_keyframe_anim(s, h);
    if (h->id == MPK_ID_SPOT_ANIM) return mpk_parse_keyframe_anim(s, h);
    if (h->id == MPK_ID_CAMERA) return mpk_parse_camera(s, h);
    if (h->id == MPK_ID_ENVIRONMENT) return mpk_parse_environment(s, h);
    if (h->id == MPK_ID_POINT_LIGHT) return mpk_parse_light(s, h, MPK_LIGHT_POINT);
    if (h->id == MPK_ID_DIR_LIGHT) return mpk_parse_light(s, h, MPK_LIGHT_DIRECTIONAL);
    if (h->id == MPK_ID_SPOT_LIGHT) return mpk_parse_light(s, h, MPK_LIGHT_SPOT);
    if (h->id == MPK_ID_HELPER) return mpk_parse_helper(s, h);
    s->model->unknown_chunk_count++;
    return mpk_skip_to_end(s, h);
}

int mpk_read_file(const char *path, mpk_model *out_model)
{
    mpk_stream s;
    mpk_chunk_header h;
    mpk_u8 tag;

    if (out_model == (mpk_model *)0 || path == (const char *)0) return MPK_FAIL;
    mpk_model_init(out_model);
    s.f = fopen(path, "rb");
    s.model = out_model;
    s.strict_tags = 1;
    if (s.f == (FILE *)0) {
        mpk_set_error(out_model, "could not open input file");
        return MPK_FAIL;
    }
    while (mpk_read_u8_raw(&s, &tag)) {
        if (tag != MPK_CHUNK_TAG) {
            fclose(s.f);
            mpk_set_error(out_model, "input is not a KF2/KFS/SKD chunk stream");
            return MPK_FAIL;
        }
        if (!mpk_read_chunk_header_after_tag(&s, &h)) {
            fclose(s.f);
            mpk_set_error(out_model, "truncated chunk header");
            return MPK_FAIL;
        }
        if (!mpk_parse_top_chunk(&s, &h)) {
            fclose(s.f);
            if (out_model->last_error[0] == '\0') mpk_set_error(out_model, "parse failure");
            return MPK_FAIL;
        }
    }
    fclose(s.f);
    return MPK_OK;
}

static int mpk_find_node_index_in_resolved(const mpk_model *m, const char *name)
{
    mpk_u32 i;
    for (i = 0; i < m->resolved_node_count; ++i) {
        if (mpk_names_equal(m->resolved_nodes[i].name, name)) return (int)i;
    }
    return -1;
}

int mpk_build_resolved_nodes(mpk_model *m)
{
    mpk_u32 i;
    mpk_u32 pass;
    int parent;
    int changed;
    if (m == (mpk_model *)0) return MPK_FAIL;
    m->resolved_node_count = 0UL;
    for (i = 0; i < m->node_count && i < MPK_MAX_RESOLVED_NODES; ++i) {
        mpk_resolved_node *rn;
        rn = &m->resolved_nodes[m->resolved_node_count++];
        memset(rn, 0, sizeof(*rn));
        mpk_copy_name(rn->name, MPK_NAME_MAX, m->nodes[i].name);
        mpk_copy_name(rn->parent, MPK_NAME_MAX, m->nodes[i].parent);
        rn->local = m->nodes[i].object_to_parent;
        rn->has_parent = m->nodes[i].has_parent;
        rn->parent_index = 0xffffffffUL;
        rn->resolved = MPK_FALSE;
    }
    for (i = 0; i < m->resolved_node_count; ++i) {
        if (!m->resolved_nodes[i].has_parent || m->resolved_nodes[i].parent[0] == '\0') {
            m->resolved_nodes[i].world = m->resolved_nodes[i].local;
            m->resolved_nodes[i].resolved = MPK_TRUE;
        }
    }
    for (pass = 0; pass < m->resolved_node_count; ++pass) {
        changed = MPK_FALSE;
        for (i = 0; i < m->resolved_node_count; ++i) {
            if (!m->resolved_nodes[i].resolved) {
                parent = mpk_find_node_index_in_resolved(m, m->resolved_nodes[i].parent);
                if (parent < 0) {
                    m->resolved_nodes[i].world = m->resolved_nodes[i].local;
                    m->resolved_nodes[i].resolved = MPK_TRUE;
                    changed = MPK_TRUE;
                } else if (m->resolved_nodes[parent].resolved) {
                    m->resolved_nodes[i].parent_index = (mpk_u32)parent;
                    mpk_mat_mul(&m->resolved_nodes[i].world, &m->resolved_nodes[i].local, &m->resolved_nodes[parent].world);
                    m->resolved_nodes[i].resolved = MPK_TRUE;
                    changed = MPK_TRUE;
                }
            }
        }
        if (!changed) break;
    }
    for (i = 0; i < m->resolved_node_count; ++i) {
        if (!m->resolved_nodes[i].resolved) {
            m->resolved_nodes[i].world = m->resolved_nodes[i].local;
            m->resolved_nodes[i].resolved = MPK_TRUE;
        }
    }
    return MPK_OK;
}

int mpk_find_animation(const mpk_model *m, const char *object_name)
{
    mpk_u32 i;
    if (m == (const mpk_model *)0 || object_name == (const char *)0) return -1;
    for (i = 0; i < m->animation_count; ++i) {
        if (mpk_names_equal(m->animations[i].object_name, object_name)) return (int)i;
    }
    return -1;
}

int mpk_eval_animation_matrix(const mpk_model *m, mpk_u32 anim_index, mpk_u32 frame, mpk_matrix4x3 *out)
{
    const mpk_animation *a;
    const mpk_keyframe *ka;
    const mpk_keyframe *kb;
    mpk_u32 i;
    int j;
    if (m == (const mpk_model *)0 || out == (mpk_matrix4x3 *)0) return MPK_FAIL;
    if (anim_index >= m->animation_count) return MPK_FAIL;
    a = &m->animations[anim_index];
    if (a->keyframe_count == 0UL) {
        mpk_mat_identity(out);
        return MPK_OK;
    }
    ka = &m->keyframes[a->first_keyframe];
    kb = ka;
    if (frame <= ka->frame_id) {
        mpk_mat_copy(out, &ka->transform);
        return MPK_OK;
    }
    for (i = 0; i + 1UL < a->keyframe_count; ++i) {
        ka = &m->keyframes[a->first_keyframe + i];
        kb = &m->keyframes[a->first_keyframe + i + 1UL];
        if (frame >= ka->frame_id && frame <= kb->frame_id) {
            mpk_u32 den;
            mpk_u32 num;
            den = kb->frame_id - ka->frame_id;
            num = frame - ka->frame_id;
            for (j = 0; j < 12; ++j) out->m[j] = mpk_fixed_lerp(ka->transform.m[j], kb->transform.m[j], num, den);
            return MPK_OK;
        }
    }
    kb = &m->keyframes[a->first_keyframe + a->keyframe_count - 1UL];
    mpk_mat_copy(out, &kb->transform);
    return MPK_OK;
}

int mpk_eval_camera_path(const mpk_model *m, mpk_u32 camera_index, mpk_u32 frame, mpk_camera_pose *out)
{
    int anim;
    if (m == (const mpk_model *)0 || out == (mpk_camera_pose *)0) return MPK_FAIL;
    if (camera_index >= m->camera_count) return MPK_FAIL;
    memset(out, 0, sizeof(*out));
    out->fov = m->cameras[camera_index].fov;
    out->front_plane = m->cameras[camera_index].front_plane;
    out->back_plane = m->cameras[camera_index].back_plane;
    out->transform = m->cameras[camera_index].node.object_to_parent;
    out->has_animation = MPK_FALSE;
    out->source_animation = 0xffffffffUL;
    anim = mpk_find_animation(m, m->cameras[camera_index].node.name);
    if (anim < 0 && m->cameras[camera_index].node.parent[0] != '\0') {
        anim = mpk_find_animation(m, m->cameras[camera_index].node.parent);
    }
    if (anim >= 0) {
        if (!mpk_eval_animation_matrix(m, (mpk_u32)anim, frame, &out->transform)) return MPK_FAIL;
        out->has_animation = MPK_TRUE;
        out->source_animation = (mpk_u32)anim;
    }
    return MPK_OK;
}

int mpk_apply_skin_to_bind_pose(mpk_model *m)
{
    mpk_u32 i;
    mpk_u32 wi;
    int had;
    if (m == (mpk_model *)0) return MPK_FAIL;
    if (m->resolved_node_count == 0UL) {
        if (!mpk_build_resolved_nodes(m)) return MPK_FAIL;
    }
    m->posed_vertex_count = m->vertex_count;
    for (i = 0; i < m->vertex_count && i < MPK_MAX_VERTICES; ++i) {
        m->posed_vertices[i] = m->vertices[i];
    }
    for (i = 0; i < m->vertex_count && i < MPK_MAX_VERTICES; ++i) {
        mpk_vec3 acc;
        acc.x = 0;
        acc.y = 0;
        acc.z = 0;
        had = MPK_FALSE;
        for (wi = 0; wi < m->skin_weight_count; ++wi) {
            if (m->skin_weights[wi].vertex_index == i) {
                int bone_node;
                mpk_vec3 tv;
                bone_node = -1;
                if (m->skin_weights[wi].bone_index < m->bone_count) {
                    bone_node = mpk_find_node_index_in_resolved(m, m->bones[m->skin_weights[wi].bone_index].name);
                }
                if (bone_node < 0 && m->skin_weights[wi].bone_index < m->resolved_node_count) {
                    bone_node = (int)m->skin_weights[wi].bone_index;
                }
                if (bone_node >= 0) {
                    mpk_mat_transform_point(&m->resolved_nodes[bone_node].world, &m->vertices[i], &tv);
                } else {
                    tv = m->vertices[i];
                }
                acc.x = (mpk_fixed)(acc.x + mpk_fixed_mul(tv.x, m->skin_weights[wi].weight));
                acc.y = (mpk_fixed)(acc.y + mpk_fixed_mul(tv.y, m->skin_weights[wi].weight));
                acc.z = (mpk_fixed)(acc.z + mpk_fixed_mul(tv.z, m->skin_weights[wi].weight));
                had = MPK_TRUE;
            }
        }
        if (had) m->posed_vertices[i] = acc;
    }
    m->posed_valid = MPK_TRUE;
    return MPK_OK;
}

static int mpk_write_obj_core(const char *path, const mpk_model *m, int posed)
{
    FILE *f;
    mpk_u32 i;
    char sx[32];
    char sy[32];
    char sz[32];
    if (path == (const char *)0 || m == (const mpk_model *)0) return MPK_FAIL;
    f = fopen(path, "w");
    if (f == (FILE *)0) return MPK_FAIL;
    fprintf(f, "# OBJ dumped by mp_kf_decoder_c89\n");
    if (posed) fprintf(f, "# posed/skinned best-effort vertices\n");
    for (i = 0; i < m->vertex_count; ++i) {
        const mpk_vec3 *v;
        v = &m->vertices[i];
        if (posed && m->posed_valid && i < m->posed_vertex_count) v = &m->posed_vertices[i];
        mpk_fixed_to_string(v->x, sx, 32);
        mpk_fixed_to_string(v->y, sy, 32);
        mpk_fixed_to_string(v->z, sz, 32);
        fprintf(f, "v %s %s %s\n", sx, sy, sz);
    }
    for (i = 0; i < m->uv_count; ++i) {
        mpk_fixed_to_string(m->uvs[i].u, sx, 32);
        mpk_fixed_to_string(m->uvs[i].v, sy, 32);
        fprintf(f, "vt %s %s\n", sx, sy);
    }
    for (i = 0; i < m->polygon_count; ++i) {
        const mpk_polygon *p;
        p = &m->polygons[i];
        if (p->count == 3) {
            fprintf(f, "f %lu/%lu %lu/%lu %lu/%lu\n",
                (unsigned long)(p->v[0] + 1UL), (unsigned long)(p->uv[0] + 1UL),
                (unsigned long)(p->v[1] + 1UL), (unsigned long)(p->uv[1] + 1UL),
                (unsigned long)(p->v[2] + 1UL), (unsigned long)(p->uv[2] + 1UL));
        } else if (p->count == 4) {
            fprintf(f, "f %lu/%lu %lu/%lu %lu/%lu %lu/%lu\n",
                (unsigned long)(p->v[0] + 1UL), (unsigned long)(p->uv[0] + 1UL),
                (unsigned long)(p->v[1] + 1UL), (unsigned long)(p->uv[1] + 1UL),
                (unsigned long)(p->v[2] + 1UL), (unsigned long)(p->uv[2] + 1UL),
                (unsigned long)(p->v[3] + 1UL), (unsigned long)(p->uv[3] + 1UL));
        }
    }
    fclose(f);
    return MPK_OK;
}

void mpk_print_summary(FILE *out, const mpk_model *m)
{
    mpk_u32 i;
    if (out == (FILE *)0 || m == (const mpk_model *)0) return;
    fprintf(out, "Max Payne KF2/KFS/SKD decode summary\n");
    fprintf(out, "  meshes:      %lu\n", (unsigned long)m->mesh_count);
    fprintf(out, "  materials:   %lu\n", (unsigned long)m->material_count);
    fprintf(out, "  nodes/bones: %lu / %lu\n", (unsigned long)m->node_count, (unsigned long)m->bone_count);
    fprintf(out, "  vertices:    %lu\n", (unsigned long)m->vertex_count);
    fprintf(out, "  normals:     %lu\n", (unsigned long)m->normal_count);
    fprintf(out, "  uvs:         %lu\n", (unsigned long)m->uv_count);
    fprintf(out, "  polygons:    %lu\n", (unsigned long)m->polygon_count);
    fprintf(out, "  skin weights:%lu\n", (unsigned long)m->skin_weight_count);
    fprintf(out, "  animations:  %lu\n", (unsigned long)m->animation_count);
    fprintf(out, "  keyframes:   %lu\n", (unsigned long)m->keyframe_count);
    fprintf(out, "  cameras:     %lu\n", (unsigned long)m->camera_count);
    fprintf(out, "  lights:      %lu\n", (unsigned long)m->light_count);
    fprintf(out, "  env chunks:  %lu\n", (unsigned long)m->environment_count);
    fprintf(out, "  SKD v1 reconstructed weights: %lu\n", (unsigned long)m->skin_v1_reconstructed_count);
    fprintf(out, "  resolved nodes / posed valid: %lu / %d\n",
        (unsigned long)m->resolved_node_count, m->posed_valid);
    fprintf(out, "  skipped/unknown chunks: %lu / %lu\n",
        (unsigned long)m->skipped_chunk_count, (unsigned long)m->unknown_chunk_count);
    for (i = 0; i < m->mesh_count; ++i) {
        fprintf(out, "  mesh[%lu]: name='%s' verts=%lu polys=%lu ref='%s'\n",
            (unsigned long)i,
            m->meshes[i].node.name,
            (unsigned long)m->meshes[i].vertex_count,
            (unsigned long)m->meshes[i].polygon_count,
            m->meshes[i].reference);
    }
    for (i = 0; i < m->material_count && i < 16UL; ++i) {
        fprintf(out, "  mat[%lu]: '%s' diffuse='%s'\n",
            (unsigned long)i,
            m->materials[i].name,
            m->materials[i].diffuse_texture);
    }
    for (i = 0; i < m->light_count && i < 16UL; ++i) {
        char sr[32];
        char sg[32];
        char sb[32];
        mpk_fixed_to_string(m->lights[i].color[0], sr, 32);
        mpk_fixed_to_string(m->lights[i].color[1], sg, 32);
        mpk_fixed_to_string(m->lights[i].color[2], sb, 32);
        fprintf(out, "  light[%lu]: %s name='%s' params=%lu vecs=%lu color=%s,%s,%s\n",
            (unsigned long)i, mpk_light_type_name(m->lights[i].type),
            m->lights[i].node.name,
            (unsigned long)m->lights[i].param_count,
            (unsigned long)m->lights[i].vector_count, sr, sg, sb);
    }
}

int mpk_write_obj(const char *path, const mpk_model *m)
{
    return mpk_write_obj_core(path, m, MPK_FALSE);
}

int mpk_write_posed_obj(const char *path, const mpk_model *m)
{
    return mpk_write_obj_core(path, m, MPK_TRUE);
}
