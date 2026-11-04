#include "m3d89.h"
#include <string.h>

static int m3d89_has(const m3d89_u8 *p, const m3d89_u8 *end, m3d89_u32 n)
{
    if(p == 0 || end == 0) return 0;
    if(p > end) return 0;
    return (m3d89_u32)(end - p) >= n;
}

static int m3d89_magic(const m3d89_u8 *p, const char *s)
{
    return p[0] == (m3d89_u8)s[0] && p[1] == (m3d89_u8)s[1] && p[2] == (m3d89_u8)s[2] && p[3] == (m3d89_u8)s[3];
}

static m3d89_u16 m3d89_get16(const m3d89_u8 *p)
{
    return (m3d89_u16)((m3d89_u16)p[0] | ((m3d89_u16)p[1] << 8));
}

static m3d89_u32 m3d89_get32(const m3d89_u8 *p)
{
    return ((m3d89_u32)p[0]) |
           ((m3d89_u32)p[1] << 8) |
           ((m3d89_u32)p[2] << 16) |
           ((m3d89_u32)p[3] << 24);
}

static m3d89_i32 m3d89_sign_extend(m3d89_u32 v, m3d89_u8 s)
{
    if(s == 1) return (m3d89_i32)((m3d89_i8)(m3d89_u8)v);
    if(s == 2) return (m3d89_i32)((m3d89_i16)(m3d89_u16)v);
    return (m3d89_i32)v;
}

static m3d89_u8 m3d89_coord_size(m3d89_u32 code)
{
    if(code == 0) return 1;
    if(code == 1) return 2;
    if(code == 2) return 4;
    return 8;
}

static m3d89_u8 m3d89_index_size(m3d89_u32 code)
{
    if(code == 0) return 1;
    if(code == 1) return 2;
    if(code == 2) return 4;
    return 0;
}

static m3d89_u32 m3d89_read_u(const m3d89_u8 **pp, m3d89_u8 s)
{
    const m3d89_u8 *p;
    m3d89_u32 v;
    p = *pp;
    if(s == 1) { v = p[0]; *pp = p + 1; return v; }
    if(s == 2) { v = m3d89_get16(p); *pp = p + 2; return v; }
    if(s == 4) { v = m3d89_get32(p); *pp = p + 4; return v; }
    return M3D89_UNDEF_U32;
}

static m3d89_i32 m3d89_read_i(const m3d89_u8 **pp, m3d89_u8 s)
{
    m3d89_u32 v;
    v = m3d89_read_u(pp, s);
    return m3d89_sign_extend(v, s);
}


static m3d89_q16 m3d89_i_to_q16_scaled(m3d89_i32 v, m3d89_i32 pos_den, m3d89_i32 neg_den)
{
    m3d89_i32 r;
    if(v >= 0) r = (m3d89_i32)((v * 65536L) / pos_den);
    else r = (m3d89_i32)((v * 65536L) / neg_den);
    if(r > 65536L) r = 65536L;
    if(r < -65536L) r = -65536L;
    return (m3d89_q16)r;
}

static m3d89_q16 m3d89_bin32_to_q16(m3d89_u32 bits)
{
    m3d89_u32 sign;
    m3d89_i32 exp;
    m3d89_u32 frac;
    m3d89_u32 sig;
    m3d89_i32 shift;
    m3d89_i32 out;
    sign = bits >> 31;
    exp = (m3d89_i32)((bits >> 23) & 255UL);
    frac = bits & 0x7FFFFFUL;
    if(exp == 0) return 0;
    if(exp == 255L) return sign ? (m3d89_q16)-2147483647L : (m3d89_q16)2147483647L;
    sig = (1UL << 23) | frac;
    shift = (exp - 127L) + 16L - 23L;
    if(shift >= 0) {
        if(shift > 7) out = 2147483647L;
        else out = (m3d89_i32)(sig << shift);
    } else {
        shift = -shift;
        if(shift >= 31) out = 0;
        else out = (m3d89_i32)(sig >> shift);
    }
    return sign ? -out : out;
}

static m3d89_q16 m3d89_bin64_to_q16(const m3d89_u8 *p)
{
    m3d89_u32 lo;
    m3d89_u32 hi;
    m3d89_u32 sign;
    m3d89_i32 exp;
    m3d89_u32 frac_hi;
    m3d89_u32 sig;
    m3d89_i32 shift;
    m3d89_i32 out;
    lo = m3d89_get32(p);
    hi = m3d89_get32(p + 4);
    (void)lo;
    sign = hi >> 31;
    exp = (m3d89_i32)((hi >> 20) & 2047UL);
    frac_hi = hi & 0xFFFFFUL;
    if(exp == 0) return 0;
    if(exp == 2047L) return sign ? (m3d89_q16)-2147483647L : (m3d89_q16)2147483647L;
    sig = (1UL << 20) | frac_hi;
    shift = (exp - 1023L) + 16L - 20L;
    if(shift >= 0) {
        if(shift > 10) out = 2147483647L;
        else out = (m3d89_i32)(sig << shift);
    } else {
        shift = -shift;
        if(shift >= 31) out = 0;
        else out = (m3d89_i32)(sig >> shift);
    }
    return sign ? -out : out;
}

static m3d89_q16 m3d89_read_coord(const m3d89_u8 **pp, m3d89_u8 s)
{
    const m3d89_u8 *p;
    m3d89_q16 q;
    p = *pp;
    q = 0;
    if(s == 1) {
        q = m3d89_i_to_q16_scaled((m3d89_i32)((m3d89_i8)p[0]), 127L, 128L);
    } else if(s == 2) {
        q = m3d89_i_to_q16_scaled((m3d89_i32)((m3d89_i16)m3d89_get16(p)), 32767L, 32768L);
    } else if(s == 4) {
        q = m3d89_bin32_to_q16(m3d89_get32(p));
    } else if(s == 8) {
        q = m3d89_bin64_to_q16(p);
    }
    *pp = p + s;
    return q;
}

static m3d89_u32 m3d89_default_color(void)
{
    return 0xFF808080UL;
}

static m3d89_u32 m3d89_color_from_index(const m3d89_model *m, m3d89_u32 idx)
{
    if(idx < m->color_count && idx < 256UL) return m->colors[idx];
    return m3d89_default_color();
}

static m3d89_u32 m3d89_read_color(const m3d89_model *m, const m3d89_u8 **pp)
{
    m3d89_u32 v;
    if(m->ci_s == 1 || m->ci_s == 2) {
        v = m3d89_read_u(pp, m->ci_s);
        return m3d89_color_from_index(m, v);
    }
    if(m->ci_s == 4) return m3d89_read_u(pp, 4);
    return m3d89_default_color();
}

static int m3d89_is_std_magic(const m3d89_u8 *p)
{
    int i;
    for(i = 0; i < 4; i++) if(p[i] < 'A' || p[i] > 'Z') return 0;
    return 1;
}

static void m3d89_add_chunk(m3d89_model *m, const m3d89_u8 *base, const m3d89_u8 *p, m3d89_u32 len)
{
    m3d89_chunk *c;
    if(m->chunk_count >= M3D89_MAX_CHUNKS) {
        m->warnings++;
        return;
    }
    c = &m->chunks[m->chunk_count++];
    c->magic[0] = (char)p[0];
    c->magic[1] = (char)p[1];
    c->magic[2] = (char)p[2];
    c->magic[3] = (char)p[3];
    c->magic[4] = 0;
    c->offset = (m3d89_u32)(p - base);
    c->length = len;
    c->flags = m3d89_is_std_magic(p) ? M3D89_CHUNK_FLAG_STD : M3D89_CHUNK_FLAG_APP;
}

static void m3d89_head_strings(m3d89_model *m)
{
    m3d89_u32 pos;
    m3d89_u32 item;
    m->name = M3D89_UNDEF_U32;
    m->license = M3D89_UNDEF_U32;
    m->author = M3D89_UNDEF_U32;
    m->description = M3D89_UNDEF_U32;
    pos = 0;
    item = 0;
    while(pos < m->strings_size && item < 4UL) {
        if(item == 0) m->name = pos;
        if(item == 1) m->license = pos;
        if(item == 2) m->author = pos;
        if(item == 3) m->description = pos;
        while(pos < m->strings_size && m->strings[pos] != 0) pos++;
        if(pos >= m->strings_size) break;
        pos++;
        item++;
    }
}

static int m3d89_parse_head(m3d89_model *m, const m3d89_u8 *chunk, m3d89_u32 len)
{
    m3d89_u32 t;
    if(len < 16UL) return M3D89_ERR_BAD_HEADER;
    if(!m3d89_magic(chunk, "HEAD")) return M3D89_ERR_BAD_HEADER;
    m->scale_q16 = m3d89_bin32_to_q16(m3d89_get32(chunk + 8));
    t = m3d89_get32(chunk + 12);
    m->type_bits = t;
    m->vc_s = m3d89_coord_size((t >> 0) & 3UL);
    m->vi_s = m3d89_index_size((t >> 2) & 3UL);
    m->si_s = m3d89_index_size((t >> 4) & 3UL);
    m->ci_s = m3d89_index_size((t >> 6) & 3UL);
    m->ti_s = m3d89_index_size((t >> 8) & 3UL);
    m->bi_s = m3d89_index_size((t >> 10) & 3UL);
    m->nb_n = (m3d89_u8)(1U << ((t >> 12) & 3UL));
    m->sk_s = m3d89_index_size((t >> 14) & 3UL);
    m->fc_s = m3d89_index_size((t >> 16) & 3UL);
    m->hi_s = m3d89_index_size((t >> 18) & 3UL);
    m->fi_s = m3d89_index_size((t >> 20) & 3UL);
    m->vd_s = m3d89_index_size((t >> 22) & 3UL);
    m->vp_s = ((t >> 24) & 1UL) ? 2 : 1;
    if(m->vi_s == 0 || m->si_s == 0) return M3D89_ERR_UNSUPPORTED_TYPE;
    m->strings = chunk + 16;
    m->strings_size = len - 16UL;
    m3d89_head_strings(m);
    return M3D89_OK;
}

static int m3d89_parse_cmap(m3d89_model *m, const m3d89_u8 *ptr, m3d89_u32 payload_len)
{
    m3d89_u32 n;
    m3d89_u32 i;
    n = payload_len / 4UL;
    m->color_count = n;
    if(m->color_count > 256UL) {
        m->color_count = 256UL;
        m->warnings++;
    }
    for(i = 0; i < m->color_count; i++) m->colors[i] = m3d89_get32(ptr + i * 4UL);
    return M3D89_OK;
}

static int m3d89_parse_tmap(m3d89_model *m, const m3d89_u8 *ptr, const m3d89_u8 *end)
{
    m3d89_u32 rec;
    const m3d89_u8 *p;
    m3d89_texcoord *t;
    rec = (m3d89_u32)m->vc_s * 2UL;
    if(rec == 0) return M3D89_ERR_UNSUPPORTED_TYPE;
    p = ptr;
    while(m3d89_has(p, end, rec)) {
        if(m->texcoord_count >= M3D89_MAX_TEXCOORDS) return M3D89_ERR_CAPACITY;
        t = &m->texcoords[m->texcoord_count++];
        t->u = m3d89_read_coord(&p, m->vc_s);
        t->v = m3d89_read_coord(&p, m->vc_s);
    }
    if(p != end) return M3D89_ERR_TRUNCATED;
    return M3D89_OK;
}

static int m3d89_parse_vrts(m3d89_model *m, const m3d89_u8 *ptr, const m3d89_u8 *end)
{
    m3d89_u32 rec;
    const m3d89_u8 *p;
    m3d89_vertex *v;
    rec = 4UL * (m3d89_u32)m->vc_s;
    if(m->ci_s != 0) rec += m->ci_s;
    if(m->sk_s != 0) rec += m->sk_s;
    if(rec == 0) return M3D89_ERR_UNSUPPORTED_TYPE;
    p = ptr;
    while(m3d89_has(p, end, rec)) {
        if(m->vertex_count >= M3D89_MAX_VERTICES) return M3D89_ERR_CAPACITY;
        v = &m->vertices[m->vertex_count++];
        v->x = m3d89_read_coord(&p, m->vc_s);
        v->y = m3d89_read_coord(&p, m->vc_s);
        v->z = m3d89_read_coord(&p, m->vc_s);
        v->w = m3d89_read_coord(&p, m->vc_s);
        if(m->ci_s != 0) v->rgba = m3d89_read_color(m, &p); else v->rgba = m3d89_default_color();
        if(m->sk_s != 0) v->skin = m3d89_read_u(&p, m->sk_s); else v->skin = M3D89_UNDEF_U32;
    }
    if(p != end) return M3D89_ERR_TRUNCATED;
    return M3D89_OK;
}

static m3d89_u16 m3d89_find_material(const m3d89_model *m, m3d89_u32 name)
{
    m3d89_u16 i;
    for(i = 0; i < m->material_count; i++) if(m->materials[i].name == name) return i;
    return (m3d89_u16)0xFFFFU;
}

static int m3d89_push_face(m3d89_model *m, m3d89_u32 *vi, m3d89_u32 *ti, m3d89_u32 *ni, m3d89_u32 mat_name, m3d89_u16 flags)
{
    m3d89_face *f;
    if(m->face_count >= M3D89_MAX_FACES) return M3D89_ERR_CAPACITY;
    f = &m->faces[m->face_count++];
    f->vertex[0] = vi[0];
    f->vertex[1] = vi[1];
    f->vertex[2] = vi[2];
    f->texcoord[0] = ti[0];
    f->texcoord[1] = ti[1];
    f->texcoord[2] = ti[2];
    f->normal[0] = ni[0];
    f->normal[1] = ni[1];
    f->normal[2] = ni[2];
    f->material_name = mat_name;
    f->material_index = m3d89_find_material(m, mat_name);
    f->flags = flags;
    return M3D89_OK;
}

static int m3d89_parse_mesh(m3d89_model *m, const m3d89_u8 *ptr, const m3d89_u8 *end)
{
    const m3d89_u8 *p;
    int np;
    int j;
    int r;
    m3d89_u32 v[15];
    m3d89_u32 t[15];
    m3d89_u32 n[15];
    m3d89_u32 tri_v[3];
    m3d89_u32 tri_t[3];
    m3d89_u32 tri_n[3];
    m3d89_u32 cur_mat;
    m3d89_u16 flags;
    m3d89_u8 rec;
    p = ptr;
    cur_mat = 0;
    while(p < end) {
        rec = *p++;
        np = (int)(rec >> 4);
        if(np == 0) {
            if((rec & 15U) == 0U || (rec & 15U) == 1U) {
                if(!m3d89_has(p, end, m->si_s)) return M3D89_ERR_TRUNCATED;
                if((rec & 15U) == 0U) cur_mat = m3d89_read_u(&p, m->si_s);
                else (void)m3d89_read_u(&p, m->si_s);
                continue;
            }
            return M3D89_ERR_BAD_RECORD;
        }
        if(np > 15) return M3D89_ERR_BAD_RECORD;
        flags = 0;
        if(rec & 1U) flags = (m3d89_u16)(flags | M3D89_FACE_HAS_UV);
        if(rec & 2U) flags = (m3d89_u16)(flags | M3D89_FACE_HAS_NORMAL);
        if(rec & 4U) flags = (m3d89_u16)(flags | M3D89_FACE_HAS_MAXIMUM);
        for(j = 0; j < np; j++) {
            if(!m3d89_has(p, end, m->vi_s)) return M3D89_ERR_TRUNCATED;
            v[j] = m3d89_read_u(&p, m->vi_s);
            t[j] = M3D89_UNDEF_U32;
            n[j] = M3D89_UNDEF_U32;
            if(rec & 1U) {
                if(!m3d89_has(p, end, m->ti_s)) return M3D89_ERR_TRUNCATED;
                t[j] = m3d89_read_u(&p, m->ti_s);
            }
            if(rec & 2U) {
                if(!m3d89_has(p, end, m->vi_s)) return M3D89_ERR_TRUNCATED;
                n[j] = m3d89_read_u(&p, m->vi_s);
            }
            if(rec & 4U) {
                if(!m3d89_has(p, end, m->vi_s)) return M3D89_ERR_TRUNCATED;
                (void)m3d89_read_u(&p, m->vi_s);
            }
        }
        if(np < 3) {
            m->warnings++;
            continue;
        }
        for(j = 1; j + 1 < np; j++) {
            tri_v[0] = v[0]; tri_v[1] = v[j]; tri_v[2] = v[j + 1];
            tri_t[0] = t[0]; tri_t[1] = t[j]; tri_t[2] = t[j + 1];
            tri_n[0] = n[0]; tri_n[1] = n[j]; tri_n[2] = n[j + 1];
            r = m3d89_push_face(m, tri_v, tri_t, tri_n, cur_mat, (m3d89_u16)(flags | (np > 3 ? M3D89_FACE_FROM_NGON : 0)));
            if(r != M3D89_OK) return r;
        }
    }
    return M3D89_OK;
}

static m3d89_u8 m3d89_prop_format(m3d89_u8 type)
{
    if(type == 0 || type == 1 || type == 2 || type == 4 || type == 5) return 1;
    if(type == 3 || type == 6 || type == 7 || (type >= 64 && type <= 68)) return 2;
    if(type == 8) return 3;
    if(type >= 128) return 4;
    return 0;
}

static int m3d89_parse_mtrl(m3d89_model *m, const m3d89_u8 *ptr, const m3d89_u8 *end)
{
    const m3d89_u8 *p;
    m3d89_material *mat;
    m3d89_property *prop;
    m3d89_u8 fmt;
    if(m->material_count >= M3D89_MAX_MATERIALS) return M3D89_ERR_CAPACITY;
    p = ptr;
    if(!m3d89_has(p, end, m->si_s)) return M3D89_ERR_TRUNCATED;
    mat = &m->materials[m->material_count++];
    mat->name = m3d89_read_u(&p, m->si_s);
    mat->first_property = m->property_count;
    mat->property_count = 0;
    while(p < end) {
        if(m->property_count >= M3D89_MAX_PROPERTIES) return M3D89_ERR_CAPACITY;
        prop = &m->properties[m->property_count];
        prop->type = *p++;
        prop->format = 0;
        prop->reserved = 0;
        prop->raw = 0;
        prop->q = 0;
        fmt = m3d89_prop_format(prop->type);
        prop->format = fmt;
        if(fmt == 1) {
            if(m->ci_s == 0 || !m3d89_has(p, end, m->ci_s)) return M3D89_ERR_TRUNCATED;
            prop->raw = m3d89_read_color(m, &p);
        } else if(fmt == 2) {
            if(!m3d89_has(p, end, 4)) return M3D89_ERR_TRUNCATED;
            prop->raw = m3d89_get32(p);
            prop->q = m3d89_bin32_to_q16(prop->raw);
            p += 4;
        } else if(fmt == 3) {
            if(!m3d89_has(p, end, 1)) return M3D89_ERR_TRUNCATED;
            prop->raw = *p++;
        } else if(fmt == 4) {
            if(!m3d89_has(p, end, m->si_s)) return M3D89_ERR_TRUNCATED;
            prop->raw = m3d89_read_u(&p, m->si_s);
        } else {
            m->warnings++;
            break;
        }
        m->property_count++;
        mat->property_count++;
    }
    return M3D89_OK;
}

static int m3d89_parse_bone(m3d89_model *m, const m3d89_u8 *ptr, const m3d89_u8 *end)
{
    const m3d89_u8 *p;
    m3d89_u32 nbone;
    m3d89_u32 nskin;
    m3d89_u32 i;
    m3d89_u32 j;
    m3d89_u8 weights[M3D89_MAX_INFLUENCES];
    m3d89_u32 nonzero;
    m3d89_skin *s;
    if(m->bi_s == 0 || m->sk_s == 0) return M3D89_ERR_UNSUPPORTED_TYPE;
    p = ptr;
    if(!m3d89_has(p, end, (m3d89_u32)m->bi_s + (m3d89_u32)m->sk_s)) return M3D89_ERR_TRUNCATED;
    nbone = m3d89_read_u(&p, m->bi_s);
    nskin = m3d89_read_u(&p, m->sk_s);
    if(nbone > M3D89_MAX_BONES || nskin > M3D89_MAX_SKINS) return M3D89_ERR_CAPACITY;
    for(i = 0; i < nbone; i++) {
        if(!m3d89_has(p, end, (m3d89_u32)m->bi_s + (m3d89_u32)m->si_s + 2UL * (m3d89_u32)m->vi_s)) return M3D89_ERR_TRUNCATED;
        m->bones[m->bone_count].parent = m3d89_read_u(&p, m->bi_s);
        m->bones[m->bone_count].name = m3d89_read_u(&p, m->si_s);
        m->bones[m->bone_count].position_vertex = m3d89_read_u(&p, m->vi_s);
        m->bones[m->bone_count].orientation_vertex = m3d89_read_u(&p, m->vi_s);
        m->bone_count++;
    }
    for(i = 0; i < nskin; i++) {
        s = &m->skins[m->skin_count];
        s->count = m->nb_n;
        for(j = 0; j < M3D89_MAX_INFLUENCES; j++) {
            s->bone[j] = M3D89_UNDEF_U32;
            s->weight[j] = 0;
            weights[j] = 0;
        }
        if(m->nb_n == 1) {
            weights[0] = 255;
            nonzero = 1;
        } else {
            if(!m3d89_has(p, end, m->nb_n)) return M3D89_ERR_TRUNCATED;
            nonzero = 0;
            for(j = 0; j < m->nb_n; j++) {
                weights[j] = *p++;
                if(weights[j] != 0) nonzero++;
            }
        }
        if(!m3d89_has(p, end, nonzero * (m3d89_u32)m->bi_s)) return M3D89_ERR_TRUNCATED;
        for(j = 0; j < m->nb_n; j++) {
            if(weights[j] != 0) s->bone[j] = m3d89_read_u(&p, m->bi_s);
            s->weight[j] = (m3d89_q16)(((m3d89_u32)weights[j] * 65536UL) / 255UL);
        }
        m->skin_count++;
    }
    if(p != end) m->warnings++;
    return M3D89_OK;
}

static int m3d89_parse_actn(m3d89_model *m, const m3d89_u8 *ptr, const m3d89_u8 *end)
{
    const m3d89_u8 *p;
    m3d89_action *a;
    m3d89_frame *f;
    m3d89_u32 nframes;
    m3d89_u32 ntr;
    m3d89_u32 i;
    m3d89_u32 j;
    if(m->action_count >= M3D89_MAX_ACTIONS) return M3D89_ERR_CAPACITY;
    if(m->fc_s == 0 || m->bi_s == 0) return M3D89_ERR_UNSUPPORTED_TYPE;
    p = ptr;
    if(!m3d89_has(p, end, (m3d89_u32)m->si_s + 2UL + 4UL)) return M3D89_ERR_TRUNCATED;
    a = &m->actions[m->action_count++];
    a->name = m3d89_read_u(&p, m->si_s);
    nframes = m3d89_get16(p); p += 2;
    a->duration_msec = m3d89_get32(p); p += 4;
    if(m->frame_count + nframes > M3D89_MAX_FRAMES) return M3D89_ERR_CAPACITY;
    a->first_frame = m->frame_count;
    a->frame_count = (m3d89_u16)nframes;
    for(i = 0; i < nframes; i++) {
        if(!m3d89_has(p, end, 4UL + (m3d89_u32)m->fc_s)) return M3D89_ERR_TRUNCATED;
        f = &m->frames[m->frame_count++];
        f->msec = m3d89_get32(p); p += 4;
        ntr = m3d89_read_u(&p, m->fc_s);
        if(ntr > 65535UL) return M3D89_ERR_CAPACITY;
        if(m->transform_count + ntr > M3D89_MAX_TRANSFORMS) return M3D89_ERR_CAPACITY;
        f->first_transform = m->transform_count;
        f->transform_count = (m3d89_u16)ntr;
        for(j = 0; j < ntr; j++) {
            if(!m3d89_has(p, end, (m3d89_u32)m->bi_s + 2UL * (m3d89_u32)m->vi_s)) return M3D89_ERR_TRUNCATED;
            m->transforms[m->transform_count].bone = m3d89_read_u(&p, m->bi_s);
            m->transforms[m->transform_count].position_vertex = m3d89_read_u(&p, m->vi_s);
            m->transforms[m->transform_count].orientation_vertex = m3d89_read_u(&p, m->vi_s);
            m->transform_count++;
        }
    }
    if(p != end) m->warnings++;
    return M3D89_OK;
}

static int m3d89_parse_voxt(m3d89_model *m, const m3d89_u8 *ptr, const m3d89_u8 *end)
{
    const m3d89_u8 *p;
    m3d89_vox_type *vt;
    m3d89_u32 items;
    m3d89_u32 i;
    p = ptr;
    while(p < end) {
        if(m->vox_type_count >= M3D89_MAX_VOX_TYPES) return M3D89_ERR_CAPACITY;
        if(!m3d89_has(p, end, (m3d89_u32)m->ci_s + (m3d89_u32)m->si_s + 3UL + (m->sk_s ? (m3d89_u32)m->sk_s : 0UL))) return M3D89_ERR_TRUNCATED;
        vt = &m->vox_types[m->vox_type_count++];
        vt->rgba = m3d89_read_color(m, &p);
        vt->name = m3d89_read_u(&p, m->si_s);
        vt->rotation = *p++;
        vt->shape = (m3d89_u16)(*p++);
        if(vt->rotation & 64U) vt->shape = (m3d89_u16)(vt->shape | 256U);
        vt->item_count = *p++;
        if(m->sk_s) vt->skin = m3d89_read_u(&p, m->sk_s); else vt->skin = M3D89_UNDEF_U32;
        vt->material_name = vt->name;
        items = vt->item_count;
        for(i = 0; i < items; i++) {
            if(!m3d89_has(p, end, 2UL + (m3d89_u32)m->si_s)) return M3D89_ERR_TRUNCATED;
            p += 2;
            (void)m3d89_read_u(&p, m->si_s);
        }
    }
    return M3D89_OK;
}

static m3d89_u32 m3d89_read_voxel_value(const m3d89_u8 **pp, m3d89_u8 s)
{
    if(s == 2) return m3d89_read_u(pp, 2);
    return m3d89_read_u(pp, 1);
}

static int m3d89_parse_voxd(m3d89_model *m, const m3d89_u8 *base, const m3d89_u8 *ptr, const m3d89_u8 *end)
{
    const m3d89_u8 *p;
    m3d89_vox_block *b;
    m3d89_u32 total;
    m3d89_u32 count;
    m3d89_u32 value;
    m3d89_u8 code;
    if(m->vox_block_count >= M3D89_MAX_VOX_BLOCKS) return M3D89_ERR_CAPACITY;
    if(m->vd_s == 0) return M3D89_ERR_UNSUPPORTED_TYPE;
    p = ptr;
    if(!m3d89_has(p, end, (m3d89_u32)m->si_s + 6UL * (m3d89_u32)m->vd_s + 2UL)) return M3D89_ERR_TRUNCATED;
    b = &m->vox_blocks[m->vox_block_count++];
    b->name = m3d89_read_u(&p, m->si_s);
    b->x = m3d89_read_i(&p, m->vd_s);
    b->y = m3d89_read_i(&p, m->vd_s);
    b->z = m3d89_read_i(&p, m->vd_s);
    b->w = (m3d89_u32)m3d89_read_i(&p, m->vd_s);
    b->h = (m3d89_u32)m3d89_read_i(&p, m->vd_s);
    b->d = (m3d89_u32)m3d89_read_i(&p, m->vd_s);
    b->uncertainty = *p++;
    b->group = *p++;
    b->reserved = 0;
    b->data_offset = (m3d89_u32)(p - base);
    b->data_length = (m3d89_u32)(end - p);
    b->encoded_runs = 0;
    b->decoded_voxels = 0;
    b->solid_voxels = 0;
    total = b->w * b->h * b->d;
    while(p < end && b->decoded_voxels < total) {
        code = *p++;
        count = (m3d89_u32)(code & 127U) + 1UL;
        if(code & 128U) {
            if(!m3d89_has(p, end, m->vp_s)) return M3D89_ERR_TRUNCATED;
            value = m3d89_read_voxel_value(&p, m->vp_s);
            if(value != ((m->vp_s == 1) ? 255UL : 65535UL) && value != ((m->vp_s == 1) ? 254UL : 65534UL)) b->solid_voxels += count;
        } else {
            while(count > 0UL) {
                if(!m3d89_has(p, end, m->vp_s)) return M3D89_ERR_TRUNCATED;
                value = m3d89_read_voxel_value(&p, m->vp_s);
                if(value != ((m->vp_s == 1) ? 255UL : 65535UL) && value != ((m->vp_s == 1) ? 254UL : 65534UL)) b->solid_voxels++;
                count--;
            }
        }
        b->encoded_runs++;
        b->decoded_voxels += ((m3d89_u32)(code & 127U) + 1UL);
    }
    if(p != end) m->warnings++;
    return M3D89_OK;
}

static int m3d89_parse_lbls(m3d89_model *m, const m3d89_u8 *ptr, const m3d89_u8 *end)
{
    const m3d89_u8 *p;
    m3d89_u32 group;
    m3d89_u32 lang;
    m3d89_u32 rgba;
    p = ptr;
    if(!m3d89_has(p, end, 2UL * (m3d89_u32)m->si_s + (m->ci_s ? (m3d89_u32)m->ci_s : 0UL))) return M3D89_ERR_TRUNCATED;
    group = m3d89_read_u(&p, m->si_s);
    lang = m3d89_read_u(&p, m->si_s);
    if(m->ci_s) rgba = m3d89_read_color(m, &p); else rgba = m3d89_default_color();
    while(p < end) {
        if(m->label_count >= M3D89_MAX_LABELS) return M3D89_ERR_CAPACITY;
        if(!m3d89_has(p, end, (m3d89_u32)m->vi_s + (m3d89_u32)m->si_s)) return M3D89_ERR_TRUNCATED;
        m->labels[m->label_count].group_name = group;
        m->labels[m->label_count].lang = lang;
        m->labels[m->label_count].rgba = rgba;
        m->labels[m->label_count].vertex = m3d89_read_u(&p, m->vi_s);
        m->labels[m->label_count].text = m3d89_read_u(&p, m->si_s);
        m->label_count++;
    }
    return M3D89_OK;
}

static int m3d89_parse_aset(m3d89_model *m, const m3d89_u8 *base, const m3d89_u8 *ptr, const m3d89_u8 *end)
{
    if(m->asset_count >= M3D89_MAX_ASSETS) return M3D89_ERR_CAPACITY;
    if(!m3d89_has(ptr, end, m->si_s)) return M3D89_ERR_TRUNCATED;
    m->assets[m->asset_count].name = m3d89_read_u(&ptr, m->si_s);
    m->assets[m->asset_count].offset = (m3d89_u32)(ptr - base);
    m->assets[m->asset_count].length = (m3d89_u32)(end - ptr);
    m->asset_count++;
    return M3D89_OK;
}

static int m3d89_parse_proc(m3d89_model *m, const m3d89_u8 *base, const m3d89_u8 *ptr, const m3d89_u8 *end)
{
    if(m->proc_count >= M3D89_MAX_PROCS) return M3D89_ERR_CAPACITY;
    if(!m3d89_has(ptr, end, m->si_s)) return M3D89_ERR_TRUNCATED;
    m->procs[m->proc_count].script_name = m3d89_read_u(&ptr, m->si_s);
    m->procs[m->proc_count].offset = (m3d89_u32)(ptr - base);
    m->procs[m->proc_count].length = (m3d89_u32)(end - ptr);
    m->proc_count++;
    return M3D89_OK;
}

static int m3d89_parse_shpe(m3d89_model *m, const m3d89_u8 *base, const m3d89_u8 *ptr, const m3d89_u8 *end)
{
    if(m->shape_count >= M3D89_MAX_SHAPES) return M3D89_ERR_CAPACITY;
    if(m->bi_s == 0) return M3D89_ERR_UNSUPPORTED_TYPE;
    if(!m3d89_has(ptr, end, (m3d89_u32)m->si_s + (m3d89_u32)m->bi_s)) return M3D89_ERR_TRUNCATED;
    m->shapes[m->shape_count].name = m3d89_read_u(&ptr, m->si_s);
    m->shapes[m->shape_count].group = m3d89_read_u(&ptr, m->bi_s);
    m->shapes[m->shape_count].offset = (m3d89_u32)(ptr - base);
    m->shapes[m->shape_count].length = (m3d89_u32)(end - ptr);
    m->shape_count++;
    return M3D89_OK;
}

void m3d89_zero_model(m3d89_model *m)
{
    if(m) memset(m, 0, sizeof(*m));
}

int m3d89_decode(m3d89_model *m, const void *data, m3d89_u32 size)
{
    const m3d89_u8 *p;
    const m3d89_u8 *end;
    m3d89_u32 declared;
    m3d89_u32 len;
    if(m == 0 || data == 0) return M3D89_ERR_BAD_ARG;
    m3d89_zero_model(m);
    if(size < 16UL) { m->error = M3D89_ERR_TOO_SMALL; return m->error; }
    p = (const m3d89_u8 *)data;
    end = p + size;
    m->source = p;
    m->source_size = size;
    if(!m3d89_magic(p, "3DMO")) { m->error = M3D89_ERR_BAD_MAGIC; return m->error; }
    declared = m3d89_get32(p + 4);
    m->file_size = declared;
    if(declared > size) m->warnings++;
    p += 8;
    if(m3d89_has(p, end, 8) && m3d89_magic(p, "PRVW")) {
        len = m3d89_get32(p + 4);
        if(len < 8UL || !m3d89_has(p, end, len)) { m->error = M3D89_ERR_TRUNCATED; return m->error; }
        p += len;
    }
    if(!m3d89_has(p, end, 4)) { m->error = M3D89_ERR_TRUNCATED; return m->error; }
    if(!m3d89_magic(p, "HEAD")) { m->error = M3D89_ERR_STREAM_PACKED; return m->error; }
    return m3d89_decode_payload(m, p, (m3d89_u32)(end - p), declared, (m3d89_u32)(p - (const m3d89_u8 *)data));
}

int m3d89_decode_payload(m3d89_model *m, const void *data, m3d89_u32 size, m3d89_u32 file_size, m3d89_u32 payload_offset)
{
    const m3d89_u8 *base;
    const m3d89_u8 *p;
    const m3d89_u8 *end;
    const m3d89_u8 *body;
    m3d89_u32 len;
    int r;
    if(m == 0 || data == 0) return M3D89_ERR_BAD_ARG;
    if(m->source == 0) {
        memset(m, 0, sizeof(*m));
        m->source = (const m3d89_u8 *)data;
        m->source_size = size;
    }
    base = (const m3d89_u8 *)data;
    p = base;
    end = base + size;
    m->payload_offset = payload_offset;
    m->payload_size = size;
    m->file_size = file_size;
    if(!m3d89_has(p, end, 16)) { m->error = M3D89_ERR_TRUNCATED; return m->error; }
    len = m3d89_get32(p + 4);
    if(len < 16UL || !m3d89_has(p, end, len)) { m->error = M3D89_ERR_BAD_HEADER; return m->error; }
    r = m3d89_parse_head(m, p, len);
    if(r != M3D89_OK) { m->error = r; return r; }
    m3d89_add_chunk(m, base, p, len);
    p += len;
    while(m3d89_has(p, end, 4)) {
        if(m3d89_magic(p, "OMD3")) {
            m3d89_add_chunk(m, base, p, 4UL);
            m->error = M3D89_OK;
            return M3D89_OK;
        }
        if(!m3d89_has(p, end, 8)) { m->error = M3D89_ERR_TRUNCATED; return m->error; }
        len = m3d89_get32(p + 4);
        if(len < 8UL || !m3d89_has(p, end, len)) { m->error = M3D89_ERR_TRUNCATED; return m->error; }
        m3d89_add_chunk(m, base, p, len);
        body = p + 8;
        if(m3d89_magic(p, "CMAP")) r = m3d89_parse_cmap(m, body, len - 8UL);
        else if(m3d89_magic(p, "TMAP")) r = m3d89_parse_tmap(m, body, p + len);
        else if(m3d89_magic(p, "VRTS")) r = m3d89_parse_vrts(m, body, p + len);
        else if(m3d89_magic(p, "MESH")) r = m3d89_parse_mesh(m, body, p + len);
        else if(m3d89_magic(p, "MTRL")) r = m3d89_parse_mtrl(m, body, p + len);
        else if(m3d89_magic(p, "BONE")) r = m3d89_parse_bone(m, body, p + len);
        else if(m3d89_magic(p, "ACTN")) r = m3d89_parse_actn(m, body, p + len);
        else if(m3d89_magic(p, "VOXT")) r = m3d89_parse_voxt(m, body, p + len);
        else if(m3d89_magic(p, "VOXD")) r = m3d89_parse_voxd(m, base, body, p + len);
        else if(m3d89_magic(p, "LBLS")) r = m3d89_parse_lbls(m, body, p + len);
        else if(m3d89_magic(p, "ASET")) r = m3d89_parse_aset(m, base, body, p + len);
        else if(m3d89_magic(p, "PROC")) r = m3d89_parse_proc(m, base, body, p + len);
        else if(m3d89_magic(p, "SHPE")) r = m3d89_parse_shpe(m, base, body, p + len);
        else r = M3D89_OK;
        if(r != M3D89_OK) { m->error = r; return r; }
        p += len;
    }
    m->warnings++;
    m->error = M3D89_OK;
    return M3D89_OK;
}

const char *m3d89_string(const m3d89_model *m, m3d89_u32 offset)
{
    if(m == 0 || m->strings == 0) return "";
    if(offset == M3D89_UNDEF_U32 || offset >= m->strings_size) return "";
    return (const char *)(m->strings + offset);
}

int m3d89_decode_voxel_block(const m3d89_model *m, m3d89_u16 block_index, m3d89_voxel_cb cb, void *user)
{
    const m3d89_vox_block *b;
    const m3d89_u8 *p;
    const m3d89_u8 *end;
    m3d89_u32 total;
    m3d89_u32 done;
    m3d89_u32 count;
    m3d89_u32 value;
    m3d89_u32 i;
    m3d89_u8 code;
    m3d89_i32 x;
    m3d89_i32 y;
    m3d89_i32 z;
    if(m == 0 || block_index >= m->vox_block_count) return M3D89_ERR_BAD_ARG;
    b = &m->vox_blocks[block_index];
    if(m->source == 0 || b->data_offset + b->data_length > m->source_size) return M3D89_ERR_RANGE;
    p = m->source + b->data_offset;
    end = p + b->data_length;
    total = b->w * b->h * b->d;
    done = 0;
    while(p < end && done < total) {
        code = *p++;
        count = (m3d89_u32)(code & 127U) + 1UL;
        if(code & 128U) {
            if(!m3d89_has(p, end, m->vp_s)) return M3D89_ERR_TRUNCATED;
            value = m3d89_read_voxel_value(&p, m->vp_s);
            for(i = 0; i < count && done < total; i++) {
                y = b->y + (m3d89_i32)(done / (b->d * b->w));
                z = b->z + (m3d89_i32)((done / b->w) % b->d);
                x = b->x + (m3d89_i32)(done % b->w);
                if(cb) cb(user, x, y, z, value);
                done++;
            }
        } else {
            for(i = 0; i < count && done < total; i++) {
                if(!m3d89_has(p, end, m->vp_s)) return M3D89_ERR_TRUNCATED;
                value = m3d89_read_voxel_value(&p, m->vp_s);
                y = b->y + (m3d89_i32)(done / (b->d * b->w));
                z = b->z + (m3d89_i32)((done / b->w) % b->d);
                x = b->x + (m3d89_i32)(done % b->w);
                if(cb) cb(user, x, y, z, value);
                done++;
            }
        }
    }
    return M3D89_OK;
}

static m3d89_u32 m3d89_abs_i32(m3d89_i32 v)
{
    if(v < 0) return (m3d89_u32)(-v);
    return (m3d89_u32)v;
}

static m3d89_u32 m3d89_frac_mul_u16(m3d89_u32 a, m3d89_u32 b)
{
    m3d89_u32 ah;
    m3d89_u32 al;
    m3d89_u32 bh;
    m3d89_u32 bl;
    m3d89_u32 mid;
    ah = (a >> 8) & 255UL;
    al = a & 255UL;
    bh = (b >> 8) & 255UL;
    bl = b & 255UL;
    mid = ah * bl + al * bh;
    return ah * bh + (mid >> 8);
}

m3d89_q16 m3d89_mul_q16(m3d89_q16 a, m3d89_q16 b)
{
    int neg;
    m3d89_u32 ua;
    m3d89_u32 ub;
    m3d89_u32 ai;
    m3d89_u32 bi;
    m3d89_u32 af;
    m3d89_u32 bf;
    m3d89_u32 r;
    neg = 0;
    if(a < 0) { neg = !neg; ua = m3d89_abs_i32(a); } else ua = (m3d89_u32)a;
    if(b < 0) { neg = !neg; ub = m3d89_abs_i32(b); } else ub = (m3d89_u32)b;
    ai = ua >> 16;
    bi = ub >> 16;
    af = ua & 65535UL;
    bf = ub & 65535UL;
    if(ai > 32767UL || bi > 32767UL) r = 2147483647UL;
    else {
        r = (ai * bi) << 16;
        r += ai * bf;
        r += bi * af;
        r += m3d89_frac_mul_u16(af, bf);
    }
    if(r > 2147483647UL) r = 2147483647UL;
    return neg ? -(m3d89_q16)r : (m3d89_q16)r;
}

m3d89_q16 m3d89_lerp_q16(m3d89_q16 a, m3d89_q16 b, m3d89_q16 t)
{
    return (m3d89_q16)(a + m3d89_mul_q16((m3d89_q16)(b - a), t));
}

void m3d89_vec4_lerp_q16(m3d89_vertex *out, const m3d89_vertex *a, const m3d89_vertex *b, m3d89_q16 t)
{
    if(out == 0 || a == 0 || b == 0) return;
    out->x = m3d89_lerp_q16(a->x, b->x, t);
    out->y = m3d89_lerp_q16(a->y, b->y, t);
    out->z = m3d89_lerp_q16(a->z, b->z, t);
    out->w = m3d89_lerp_q16(a->w, b->w, t);
    out->rgba = a->rgba;
    out->skin = a->skin;
}

static m3d89_q16 m3d89_div_q16(m3d89_q16 a, m3d89_q16 b)
{
    int neg;
    m3d89_u32 ua;
    m3d89_u32 ub;
    m3d89_u32 hi;
    m3d89_u32 lo;
    m3d89_u32 rem;
    m3d89_u32 q;
    int bit;
    m3d89_u32 next;
    if(b == 0) return 0;
    neg = 0;
    if(a < 0) { neg = !neg; ua = m3d89_abs_i32(a); } else ua = (m3d89_u32)a;
    if(b < 0) { neg = !neg; ub = m3d89_abs_i32(b); } else ub = (m3d89_u32)b;
    hi = ua >> 16;
    lo = (ua & 65535UL) << 16;
    rem = 0;
    q = 0;
    for(bit = 32; bit >= 0; bit--) {
        if(bit == 32) next = hi & 1UL;
        else next = (lo >> bit) & 1UL;
        rem = (rem << 1) | next;
        if(rem >= ub) {
            rem -= ub;
            if(bit < 32) q |= (1UL << bit);
        }
    }
    if(q > 2147483647UL) q = 2147483647UL;
    return neg ? -(m3d89_q16)q : (m3d89_q16)q;
}

static m3d89_q16 m3d89_sqrt_q16(m3d89_q16 x)
{
    m3d89_q16 lo;
    m3d89_q16 hi;
    m3d89_q16 mid;
    m3d89_q16 mm;
    int i;
    if(x <= 0) return 0;
    lo = 0;
    hi = x > M3D89_Q16_ONE ? x : M3D89_Q16_ONE;
    for(i = 0; i < 24; i++) {
        mid = (m3d89_q16)((lo + hi + 1) / 2);
        mm = m3d89_mul_q16(mid, mid);
        if(mm <= x) lo = mid; else hi = (m3d89_q16)(mid - 1);
    }
    return lo;
}

void m3d89_quat_nlerp_q16(m3d89_vertex *out, const m3d89_vertex *a, const m3d89_vertex *b, m3d89_q16 t)
{
    m3d89_vertex bb;
    m3d89_q16 dot;
    m3d89_q16 norm;
    if(out == 0 || a == 0 || b == 0) return;
    bb = *b;
    dot = m3d89_mul_q16(a->x, b->x) + m3d89_mul_q16(a->y, b->y) + m3d89_mul_q16(a->z, b->z) + m3d89_mul_q16(a->w, b->w);
    if(dot < 0) { bb.x = -bb.x; bb.y = -bb.y; bb.z = -bb.z; bb.w = -bb.w; }
    m3d89_vec4_lerp_q16(out, a, &bb, t);
    norm = m3d89_sqrt_q16(m3d89_mul_q16(out->x, out->x) + m3d89_mul_q16(out->y, out->y) + m3d89_mul_q16(out->z, out->z) + m3d89_mul_q16(out->w, out->w));
    if(norm > 0) {
        out->x = m3d89_div_q16(out->x, norm);
        out->y = m3d89_div_q16(out->y, norm);
        out->z = m3d89_div_q16(out->z, norm);
        out->w = m3d89_div_q16(out->w, norm);
    }
}

int m3d89_sample_action(const m3d89_model *m, m3d89_u16 action_index, m3d89_u32 msec, m3d89_vertex *out_pos, m3d89_vertex *out_ori, m3d89_u16 out_bones)
{
    const m3d89_action *a;
    const m3d89_frame *f;
    const m3d89_transform *tr;
    m3d89_u32 i;
    m3d89_u32 j;
    m3d89_u32 b;
    if(m == 0 || action_index >= m->action_count || out_pos == 0 || out_ori == 0) return M3D89_ERR_BAD_ARG;
    if(out_bones < m->bone_count) return M3D89_ERR_CAPACITY;
    for(i = 0; i < m->bone_count; i++) {
        if(m->bones[i].position_vertex < m->vertex_count) out_pos[i] = m->vertices[m->bones[i].position_vertex];
        else memset(&out_pos[i], 0, sizeof(out_pos[i]));
        if(m->bones[i].orientation_vertex < m->vertex_count) out_ori[i] = m->vertices[m->bones[i].orientation_vertex];
        else memset(&out_ori[i], 0, sizeof(out_ori[i]));
    }
    a = &m->actions[action_index];
    for(i = 0; i < a->frame_count; i++) {
        f = &m->frames[a->first_frame + i];
        if(f->msec > msec) break;
        for(j = 0; j < f->transform_count; j++) {
            tr = &m->transforms[f->first_transform + j];
            b = tr->bone;
            if(b < m->bone_count) {
                if(tr->position_vertex < m->vertex_count) out_pos[b] = m->vertices[tr->position_vertex];
                if(tr->orientation_vertex < m->vertex_count) out_ori[b] = m->vertices[tr->orientation_vertex];
            }
        }
    }
    return M3D89_OK;
}
