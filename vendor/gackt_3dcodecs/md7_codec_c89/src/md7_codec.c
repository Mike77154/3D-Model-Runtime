#include "md7_codec.h"

#define MD7_HEADER_SIZE 48UL
#define MD7_GROUP_SIZE 44UL
#define MD7_DEFORMER_SIZE 16UL
#define MD7_DEFORMER_ELEMENT_SIZE 28UL
#define MD7_DEFORMER_WEIGHT_SIZE 8UL

#define MD7_TRI_ONE_UV_NO_MAT 12UL
#define MD7_TRI_ONE_UV_MAT    16UL
#define MD7_TRI_TWO_UV        26UL
#define MD7_VERTEX_SMALL      16UL
#define MD7_VERTEX_BIG        26UL
#define MD7_BONETRANS_MIN     66UL
#define MD7_BONETRANS_STD     68UL

static md7_u16 md7_rd_u16(const md7_u8 *p)
{
    return (md7_u16)((md7_u16)p[0] | (md7_u16)((md7_u16)p[1] << 8));
}

static md7_u32 md7_rd_u32(const md7_u8 *p)
{
    md7_u32 a;
    md7_u32 b;
    md7_u32 c;
    md7_u32 d;
    a = (md7_u32)p[0];
    b = (md7_u32)p[1] << 8;
    c = (md7_u32)p[2] << 16;
    d = (md7_u32)p[3] << 24;
    return a | b | c | d;
}

static md7_i32 md7_rd_i32(const md7_u8 *p)
{
    return (md7_i32)md7_rd_u32(p);
}

static int md7_need(md7_u32 off, md7_u32 n, md7_u32 size)
{
    if (off > size) {
        return 0;
    }
    if (n > size - off) {
        return 0;
    }
    return 1;
}

static int md7_add_overflow(md7_u32 a, md7_u32 b, md7_u32 *out)
{
    if (a > MD7_U32_MAX - b) {
        return 1;
    }
    *out = a + b;
    return 0;
}

static int md7_mul_overflow(md7_u32 a, md7_u32 b, md7_u32 *out)
{
    if (a != 0UL && b > MD7_U32_MAX / a) {
        return 1;
    }
    *out = a * b;
    return 0;
}

static void md7_copy_name(char *dst, md7_u32 dst_cap, const md7_u8 *src, md7_u32 src_len)
{
    md7_u32 i;
    md7_u32 lim;
    lim = dst_cap;
    if (lim == 0UL) {
        return;
    }
    lim--;
    if (src_len < lim) {
        lim = src_len;
    }
    for (i = 0UL; i < lim; ++i) {
        if (src[i] == 0U) {
            break;
        }
        dst[i] = (char)src[i];
    }
    dst[i] = '\0';
}

static void md7_bounds_reset(struct md7_bounds *b)
{
    b->min_x = 0;
    b->min_y = 0;
    b->min_z = 0;
    b->max_x = 0;
    b->max_y = 0;
    b->max_z = 0;
    b->valid = 0U;
}

static void md7_bounds_add(struct md7_bounds *b, md7_fx16 x, md7_fx16 y, md7_fx16 z)
{
    if (!b->valid) {
        b->min_x = x;
        b->min_y = y;
        b->min_z = z;
        b->max_x = x;
        b->max_y = y;
        b->max_z = z;
        b->valid = 1U;
        return;
    }
    if (x < b->min_x) { b->min_x = x; }
    if (y < b->min_y) { b->min_y = y; }
    if (z < b->min_z) { b->min_z = z; }
    if (x > b->max_x) { b->max_x = x; }
    if (y > b->max_y) { b->max_y = y; }
    if (z > b->max_z) { b->max_z = z; }
}

static void md7_zero_header(struct md7_header *h)
{
    h->ident[0] = '\0'; h->ident[1] = '\0'; h->ident[2] = '\0'; h->ident[3] = '\0'; h->ident[4] = '\0';
    h->version = 0; h->bones_num = 0UL; h->groups_num = 0UL; h->data_size = 0UL;
    h->entlump_size = 0; h->medlump_size = 0; h->bone_stc_size = 0U; h->skin_stc_size = 0U;
    h->colorvalue_stc_size = 0U; h->material_stc_size = 0U; h->skinpoint_stc_size = 0U;
    h->triangle_stc_size = 0U; h->mainvertex_stc_size = 0U; h->framevertex_stc_size = 0U;
    h->bonetrans_stc_size = 0U; h->frame_stc_size = 0U;
}

void md7_model_reset(struct md7_model *m)
{
    if (m == 0) {
        return;
    }
    md7_zero_header(&m->header);
    m->bone_count = 0UL; m->bone_needed = 0UL;
    m->group_count = 0UL; m->group_needed = 0UL;
    m->skin_count = 0UL; m->skin_needed = 0UL;
    m->material_count = 0UL; m->material_needed = 0UL;
    m->uv_count = 0UL; m->uv_needed = 0UL;
    m->triangle_count = 0UL; m->triangle_needed = 0UL;
    m->vertex_count = 0UL; m->vertex_needed = 0UL;
    m->frame_count = 0UL; m->frame_needed = 0UL;
    m->framevertex_count = 0UL; m->framevertex_needed = 0UL;
    m->transform_count = 0UL; m->transform_needed = 0UL;
    m->deformer_count = 0UL; m->deformer_needed = 0UL;
    m->deformer_element_count = 0UL; m->deformer_element_needed = 0UL;
    m->deformer_weight_count = 0UL; m->deformer_weight_needed = 0UL;
    m->entlump_offset = 0UL; m->entlump_size = 0UL;
    m->medlump_offset = 0UL; m->medlump_size = 0UL;
    md7_bounds_reset(&m->bounds);
    m->warnings = 0UL;
}

md7_fx16 md7_f32le_to_fx16(const md7_u8 *p, md7_u32 *warn_flags)
{
    md7_u32 bits;
    md7_u32 sign;
    md7_u32 exp;
    md7_u32 mant;
    int shift;
    unsigned long value;
    md7_i32 out;

    bits = md7_rd_u32(p);
    sign = bits >> 31;
    exp = (bits >> 23) & 255UL;
    mant = bits & 0x007fffffUL;

    if (exp == 0UL) {
        return 0;
    }
    if (exp == 255UL) {
        if (warn_flags != 0) {
            *warn_flags |= MD7_WARN_CLAMPED_FIXED;
        }
        return sign ? MD7_I32_MIN : MD7_I32_MAX;
    }

    mant |= 0x00800000UL;
    shift = (int)exp - 127 - 23 + MD7_FX_SHIFT;

    if (shift > 7) {
        if (warn_flags != 0) {
            *warn_flags |= MD7_WARN_CLAMPED_FIXED;
        }
        return sign ? MD7_I32_MIN : MD7_I32_MAX;
    }

    if (shift >= 0) {
        value = ((unsigned long)mant) << shift;
        if (value > 0x7fffffffUL) {
            if (warn_flags != 0) {
                *warn_flags |= MD7_WARN_CLAMPED_FIXED;
            }
            return sign ? MD7_I32_MIN : MD7_I32_MAX;
        }
    } else {
        int rshift;
        rshift = -shift;
        if (rshift >= 32) {
            value = 0UL;
        } else {
            value = ((unsigned long)mant) >> rshift;
        }
    }

    out = (md7_i32)value;
    if (sign) {
        out = -out;
    }
    return out;
}

md7_i32 md7_fx16_floor_to_i32(md7_fx16 v)
{
    return (md7_i32)(v / MD7_FX_ONE);
}

md7_i32 md7_fx16_round_to_i32(md7_fx16 v)
{
    if (v >= 0) {
        return (md7_i32)((v + MD7_FX_HALF) / MD7_FX_ONE);
    }
    return (md7_i32)((v - MD7_FX_HALF) / MD7_FX_ONE);
}

md7_i32 md7_fx16_to_milli(md7_fx16 v)
{
    md7_i32 sign;
    md7_i32 av;
    md7_i32 out;
    sign = 1;
    av = v;
    if (av < 0) {
        sign = -1;
        av = -av;
    }
    out = (md7_i32)((av * 1000L + MD7_FX_HALF) / MD7_FX_ONE);
    return out * sign;
}

static int md7_parse_header(const md7_u8 *data, md7_u32 size, struct md7_header *h)
{
    if (!md7_need(0UL, MD7_HEADER_SIZE, size)) {
        return MD7_E_TOO_SMALL;
    }
    h->ident[0] = (char)data[0];
    h->ident[1] = (char)data[1];
    h->ident[2] = (char)data[2];
    h->ident[3] = (char)data[3];
    h->ident[4] = '\0';
    if (!(data[0] == 'M' && data[1] == 'D' && data[2] == 'L' && data[3] == '7')) {
        return MD7_E_BAD_MAGIC;
    }
    h->version = md7_rd_i32(data + 4UL);
    h->bones_num = md7_rd_u32(data + 8UL);
    h->groups_num = md7_rd_u32(data + 12UL);
    h->data_size = md7_rd_u32(data + 16UL);
    h->entlump_size = md7_rd_i32(data + 20UL);
    h->medlump_size = md7_rd_i32(data + 24UL);
    h->bone_stc_size = md7_rd_u16(data + 28UL);
    h->skin_stc_size = md7_rd_u16(data + 30UL);
    h->colorvalue_stc_size = md7_rd_u16(data + 32UL);
    h->material_stc_size = md7_rd_u16(data + 34UL);
    h->skinpoint_stc_size = md7_rd_u16(data + 36UL);
    h->triangle_stc_size = md7_rd_u16(data + 38UL);
    h->mainvertex_stc_size = md7_rd_u16(data + 40UL);
    h->framevertex_stc_size = md7_rd_u16(data + 42UL);
    h->bonetrans_stc_size = md7_rd_u16(data + 44UL);
    h->frame_stc_size = md7_rd_u16(data + 46UL);
    if (h->groups_num == 0UL) {
        return MD7_E_BAD_COUNT;
    }
    if (h->data_size != 0UL && h->data_size > size) {
        return MD7_E_BOUNDS;
    }
    if (h->colorvalue_stc_size != 16U || h->material_stc_size < 68U) {
        return MD7_E_BAD_SIZE;
    }
    if (h->skinpoint_stc_size != 8U || h->skin_stc_size < 28U || h->frame_stc_size < 24U) {
        return MD7_E_BAD_SIZE;
    }
    if (h->mainvertex_stc_size < MD7_VERTEX_SMALL || h->framevertex_stc_size < MD7_VERTEX_SMALL) {
        return MD7_E_BAD_SIZE;
    }
    if (h->triangle_stc_size < 6U) {
        return MD7_E_BAD_SIZE;
    }
    return MD7_OK;
}

static md7_u8 md7_skin_bpp(md7_u8 typ)
{
    md7_u32 base_typ;
    base_typ = (md7_u32)(typ & 0x07U);
    if ((typ & MD7_SKIN_RGBFLAG) != 0U) {
        return 4U;
    }
    if (base_typ == 2UL || base_typ == 3UL) {
        return 2U;
    }
    if (base_typ == 4UL) {
        return 3U;
    }
    if (base_typ == 5UL) {
        return 4U;
    }
    return 1U;
}

static int md7_calc_pixel_payload(md7_i32 width, md7_i32 height, md7_u8 typ, md7_u32 *out)
{
    md7_u32 w;
    md7_u32 h;
    md7_u32 bpp;
    md7_u32 pixels;
    md7_u32 total;
    md7_u32 level;
    md7_u32 tmp;

    *out = 0UL;
    if (width <= 0 || height <= 0) {
        return 0;
    }
    w = (md7_u32)width;
    h = (md7_u32)height;
    bpp = (md7_u32)md7_skin_bpp(typ);
    if (md7_mul_overflow(w, h, &pixels)) {
        return MD7_E_OVERFLOW;
    }
    if (md7_mul_overflow(pixels, bpp, &total)) {
        return MD7_E_OVERFLOW;
    }
    if ((typ & MD7_SKIN_MIPFLAG) != 0U) {
        level = total;
        while (level > 1UL) {
            level = level / 4UL;
            if (md7_add_overflow(total, level, &tmp)) {
                return MD7_E_OVERFLOW;
            }
            total = tmp;
            if (level <= 1UL) {
                break;
            }
        }
    }
    *out = total;
    return 0;
}

static int md7_store_material(struct md7_model *m, const md7_u8 *data, md7_u32 off, md7_u32 *out_index)
{
    struct md7_material *mat;
    md7_u32 i;
    md7_u32 base;
    *out_index = m->material_needed;
    m->material_needed++;
    if (m->material_count >= m->material_cap || m->materials == 0) {
        m->warnings |= MD7_WARN_CAP_MATERIALS;
        return MD7_OK;
    }
    mat = &m->materials[m->material_count];
    mat->file_offset = off;
    base = off;
    for (i = 0UL; i < 4UL; ++i) { mat->diffuse[i] = md7_f32le_to_fx16(data + base + i * 4UL, &m->warnings); }
    base += 16UL;
    for (i = 0UL; i < 4UL; ++i) { mat->ambient[i] = md7_f32le_to_fx16(data + base + i * 4UL, &m->warnings); }
    base += 16UL;
    for (i = 0UL; i < 4UL; ++i) { mat->specular[i] = md7_f32le_to_fx16(data + base + i * 4UL, &m->warnings); }
    base += 16UL;
    for (i = 0UL; i < 4UL; ++i) { mat->emissive[i] = md7_f32le_to_fx16(data + base + i * 4UL, &m->warnings); }
    base += 16UL;
    mat->power = md7_f32le_to_fx16(data + base, &m->warnings);
    m->material_count++;
    return MD7_OK;
}

static int md7_store_bone(struct md7_model *m, const md7_u8 *data, md7_u32 off, md7_u16 stc)
{
    struct md7_bone *b;
    m->bone_needed++;
    if (m->bone_count >= m->bone_cap || m->bones == 0) {
        m->warnings |= MD7_WARN_CAP_BONES;
        return MD7_OK;
    }
    b = &m->bones[m->bone_count];
    b->file_offset = off;
    b->parent_index = md7_rd_u16(data + off);
    b->x = md7_f32le_to_fx16(data + off + 4UL, &m->warnings);
    b->y = md7_f32le_to_fx16(data + off + 8UL, &m->warnings);
    b->z = md7_f32le_to_fx16(data + off + 12UL, &m->warnings);
    if (stc > 16U) {
        md7_copy_name(b->name, MD7_NAME_BONE + 1UL, data + off + 16UL, (md7_u32)stc - 16UL);
    } else {
        b->name[0] = '\0';
    }
    m->bone_count++;
    return MD7_OK;
}

static int md7_store_skin(struct md7_model *m, const md7_u8 *data, md7_u32 off, md7_u32 payload_off, md7_u32 payload_size, md7_u32 raw_off, md7_u32 raw_size, md7_u32 asc_off, md7_u32 asc_size, md7_u32 mat_idx)
{
    struct md7_skin *s;
    m->skin_needed++;
    if (m->skin_count >= m->skin_cap || m->skins == 0) {
        m->warnings |= MD7_WARN_CAP_SKINS;
        return MD7_OK;
    }
    s = &m->skins[m->skin_count];
    s->file_offset = off;
    s->payload_offset = payload_off;
    s->payload_size = payload_size;
    s->raw_pixels_offset = raw_off;
    s->raw_pixels_size = raw_size;
    s->ascdef_offset = asc_off;
    s->ascdef_size = asc_size;
    s->material_index = mat_idx;
    s->typ = data[off];
    s->bytes_per_pixel = md7_skin_bpp(s->typ);
    s->has_mips = (s->typ & MD7_SKIN_MIPFLAG) ? 1U : 0U;
    s->width = md7_rd_i32(data + off + 4UL);
    s->height = md7_rd_i32(data + off + 8UL);
    md7_copy_name(s->texture_name, MD7_NAME_TEX + 1UL, data + off + 12UL, MD7_NAME_TEX);
    m->skin_count++;
    return MD7_OK;
}

static int md7_store_uv(struct md7_model *m, const md7_u8 *data, md7_u32 off)
{
    struct md7_uv *u;
    m->uv_needed++;
    if (m->uv_count >= m->uv_cap || m->uvs == 0) {
        m->warnings |= MD7_WARN_CAP_UVS;
        return MD7_OK;
    }
    u = &m->uvs[m->uv_count];
    u->file_offset = off;
    u->u = md7_f32le_to_fx16(data + off, &m->warnings);
    u->v = md7_f32le_to_fx16(data + off + 4UL, &m->warnings);
    m->uv_count++;
    return MD7_OK;
}

static int md7_store_triangle(struct md7_model *m, const md7_u8 *data, md7_u32 off, md7_u16 stc)
{
    struct md7_triangle *t;
    md7_u32 i;
    m->triangle_needed++;
    if (m->triangle_count >= m->triangle_cap || m->triangles == 0) {
        m->warnings |= MD7_WARN_CAP_TRIANGLES;
        return MD7_OK;
    }
    t = &m->triangles[m->triangle_count];
    t->file_offset = off;
    for (i = 0UL; i < 3UL; ++i) {
        t->v_index[i] = md7_rd_u16(data + off + i * 2UL);
        t->uv1_index[i] = 0U;
        t->uv2_index[i] = 0U;
    }
    t->material1 = -1L;
    t->material2 = -1L;
    t->has_uv1 = 0U;
    t->has_mat1 = 0U;
    t->has_uv2 = 0U;
    t->has_mat2 = 0U;
    t->index_warning = 0U;
    if (stc >= MD7_TRI_ONE_UV_NO_MAT) {
        for (i = 0UL; i < 3UL; ++i) {
            t->uv1_index[i] = md7_rd_u16(data + off + 6UL + i * 2UL);
        }
        t->has_uv1 = 1U;
    }
    if (stc >= MD7_TRI_ONE_UV_MAT) {
        t->material1 = md7_rd_i32(data + off + 12UL);
        t->has_mat1 = 1U;
    }
    if (stc >= MD7_TRI_TWO_UV) {
        for (i = 0UL; i < 3UL; ++i) {
            t->uv2_index[i] = md7_rd_u16(data + off + 16UL + i * 2UL);
        }
        t->material2 = md7_rd_i32(data + off + 22UL);
        t->has_uv2 = 1U;
        t->has_mat2 = 1U;
    }
    m->triangle_count++;
    return MD7_OK;
}

static int md7_store_vertex_any(struct md7_model *m, const md7_u8 *data, md7_u32 off, md7_u16 stc, int frame_vertex, struct md7_bounds *bounds)
{
    struct md7_vertex *v;
    md7_fx16 x;
    md7_fx16 y;
    md7_fx16 z;

    x = md7_f32le_to_fx16(data + off, &m->warnings);
    y = md7_f32le_to_fx16(data + off + 4UL, &m->warnings);
    z = md7_f32le_to_fx16(data + off + 8UL, &m->warnings);
    if (bounds != 0) {
        md7_bounds_add(bounds, x, y, z);
    }
    md7_bounds_add(&m->bounds, x, y, z);

    if (frame_vertex) {
        m->framevertex_needed++;
        if (m->framevertex_count >= m->framevertex_cap || m->framevertices == 0) {
            m->warnings |= MD7_WARN_CAP_FRAMEVERTS;
            return MD7_OK;
        }
        v = &m->framevertices[m->framevertex_count];
    } else {
        m->vertex_needed++;
        if (m->vertex_count >= m->vertex_cap || m->vertices == 0) {
            m->warnings |= MD7_WARN_CAP_VERTICES;
            return MD7_OK;
        }
        v = &m->vertices[m->vertex_count];
    }

    v->file_offset = off;
    v->x = x;
    v->y = y;
    v->z = z;
    v->index = md7_rd_u16(data + off + 12UL);
    v->normal_mode = 0U;
    v->normal_index = 0U;
    v->nx = 0; v->ny = 0; v->nz = 0;
    if (stc >= MD7_VERTEX_BIG) {
        v->normal_mode = 2U;
        v->nx = md7_f32le_to_fx16(data + off + 14UL, &m->warnings);
        v->ny = md7_f32le_to_fx16(data + off + 18UL, &m->warnings);
        v->nz = md7_f32le_to_fx16(data + off + 22UL, &m->warnings);
    } else if (stc >= MD7_VERTEX_SMALL) {
        v->normal_mode = 1U;
        v->normal_index = data[off + 14UL];
    }

    if (frame_vertex) {
        m->framevertex_count++;
    } else {
        m->vertex_count++;
    }
    return MD7_OK;
}

static int md7_store_transform(struct md7_model *m, const md7_u8 *data, md7_u32 off, md7_u16 stc)
{
    struct md7_bonetransform *bt;
    md7_u32 i;
    m->transform_needed++;
    if (m->transform_count >= m->transform_cap || m->transforms == 0) {
        m->warnings |= MD7_WARN_CAP_TRANSFORMS;
        return MD7_OK;
    }
    bt = &m->transforms[m->transform_count];
    bt->file_offset = off;
    for (i = 0UL; i < 16UL; ++i) {
        if (4UL * i + 4UL <= (md7_u32)stc) {
            bt->m[i] = md7_f32le_to_fx16(data + off + i * 4UL, &m->warnings);
        } else {
            bt->m[i] = 0;
        }
    }
    if (stc >= MD7_BONETRANS_MIN) {
        bt->bone_index = md7_rd_u16(data + off + 64UL);
    } else {
        bt->bone_index = 0U;
    }
    m->transform_count++;
    return MD7_OK;
}

static int md7_parse_skin_payload(struct md7_model *m, const md7_u8 *data, md7_u32 group_end, md7_u32 skin_off, md7_u32 *next_off)
{
    md7_u32 cur;
    md7_u32 payload_off;
    md7_u32 raw_off;
    md7_u32 raw_size;
    md7_u32 asc_off;
    md7_u32 asc_size;
    md7_u32 mat_idx;
    md7_u32 pixel_size;
    md7_i32 skin_w;
    md7_i32 skin_h;
    md7_i32 asc_signed;
    md7_u8 skin_typ;
    char skin_name[MD7_NAME_TEX + 1];
    int pr;

    payload_off = skin_off + (md7_u32)m->header.skin_stc_size;
    cur = payload_off;
    raw_off = 0UL;
    raw_size = 0UL;
    asc_off = 0UL;
    asc_size = 0UL;
    mat_idx = MD7_U32_MAX;
    skin_typ = data[skin_off];
    skin_w = md7_rd_i32(data + skin_off + 4UL);
    skin_h = md7_rd_i32(data + skin_off + 8UL);
    md7_copy_name(skin_name, MD7_NAME_TEX + 1UL, data + skin_off + 12UL, MD7_NAME_TEX);

    if ((skin_typ & MD7_SKIN_MATERIAL) != 0U) {
        if (!md7_need(cur, (md7_u32)m->header.material_stc_size, group_end)) {
            return MD7_E_BOUNDS;
        }
        md7_store_material(m, data, cur, &mat_idx);
        cur += (md7_u32)m->header.material_stc_size;
    }

    if ((skin_typ & MD7_SKIN_MATERIAL_ASCDEF) != 0U) {
        m->warnings |= MD7_WARN_ASCDEF;
        if (!md7_need(cur, 4UL, group_end)) {
            return MD7_E_BOUNDS;
        }
        asc_signed = md7_rd_i32(data + cur);
        cur += 4UL;
        if (asc_signed < 0) {
            return MD7_E_BAD_SIZE;
        }
        asc_size = (md7_u32)asc_signed;
        asc_off = cur;
        if (!md7_need(cur, asc_size, group_end)) {
            return MD7_E_BOUNDS;
        }
        cur += asc_size;
    }

    if (skin_name[0] == '\0') {
        pr = md7_calc_pixel_payload(skin_w, skin_h, skin_typ, &pixel_size);
        if (pr != 0) {
            return pr;
        }
        raw_off = cur;
        raw_size = pixel_size;
        if (!md7_need(cur, pixel_size, group_end)) {
            m->warnings |= MD7_WARN_SKIN_GUESS;
            raw_off = 0UL;
            raw_size = 0UL;
            pixel_size = 0UL;
        }
        cur += pixel_size;
    }

    md7_store_skin(m, data, skin_off, payload_off, cur - payload_off, raw_off, raw_size, asc_off, asc_size, mat_idx);
    *next_off = cur;
    return MD7_OK;
}

static void md7_validate_group_indices(struct md7_model *m, struct md7_group *g)
{
    md7_u32 ti;
    md7_u32 k;
    struct md7_triangle *t;
    if (m->triangles == 0 || m->triangle_cap == 0UL) {
        return;
    }
    ti = g->first_triangle;
    while (ti < g->first_triangle + g->numtris && ti < m->triangle_count) {
        t = &m->triangles[ti];
        for (k = 0UL; k < 3UL; ++k) {
            if ((md7_u32)t->v_index[k] >= g->numverts) {
                g->invalid_vertex_indices++;
                t->index_warning = 1U;
            }
            if (t->has_uv1 && (md7_u32)t->uv1_index[k] >= g->num_stpts) {
                g->invalid_uv_indices++;
                t->index_warning = 1U;
            }
            if (t->has_uv2 && (md7_u32)t->uv2_index[k] >= g->num_stpts) {
                g->invalid_uv_indices++;
                t->index_warning = 1U;
            }
        }
        if (t->has_mat1 && (t->material1 < 0 || (md7_u32)t->material1 >= g->numskins)) {
            g->invalid_material_indices++;
            t->index_warning = 1U;
        }
        if (t->has_mat2 && (t->material2 < 0 || (md7_u32)t->material2 >= g->numskins)) {
            g->invalid_material_indices++;
            t->index_warning = 1U;
        }
        ti++;
    }
    if (g->invalid_vertex_indices != 0UL || g->invalid_uv_indices != 0UL || g->invalid_material_indices != 0UL) {
        m->warnings |= MD7_WARN_INDEX_RANGE;
    }
}

static int md7_store_deformer(struct md7_model *m, const md7_u8 *data, md7_u32 off, md7_u32 first_element, md7_u32 elements, md7_u32 data_size, md7_u32 *out_index)
{
    struct md7_deformer *d;
    *out_index = m->deformer_needed;
    m->deformer_needed++;
    if (m->deformer_count >= m->deformer_cap || m->deformers == 0) {
        m->warnings |= MD7_WARN_CAP_DEFORMERS;
        return MD7_OK;
    }
    d = &m->deformers[m->deformer_count];
    d->file_offset = off;
    d->version = (md7_i8)data[off];
    d->typ = (md7_i8)data[off + 1UL];
    d->group_index = md7_rd_i32(data + off + 4UL);
    d->first_element = first_element;
    d->elements = elements;
    d->data_size = data_size;
    m->deformer_count++;
    return MD7_OK;
}

static int md7_store_deformer_element(struct md7_model *m, const md7_u8 *data, md7_u32 off, md7_u32 deformer_index, md7_u32 first_weight, md7_u32 weights, md7_u32 *out_index)
{
    struct md7_deformer_element *e;
    *out_index = m->deformer_element_needed;
    m->deformer_element_needed++;
    if (m->deformer_element_count >= m->deformer_element_cap || m->deformer_elements == 0) {
        m->warnings |= MD7_WARN_CAP_ELEMENTS;
        return MD7_OK;
    }
    e = &m->deformer_elements[m->deformer_element_count];
    e->file_offset = off;
    e->deformer_index = deformer_index;
    e->element_index = md7_rd_i32(data + off);
    md7_copy_name(e->element_name, MD7_NAME_DEFORMER_ELEMENT + 1UL, data + off + 4UL, MD7_NAME_DEFORMER_ELEMENT);
    e->first_weight = first_weight;
    e->weights = weights;
    m->deformer_element_count++;
    return MD7_OK;
}

static int md7_store_deformer_weight(struct md7_model *m, const md7_u8 *data, md7_u32 off, md7_u32 element_index)
{
    struct md7_deformer_weight *w;
    m->deformer_weight_needed++;
    if (m->deformer_weight_count >= m->deformer_weight_cap || m->deformer_weights == 0) {
        m->warnings |= MD7_WARN_CAP_WEIGHTS;
        return MD7_OK;
    }
    w = &m->deformer_weights[m->deformer_weight_count];
    w->file_offset = off;
    w->element_index = element_index;
    w->vertex_index = md7_rd_i32(data + off);
    w->weight = md7_f32le_to_fx16(data + off + 4UL, &m->warnings);
    m->deformer_weight_count++;
    return MD7_OK;
}

static int md7_parse_deformers(struct md7_model *m, const md7_u8 *data, md7_u32 *p_off, md7_u32 group_end, struct md7_group *g)
{
    md7_u32 count;
    md7_u32 di;
    md7_u32 off;
    md7_u32 deformer_start;
    md7_u32 deformer_size;
    md7_u32 deformer_end;
    md7_u32 elements;
    md7_u32 ei;
    md7_u32 wi;
    md7_u32 weights;
    md7_u32 def_index;
    md7_u32 elem_index;
    md7_u32 elem_first_weight;

    off = *p_off;
    if (g->deformers <= 0) {
        return MD7_OK;
    }
    count = (md7_u32)((md7_u8)g->deformers);
    for (di = 0UL; di < count; ++di) {
        if (!md7_need(off, MD7_DEFORMER_SIZE, group_end)) {
            m->warnings |= MD7_WARN_DEFORMER_PARSE;
            return MD7_OK;
        }
        deformer_start = off;
        elements = md7_rd_u32(data + off + 8UL);
        deformer_size = md7_rd_u32(data + off + 12UL);
        if (deformer_size < MD7_DEFORMER_SIZE || !md7_need(deformer_start, deformer_size, group_end)) {
            deformer_end = group_end;
            m->warnings |= MD7_WARN_DEFORMER_PARSE;
        } else {
            deformer_end = deformer_start + deformer_size;
        }
        off += MD7_DEFORMER_SIZE;
        md7_store_deformer(m, data, deformer_start, m->deformer_element_needed, elements, deformer_size, &def_index);
        g->num_deformers_parsed++;
        for (ei = 0UL; ei < elements; ++ei) {
            if (!md7_need(off, MD7_DEFORMER_ELEMENT_SIZE, deformer_end)) {
                m->warnings |= MD7_WARN_DEFORMER_PARSE;
                off = deformer_end;
                break;
            }
            weights = md7_rd_u32(data + off + 24UL);
            elem_first_weight = m->deformer_weight_needed;
            md7_store_deformer_element(m, data, off, def_index, elem_first_weight, weights, &elem_index);
            off += MD7_DEFORMER_ELEMENT_SIZE;
            for (wi = 0UL; wi < weights; ++wi) {
                if (!md7_need(off, MD7_DEFORMER_WEIGHT_SIZE, deformer_end)) {
                    m->warnings |= MD7_WARN_DEFORMER_PARSE;
                    off = deformer_end;
                    break;
                }
                md7_store_deformer_weight(m, data, off, elem_index);
                off += MD7_DEFORMER_WEIGHT_SIZE;
            }
        }
        off = deformer_end;
    }
    *p_off = off;
    return MD7_OK;
}

static int md7_parse_group(struct md7_model *m, const md7_u8 *data, md7_u32 size, md7_u32 *p_off)
{
    struct md7_group temp;
    struct md7_group *g;
    md7_u32 off;
    md7_u32 group_start;
    md7_u32 group_end;
    md7_u32 i;
    md7_u32 framevertex_start;
    md7_u32 transform_start;
    md7_u32 tmp;
    int r;

    off = *p_off;
    group_start = off;
    if (!md7_need(off, MD7_GROUP_SIZE, size)) {
        return MD7_E_BOUNDS;
    }

    temp.file_offset = off;
    temp.typ = data[off];
    temp.deformers = (md7_i8)data[off + 1UL];
    temp.max_weights = (md7_i8)data[off + 2UL];
    temp.data_size = md7_rd_u32(data + off + 4UL);
    temp.data_offset = off;
    md7_copy_name(temp.name, MD7_NAME_GROUP + 1UL, data + off + 8UL, MD7_NAME_GROUP);
    temp.numskins = md7_rd_u32(data + off + 24UL);
    temp.num_stpts = md7_rd_u32(data + off + 28UL);
    temp.numtris = md7_rd_u32(data + off + 32UL);
    temp.numverts = md7_rd_u32(data + off + 36UL);
    temp.numframes = md7_rd_u32(data + off + 40UL);
    temp.first_skin = m->skin_needed;
    temp.first_uv = m->uv_needed;
    temp.first_triangle = m->triangle_needed;
    temp.first_vertex = m->vertex_needed;
    temp.first_frame = m->frame_needed;
    temp.first_deformer = m->deformer_needed;
    temp.num_deformers_parsed = 0UL;
    temp.invalid_vertex_indices = 0UL;
    temp.invalid_uv_indices = 0UL;
    temp.invalid_material_indices = 0UL;
    temp.extra_offset = 0UL;
    temp.extra_size = 0UL;
    md7_bounds_reset(&temp.bounds);

    if (temp.typ != 1U) {
        m->warnings |= MD7_WARN_NON_TRI_GROUP;
    }
    if (temp.numtris == 0UL || temp.numverts == 0UL) {
        m->warnings |= MD7_WARN_ZERO_COUNTS;
    }
    if (temp.data_size < MD7_GROUP_SIZE) {
        return MD7_E_BAD_SIZE;
    }
    if (md7_add_overflow(group_start, temp.data_size, &group_end)) {
        return MD7_E_OVERFLOW;
    }
    if (group_end > size) {
        return MD7_E_BOUNDS;
    }
    off += MD7_GROUP_SIZE;

    for (i = 0UL; i < temp.numskins; ++i) {
        if (!md7_need(off, (md7_u32)m->header.skin_stc_size, group_end)) {
            return MD7_E_BOUNDS;
        }
        r = md7_parse_skin_payload(m, data, group_end, off, &off);
        if (r != MD7_OK) {
            return r;
        }
    }

    temp.first_uv = m->uv_needed;
    if (md7_mul_overflow(temp.num_stpts, (md7_u32)m->header.skinpoint_stc_size, &tmp)) {
        return MD7_E_OVERFLOW;
    }
    if (!md7_need(off, tmp, group_end)) {
        return MD7_E_BOUNDS;
    }
    for (i = 0UL; i < temp.num_stpts; ++i) {
        md7_store_uv(m, data, off);
        off += (md7_u32)m->header.skinpoint_stc_size;
    }

    temp.first_triangle = m->triangle_needed;
    if (md7_mul_overflow(temp.numtris, (md7_u32)m->header.triangle_stc_size, &tmp)) {
        return MD7_E_OVERFLOW;
    }
    if (!md7_need(off, tmp, group_end)) {
        return MD7_E_BOUNDS;
    }
    for (i = 0UL; i < temp.numtris; ++i) {
        md7_store_triangle(m, data, off, m->header.triangle_stc_size);
        off += (md7_u32)m->header.triangle_stc_size;
    }

    temp.first_vertex = m->vertex_needed;
    if (md7_mul_overflow(temp.numverts, (md7_u32)m->header.mainvertex_stc_size, &tmp)) {
        return MD7_E_OVERFLOW;
    }
    if (!md7_need(off, tmp, group_end)) {
        return MD7_E_BOUNDS;
    }
    for (i = 0UL; i < temp.numverts; ++i) {
        md7_store_vertex_any(m, data, off, m->header.mainvertex_stc_size, 0, &temp.bounds);
        off += (md7_u32)m->header.mainvertex_stc_size;
    }

    temp.first_frame = m->frame_needed;
    for (i = 0UL; i < temp.numframes; ++i) {
        struct md7_frame *f;
        struct md7_bounds fbounds;
        md7_u32 frame_off;
        md7_u32 vertex_count;
        md7_u32 trans_count;
        md7_u32 fv_i;
        md7_u32 tr_i;
        md7_u32 frame_bytes;
        md7_u32 frame_vert_bytes;
        md7_u32 frame_trans_bytes;

        frame_off = off;
        md7_bounds_reset(&fbounds);
        if (!md7_need(off, (md7_u32)m->header.frame_stc_size, group_end)) {
            return MD7_E_BOUNDS;
        }
        vertex_count = md7_rd_u32(data + off + 16UL);
        trans_count = md7_rd_u32(data + off + 20UL);
        if (md7_mul_overflow(vertex_count, (md7_u32)m->header.framevertex_stc_size, &frame_vert_bytes)) {
            return MD7_E_OVERFLOW;
        }
        if (md7_mul_overflow(trans_count, (md7_u32)m->header.bonetrans_stc_size, &frame_trans_bytes)) {
            return MD7_E_OVERFLOW;
        }
        frame_bytes = (md7_u32)m->header.frame_stc_size;
        if (md7_add_overflow(frame_bytes, frame_vert_bytes, &frame_bytes)) {
            return MD7_E_OVERFLOW;
        }
        if (md7_add_overflow(frame_bytes, frame_trans_bytes, &frame_bytes)) {
            return MD7_E_OVERFLOW;
        }
        if (!md7_need(off, frame_bytes, group_end)) {
            return MD7_E_BOUNDS;
        }

        framevertex_start = m->framevertex_needed;
        transform_start = m->transform_needed;
        m->frame_needed++;
        if (m->frame_count >= m->frame_cap || m->frames == 0) {
            m->warnings |= MD7_WARN_CAP_FRAMES;
        } else {
            f = &m->frames[m->frame_count];
            f->file_offset = frame_off;
            f->vertices_offset = off + (md7_u32)m->header.frame_stc_size;
            f->transforms_offset = f->vertices_offset + frame_vert_bytes;
            md7_copy_name(f->name, MD7_NAME_FRAME + 1UL, data + off, MD7_NAME_FRAME);
            f->first_framevertex = framevertex_start;
            f->vertices_count = vertex_count;
            f->first_transform = transform_start;
            f->transmatrix_count = trans_count;
            md7_bounds_reset(&f->bounds);
            m->frame_count++;
        }

        off += (md7_u32)m->header.frame_stc_size;
        for (fv_i = 0UL; fv_i < vertex_count; ++fv_i) {
            md7_store_vertex_any(m, data, off, m->header.framevertex_stc_size, 1, &fbounds);
            off += (md7_u32)m->header.framevertex_stc_size;
        }
        if (m->frames != 0 && m->frame_count > 0UL && m->frame_count <= m->frame_cap) {
            m->frames[m->frame_count - 1UL].bounds = fbounds;
        }
        for (tr_i = 0UL; tr_i < trans_count; ++tr_i) {
            md7_store_transform(m, data, off, m->header.bonetrans_stc_size);
            off += (md7_u32)m->header.bonetrans_stc_size;
        }
    }

    r = md7_parse_deformers(m, data, &off, group_end, &temp);
    if (r != MD7_OK) {
        return r;
    }

    if (off < group_end) {
        temp.extra_offset = off;
        temp.extra_size = group_end - off;
        m->warnings |= MD7_WARN_TRAILING_GROUP;
    }

    md7_validate_group_indices(m, &temp);

    m->group_needed++;
    if (m->group_count >= m->group_cap || m->groups == 0) {
        m->warnings |= MD7_WARN_CAP_GROUPS;
    } else {
        g = &m->groups[m->group_count];
        *g = temp;
        m->group_count++;
    }

    *p_off = group_end;
    return MD7_OK;
}

int md7_load_memory(const md7_u8 *data, md7_u32 size, struct md7_model *out_model)
{
    int r;
    md7_u32 off;
    md7_u32 i;
    md7_u32 data_limit;
    md7_u32 bone_bytes;
    md7_u32 next;

    if (data == 0 || out_model == 0) {
        return MD7_E_NULL;
    }
    md7_model_reset(out_model);
    r = md7_parse_header(data, size, &out_model->header);
    if (r != MD7_OK) {
        return r;
    }

    data_limit = out_model->header.data_size;
    if (data_limit == 0UL) {
        data_limit = size;
    }

    off = MD7_HEADER_SIZE;
    if (out_model->header.bone_stc_size != 16U && out_model->header.bone_stc_size != 36U && out_model->header.bone_stc_size != 48U) {
        return MD7_E_BAD_SIZE;
    }
    if (md7_mul_overflow(out_model->header.bones_num, (md7_u32)out_model->header.bone_stc_size, &bone_bytes)) {
        return MD7_E_OVERFLOW;
    }
    if (!md7_need(off, bone_bytes, data_limit)) {
        return MD7_E_BOUNDS;
    }
    for (i = 0UL; i < out_model->header.bones_num; ++i) {
        md7_store_bone(out_model, data, off, out_model->header.bone_stc_size);
        off += (md7_u32)out_model->header.bone_stc_size;
    }

    for (i = 0UL; i < out_model->header.groups_num; ++i) {
        r = md7_parse_group(out_model, data, data_limit, &off);
        if (r != MD7_OK) {
            return r;
        }
    }

    next = data_limit;
    if (out_model->header.entlump_size > 0) {
        out_model->entlump_offset = next;
        out_model->entlump_size = (md7_u32)out_model->header.entlump_size;
        if (!md7_need(out_model->entlump_offset, out_model->entlump_size, size)) {
            return MD7_E_BOUNDS;
        }
        next += out_model->entlump_size;
    }
    if (out_model->header.medlump_size > 0) {
        out_model->medlump_offset = next;
        out_model->medlump_size = (md7_u32)out_model->header.medlump_size;
        if (!md7_need(out_model->medlump_offset, out_model->medlump_size, size)) {
            return MD7_E_BOUNDS;
        }
    }

    if ((out_model->warnings & md7_warning_capacity_mask()) != 0UL) {
        return MD7_E_CAPACITY;
    }
    return MD7_OK;
}

md7_u32 md7_warning_capacity_mask(void)
{
    return MD7_WARN_CAP_BONES | MD7_WARN_CAP_GROUPS | MD7_WARN_CAP_SKINS |
        MD7_WARN_CAP_UVS | MD7_WARN_CAP_TRIANGLES | MD7_WARN_CAP_VERTICES |
        MD7_WARN_CAP_FRAMES | MD7_WARN_CAP_FRAMEVERTS | MD7_WARN_CAP_TRANSFORMS |
        MD7_WARN_CAP_MATERIALS | MD7_WARN_CAP_DEFORMERS | MD7_WARN_CAP_ELEMENTS |
        MD7_WARN_CAP_WEIGHTS;
}

const char *md7_warning_name(md7_u32 bit)
{
    switch (bit) {
    case MD7_WARN_CAP_BONES: return "MD7_WARN_CAP_BONES";
    case MD7_WARN_CAP_GROUPS: return "MD7_WARN_CAP_GROUPS";
    case MD7_WARN_CAP_SKINS: return "MD7_WARN_CAP_SKINS";
    case MD7_WARN_CAP_UVS: return "MD7_WARN_CAP_UVS";
    case MD7_WARN_CAP_TRIANGLES: return "MD7_WARN_CAP_TRIANGLES";
    case MD7_WARN_CAP_VERTICES: return "MD7_WARN_CAP_VERTICES";
    case MD7_WARN_CAP_FRAMES: return "MD7_WARN_CAP_FRAMES";
    case MD7_WARN_CAP_FRAMEVERTS: return "MD7_WARN_CAP_FRAMEVERTS";
    case MD7_WARN_CAP_TRANSFORMS: return "MD7_WARN_CAP_TRANSFORMS";
    case MD7_WARN_CAP_MATERIALS: return "MD7_WARN_CAP_MATERIALS";
    case MD7_WARN_CAP_DEFORMERS: return "MD7_WARN_CAP_DEFORMERS";
    case MD7_WARN_CAP_ELEMENTS: return "MD7_WARN_CAP_ELEMENTS";
    case MD7_WARN_CAP_WEIGHTS: return "MD7_WARN_CAP_WEIGHTS";
    case MD7_WARN_SKIN_GUESS: return "MD7_WARN_SKIN_GUESS";
    case MD7_WARN_TRAILING_GROUP: return "MD7_WARN_TRAILING_GROUP";
    case MD7_WARN_INDEX_RANGE: return "MD7_WARN_INDEX_RANGE";
    case MD7_WARN_CLAMPED_FIXED: return "MD7_WARN_CLAMPED_FIXED";
    case MD7_WARN_NON_TRI_GROUP: return "MD7_WARN_NON_TRI_GROUP";
    case MD7_WARN_ASCDEF: return "MD7_WARN_ASCDEF";
    case MD7_WARN_DEFORMER_PARSE: return "MD7_WARN_DEFORMER_PARSE";
    case MD7_WARN_ZERO_COUNTS: return "MD7_WARN_ZERO_COUNTS";
    default: return "MD7_WARN_UNKNOWN";
    }
}

const char *md7_result_name(int code)
{
    switch (code) {
    case MD7_OK: return "MD7_OK";
    case MD7_E_NULL: return "MD7_E_NULL";
    case MD7_E_TOO_SMALL: return "MD7_E_TOO_SMALL";
    case MD7_E_BAD_MAGIC: return "MD7_E_BAD_MAGIC";
    case MD7_E_UNSUPPORTED: return "MD7_E_UNSUPPORTED";
    case MD7_E_BOUNDS: return "MD7_E_BOUNDS";
    case MD7_E_CAPACITY: return "MD7_E_CAPACITY";
    case MD7_E_BAD_SIZE: return "MD7_E_BAD_SIZE";
    case MD7_E_BAD_COUNT: return "MD7_E_BAD_COUNT";
    case MD7_E_OVERFLOW: return "MD7_E_OVERFLOW";
    default: return "MD7_UNKNOWN";
    }
}
