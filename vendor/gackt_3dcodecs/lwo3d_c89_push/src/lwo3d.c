#include "lwo3d.h"

#define ID_FORM LWO3D_FOURCC('F','O','R','M')
#define ID_LWO2 LWO3D_ID_LWO2
#define ID_LWO3 LWO3D_ID_LWO3
#define ID_LWOB LWO3D_ID_LWOB
#define ID_LAYR LWO3D_FOURCC('L','A','Y','R')
#define ID_PNTS LWO3D_FOURCC('P','N','T','S')
#define ID_BBOX LWO3D_FOURCC('B','B','O','X')
#define ID_POLS LWO3D_FOURCC('P','O','L','S')
#define ID_TAGS LWO3D_FOURCC('T','A','G','S')
#define ID_PTAG LWO3D_FOURCC('P','T','A','G')
#define ID_VMAP LWO3D_FOURCC('V','M','A','P')
#define ID_VMAD LWO3D_FOURCC('V','M','A','D')
#define ID_VMED LWO3D_FOURCC('V','M','E','D')
#define ID_SURF LWO3D_FOURCC('S','U','R','F')
#define ID_CLIP LWO3D_FOURCC('C','L','I','P')
#define ID_ENVL LWO3D_FOURCC('E','N','V','L')
#define ID_DESC LWO3D_FOURCC('D','E','S','C')
#define ID_ICON LWO3D_FOURCC('I','C','O','N')
#define ID_SRFS LWO3D_FOURCC('S','R','F','S')
#define ID_CRVS LWO3D_FOURCC('C','R','V','S')
#define ID_FACE LWO3D_ID_FACE
#define ID_SUBD LWO3D_ID_SUBD
#define ID_PSUB LWO3D_ID_PSUB
#define ID_PTCH LWO3D_ID_PTCH
#define ID_SPCH LWO3D_ID_SPCH
#define ID_CURV LWO3D_ID_CURV
#define ID_LINE LWO3D_ID_LINE
#define ID_BEZR LWO3D_ID_BEZR
#define ID_HCRV LWO3D_ID_HCRV
#define ID_BCRV LWO3D_ID_BCRV
#define ID_BSPL LWO3D_ID_BSPL
#define ID_OPNT LWO3D_ID_OPNT
#define ID_SURF_TAG LWO3D_FOURCC('S','U','R','F')
#define ID_MATR LWO3D_FOURCC('M','A','T','R')
#define ID_PART LWO3D_FOURCC('P','A','R','T')
#define ID_SMGP LWO3D_FOURCC('S','M','G','P')
#define ID_PICK LWO3D_FOURCC('P','I','C','K')
#define ID_COLR LWO3D_FOURCC('C','O','L','R')
#define ID_DIFF LWO3D_FOURCC('D','I','F','F')
#define ID_SPEC LWO3D_FOURCC('S','P','E','C')
#define ID_LUMI LWO3D_FOURCC('L','U','M','I')
#define ID_TRAN LWO3D_FOURCC('T','R','A','N')
#define ID_GLOS LWO3D_FOURCC('G','L','O','S')
#define ID_SMAN LWO3D_FOURCC('S','M','A','N')
#define ID_BLOK LWO3D_FOURCC('B','L','O','K')
#define ID_IMAP LWO3D_FOURCC('I','M','A','P')
#define ID_PROC LWO3D_FOURCC('P','R','O','C')
#define ID_GRAD LWO3D_FOURCC('G','R','A','D')
#define ID_SHDR LWO3D_FOURCC('S','H','D','R')
#define ID_CHAN LWO3D_FOURCC('C','H','A','N')
#define ID_ENAB LWO3D_FOURCC('E','N','A','B')
#define ID_OPAC LWO3D_FOURCC('O','P','A','C')
#define ID_IMAG LWO3D_FOURCC('I','M','A','G')
#define ID_VMAP_S LWO3D_FOURCC('V','M','A','P')
#define ID_STIL LWO3D_FOURCC('S','T','I','L')
#define ID_ANIM LWO3D_FOURCC('A','N','I','M')
#define ID_ISEQ LWO3D_FOURCC('I','S','E','Q')
#define ID_XREF LWO3D_FOURCC('X','R','E','F')
#define ID_STCC LWO3D_FOURCC('S','T','C','C')

#define LWO3D_TRUE 1
#define LWO3D_FALSE 0

typedef struct lwo3d_reader_s {
    const lwo3d_u8 *data;
    lwo3d_u32 size;
    lwo3d_u32 pos;
    int err;
} lwo3d_reader;

typedef struct lwo3d_parse_s {
    lwo3d_model *model;
    lwo3d_u16 current_layer;
    lwo3d_u32 current_point_base;
    lwo3d_u32 current_poly_base;
} lwo3d_parse;

static void parse_form_body(lwo3d_reader *r, lwo3d_u32 end, lwo3d_u32 form_type, lwo3d_u16 depth, lwo3d_parse *ps);

static void lwo3d_zero(void *ptr, lwo3d_u32 n)
{
    lwo3d_u8 *p;
    p = (lwo3d_u8 *)ptr;
    while (n > 0U) {
        *p = 0U;
        ++p;
        --n;
    }
}

static void copy_text(char *dst, lwo3d_u32 dst_size, const char *src)
{
    lwo3d_u32 i;
    if (dst_size == 0U) {
        return;
    }
    for (i = 0U; i + 1U < dst_size; ++i) {
        dst[i] = src[i];
        if (src[i] == '\0') {
            return;
        }
    }
    dst[dst_size - 1U] = '\0';
}

static int rd_need(lwo3d_reader *r, lwo3d_u32 n)
{
    if (r->err != LWO3D_OK) {
        return LWO3D_FALSE;
    }
    if (n > r->size || r->pos > r->size - n) {
        r->err = LWO3D_ERR_SHORT;
        return LWO3D_FALSE;
    }
    return LWO3D_TRUE;
}

static lwo3d_u8 rd_u1(lwo3d_reader *r)
{
    lwo3d_u8 v;
    v = 0U;
    if (rd_need(r, 1U)) {
        v = r->data[r->pos];
        r->pos += 1U;
    }
    return v;
}

static lwo3d_u16 rd_u2(lwo3d_reader *r)
{
    lwo3d_u16 v;
    v = 0U;
    if (rd_need(r, 2U)) {
        v = (lwo3d_u16)(((lwo3d_u16)r->data[r->pos] << 8) |
                         (lwo3d_u16)r->data[r->pos + 1U]);
        r->pos += 2U;
    }
    return v;
}

static lwo3d_i16 rd_i2(lwo3d_reader *r)
{
    lwo3d_u16 u;
    u = rd_u2(r);
    return (lwo3d_i16)u;
}

static lwo3d_u32 rd_u4(lwo3d_reader *r)
{
    lwo3d_u32 v;
    v = 0U;
    if (rd_need(r, 4U)) {
        v = (((lwo3d_u32)r->data[r->pos]) << 24) |
            (((lwo3d_u32)r->data[r->pos + 1U]) << 16) |
            (((lwo3d_u32)r->data[r->pos + 2U]) << 8) |
            ((lwo3d_u32)r->data[r->pos + 3U]);
        r->pos += 4U;
    }
    return v;
}

static lwo3d_u32 rd_id(lwo3d_reader *r)
{
    return rd_u4(r);
}

static void rd_skip(lwo3d_reader *r, lwo3d_u32 n)
{
    if (rd_need(r, n)) {
        r->pos += n;
    }
}

static lwo3d_u32 rd_vx(lwo3d_reader *r)
{
    lwo3d_u8 b0;
    lwo3d_u32 b1;
    lwo3d_u32 b2;
    lwo3d_u32 b3;
    lwo3d_u32 v;
    b0 = rd_u1(r);
    if (r->err != LWO3D_OK) {
        return 0U;
    }
    if (b0 == 0xffU) {
        b1 = (lwo3d_u32)rd_u1(r);
        b2 = (lwo3d_u32)rd_u1(r);
        b3 = (lwo3d_u32)rd_u1(r);
        v = (b1 << 16) | (b2 << 8) | b3;
    } else {
        b1 = (lwo3d_u32)rd_u1(r);
        v = (((lwo3d_u32)b0) << 8) | b1;
    }
    return v;
}

static void rd_s0(lwo3d_reader *r, lwo3d_u32 end, char *out, lwo3d_u32 out_size, lwo3d_model *model)
{
    lwo3d_u32 start;
    lwo3d_u32 used;
    lwo3d_u8 c;
    int done;
    if (out_size > 0U) {
        out[0] = '\0';
    }
    start = r->pos;
    used = 0U;
    done = LWO3D_FALSE;
    while (r->pos < end && r->err == LWO3D_OK && !done) {
        c = rd_u1(r);
        if (c == 0U) {
            done = LWO3D_TRUE;
        } else {
            if (out_size > 0U && used + 1U < out_size) {
                out[used] = (char)c;
                used += 1U;
                out[used] = '\0';
            } else if (model != 0) {
                model->warnings |= LWO3D_WARN_TRUNCATED_STRINGS;
            }
        }
    }
    if (!done && r->err == LWO3D_OK) {
        r->err = LWO3D_ERR_CORRUPT;
    }
    if ((((r->pos - start) & 1U) != 0U) && r->pos < end) {
        rd_skip(r, 1U);
    }
}

static lwo3d_fixed rd_f4(lwo3d_reader *r)
{
    lwo3d_u32 bits;
    bits = rd_u4(r);
    return lwo3d_fixed_from_ieee754_be(bits);
}

static void rd_vec3(lwo3d_reader *r, lwo3d_vec3 *v)
{
    v->x = rd_f4(r);
    v->y = rd_f4(r);
    v->z = rd_f4(r);
}

void lwo3d_init(lwo3d_model *model)
{
    if (model != 0) {
        lwo3d_zero(model, (lwo3d_u32)sizeof(*model));
    }
}

long lwo3d_fixed_to_int(lwo3d_fixed v)
{
    if (v >= 0) {
        return (long)(v >> LWO3D_FIXED_SHIFT);
    }
    return (long)(-((-v) >> LWO3D_FIXED_SHIFT));
}

lwo3d_fixed lwo3d_fixed_abs(lwo3d_fixed v)
{
    if (v < 0) {
        if (v == (lwo3d_fixed)(-2147483647L - 1L)) {
            return (lwo3d_fixed)2147483647L;
        }
        return -v;
    }
    return v;
}

lwo3d_fixed lwo3d_fixed_mul_small(lwo3d_fixed a, lwo3d_fixed b)
{
    lwo3d_i32 sign;
    lwo3d_u32 ua;
    lwo3d_u32 ub;
    lwo3d_u32 ahi;
    lwo3d_u32 alo;
    lwo3d_u32 bhi;
    lwo3d_u32 blo;
    lwo3d_u32 mid;
    lwo3d_u32 res;
    sign = 1;
    if (a < 0) {
        if (a == (lwo3d_fixed)(-2147483647L - 1L)) {
            a = (lwo3d_fixed)2147483647L;
        } else {
            a = -a;
        }
        sign = -sign;
    }
    if (b < 0) {
        if (b == (lwo3d_fixed)(-2147483647L - 1L)) {
            b = (lwo3d_fixed)2147483647L;
        } else {
            b = -b;
        }
        sign = -sign;
    }
    ua = (lwo3d_u32)a;
    ub = (lwo3d_u32)b;
    ahi = ua >> 16;
    alo = ua & 0xffffUL;
    bhi = ub >> 16;
    blo = ub & 0xffffUL;
    if (ahi > 32767U || bhi > 32767U) {
        return (sign < 0) ? (lwo3d_fixed)(-2147483647L - 1L) : (lwo3d_fixed)2147483647L;
    }
    res = (ahi * bhi) << 16;
    mid = ahi * blo + bhi * alo;
    if (res > 0x7fffffffUL - mid) {
        res = 0x7fffffffUL;
    } else {
        res += mid;
    }
    res += (alo * blo) >> 16;
    if (res > 0x7fffffffUL) {
        res = 0x7fffffffUL;
    }
    if (sign < 0) {
        return -(lwo3d_fixed)res;
    }
    return (lwo3d_fixed)res;
}

lwo3d_fixed lwo3d_fixed_from_ieee754_be(lwo3d_u32 bits)
{
    lwo3d_u32 sign;
    lwo3d_u32 exp;
    lwo3d_u32 mant;
    int shift;
    lwo3d_u32 q;
    lwo3d_u32 limit;
    sign = (bits >> 31) & 1U;
    exp = (bits >> 23) & 255U;
    mant = bits & 0x7fffffUL;
    if (exp == 255U) {
        return (sign != 0U) ? (lwo3d_fixed)(-2147483647L - 1L) : (lwo3d_fixed)2147483647L;
    }
    if (exp == 0U) {
        return 0;
    }
    mant |= 0x800000UL;
    shift = (int)exp - 134;
    if (shift >= 0) {
        if (shift >= 31) {
            q = 0x7fffffffUL;
        } else {
            limit = 0x7fffffffUL >> shift;
            if (mant > limit) {
                q = 0x7fffffffUL;
            } else {
                q = mant << shift;
            }
        }
    } else {
        shift = -shift;
        if (shift >= 31) {
            q = 0U;
        } else {
            q = mant >> shift;
        }
    }
    if (sign != 0U) {
        if (q >= 0x80000000UL) {
            return (lwo3d_fixed)(-2147483647L - 1L);
        }
        return -(lwo3d_fixed)q;
    }
    if (q > 0x7fffffffUL) {
        q = 0x7fffffffUL;
    }
    return (lwo3d_fixed)q;
}

void lwo3d_fourcc_text(lwo3d_u32 id, char out[5])
{
    out[0] = (char)((id >> 24) & 255U);
    out[1] = (char)((id >> 16) & 255U);
    out[2] = (char)((id >> 8) & 255U);
    out[3] = (char)(id & 255U);
    out[4] = '\0';
}

static void log_chunk(lwo3d_model *m, lwo3d_u32 id, lwo3d_u32 form_type, lwo3d_u32 offset, lwo3d_u32 size, lwo3d_u16 depth, lwo3d_u16 handled)
{
    lwo3d_chunk_info *ci;
    if (m->chunk_log_count < LWO3D_MAX_CHUNK_LOG) {
        ci = &m->chunk_log[m->chunk_log_count];
        ci->id = id;
        ci->form_type = form_type;
        ci->offset = offset;
        ci->size = size;
        ci->depth = depth;
        ci->handled = handled;
        m->chunk_log_count += 1U;
    } else {
        m->warnings |= LWO3D_WARN_LIMITS;
    }
}

static lwo3d_u16 ensure_layer(lwo3d_parse *ps)
{
    lwo3d_layer *ly;
    if (ps->current_layer != LWO3D_INDEX_NONE) {
        return ps->current_layer;
    }
    if (ps->model->layer_count >= LWO3D_MAX_LAYERS) {
        ps->model->warnings |= LWO3D_WARN_LIMITS;
        return 0U;
    }
    ly = &ps->model->layers[ps->model->layer_count];
    ly->number = 0U;
    ly->flags = 0U;
    ly->parent = LWO3D_INDEX_NONE;
    ly->pivot.x = 0;
    ly->pivot.y = 0;
    ly->pivot.z = 0;
    ly->point_start = ps->model->point_count;
    ly->point_count = 0U;
    ly->polygon_start = ps->model->polygon_count;
    ly->polygon_count = 0U;
    copy_text(ly->name, LWO3D_NAME_MAX, "Default");
    ps->current_layer = (lwo3d_u16)ps->model->layer_count;
    ps->model->layer_count += 1U;
    return ps->current_layer;
}

static void bump_layer_points(lwo3d_parse *ps, lwo3d_u16 layer, lwo3d_u32 n)
{
    if (layer < ps->model->layer_count) {
        ps->model->layers[layer].point_count += n;
    }
}

static void bump_layer_polys(lwo3d_parse *ps, lwo3d_u16 layer, lwo3d_u32 n)
{
    if (layer < ps->model->layer_count) {
        ps->model->layers[layer].polygon_count += n;
    }
}

static int same_text(const char *a, const char *b)
{
    lwo3d_u32 i;
    for (i = 0U; i < LWO3D_NAME_MAX; ++i) {
        if (a[i] != b[i]) {
            return LWO3D_FALSE;
        }
        if (a[i] == '\0') {
            return LWO3D_TRUE;
        }
    }
    return LWO3D_TRUE;
}

static int name_matches(const char *a, const char *b)
{
    if (b == 0) {
        return LWO3D_TRUE;
    }
    return same_text(a, b);
}

static lwo3d_u16 find_or_add_map(lwo3d_model *m, lwo3d_u32 type, lwo3d_u16 dim, const char *name)
{
    lwo3d_u32 i;
    for (i = 0U; i < m->map_count; ++i) {
        if (m->maps[i].type == type && m->maps[i].dimension == dim && same_text(m->maps[i].name, name)) {
            return (lwo3d_u16)i;
        }
    }
    if (m->map_count >= LWO3D_MAX_MAPS) {
        m->warnings |= LWO3D_WARN_LIMITS;
        return LWO3D_INDEX_NONE;
    }
    m->maps[m->map_count].type = type;
    m->maps[m->map_count].dimension = dim;
    copy_text(m->maps[m->map_count].name, LWO3D_NAME_MAX, name);
    m->map_count += 1U;
    return (lwo3d_u16)(m->map_count - 1U);
}

static lwo3d_u16 add_surface_named(lwo3d_model *m, const char *name)
{
    lwo3d_surface *s;
    if (m->surface_count >= LWO3D_MAX_SURFACES) {
        m->warnings |= LWO3D_WARN_LIMITS;
        return LWO3D_INDEX_NONE;
    }
    s = &m->surfaces[m->surface_count];
    lwo3d_zero(s, (lwo3d_u32)sizeof(*s));
    copy_text(s->name, LWO3D_NAME_MAX, name);
    s->color[0] = LWO3D_FIXED_ONE;
    s->color[1] = LWO3D_FIXED_ONE;
    s->color[2] = LWO3D_FIXED_ONE;
    s->diffuse = LWO3D_FIXED_ONE;
    m->surface_count += 1U;
    return (lwo3d_u16)(m->surface_count - 1U);
}

static void parse_layr(lwo3d_reader *r, lwo3d_u32 end, lwo3d_parse *ps)
{
    lwo3d_layer *ly;
    if (ps->model->layer_count >= LWO3D_MAX_LAYERS) {
        ps->model->warnings |= LWO3D_WARN_LIMITS;
        r->pos = end;
        return;
    }
    ly = &ps->model->layers[ps->model->layer_count];
    ly->number = rd_u2(r);
    ly->flags = rd_u2(r);
    rd_vec3(r, &ly->pivot);
    rd_s0(r, end, ly->name, LWO3D_NAME_MAX, ps->model);
    if (r->pos + 2U <= end) {
        ly->parent = rd_u2(r);
    } else {
        ly->parent = LWO3D_INDEX_NONE;
    }
    ly->point_start = ps->model->point_count;
    ly->point_count = 0U;
    ly->polygon_start = ps->model->polygon_count;
    ly->polygon_count = 0U;
    ps->current_layer = (lwo3d_u16)ps->model->layer_count;
    ps->current_point_base = ps->model->point_count;
    ps->current_poly_base = ps->model->polygon_count;
    ps->model->layer_count += 1U;
    r->pos = end;
}

static void parse_pnts(lwo3d_reader *r, lwo3d_u32 end, lwo3d_parse *ps)
{
    lwo3d_u16 layer;
    lwo3d_point *p;
    lwo3d_u32 added;
    layer = ensure_layer(ps);
    ps->current_point_base = ps->model->point_count;
    added = 0U;
    while (r->pos + 12U <= end && r->err == LWO3D_OK) {
        if (ps->model->point_count < LWO3D_MAX_POINTS) {
            p = &ps->model->points[ps->model->point_count];
            rd_vec3(r, &p->p);
            p->layer = layer;
            ps->model->point_count += 1U;
            added += 1U;
        } else {
            ps->model->warnings |= LWO3D_WARN_LIMITS;
            rd_skip(r, 12U);
        }
    }
    bump_layer_points(ps, layer, added);
    r->pos = end;
}

static void parse_bbox(lwo3d_reader *r, lwo3d_u32 end, lwo3d_parse *ps)
{
    if (r->pos + 24U <= end) {
        rd_vec3(r, &ps->model->bbox.minv);
        rd_vec3(r, &ps->model->bbox.maxv);
        ps->model->bbox.valid = LWO3D_TRUE;
    } else {
        ps->model->warnings |= LWO3D_WARN_UNSUPPORTED_CHUNKS;
    }
    r->pos = end;
}

static int pol_type_has_u4_flags(lwo3d_u32 type)
{
    if (type == ID_HCRV || type == ID_BCRV || type == ID_BSPL || type == ID_LINE) {
        return LWO3D_TRUE;
    }
    return LWO3D_FALSE;
}

static void parse_lwo2_pols(lwo3d_reader *r, lwo3d_u32 end, lwo3d_parse *ps)
{
    lwo3d_u32 type;
    lwo3d_u16 raw;
    lwo3d_u16 count;
    lwo3d_u16 flags16;
    lwo3d_u32 flags32;
    lwo3d_u32 i;
    lwo3d_u32 idx;
    lwo3d_polygon *poly;
    lwo3d_u32 first;
    lwo3d_u16 layer;
    lwo3d_u32 added;
    type = rd_id(r);
    layer = ensure_layer(ps);
    ps->current_poly_base = ps->model->polygon_count;
    added = 0U;
    while (r->pos + 2U <= end && r->err == LWO3D_OK) {
        raw = rd_u2(r);
        flags32 = 0U;
        if (pol_type_has_u4_flags(type)) {
            count = raw;
            flags16 = 0U;
            if (r->pos + 4U <= end) {
                flags32 = rd_u4(r);
            } else {
                r->err = LWO3D_ERR_CORRUPT;
                break;
            }
        } else if (type == ID_CURV || type == ID_BEZR) {
            count = (lwo3d_u16)(raw & 0x3fffU);
            flags16 = (lwo3d_u16)(raw & 0xc000U);
        } else {
            count = (lwo3d_u16)(raw & 0x03ffU);
            flags16 = (lwo3d_u16)(raw & 0xfc00U);
        }
        first = ps->model->poly_index_count;
        for (i = 0U; i < (lwo3d_u32)count && r->pos < end && r->err == LWO3D_OK; ++i) {
            idx = rd_vx(r);
            idx += ps->current_point_base;
            if (ps->model->poly_index_count < LWO3D_MAX_POLY_INDICES) {
                ps->model->poly_indices[ps->model->poly_index_count] = idx;
                ps->model->poly_index_count += 1U;
            } else {
                ps->model->warnings |= LWO3D_WARN_LIMITS;
            }
        }
        if (ps->model->polygon_count < LWO3D_MAX_POLYGONS) {
            poly = &ps->model->polygons[ps->model->polygon_count];
            poly->first_index = first;
            poly->index_count = (lwo3d_u16)(ps->model->poly_index_count - first);
            poly->flags16 = flags16;
            poly->flags32 = flags32;
            poly->layer = layer;
            poly->surface = LWO3D_INDEX_NONE;
            poly->material = LWO3D_INDEX_NONE;
            poly->part = LWO3D_INDEX_NONE;
            poly->smgp = LWO3D_INDEX_NONE;
            poly->type = type;
            ps->model->polygon_count += 1U;
            added += 1U;
        } else {
            ps->model->warnings |= LWO3D_WARN_LIMITS;
        }
    }
    bump_layer_polys(ps, layer, added);
    r->pos = end;
}

static void parse_tags(lwo3d_reader *r, lwo3d_u32 end, lwo3d_parse *ps)
{
    char tmp[LWO3D_NAME_MAX];
    while (r->pos < end && r->err == LWO3D_OK) {
        rd_s0(r, end, tmp, LWO3D_NAME_MAX, ps->model);
        if (ps->model->tag_count < LWO3D_MAX_TAGS) {
            copy_text(ps->model->tags[ps->model->tag_count], LWO3D_NAME_MAX, tmp);
            ps->model->tag_count += 1U;
        } else {
            ps->model->warnings |= LWO3D_WARN_LIMITS;
        }
    }
}

static void parse_lwo2_ptag(lwo3d_reader *r, lwo3d_u32 end, lwo3d_parse *ps)
{
    lwo3d_u32 type;
    lwo3d_u32 pidx;
    lwo3d_u16 tag;
    lwo3d_polygon *poly;
    type = rd_id(r);
    while (r->pos < end && r->err == LWO3D_OK) {
        pidx = rd_vx(r);
        if (r->pos + 2U > end) {
            r->err = LWO3D_ERR_CORRUPT;
            break;
        }
        tag = rd_u2(r);
        pidx += ps->current_poly_base;
        if (pidx < ps->model->polygon_count) {
            poly = &ps->model->polygons[pidx];
            if (type == ID_SURF_TAG) {
                poly->surface = tag;
            } else if (type == ID_MATR) {
                poly->material = tag;
            } else if (type == ID_PART) {
                poly->part = tag;
            } else if (type == ID_SMGP) {
                poly->smgp = tag;
            }
        }
    }
    r->pos = end;
}

static void parse_lwo2_vmap(lwo3d_reader *r, lwo3d_u32 end, lwo3d_parse *ps)
{
    lwo3d_u32 type;
    lwo3d_u16 dim;
    lwo3d_u16 map_index;
    char name[LWO3D_NAME_MAX];
    lwo3d_u32 point;
    lwo3d_u32 i;
    lwo3d_fixed vals[4];
    lwo3d_map_entry *e;
    type = rd_id(r);
    dim = rd_u2(r);
    rd_s0(r, end, name, LWO3D_NAME_MAX, ps->model);
    map_index = find_or_add_map(ps->model, type, dim, name);
    while (r->pos < end && r->err == LWO3D_OK) {
        point = rd_vx(r) + ps->current_point_base;
        vals[0] = 0;
        vals[1] = 0;
        vals[2] = 0;
        vals[3] = 0;
        for (i = 0U; i < (lwo3d_u32)dim; ++i) {
            if (r->pos + 4U > end) {
                r->err = LWO3D_ERR_CORRUPT;
                break;
            }
            if (i < 4U) {
                vals[i] = rd_f4(r);
            } else {
                (void)rd_f4(r);
            }
        }
        if (r->err != LWO3D_OK) {
            break;
        }
        if (ps->model->vmap_entry_count < LWO3D_MAX_VMAP_ENTRIES && map_index != LWO3D_INDEX_NONE) {
            e = &ps->model->vmap_entries[ps->model->vmap_entry_count];
            e->type = type;
            e->dimension = dim;
            e->map_index = map_index;
            e->point = point;
            e->value[0] = vals[0];
            e->value[1] = vals[1];
            e->value[2] = vals[2];
            e->value[3] = vals[3];
            ps->model->vmap_entry_count += 1U;
        } else {
            ps->model->warnings |= LWO3D_WARN_LIMITS;
        }
    }
    r->pos = end;
}

static void parse_lwo2_vmad(lwo3d_reader *r, lwo3d_u32 end, lwo3d_parse *ps)
{
    lwo3d_u32 type;
    lwo3d_u16 dim;
    lwo3d_u16 map_index;
    char name[LWO3D_NAME_MAX];
    lwo3d_u32 point;
    lwo3d_u32 poly;
    lwo3d_u32 i;
    lwo3d_fixed vals[4];
    lwo3d_mad_entry *e;
    type = rd_id(r);
    dim = rd_u2(r);
    rd_s0(r, end, name, LWO3D_NAME_MAX, ps->model);
    map_index = find_or_add_map(ps->model, type, dim, name);
    while (r->pos < end && r->err == LWO3D_OK) {
        point = rd_vx(r) + ps->current_point_base;
        poly = rd_vx(r) + ps->current_poly_base;
        vals[0] = 0;
        vals[1] = 0;
        vals[2] = 0;
        vals[3] = 0;
        for (i = 0U; i < (lwo3d_u32)dim; ++i) {
            if (r->pos + 4U > end) {
                r->err = LWO3D_ERR_CORRUPT;
                break;
            }
            if (i < 4U) {
                vals[i] = rd_f4(r);
            } else {
                (void)rd_f4(r);
            }
        }
        if (r->err != LWO3D_OK) {
            break;
        }
        if (ps->model->vmad_entry_count < LWO3D_MAX_VMAD_ENTRIES && map_index != LWO3D_INDEX_NONE) {
            e = &ps->model->vmad_entries[ps->model->vmad_entry_count];
            e->type = type;
            e->dimension = dim;
            e->map_index = map_index;
            e->point = point;
            e->polygon = poly;
            e->value[0] = vals[0];
            e->value[1] = vals[1];
            e->value[2] = vals[2];
            e->value[3] = vals[3];
            ps->model->vmad_entry_count += 1U;
        } else {
            ps->model->warnings |= LWO3D_WARN_LIMITS;
        }
    }
    r->pos = end;
}

static void parse_lwo2_vmed(lwo3d_reader *r, lwo3d_u32 end, lwo3d_parse *ps)
{
    lwo3d_u32 type;
    lwo3d_u16 dim;
    lwo3d_u16 map_index;
    char name[LWO3D_NAME_MAX];
    lwo3d_u32 a;
    lwo3d_u32 b;
    lwo3d_u32 i;
    lwo3d_fixed vals[4];
    lwo3d_med_entry *e;
    type = rd_id(r);
    dim = rd_u2(r);
    rd_s0(r, end, name, LWO3D_NAME_MAX, ps->model);
    map_index = find_or_add_map(ps->model, type, dim, name);
    while (r->pos < end && r->err == LWO3D_OK) {
        a = rd_vx(r) + ps->current_point_base;
        b = rd_vx(r) + ps->current_point_base;
        vals[0] = 0;
        vals[1] = 0;
        vals[2] = 0;
        vals[3] = 0;
        for (i = 0U; i < (lwo3d_u32)dim; ++i) {
            if (r->pos + 4U > end) {
                r->err = LWO3D_ERR_CORRUPT;
                break;
            }
            if (i < 4U) {
                vals[i] = rd_f4(r);
            } else {
                (void)rd_f4(r);
            }
        }
        if (r->err != LWO3D_OK) {
            break;
        }
        if (ps->model->vmed_entry_count < LWO3D_MAX_VMED_ENTRIES && map_index != LWO3D_INDEX_NONE) {
            e = &ps->model->vmed_entries[ps->model->vmed_entry_count];
            e->type = type;
            e->dimension = dim;
            e->map_index = map_index;
            e->point_a = a;
            e->point_b = b;
            e->value[0] = vals[0];
            e->value[1] = vals[1];
            e->value[2] = vals[2];
            e->value[3] = vals[3];
            ps->model->vmed_entry_count += 1U;
        } else {
            ps->model->warnings |= LWO3D_WARN_LIMITS;
        }
    }
    r->pos = end;
}

static void parse_surface_block(lwo3d_reader *r, lwo3d_u32 end, lwo3d_parse *ps, lwo3d_u16 surface_index)
{
    lwo3d_surface_block *b;
    lwo3d_u32 sid;
    lwo3d_u16 slen;
    lwo3d_u32 send;
    char tmp[LWO3D_NAME_MAX];
    if (ps->model->surface_block_count < LWO3D_MAX_SURFACE_BLOCKS) {
        b = &ps->model->surface_blocks[ps->model->surface_block_count];
        lwo3d_zero(b, (lwo3d_u32)sizeof(*b));
        b->surface = surface_index;
        b->clip_index = LWO3D_U32_NONE;
        b->opacity = LWO3D_FIXED_ONE;
        b->enabled = 1U;
        ps->model->surface_block_count += 1U;
    } else {
        b = 0;
        ps->model->warnings |= LWO3D_WARN_LIMITS;
    }
    while (r->pos + 6U <= end && r->err == LWO3D_OK) {
        sid = rd_id(r);
        slen = rd_u2(r);
        send = r->pos + (lwo3d_u32)slen;
        if (send < r->pos || send > end) {
            r->err = LWO3D_ERR_CORRUPT;
            return;
        }
        if (sid == ID_IMAP || sid == ID_PROC || sid == ID_GRAD || sid == ID_SHDR) {
            if (b != 0) {
                b->block_type = sid;
            }
            if (r->pos < send) {
                rd_s0(r, send, tmp, LWO3D_NAME_MAX, ps->model);
                if (b != 0) {
                    b->ordinal = (lwo3d_u32)(lwo3d_u8)tmp[0];
                }
            }
        } else if (sid == ID_CHAN && slen >= 4U) {
            if (b != 0) {
                b->channel = rd_id(r);
            } else {
                rd_skip(r, 4U);
            }
        } else if (sid == ID_ENAB && slen >= 2U) {
            if (b != 0) {
                b->enabled = rd_u2(r);
            } else {
                rd_skip(r, 2U);
            }
        } else if (sid == ID_OPAC && slen >= 6U) {
            (void)rd_u2(r);
            if (b != 0) {
                b->opacity = rd_f4(r);
            } else {
                (void)rd_f4(r);
            }
        } else if (sid == ID_IMAG) {
            if (slen == 2U) {
                if (b != 0) {
                    b->clip_index = (lwo3d_u32)rd_u2(r);
                } else {
                    (void)rd_u2(r);
                }
            } else if (slen >= 4U) {
                if (b != 0) {
                    b->clip_index = rd_vx(r);
                } else {
                    (void)rd_vx(r);
                }
            }
        } else if (sid == ID_VMAP_S) {
            rd_s0(r, send, tmp, LWO3D_NAME_MAX, ps->model);
            if (b != 0) {
                copy_text(b->map_name, LWO3D_NAME_MAX, tmp);
            }
        } else {
            ps->model->warnings |= LWO3D_WARN_UNSUPPORTED_CHUNKS;
        }
        r->pos = send;
        if ((slen & 1U) != 0U && r->pos < end) {
            rd_skip(r, 1U);
        }
    }
    r->pos = end;
}

static void parse_lwo2_surf(lwo3d_reader *r, lwo3d_u32 end, lwo3d_parse *ps)
{
    char name[LWO3D_NAME_MAX];
    lwo3d_u16 sidx;
    lwo3d_surface *s;
    lwo3d_u32 sid;
    lwo3d_u16 slen;
    lwo3d_u32 send;
    rd_s0(r, end, name, LWO3D_NAME_MAX, ps->model);
    sidx = add_surface_named(ps->model, name);
    if (sidx == LWO3D_INDEX_NONE) {
        r->pos = end;
        return;
    }
    s = &ps->model->surfaces[sidx];
    if (r->pos < end) {
        rd_s0(r, end, s->source, LWO3D_NAME_MAX, ps->model);
    }
    while (r->pos + 6U <= end && r->err == LWO3D_OK) {
        sid = rd_id(r);
        slen = rd_u2(r);
        send = r->pos + (lwo3d_u32)slen;
        if (send < r->pos || send > end) {
            r->err = LWO3D_ERR_CORRUPT;
            return;
        }
        if (sid == ID_COLR && slen >= 12U) {
            s->color[0] = rd_f4(r);
            s->color[1] = rd_f4(r);
            s->color[2] = rd_f4(r);
        } else if (sid == ID_DIFF && slen >= 4U) {
            s->diffuse = rd_f4(r);
        } else if (sid == ID_SPEC && slen >= 4U) {
            s->specular = rd_f4(r);
        } else if (sid == ID_LUMI && slen >= 4U) {
            s->luminosity = rd_f4(r);
        } else if (sid == ID_TRAN && slen >= 4U) {
            s->transparency = rd_f4(r);
        } else if (sid == ID_GLOS && slen >= 4U) {
            s->glossiness = rd_f4(r);
        } else if (sid == ID_SMAN && slen >= 4U) {
            s->smooth_angle = rd_f4(r);
        } else if (sid == ID_BLOK) {
            parse_surface_block(r, send, ps, sidx);
        } else {
            ps->model->warnings |= LWO3D_WARN_UNSUPPORTED_CHUNKS;
        }
        r->pos = send;
        if ((slen & 1U) != 0U && r->pos < end) {
            rd_skip(r, 1U);
        }
    }
    r->pos = end;
}

static void parse_lwo2_clip(lwo3d_reader *r, lwo3d_u32 end, lwo3d_parse *ps)
{
    lwo3d_clip *clip;
    lwo3d_u32 cid;
    lwo3d_u16 clen;
    lwo3d_u32 cend;
    lwo3d_u32 index;
    char tmp[LWO3D_PATH_MAX];
    index = rd_vx(r);
    if (ps->model->clip_count < LWO3D_MAX_CLIPS) {
        clip = &ps->model->clips[ps->model->clip_count];
        lwo3d_zero(clip, (lwo3d_u32)sizeof(*clip));
        clip->index = index;
        ps->model->clip_count += 1U;
    } else {
        ps->model->warnings |= LWO3D_WARN_LIMITS;
        clip = 0;
    }
    while (r->pos + 6U <= end && r->err == LWO3D_OK) {
        cid = rd_id(r);
        clen = rd_u2(r);
        cend = r->pos + (lwo3d_u32)clen;
        if (cend < r->pos || cend > end) {
            r->err = LWO3D_ERR_CORRUPT;
            return;
        }
        if (cid == ID_STIL || cid == ID_STCC || cid == ID_ISEQ || cid == ID_XREF) {
            rd_s0(r, cend, tmp, LWO3D_PATH_MAX, ps->model);
            if (clip != 0) {
                copy_text(clip->path, LWO3D_PATH_MAX, tmp);
            }
        } else if (cid == ID_ANIM) {
            rd_s0(r, cend, tmp, LWO3D_PATH_MAX, ps->model);
            if (clip != 0) {
                copy_text(clip->path, LWO3D_PATH_MAX, tmp);
            }
            if (r->pos < cend) {
                rd_s0(r, cend, tmp, LWO3D_NAME_MAX, ps->model);
                if (clip != 0) {
                    copy_text(clip->server, LWO3D_NAME_MAX, tmp);
                }
            }
        } else {
            ps->model->warnings |= LWO3D_WARN_UNSUPPORTED_CHUNKS;
        }
        r->pos = cend;
        if ((clen & 1U) != 0U && r->pos < end) {
            rd_skip(r, 1U);
        }
    }
    r->pos = end;
}

static void parse_lwob_srfs(lwo3d_reader *r, lwo3d_u32 end, lwo3d_parse *ps)
{
    char tmp[LWO3D_NAME_MAX];
    while (r->pos < end && r->err == LWO3D_OK) {
        rd_s0(r, end, tmp, LWO3D_NAME_MAX, ps->model);
        if (ps->model->tag_count < LWO3D_MAX_TAGS) {
            copy_text(ps->model->tags[ps->model->tag_count], LWO3D_NAME_MAX, tmp);
            ps->model->tag_count += 1U;
        } else {
            ps->model->warnings |= LWO3D_WARN_LIMITS;
        }
        (void)add_surface_named(ps->model, tmp);
    }
}

static void parse_lwob_pols(lwo3d_reader *r, lwo3d_u32 end, lwo3d_parse *ps)
{
    lwo3d_u16 count;
    lwo3d_u32 i;
    lwo3d_u32 idx;
    lwo3d_u32 first;
    lwo3d_polygon *poly;
    lwo3d_i16 surf;
    lwo3d_u16 layer;
    lwo3d_u32 added;
    layer = ensure_layer(ps);
    ps->current_poly_base = ps->model->polygon_count;
    added = 0U;
    while (r->pos + 2U <= end && r->err == LWO3D_OK) {
        count = rd_u2(r);
        first = ps->model->poly_index_count;
        for (i = 0U; i < (lwo3d_u32)count && r->pos + 2U <= end; ++i) {
            idx = (lwo3d_u32)rd_u2(r) + ps->current_point_base;
            if (ps->model->poly_index_count < LWO3D_MAX_POLY_INDICES) {
                ps->model->poly_indices[ps->model->poly_index_count] = idx;
                ps->model->poly_index_count += 1U;
            } else {
                ps->model->warnings |= LWO3D_WARN_LIMITS;
            }
        }
        if (r->pos + 2U > end) {
            r->err = LWO3D_ERR_CORRUPT;
            break;
        }
        surf = rd_i2(r);
        if (ps->model->polygon_count < LWO3D_MAX_POLYGONS) {
            poly = &ps->model->polygons[ps->model->polygon_count];
            poly->first_index = first;
            poly->index_count = (lwo3d_u16)(ps->model->poly_index_count - first);
            poly->flags16 = 0U;
            poly->flags32 = 0U;
            poly->layer = layer;
            poly->part = LWO3D_INDEX_NONE;
            poly->smgp = LWO3D_INDEX_NONE;
            poly->material = LWO3D_INDEX_NONE;
            poly->type = ID_FACE;
            if (surf > 0) {
                poly->surface = (lwo3d_u16)(surf - 1);
            } else if (surf < 0) {
                poly->surface = (lwo3d_u16)((-surf) - 1);
                ps->model->warnings |= LWO3D_WARN_UNSUPPORTED_CHUNKS;
            } else {
                poly->surface = LWO3D_INDEX_NONE;
            }
            ps->model->polygon_count += 1U;
            added += 1U;
        } else {
            ps->model->warnings |= LWO3D_WARN_LIMITS;
        }
    }
    bump_layer_polys(ps, layer, added);
    r->pos = end;
}

static void parse_chunk_common(lwo3d_reader *r, lwo3d_u32 chunk_id, lwo3d_u32 end, lwo3d_u32 form_type, lwo3d_u16 depth, lwo3d_parse *ps)
{
    if (chunk_id == ID_FORM) {
        lwo3d_u32 child_type;
        if (r->pos + 4U <= end) {
            child_type = rd_id(r);
            ps->model->warnings |= LWO3D_WARN_NESTED_FORMS;
            parse_form_body(r, end, child_type, (lwo3d_u16)(depth + 1U), ps);
        } else {
            r->err = LWO3D_ERR_CORRUPT;
        }
    } else if (chunk_id == ID_LAYR) {
        parse_layr(r, end, ps);
    } else if (chunk_id == ID_PNTS) {
        parse_pnts(r, end, ps);
    } else if (chunk_id == ID_BBOX) {
        parse_bbox(r, end, ps);
    } else if (chunk_id == ID_POLS) {
        parse_lwo2_pols(r, end, ps);
    } else if (chunk_id == ID_TAGS) {
        parse_tags(r, end, ps);
    } else if (chunk_id == ID_PTAG) {
        parse_lwo2_ptag(r, end, ps);
    } else if (chunk_id == ID_VMAP) {
        parse_lwo2_vmap(r, end, ps);
    } else if (chunk_id == ID_VMAD) {
        parse_lwo2_vmad(r, end, ps);
    } else if (chunk_id == ID_VMED) {
        parse_lwo2_vmed(r, end, ps);
    } else if (chunk_id == ID_SURF) {
        parse_lwo2_surf(r, end, ps);
    } else if (chunk_id == ID_CLIP) {
        parse_lwo2_clip(r, end, ps);
    } else if (chunk_id == ID_ENVL || chunk_id == ID_DESC || chunk_id == ID_ICON || chunk_id == ID_CRVS) {
        ps->model->warnings |= LWO3D_WARN_UNSUPPORTED_CHUNKS;
        r->pos = end;
    } else {
        (void)form_type;
        ps->model->warnings |= LWO3D_WARN_UNKNOWN_CHUNKS;
        ps->model->unknown_chunks += 1U;
        r->pos = end;
    }
}

static void parse_chunk_lwob(lwo3d_reader *r, lwo3d_u32 chunk_id, lwo3d_u32 end, lwo3d_parse *ps)
{
    if (chunk_id == ID_PNTS) {
        parse_pnts(r, end, ps);
    } else if (chunk_id == ID_SRFS) {
        parse_lwob_srfs(r, end, ps);
    } else if (chunk_id == ID_POLS) {
        parse_lwob_pols(r, end, ps);
    } else if (chunk_id == ID_SURF || chunk_id == ID_CRVS) {
        ps->model->warnings |= LWO3D_WARN_UNSUPPORTED_CHUNKS;
        r->pos = end;
    } else {
        ps->model->warnings |= LWO3D_WARN_UNKNOWN_CHUNKS;
        ps->model->unknown_chunks += 1U;
        r->pos = end;
    }
}

static void parse_form_body(lwo3d_reader *r, lwo3d_u32 end, lwo3d_u32 form_type, lwo3d_u16 depth, lwo3d_parse *ps)
{
    lwo3d_u32 chunk_id;
    lwo3d_u32 chunk_size;
    lwo3d_u32 chunk_offset;
    lwo3d_u32 chunk_end;
    lwo3d_u16 handled;
    if (depth > (lwo3d_u16)LWO3D_MAX_FORM_DEPTH) {
        r->err = LWO3D_ERR_DEPTH;
        return;
    }
    ps->model->form_count += 1U;
    if ((lwo3d_u32)depth > ps->model->max_depth_seen) {
        ps->model->max_depth_seen = (lwo3d_u32)depth;
    }
    while (r->pos + 8U <= end && r->err == LWO3D_OK) {
        chunk_offset = r->pos;
        chunk_id = rd_id(r);
        chunk_size = rd_u4(r);
        chunk_end = r->pos + chunk_size;
        if (chunk_end < r->pos || chunk_end > end) {
            r->err = LWO3D_ERR_CORRUPT;
            break;
        }
        handled = 1U;
        if (form_type == ID_LWOB) {
            parse_chunk_lwob(r, chunk_id, chunk_end, ps);
        } else {
            parse_chunk_common(r, chunk_id, chunk_end, form_type, depth, ps);
        }
        if (r->pos == chunk_end && (ps->model->warnings & LWO3D_WARN_UNKNOWN_CHUNKS) != 0U) {
            handled = 0U;
        }
        log_chunk(ps->model, chunk_id, form_type, chunk_offset, chunk_size, depth, handled);
        r->pos = chunk_end;
        if ((chunk_size & 1U) != 0U && r->pos < end) {
            rd_skip(r, 1U);
        }
    }
}

int lwo3d_decode_memory(const lwo3d_u8 *data, lwo3d_u32 size, lwo3d_model *model)
{
    lwo3d_reader r;
    lwo3d_parse ps;
    lwo3d_u32 id;
    lwo3d_u32 form_size;
    lwo3d_u32 form_type;
    lwo3d_u32 form_end;
    if (data == 0 || model == 0) {
        return LWO3D_ERR_ARG;
    }
    lwo3d_init(model);
    r.data = data;
    r.size = size;
    r.pos = 0U;
    r.err = LWO3D_OK;
    id = rd_id(&r);
    if (id != ID_FORM) {
        return LWO3D_ERR_FORM;
    }
    form_size = rd_u4(&r);
    form_type = rd_id(&r);
    if (r.err != LWO3D_OK) {
        return r.err;
    }
    if (form_type != ID_LWO2 && form_type != ID_LWO3 && form_type != ID_LWOB) {
        return LWO3D_ERR_UNSUPPORTED;
    }
    form_end = 8U + form_size;
    if (form_end > size || form_end < 12U) {
        form_end = size;
        model->warnings |= LWO3D_WARN_RECOVERED_SIZE;
    }
    model->form_type = form_type;
    if (form_type == ID_LWO3) {
        model->warnings |= LWO3D_WARN_LWO3_PARTIAL;
    }
    ps.model = model;
    ps.current_layer = LWO3D_INDEX_NONE;
    ps.current_point_base = 0U;
    ps.current_poly_base = 0U;
    parse_form_body(&r, form_end, form_type, 0U, &ps);
    if (r.err != LWO3D_OK) {
        return r.err;
    }
    return LWO3D_OK;
}

static int is_tri_renderable_type(lwo3d_u32 type)
{
    if (type == ID_FACE || type == ID_SUBD || type == ID_PSUB || type == ID_PTCH || type == ID_SPCH) {
        return LWO3D_TRUE;
    }
    return LWO3D_FALSE;
}

int lwo3d_compute_polygon_raw_normal(const lwo3d_model *model, lwo3d_u32 polygon_index, lwo3d_vec3 *out_normal)
{
    const lwo3d_polygon *poly;
    lwo3d_u32 a;
    lwo3d_u32 b;
    lwo3d_u32 c;
    lwo3d_vec3 e1;
    lwo3d_vec3 e2;
    if (model == 0 || out_normal == 0 || polygon_index >= model->polygon_count) {
        return LWO3D_FALSE;
    }
    poly = &model->polygons[polygon_index];
    if (poly->index_count < 3U) {
        return LWO3D_FALSE;
    }
    a = model->poly_indices[poly->first_index];
    b = model->poly_indices[poly->first_index + 1U];
    c = model->poly_indices[poly->first_index + 2U];
    if (a >= model->point_count || b >= model->point_count || c >= model->point_count) {
        return LWO3D_FALSE;
    }
    e1.x = model->points[b].p.x - model->points[a].p.x;
    e1.y = model->points[b].p.y - model->points[a].p.y;
    e1.z = model->points[b].p.z - model->points[a].p.z;
    e2.x = model->points[c].p.x - model->points[a].p.x;
    e2.y = model->points[c].p.y - model->points[a].p.y;
    e2.z = model->points[c].p.z - model->points[a].p.z;
    out_normal->x = lwo3d_fixed_mul_small(e1.y, e2.z) - lwo3d_fixed_mul_small(e1.z, e2.y);
    out_normal->y = lwo3d_fixed_mul_small(e1.z, e2.x) - lwo3d_fixed_mul_small(e1.x, e2.z);
    out_normal->z = lwo3d_fixed_mul_small(e1.x, e2.y) - lwo3d_fixed_mul_small(e1.y, e2.x);
    return LWO3D_TRUE;
}

void lwo3d_emit_triangles(const lwo3d_model *model, lwo3d_triangle_cb cb, void *user)
{
    lwo3d_u32 p;
    lwo3d_u32 j;
    lwo3d_u32 a;
    lwo3d_u32 b;
    lwo3d_u32 c;
    lwo3d_triangle tri;
    const lwo3d_polygon *poly;
    lwo3d_vec2 uv;
    if (model == 0 || cb == 0) {
        return;
    }
    for (p = 0U; p < model->polygon_count; ++p) {
        poly = &model->polygons[p];
        if (poly->index_count < 3U || !is_tri_renderable_type(poly->type)) {
            continue;
        }
        a = model->poly_indices[poly->first_index];
        for (j = 1U; j + 1U < (lwo3d_u32)poly->index_count; ++j) {
            b = model->poly_indices[poly->first_index + j];
            c = model->poly_indices[poly->first_index + j + 1U];
            if (a >= model->point_count || b >= model->point_count || c >= model->point_count) {
                continue;
            }
            lwo3d_zero(&tri, (lwo3d_u32)sizeof(tri));
            tri.polygon = p;
            tri.point_index[0] = a;
            tri.point_index[1] = b;
            tri.point_index[2] = c;
            tri.p[0] = model->points[a].p;
            tri.p[1] = model->points[b].p;
            tri.p[2] = model->points[c].p;
            tri.surface = poly->surface;
            tri.material = poly->material;
            tri.layer = poly->layer;
            (void)lwo3d_compute_polygon_raw_normal(model, p, &tri.raw_normal);
            if (lwo3d_find_txuv(model, 0, p, a, &uv)) {
                tri.uv[0] = uv;
                tri.has_uv[0] = 1U;
            }
            if (lwo3d_find_txuv(model, 0, p, b, &uv)) {
                tri.uv[1] = uv;
                tri.has_uv[1] = 1U;
            }
            if (lwo3d_find_txuv(model, 0, p, c, &uv)) {
                tri.uv[2] = uv;
                tri.has_uv[2] = 1U;
            }
            cb(user, &tri);
        }
    }
}

int lwo3d_find_vmap_value(const lwo3d_model *model, lwo3d_u32 type, const char *map_name, lwo3d_u32 point, lwo3d_fixed out_value[4], lwo3d_u16 *out_dimension)
{
    lwo3d_u32 i;
    lwo3d_u16 mi;
    const lwo3d_map_entry *e;
    if (model == 0 || out_value == 0) {
        return LWO3D_FALSE;
    }
    for (i = 0U; i < model->vmap_entry_count; ++i) {
        e = &model->vmap_entries[i];
        mi = e->map_index;
        if (e->type == type && e->point == point && mi < model->map_count && name_matches(model->maps[mi].name, map_name)) {
            out_value[0] = e->value[0];
            out_value[1] = e->value[1];
            out_value[2] = e->value[2];
            out_value[3] = e->value[3];
            if (out_dimension != 0) {
                *out_dimension = e->dimension;
            }
            return LWO3D_TRUE;
        }
    }
    return LWO3D_FALSE;
}

int lwo3d_find_vmad_value(const lwo3d_model *model, lwo3d_u32 type, const char *map_name, lwo3d_u32 polygon, lwo3d_u32 point, lwo3d_fixed out_value[4], lwo3d_u16 *out_dimension)
{
    lwo3d_u32 i;
    lwo3d_u16 mi;
    const lwo3d_mad_entry *e;
    if (model == 0 || out_value == 0) {
        return LWO3D_FALSE;
    }
    for (i = 0U; i < model->vmad_entry_count; ++i) {
        e = &model->vmad_entries[i];
        mi = e->map_index;
        if (e->type == type && e->point == point && e->polygon == polygon && mi < model->map_count && name_matches(model->maps[mi].name, map_name)) {
            out_value[0] = e->value[0];
            out_value[1] = e->value[1];
            out_value[2] = e->value[2];
            out_value[3] = e->value[3];
            if (out_dimension != 0) {
                *out_dimension = e->dimension;
            }
            return LWO3D_TRUE;
        }
    }
    return LWO3D_FALSE;
}

int lwo3d_find_vmed_value(const lwo3d_model *model, lwo3d_u32 type, const char *map_name, lwo3d_u32 point_a, lwo3d_u32 point_b, lwo3d_fixed out_value[4], lwo3d_u16 *out_dimension)
{
    lwo3d_u32 i;
    lwo3d_u16 mi;
    const lwo3d_med_entry *e;
    if (model == 0 || out_value == 0) {
        return LWO3D_FALSE;
    }
    for (i = 0U; i < model->vmed_entry_count; ++i) {
        e = &model->vmed_entries[i];
        mi = e->map_index;
        if (e->type == type && mi < model->map_count && name_matches(model->maps[mi].name, map_name)) {
            if ((e->point_a == point_a && e->point_b == point_b) || (e->point_a == point_b && e->point_b == point_a)) {
                out_value[0] = e->value[0];
                out_value[1] = e->value[1];
                out_value[2] = e->value[2];
                out_value[3] = e->value[3];
                if (out_dimension != 0) {
                    *out_dimension = e->dimension;
                }
                return LWO3D_TRUE;
            }
        }
    }
    return LWO3D_FALSE;
}

int lwo3d_find_txuv(const lwo3d_model *model, const char *map_name, lwo3d_u32 polygon, lwo3d_u32 point, lwo3d_vec2 *out_uv)
{
    lwo3d_fixed vals[4];
    lwo3d_u16 dim;
    if (model == 0 || out_uv == 0) {
        return LWO3D_FALSE;
    }
    if (lwo3d_find_vmad_value(model, LWO3D_ID_TXUV, map_name, polygon, point, vals, &dim)) {
        if (dim >= 2U) {
            out_uv->x = vals[0];
            out_uv->y = vals[1];
            return LWO3D_TRUE;
        }
    }
    if (lwo3d_find_vmap_value(model, LWO3D_ID_TXUV, map_name, point, vals, &dim)) {
        if (dim >= 2U) {
            out_uv->x = vals[0];
            out_uv->y = vals[1];
            return LWO3D_TRUE;
        }
    }
    return LWO3D_FALSE;
}

const char *lwo3d_error_string(int code)
{
    if (code == LWO3D_OK) {
        return "ok";
    }
    if (code == LWO3D_ERR_ARG) {
        return "bad argument";
    }
    if (code == LWO3D_ERR_SHORT) {
        return "short input";
    }
    if (code == LWO3D_ERR_FORM) {
        return "not an IFF FORM";
    }
    if (code == LWO3D_ERR_UNSUPPORTED) {
        return "unsupported FORM type";
    }
    if (code == LWO3D_ERR_CORRUPT) {
        return "corrupt LWO data";
    }
    if (code == LWO3D_ERR_DEPTH) {
        return "FORM nesting is too deep";
    }
    return "unknown error";
}
