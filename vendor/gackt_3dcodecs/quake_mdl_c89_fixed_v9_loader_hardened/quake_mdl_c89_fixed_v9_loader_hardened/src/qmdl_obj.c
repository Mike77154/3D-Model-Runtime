#include "qmdl_internal.h"

#define QMDL_OBJ_NO_INDEX (-1)

typedef struct qmdl_obj_writer_s {
    char *dst;
    size_t cap;
    size_t len;
} qmdl_obj_writer;

typedef struct qmdl_obj_token_s {
    const char *start;
    const char *end;
} qmdl_obj_token;

static int qmdl_obj_is_space(char c)
{
    return (c == ' ' || c == '\t' || c == '\r') ? 1 : 0;
}

static int qmdl_obj_is_digit(char c)
{
    return (c >= '0' && c <= '9') ? 1 : 0;
}

static const char *qmdl_obj_skip_space(const char *p, const char *end)
{
    while (p < end && qmdl_obj_is_space(*p))
    {
        ++p;
    }
    return p;
}

static int qmdl_obj_next_token(const char **pp, const char *end, qmdl_obj_token *tok)
{
    const char *p;

    if (pp == 0 || tok == 0)
    {
        return 0;
    }

    p = qmdl_obj_skip_space(*pp, end);
    if (p >= end || *p == '#')
    {
        *pp = end;
        return 0;
    }

    tok->start = p;
    while (p < end && !qmdl_obj_is_space(*p))
    {
        ++p;
    }
    tok->end = p;
    *pp = p;
    return 1;
}

static int qmdl_obj_token_equals(const qmdl_obj_token *tok, const char *lit)
{
    const char *p;
    const char *q;

    if (tok == 0 || lit == 0)
    {
        return 0;
    }

    p = tok->start;
    q = lit;
    while (p < tok->end && *q != '\0')
    {
        if (*p != *q)
        {
            return 0;
        }
        ++p;
        ++q;
    }
    return (p == tok->end && *q == '\0') ? 1 : 0;
}

static int qmdl_obj_parse_i32(const qmdl_obj_token *tok, qmdl_i32 *out_value)
{
    const char *p;
    int sign;
    qmdl_u32 value;
    int have_digit;

    if (tok == 0 || out_value == 0)
    {
        return QMDL_ERR_ARGS;
    }

    p = tok->start;
    sign = 1;
    value = 0u;
    have_digit = 0;

    if (p < tok->end && (*p == '+' || *p == '-'))
    {
        if (*p == '-')
        {
            sign = -1;
        }
        ++p;
    }

    while (p < tok->end && qmdl_obj_is_digit(*p))
    {
        qmdl_u32 digit;
        have_digit = 1;
        digit = (qmdl_u32)(*p - '0');
        if (value > 214748364u || (value == 214748364u && digit > 8u))
        {
            return QMDL_ERR_OVERFLOW;
        }
        value = value * 10u + digit;
        ++p;
    }

    if (!have_digit || p != tok->end)
    {
        return QMDL_ERR_FORMAT;
    }

    if (sign < 0)
    {
        if (value == 2147483648u)
        {
            *out_value = QMDL_I32_MIN;
        }
        else
        {
            *out_value = -(qmdl_i32)value;
        }
    }
    else
    {
        if (value > 2147483647u)
        {
            return QMDL_ERR_OVERFLOW;
        }
        *out_value = (qmdl_i32)value;
    }

    return QMDL_OK;
}

static int qmdl_obj_parse_u32(const qmdl_obj_token *tok, qmdl_u32 *out_value)
{
    qmdl_i32 signed_value;
    int rc;

    if (tok == 0 || out_value == 0)
    {
        return QMDL_ERR_ARGS;
    }

    rc = qmdl_obj_parse_i32(tok, &signed_value);
    if (rc != QMDL_OK)
    {
        return rc;
    }
    if (signed_value < 0)
    {
        return QMDL_ERR_RANGE;
    }
    *out_value = (qmdl_u32)signed_value;
    return QMDL_OK;
}

static int qmdl_obj_parse_fx16(const qmdl_obj_token *tok, qmdl_fx16 *out_value)
{
    const char *p;
    int sign;
    qmdl_u32 whole;
    qmdl_u32 frac_num;
    qmdl_u32 frac_den;
    int have_digit;
    qmdl_u32 frac;
    qmdl_u32 magnitude;

    if (tok == 0 || out_value == 0)
    {
        return QMDL_ERR_ARGS;
    }

    p = tok->start;
    sign = 1;
    whole = 0u;
    frac_num = 0u;
    frac_den = 1u;
    have_digit = 0;

    if (p < tok->end && (*p == '+' || *p == '-'))
    {
        if (*p == '-')
        {
            sign = -1;
        }
        ++p;
    }

    while (p < tok->end && qmdl_obj_is_digit(*p))
    {
        qmdl_u32 digit;
        have_digit = 1;
        digit = (qmdl_u32)(*p - '0');
        if (whole > 32767u || (whole == 32767u && digit > 0u))
        {
            return QMDL_ERR_RANGE;
        }
        whole = whole * 10u + digit;
        ++p;
    }

    if (p < tok->end && *p == '.')
    {
        int frac_digits;
        ++p;
        frac_digits = 0;
        while (p < tok->end && qmdl_obj_is_digit(*p))
        {
            qmdl_u32 digit;
            have_digit = 1;
            digit = (qmdl_u32)(*p - '0');
            if (frac_digits < 6)
            {
                frac_num = frac_num * 10u + digit;
                frac_den *= 10u;
                ++frac_digits;
            }
            ++p;
        }
    }

    if (!have_digit || p != tok->end)
    {
        return QMDL_ERR_FORMAT;
    }

    frac = (qmdl_u32)(((unsigned long)frac_num * 65536ul + (unsigned long)(frac_den / 2u)) /
                      (unsigned long)frac_den);
    if (frac >= 65536u)
    {
        frac -= 65536u;
        whole += 1u;
        if (whole > 32767u)
        {
            return QMDL_ERR_RANGE;
        }
    }

    magnitude = (whole << 16) + frac;
    if (sign < 0)
    {
        if (magnitude == 0x80000000u)
        {
            *out_value = QMDL_I32_MIN;
        }
        else
        {
            *out_value = -(qmdl_i32)magnitude;
        }
    }
    else
    {
        if (magnitude > 0x7FFFFFFFu)
        {
            return QMDL_ERR_RANGE;
        }
        *out_value = (qmdl_i32)magnitude;
    }

    return QMDL_OK;
}

static int qmdl_obj_resolve_index(qmdl_i32 raw, qmdl_u32 count, qmdl_i32 *out_index)
{
    qmdl_i32 idx;

    if (out_index == 0)
    {
        return QMDL_ERR_ARGS;
    }
    if (raw == 0)
    {
        return QMDL_ERR_FORMAT;
    }

    if (raw > 0)
    {
        idx = raw - 1;
    }
    else
    {
        idx = (qmdl_i32)count + raw;
    }

    if (idx < 0 || (qmdl_u32)idx >= count)
    {
        return QMDL_ERR_RANGE;
    }

    *out_index = idx;
    return QMDL_OK;
}

static int qmdl_obj_parse_face_ref(const qmdl_obj_token *tok,
                                   qmdl_i32 *out_v,
                                   qmdl_i32 *out_vt)
{
    const char *slash1;
    const char *slash2;
    qmdl_obj_token part;
    qmdl_i32 value;
    int rc;

    if (tok == 0 || out_v == 0 || out_vt == 0)
    {
        return QMDL_ERR_ARGS;
    }

    slash1 = tok->start;
    while (slash1 < tok->end && *slash1 != '/')
    {
        ++slash1;
    }

    part.start = tok->start;
    part.end = slash1;
    rc = qmdl_obj_parse_i32(&part, out_v);
    if (rc != QMDL_OK)
    {
        return rc;
    }

    *out_vt = 0;
    if (slash1 >= tok->end)
    {
        return QMDL_OK;
    }

    slash2 = slash1 + 1;
    while (slash2 < tok->end && *slash2 != '/')
    {
        ++slash2;
    }

    if (slash2 > slash1 + 1)
    {
        part.start = slash1 + 1;
        part.end = slash2;
        rc = qmdl_obj_parse_i32(&part, &value);
        if (rc != QMDL_OK)
        {
            return rc;
        }
        *out_vt = value;
    }

    return QMDL_OK;
}

static int qmdl_obj_writer_bytes(qmdl_obj_writer *wr, const char *src, size_t count)
{
    if (wr == 0 || (src == 0 && count != 0u))
    {
        return QMDL_ERR_ARGS;
    }
    if (((size_t)-1) - wr->len < count)
    {
        return QMDL_ERR_OVERFLOW;
    }
    if (wr->dst != 0)
    {
        if (count > wr->cap - wr->len)
        {
            return QMDL_ERR_SIZE;
        }
        memcpy(wr->dst + wr->len, src, count);
    }
    wr->len += count;
    return QMDL_OK;
}

static int qmdl_obj_writer_char(qmdl_obj_writer *wr, char c)
{
    return qmdl_obj_writer_bytes(wr, &c, 1u);
}

static int qmdl_obj_writer_cstr(qmdl_obj_writer *wr, const char *s)
{
    size_t n;

    if (s == 0)
    {
        return QMDL_ERR_ARGS;
    }
    n = strlen(s);
    return qmdl_obj_writer_bytes(wr, s, n);
}

static int qmdl_obj_writer_u32(qmdl_obj_writer *wr, qmdl_u32 value)
{
    char tmp[16];
    size_t n;

    n = 0u;
    do
    {
        tmp[n++] = (char)('0' + (value % 10u));
        value /= 10u;
    } while (value != 0u);

    while (n > 0u)
    {
        --n;
        if (qmdl_obj_writer_char(wr, tmp[n]) != QMDL_OK)
        {
            return QMDL_ERR_SIZE;
        }
    }
    return QMDL_OK;
}

static int qmdl_obj_writer_i32(qmdl_obj_writer *wr, qmdl_i32 value)
{
    qmdl_u32 mag;
    int rc;

    if (value < 0)
    {
        rc = qmdl_obj_writer_char(wr, '-');
        if (rc != QMDL_OK)
        {
            return rc;
        }
        if (value == QMDL_I32_MIN)
        {
            mag = 0x80000000u;
        }
        else
        {
            mag = (qmdl_u32)(-value);
        }
    }
    else
    {
        mag = (qmdl_u32)value;
    }
    return qmdl_obj_writer_u32(wr, mag);
}

static int qmdl_obj_writer_fx16(qmdl_obj_writer *wr, qmdl_fx16 value)
{
    qmdl_u32 mag;
    qmdl_u32 whole;
    qmdl_u32 frac;
    char digits[6];
    int i;
    int rc;

    if (value < 0)
    {
        rc = qmdl_obj_writer_char(wr, '-');
        if (rc != QMDL_OK)
        {
            return rc;
        }
        if (value == QMDL_I32_MIN)
        {
            mag = 0x80000000u;
        }
        else
        {
            mag = (qmdl_u32)(-value);
        }
    }
    else
    {
        mag = (qmdl_u32)value;
    }

    whole = mag >> 16;
    frac = (qmdl_u32)(((unsigned long)(mag & 0xFFFFu) * 1000000ul + 32768ul) / 65536ul);
    if (frac >= 1000000u)
    {
        frac -= 1000000u;
        ++whole;
    }

    rc = qmdl_obj_writer_u32(wr, whole);
    if (rc != QMDL_OK)
    {
        return rc;
    }
    rc = qmdl_obj_writer_char(wr, '.');
    if (rc != QMDL_OK)
    {
        return rc;
    }

    for (i = 5; i >= 0; --i)
    {
        digits[i] = (char)('0' + (frac % 10u));
        frac /= 10u;
    }
    return qmdl_obj_writer_bytes(wr, digits, 6u);
}

static int qmdl_obj_writer_name16(qmdl_obj_writer *wr, const char name[16])
{
    size_t n;

    n = 0u;
    while (n < 16u && name[n] != '\0')
    {
        ++n;
    }
    return qmdl_obj_writer_bytes(wr, name, n);
}

static int qmdl_obj_write_metadata(qmdl_obj_writer *wr,
                                   const qmdl_model *model,
                                   qmdl_u32 subframe_index)
{
    const qmdl_subframe *subframe;
    qmdl_u32 i;
    int rc;

    subframe = &model->subframes[subframe_index];

    rc = qmdl_obj_writer_cstr(wr, "# qmdl_obj 1\n# qmdl_skin ");
    if (rc != QMDL_OK) return rc;
    rc = qmdl_obj_writer_u32(wr, model->header.skinwidth);
    if (rc != QMDL_OK) return rc;
    rc = qmdl_obj_writer_char(wr, ' ');
    if (rc != QMDL_OK) return rc;
    rc = qmdl_obj_writer_u32(wr, model->header.skinheight);
    if (rc != QMDL_OK) return rc;
    rc = qmdl_obj_writer_cstr(wr, "\n# qmdl_frame_name ");
    if (rc != QMDL_OK) return rc;
    rc = qmdl_obj_writer_name16(wr, subframe->name);
    if (rc != QMDL_OK) return rc;
    rc = qmdl_obj_writer_char(wr, '\n');
    if (rc != QMDL_OK) return rc;

    for (i = 0u; i < model->header.numverts; ++i)
    {
        const qmdl_trivertx *packed;
        packed = &model->packed_verts[subframe->first_vert + i];
        rc = qmdl_obj_writer_cstr(wr, "# qmdl_normal ");
        if (rc != QMDL_OK) return rc;
        rc = qmdl_obj_writer_u32(wr, i);
        if (rc != QMDL_OK) return rc;
        rc = qmdl_obj_writer_char(wr, ' ');
        if (rc != QMDL_OK) return rc;
        rc = qmdl_obj_writer_u32(wr, (qmdl_u32)packed->lightnormalindex);
        if (rc != QMDL_OK) return rc;
        rc = qmdl_obj_writer_char(wr, '\n');
        if (rc != QMDL_OK) return rc;
    }

    for (i = 0u; i < model->header.numverts; ++i)
    {
        const qmdl_stvert *st;
        st = &model->stverts[i];
        rc = qmdl_obj_writer_cstr(wr, "# qmdl_st ");
        if (rc != QMDL_OK) return rc;
        rc = qmdl_obj_writer_u32(wr, i);
        if (rc != QMDL_OK) return rc;
        rc = qmdl_obj_writer_char(wr, ' ');
        if (rc != QMDL_OK) return rc;
        rc = qmdl_obj_writer_i32(wr, st->onseam);
        if (rc != QMDL_OK) return rc;
        rc = qmdl_obj_writer_char(wr, ' ');
        if (rc != QMDL_OK) return rc;
        rc = qmdl_obj_writer_i32(wr, st->s);
        if (rc != QMDL_OK) return rc;
        rc = qmdl_obj_writer_char(wr, ' ');
        if (rc != QMDL_OK) return rc;
        rc = qmdl_obj_writer_i32(wr, st->t);
        if (rc != QMDL_OK) return rc;
        rc = qmdl_obj_writer_char(wr, '\n');
        if (rc != QMDL_OK) return rc;
    }

    for (i = 0u; i < model->header.numtris; ++i)
    {
        const qmdl_triangle *tri;
        tri = &model->triangles[i];
        rc = qmdl_obj_writer_cstr(wr, "# qmdl_tri ");
        if (rc != QMDL_OK) return rc;
        rc = qmdl_obj_writer_u32(wr, i);
        if (rc != QMDL_OK) return rc;
        rc = qmdl_obj_writer_char(wr, ' ');
        if (rc != QMDL_OK) return rc;
        rc = qmdl_obj_writer_i32(wr, tri->facesfront);
        if (rc != QMDL_OK) return rc;
        rc = qmdl_obj_writer_char(wr, ' ');
        if (rc != QMDL_OK) return rc;
        rc = qmdl_obj_writer_i32(wr, tri->vertindex[0]);
        if (rc != QMDL_OK) return rc;
        rc = qmdl_obj_writer_char(wr, ' ');
        if (rc != QMDL_OK) return rc;
        rc = qmdl_obj_writer_i32(wr, tri->vertindex[1]);
        if (rc != QMDL_OK) return rc;
        rc = qmdl_obj_writer_char(wr, ' ');
        if (rc != QMDL_OK) return rc;
        rc = qmdl_obj_writer_i32(wr, tri->vertindex[2]);
        if (rc != QMDL_OK) return rc;
        rc = qmdl_obj_writer_char(wr, '\n');
        if (rc != QMDL_OK) return rc;
    }

    return QMDL_OK;
}

static int qmdl_obj_encode_internal(qmdl_obj_writer *wr,
                                    const qmdl_model *model,
                                    qmdl_u32 subframe_index,
                                    qmdl_u32 flags)
{
    const qmdl_subframe *subframe;
    qmdl_u32 i;
    qmdl_u32 vt_index;
    qmdl_fx16 xyz[3];
    int rc;

    if (wr == 0 || model == 0 || model->frames == 0 || model->subframes == 0 ||
        model->packed_verts == 0 || model->stverts == 0 || model->triangles == 0)
    {
        return QMDL_ERR_ARGS;
    }
    if (subframe_index >= model->num_subframes)
    {
        return QMDL_ERR_RANGE;
    }

    subframe = &model->subframes[subframe_index];
    if (model->header.numverts > model->num_packed_verts - subframe->first_vert)
    {
        return QMDL_ERR_RANGE;
    }

    if ((flags & QMDL_OBJ_EXPORT_METADATA) != 0u)
    {
        rc = qmdl_obj_write_metadata(wr, model, subframe_index);
        if (rc != QMDL_OK)
        {
            return rc;
        }
    }

    rc = qmdl_obj_writer_cstr(wr, "o ");
    if (rc != QMDL_OK) return rc;
    if (subframe->name[0] != '\0')
    {
        rc = qmdl_obj_writer_name16(wr, subframe->name);
        if (rc != QMDL_OK) return rc;
    }
    else
    {
        rc = qmdl_obj_writer_cstr(wr, "frame_");
        if (rc != QMDL_OK) return rc;
        rc = qmdl_obj_writer_u32(wr, subframe_index);
        if (rc != QMDL_OK) return rc;
    }
    rc = qmdl_obj_writer_char(wr, '\n');
    if (rc != QMDL_OK) return rc;

    for (i = 0u; i < model->header.numverts; ++i)
    {
        qmdl_decode_vertex(&model->header, &model->packed_verts[subframe->first_vert + i], xyz);
        rc = qmdl_obj_writer_cstr(wr, "v ");
        if (rc != QMDL_OK) return rc;
        rc = qmdl_obj_writer_fx16(wr, xyz[0]);
        if (rc != QMDL_OK) return rc;
        rc = qmdl_obj_writer_char(wr, ' ');
        if (rc != QMDL_OK) return rc;
        rc = qmdl_obj_writer_fx16(wr, xyz[1]);
        if (rc != QMDL_OK) return rc;
        rc = qmdl_obj_writer_char(wr, ' ');
        if (rc != QMDL_OK) return rc;
        rc = qmdl_obj_writer_fx16(wr, xyz[2]);
        if (rc != QMDL_OK) return rc;
        rc = qmdl_obj_writer_char(wr, '\n');
        if (rc != QMDL_OK) return rc;
    }

    for (i = 0u; i < model->header.numverts; ++i)
    {
        const qmdl_trivertx *packed;
        qmdl_fx16 nxyz[3];
        packed = &model->packed_verts[subframe->first_vert + i];
        if (!qmdl_vertex_normal_is_valid(packed->lightnormalindex))
        {
            qmdl_get_vertex_normal_fx16(0u, nxyz);
        }
        else
        {
            qmdl_get_vertex_normal_fx16(packed->lightnormalindex, nxyz);
        }
        rc = qmdl_obj_writer_cstr(wr, "vn ");
        if (rc != QMDL_OK) return rc;
        rc = qmdl_obj_writer_fx16(wr, nxyz[0]);
        if (rc != QMDL_OK) return rc;
        rc = qmdl_obj_writer_char(wr, ' ');
        if (rc != QMDL_OK) return rc;
        rc = qmdl_obj_writer_fx16(wr, nxyz[1]);
        if (rc != QMDL_OK) return rc;
        rc = qmdl_obj_writer_char(wr, ' ');
        if (rc != QMDL_OK) return rc;
        rc = qmdl_obj_writer_fx16(wr, nxyz[2]);
        if (rc != QMDL_OK) return rc;
        rc = qmdl_obj_writer_char(wr, '\n');
        if (rc != QMDL_OK) return rc;
    }

    for (i = 0u; i < model->header.numtris; ++i)
    {
        const qmdl_triangle *tri;
        int corner;
        tri = &model->triangles[i];
        for (corner = 0; corner < 3; ++corner)
        {
            qmdl_i32 vi;
            const qmdl_stvert *st;
            qmdl_i32 s;
            qmdl_fx16 uv[2];

            vi = tri->vertindex[corner];
            if (vi < 0 || (qmdl_u32)vi >= model->header.numverts)
            {
                return QMDL_ERR_RANGE;
            }
            st = &model->stverts[(qmdl_u32)vi];
            s = qmdl_resolve_seam_s(&model->header, st, tri);
            uv[0] = qmdl_fx_from_ratio((int)s,
                                       model->header.skinwidth == 0u ? 1 : (int)model->header.skinwidth);
            uv[1] = qmdl_fx_from_ratio((int)st->t,
                                       model->header.skinheight == 0u ? 1 : (int)model->header.skinheight);

            rc = qmdl_obj_writer_cstr(wr, "vt ");
            if (rc != QMDL_OK) return rc;
            rc = qmdl_obj_writer_fx16(wr, uv[0]);
            if (rc != QMDL_OK) return rc;
            rc = qmdl_obj_writer_char(wr, ' ');
            if (rc != QMDL_OK) return rc;
            rc = qmdl_obj_writer_fx16(wr, uv[1]);
            if (rc != QMDL_OK) return rc;
            rc = qmdl_obj_writer_char(wr, '\n');
            if (rc != QMDL_OK) return rc;
        }
    }

    vt_index = 1u;
    for (i = 0u; i < model->header.numtris; ++i)
    {
        const qmdl_triangle *tri;
        int corner;
        tri = &model->triangles[i];
        rc = qmdl_obj_writer_cstr(wr, "f ");
        if (rc != QMDL_OK) return rc;
        for (corner = 0; corner < 3; ++corner)
        {
            qmdl_i32 vi;
            vi = tri->vertindex[corner];
            if (vi < 0 || (qmdl_u32)vi >= model->header.numverts)
            {
                return QMDL_ERR_RANGE;
            }
            rc = qmdl_obj_writer_u32(wr, (qmdl_u32)vi + 1u);
            if (rc != QMDL_OK) return rc;
            rc = qmdl_obj_writer_char(wr, '/');
            if (rc != QMDL_OK) return rc;
            rc = qmdl_obj_writer_u32(wr, vt_index++);
            if (rc != QMDL_OK) return rc;
            rc = qmdl_obj_writer_char(wr, '/');
            if (rc != QMDL_OK) return rc;
            rc = qmdl_obj_writer_u32(wr, (qmdl_u32)vi + 1u);
            if (rc != QMDL_OK) return rc;
            if (corner != 2)
            {
                rc = qmdl_obj_writer_char(wr, ' ');
                if (rc != QMDL_OK) return rc;
            }
        }
        rc = qmdl_obj_writer_char(wr, '\n');
        if (rc != QMDL_OK) return rc;
    }

    return QMDL_OK;
}

void qmdl_obj_storage_clear(qmdl_obj_storage *storage)
{
    if (storage != 0)
    {
        memset(storage, 0, sizeof(*storage));
    }
}

void qmdl_obj_mesh_clear(qmdl_obj_mesh *mesh)
{
    if (mesh != 0)
    {
        memset(mesh, 0, sizeof(*mesh));
    }
}

static int qmdl_obj_parse_comment(const char *line_start,
                                  const char *line_end,
                                  const qmdl_obj_storage *storage,
                                  qmdl_obj_mesh *mesh)
{
    const char *p;
    qmdl_obj_token cmd;
    qmdl_obj_token tok;
    qmdl_u32 index;
    int rc;

    p = qmdl_obj_skip_space(line_start, line_end);
    if (p >= line_end || *p != '#')
    {
        return QMDL_OK;
    }
    ++p;
    p = qmdl_obj_skip_space(p, line_end);
    if (!qmdl_obj_next_token(&p, line_end, &cmd))
    {
        return QMDL_OK;
    }
    if (qmdl_obj_token_equals(&cmd, "qmdl_obj"))
    {
        mesh->flags |= QMDL_OBJ_HAS_METADATA;
        return QMDL_OK;
    }
    if (qmdl_obj_token_equals(&cmd, "qmdl_skin"))
    {
        mesh->flags |= QMDL_OBJ_HAS_METADATA;
        if (!qmdl_obj_next_token(&p, line_end, &tok)) return QMDL_ERR_FORMAT;
        rc = qmdl_obj_parse_u32(&tok, &mesh->qmdl_skinwidth);
        if (rc != QMDL_OK) return rc;
        if (!qmdl_obj_next_token(&p, line_end, &tok)) return QMDL_ERR_FORMAT;
        rc = qmdl_obj_parse_u32(&tok, &mesh->qmdl_skinheight);
        if (rc != QMDL_OK) return rc;
        return QMDL_OK;
    }
    if (qmdl_obj_token_equals(&cmd, "qmdl_frame_name"))
    {
        size_t n;
        mesh->flags |= QMDL_OBJ_HAS_METADATA;
        p = qmdl_obj_skip_space(p, line_end);
        n = (size_t)(line_end - p);
        if (n > 16u)
        {
            n = 16u;
        }
        memset(mesh->frame_name, 0, 16u);
        memcpy(mesh->frame_name, p, n);
        return QMDL_OK;
    }
    if (qmdl_obj_token_equals(&cmd, "qmdl_st"))
    {
        qmdl_i32 v0;
        if (storage == 0 || storage->qmdl_stverts == 0) return QMDL_ERR_CAPACITY;
        mesh->flags |= QMDL_OBJ_HAS_METADATA;
        if (!qmdl_obj_next_token(&p, line_end, &tok)) return QMDL_ERR_FORMAT;
        rc = qmdl_obj_parse_u32(&tok, &index);
        if (rc != QMDL_OK) return rc;
        if (index >= storage->qmdl_stverts_cap) return QMDL_ERR_CAPACITY;
        if (!qmdl_obj_next_token(&p, line_end, &tok)) return QMDL_ERR_FORMAT;
        rc = qmdl_obj_parse_i32(&tok, &storage->qmdl_stverts[index].onseam);
        if (rc != QMDL_OK) return rc;
        if (!qmdl_obj_next_token(&p, line_end, &tok)) return QMDL_ERR_FORMAT;
        rc = qmdl_obj_parse_i32(&tok, &storage->qmdl_stverts[index].s);
        if (rc != QMDL_OK) return rc;
        if (!qmdl_obj_next_token(&p, line_end, &tok)) return QMDL_ERR_FORMAT;
        rc = qmdl_obj_parse_i32(&tok, &storage->qmdl_stverts[index].t);
        if (rc != QMDL_OK) return rc;
        if (mesh->num_qmdl_stverts < index + 1u)
        {
            mesh->num_qmdl_stverts = index + 1u;
        }
        (void)v0;
        return QMDL_OK;
    }
    if (qmdl_obj_token_equals(&cmd, "qmdl_tri"))
    {
        if (storage == 0 || storage->qmdl_triangles == 0) return QMDL_ERR_CAPACITY;
        mesh->flags |= QMDL_OBJ_HAS_METADATA;
        if (!qmdl_obj_next_token(&p, line_end, &tok)) return QMDL_ERR_FORMAT;
        rc = qmdl_obj_parse_u32(&tok, &index);
        if (rc != QMDL_OK) return rc;
        if (index >= storage->qmdl_triangles_cap) return QMDL_ERR_CAPACITY;
        if (!qmdl_obj_next_token(&p, line_end, &tok)) return QMDL_ERR_FORMAT;
        rc = qmdl_obj_parse_i32(&tok, &storage->qmdl_triangles[index].facesfront);
        if (rc != QMDL_OK) return rc;
        if (!qmdl_obj_next_token(&p, line_end, &tok)) return QMDL_ERR_FORMAT;
        rc = qmdl_obj_parse_i32(&tok, &storage->qmdl_triangles[index].vertindex[0]);
        if (rc != QMDL_OK) return rc;
        if (!qmdl_obj_next_token(&p, line_end, &tok)) return QMDL_ERR_FORMAT;
        rc = qmdl_obj_parse_i32(&tok, &storage->qmdl_triangles[index].vertindex[1]);
        if (rc != QMDL_OK) return rc;
        if (!qmdl_obj_next_token(&p, line_end, &tok)) return QMDL_ERR_FORMAT;
        rc = qmdl_obj_parse_i32(&tok, &storage->qmdl_triangles[index].vertindex[2]);
        if (rc != QMDL_OK) return rc;
        if (mesh->num_qmdl_triangles < index + 1u)
        {
            mesh->num_qmdl_triangles = index + 1u;
        }
        return QMDL_OK;
    }
    if (qmdl_obj_token_equals(&cmd, "qmdl_normal"))
    {
        qmdl_u32 value;
        if (storage == 0 || storage->qmdl_normal_indices == 0) return QMDL_ERR_CAPACITY;
        mesh->flags |= QMDL_OBJ_HAS_METADATA;
        if (!qmdl_obj_next_token(&p, line_end, &tok)) return QMDL_ERR_FORMAT;
        rc = qmdl_obj_parse_u32(&tok, &index);
        if (rc != QMDL_OK) return rc;
        if (index >= storage->qmdl_normal_indices_cap) return QMDL_ERR_CAPACITY;
        if (!qmdl_obj_next_token(&p, line_end, &tok)) return QMDL_ERR_FORMAT;
        rc = qmdl_obj_parse_u32(&tok, &value);
        if (rc != QMDL_OK) return rc;
        if (value > 255u) return QMDL_ERR_RANGE;
        storage->qmdl_normal_indices[index] = (qmdl_u8)value;
        if (mesh->num_qmdl_normal_indices < index + 1u)
        {
            mesh->num_qmdl_normal_indices = index + 1u;
        }
        return QMDL_OK;
    }

    return QMDL_OK;
}

static int qmdl_obj_parse_vertex_line(const char *line_start,
                                      const char *line_end,
                                      const qmdl_obj_storage *storage,
                                      qmdl_obj_mesh *mesh)
{
    const char *p;
    qmdl_obj_token tok;
    qmdl_obj_vertex *v;
    int rc;

    if (storage == 0 || storage->vertices == 0)
    {
        return QMDL_ERR_CAPACITY;
    }
    if (mesh->num_vertices >= storage->vertices_cap)
    {
        return QMDL_ERR_CAPACITY;
    }

    v = &storage->vertices[mesh->num_vertices];
    p = line_start;
    qmdl_obj_next_token(&p, line_end, &tok);
    if (!qmdl_obj_next_token(&p, line_end, &tok)) return QMDL_ERR_FORMAT;
    rc = qmdl_obj_parse_fx16(&tok, &v->xyz[0]);
    if (rc != QMDL_OK) return rc;
    if (!qmdl_obj_next_token(&p, line_end, &tok)) return QMDL_ERR_FORMAT;
    rc = qmdl_obj_parse_fx16(&tok, &v->xyz[1]);
    if (rc != QMDL_OK) return rc;
    if (!qmdl_obj_next_token(&p, line_end, &tok)) return QMDL_ERR_FORMAT;
    rc = qmdl_obj_parse_fx16(&tok, &v->xyz[2]);
    if (rc != QMDL_OK) return rc;
    ++mesh->num_vertices;
    return QMDL_OK;
}

static int qmdl_obj_parse_texcoord_line(const char *line_start,
                                        const char *line_end,
                                        const qmdl_obj_storage *storage,
                                        qmdl_obj_mesh *mesh)
{
    const char *p;
    qmdl_obj_token tok;
    qmdl_obj_texcoord *vt;
    int rc;

    if (storage == 0 || storage->texcoords == 0)
    {
        return QMDL_ERR_CAPACITY;
    }
    if (mesh->num_texcoords >= storage->texcoords_cap)
    {
        return QMDL_ERR_CAPACITY;
    }

    vt = &storage->texcoords[mesh->num_texcoords];
    p = line_start;
    qmdl_obj_next_token(&p, line_end, &tok);
    if (!qmdl_obj_next_token(&p, line_end, &tok)) return QMDL_ERR_FORMAT;
    rc = qmdl_obj_parse_fx16(&tok, &vt->uv[0]);
    if (rc != QMDL_OK) return rc;
    if (!qmdl_obj_next_token(&p, line_end, &tok)) return QMDL_ERR_FORMAT;
    rc = qmdl_obj_parse_fx16(&tok, &vt->uv[1]);
    if (rc != QMDL_OK) return rc;
    ++mesh->num_texcoords;
    return QMDL_OK;
}

static int qmdl_obj_emit_face(qmdl_obj_mesh *mesh,
                              const qmdl_obj_storage *storage,
                              qmdl_i32 a_v,
                              qmdl_i32 a_vt,
                              qmdl_i32 b_v,
                              qmdl_i32 b_vt,
                              qmdl_i32 c_v,
                              qmdl_i32 c_vt)
{
    qmdl_obj_face *face;

    if (storage == 0 || storage->faces == 0)
    {
        return QMDL_ERR_CAPACITY;
    }
    if (mesh->num_faces >= storage->faces_cap)
    {
        return QMDL_ERR_CAPACITY;
    }

    face = &storage->faces[mesh->num_faces];
    face->v[0] = a_v;
    face->v[1] = b_v;
    face->v[2] = c_v;
    face->vt[0] = a_vt;
    face->vt[1] = b_vt;
    face->vt[2] = c_vt;
    ++mesh->num_faces;
    return QMDL_OK;
}

static int qmdl_obj_parse_face_line(const char *line_start,
                                    const char *line_end,
                                    const qmdl_obj_storage *storage,
                                    qmdl_obj_mesh *mesh)
{
    const char *p;
    qmdl_obj_token tok;
    qmdl_i32 first_v;
    qmdl_i32 first_vt;
    qmdl_i32 prev_v;
    qmdl_i32 prev_vt;
    qmdl_i32 curr_v;
    qmdl_i32 curr_vt;
    qmdl_i32 raw_v;
    qmdl_i32 raw_vt;
    int count;
    int rc;

    p = line_start;
    qmdl_obj_next_token(&p, line_end, &tok);

    count = 0;
    first_v = first_vt = prev_v = prev_vt = 0;

    while (qmdl_obj_next_token(&p, line_end, &tok))
    {
        rc = qmdl_obj_parse_face_ref(&tok, &raw_v, &raw_vt);
        if (rc != QMDL_OK) return rc;
        rc = qmdl_obj_resolve_index(raw_v, mesh->num_vertices, &curr_v);
        if (rc != QMDL_OK) return rc;
        if (raw_vt == 0)
        {
            curr_vt = QMDL_OBJ_NO_INDEX;
        }
        else
        {
            rc = qmdl_obj_resolve_index(raw_vt, mesh->num_texcoords, &curr_vt);
            if (rc != QMDL_OK) return rc;
        }

        if (count == 0)
        {
            first_v = curr_v;
            first_vt = curr_vt;
        }
        else if (count == 1)
        {
            prev_v = curr_v;
            prev_vt = curr_vt;
        }
        else
        {
            rc = qmdl_obj_emit_face(mesh, storage,
                                    first_v, first_vt,
                                    prev_v, prev_vt,
                                    curr_v, curr_vt);
            if (rc != QMDL_OK) return rc;
            prev_v = curr_v;
            prev_vt = curr_vt;
        }
        ++count;
    }

    if (count < 3)
    {
        return QMDL_ERR_FORMAT;
    }
    return QMDL_OK;
}

int qmdl_obj_decode(const void *text,
                    size_t text_size,
                    const qmdl_obj_storage *storage,
                    qmdl_obj_mesh *mesh)
{
    const char *p;
    const char *end;
    const char *line_start;
    const char *line_end;
    const char *cmd_end;
    int rc;

    if (text == 0 || storage == 0 || mesh == 0)
    {
        return QMDL_ERR_ARGS;
    }

    qmdl_obj_mesh_clear(mesh);
    mesh->vertices = storage->vertices;
    mesh->texcoords = storage->texcoords;
    mesh->faces = storage->faces;
    mesh->qmdl_stverts = storage->qmdl_stverts;
    mesh->qmdl_triangles = storage->qmdl_triangles;
    mesh->qmdl_normal_indices = storage->qmdl_normal_indices;

    p = (const char *)text;
    end = p + text_size;

    while (p < end)
    {
        line_start = p;
        while (p < end && *p != '\n')
        {
            ++p;
        }
        line_end = p;
        if (p < end && *p == '\n')
        {
            ++p;
        }

        line_start = qmdl_obj_skip_space(line_start, line_end);
        if (line_start >= line_end)
        {
            continue;
        }
        if (*line_start == '#')
        {
            rc = qmdl_obj_parse_comment(line_start, line_end, storage, mesh);
            if (rc != QMDL_OK) return rc;
            continue;
        }

        cmd_end = line_start;
        while (cmd_end < line_end && !qmdl_obj_is_space(*cmd_end))
        {
            ++cmd_end;
        }

        if ((size_t)(cmd_end - line_start) == 1u && line_start[0] == 'v')
        {
            rc = qmdl_obj_parse_vertex_line(line_start, line_end, storage, mesh);
            if (rc != QMDL_OK) return rc;
        }
        else if ((size_t)(cmd_end - line_start) == 2u && line_start[0] == 'v' && line_start[1] == 't')
        {
            rc = qmdl_obj_parse_texcoord_line(line_start, line_end, storage, mesh);
            if (rc != QMDL_OK) return rc;
        }
        else if ((size_t)(cmd_end - line_start) == 1u && line_start[0] == 'f')
        {
            rc = qmdl_obj_parse_face_line(line_start, line_end, storage, mesh);
            if (rc != QMDL_OK) return rc;
        }
    }

    return QMDL_OK;
}

int qmdl_obj_encode_measure(const qmdl_model *model,
                            qmdl_u32 subframe_index,
                            qmdl_u32 flags,
                            size_t *text_size)
{
    qmdl_obj_writer wr;
    int rc;

    if (text_size == 0)
    {
        return QMDL_ERR_ARGS;
    }

    wr.dst = 0;
    wr.cap = 0u;
    wr.len = 0u;
    rc = qmdl_obj_encode_internal(&wr, model, subframe_index, flags);
    if (rc != QMDL_OK)
    {
        return rc;
    }
    *text_size = wr.len;
    return QMDL_OK;
}

int qmdl_obj_encode(const qmdl_model *model,
                    qmdl_u32 subframe_index,
                    qmdl_u32 flags,
                    void *dst,
                    size_t dst_size,
                    size_t *text_size)
{
    qmdl_obj_writer wr;
    int rc;

    if (dst == 0)
    {
        return QMDL_ERR_ARGS;
    }

    wr.dst = (char *)dst;
    wr.cap = dst_size;
    wr.len = 0u;
    rc = qmdl_obj_encode_internal(&wr, model, subframe_index, flags);
    if (rc != QMDL_OK)
    {
        return rc;
    }
    if (text_size != 0)
    {
        *text_size = wr.len;
    }
    return QMDL_OK;
}
