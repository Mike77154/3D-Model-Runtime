#include "hl2modelib.h"
#include <string.h>

#define HL2M_STUDIOHDR_MIN 244u
#define HL2M_STUDIO_BONE_SIZE 216u
#define HL2M_STUDIO_TEXTURE_SIZE 64u
#define HL2M_STUDIO_BODYPART_SIZE 16u
#define HL2M_STUDIO_MODEL_SIZE 148u
#define HL2M_STUDIO_MESH_SIZE 116u
#define HL2M_STUDIO_HITBOXSET_SIZE 12u
#define HL2M_STUDIO_HITBOX_SIZE 68u
#define HL2M_VVD_HEADER_SIZE 64u
#define HL2M_VVD_FIXUP_SIZE 12u
#define HL2M_VVD_VERTEX_SIZE 48u
#define HL2M_VTX_FILE_SIZE 36u
#define HL2M_VTX_BODYPART_SIZE 8u
#define HL2M_VTX_MODEL_SIZE 8u
#define HL2M_VTX_LOD_SIZE 12u
#define HL2M_VTX_MESH_SIZE 9u
#define HL2M_VTX_STRIPGROUP_SIZE 25u
#define HL2M_VTX_STRIP_SIZE 27u
#define HL2M_VTX_VERTEX_SIZE 9u
#define HL2M_VTX_MATERIAL_REPLACEMENT_LIST_SIZE 8u
#define HL2M_VTX_MATERIAL_REPLACEMENT_SIZE 6u
#define HL2M_STUDIO_ANIMBLOCK_SIZE 8u
#define HL2M_STUDIO_ANIMDESC_SIZE 100u
#define HL2M_STUDIO_SEQDESC_SIZE 212u
#define HL2M_STUDIO_EVENT_SIZE 80u
#define HL2M_STUDIO_IKRULE_SIZE 152u
#define HL2M_PHY_HEADER_SIZE 16u
#define HL2M_PHY_COMPACT_SURFACE_HEADER_SIZE 32u
#define HL2M_PHY_LEGACY_SURFACE_HEADER_SIZE 52u
#define HL2M_LINE_MAX 768
#define HL2M_TOKEN_MAX 160

static int hl2m_smd_frame_count(const hl2m_smd *smd);
static void hl2m_compile_get_options(const hl2m_compile_options *in_opt, hl2m_compile_options *out_opt);
static int hl2m_smd_bounds(const hl2m_smd *smd, hl2m_vec3 *out_min, hl2m_vec3 *out_max);
static unsigned int hl2m_align4u(unsigned int v);
static int hl2m_put_range(unsigned int off, unsigned int need, unsigned int cap);
static int hl2m_put_u16(hl2m_u8 *data, unsigned int cap, unsigned int off, int v);
static int hl2m_put_i32(hl2m_u8 *data, unsigned int cap, unsigned int off, int v);
static int hl2m_put_vec3(hl2m_u8 *data, unsigned int cap, unsigned int off, hl2m_vec3 v);
static int hl2m_put_z(hl2m_u8 *data, unsigned int cap, unsigned int off, const char *text, unsigned int max_chars);

static void hl2m_memzero(void *p, unsigned int n)
{
    unsigned char *q;
    unsigned int i;
    q = (unsigned char *)p;
    for (i = 0u; i < n; ++i) {
        q[i] = 0u;
    }
}

static int hl2m_in_range(unsigned int off, unsigned int need, unsigned int size)
{
    if (off > size) {
        return 0;
    }
    if (need > size - off) {
        return 0;
    }
    return 1;
}

static hl2m_u16 hl2m_le_u16(const hl2m_u8 *p)
{
    return (hl2m_u16)(((hl2m_u16)p[0]) | ((hl2m_u16)p[1] << 8));
}

static hl2m_u32 hl2m_le_u32(const hl2m_u8 *p)
{
    return ((hl2m_u32)p[0]) |
           ((hl2m_u32)p[1] << 8) |
           ((hl2m_u32)p[2] << 16) |
           ((hl2m_u32)p[3] << 24);
}

static int hl2m_le_i32(const hl2m_u8 *p)
{
    return (int)hl2m_le_u32(p);
}

static int hl2m_read_i32(const hl2m_u8 *data, unsigned int size, unsigned int off, int *out_v)
{
    if (!hl2m_in_range(off, 4u, size)) {
        return HL2M_ERR_RANGE;
    }
    *out_v = hl2m_le_i32(data + off);
    return HL2M_OK;
}

static int hl2m_read_u32(const hl2m_u8 *data, unsigned int size, unsigned int off, hl2m_u32 *out_v)
{
    if (!hl2m_in_range(off, 4u, size)) {
        return HL2M_ERR_RANGE;
    }
    *out_v = hl2m_le_u32(data + off);
    return HL2M_OK;
}

static int hl2m_read_u16(const hl2m_u8 *data, unsigned int size, unsigned int off, int *out_v)
{
    if (!hl2m_in_range(off, 2u, size)) {
        return HL2M_ERR_RANGE;
    }
    *out_v = (int)hl2m_le_u16(data + off);
    return HL2M_OK;
}

static int hl2m_read_s16(const hl2m_u8 *data, unsigned int size, unsigned int off, int *out_v)
{
    hl2m_u16 u;
    if (!hl2m_in_range(off, 2u, size)) {
        return HL2M_ERR_RANGE;
    }
    u = hl2m_le_u16(data + off);
    if (u & 0x8000u) {
        *out_v = (int)u - 65536;
    } else {
        *out_v = (int)u;
    }
    return HL2M_OK;
}

static int hl2m_read_u8(const hl2m_u8 *data, unsigned int size, unsigned int off, int *out_v)
{
    if (!hl2m_in_range(off, 1u, size)) {
        return HL2M_ERR_RANGE;
    }
    *out_v = (int)data[off];
    return HL2M_OK;
}

static int hl2m_read_s8(const hl2m_u8 *data, unsigned int size, unsigned int off, int *out_v)
{
    int v;
    if (!hl2m_in_range(off, 1u, size)) {
        return HL2M_ERR_RANGE;
    }
    v = (int)data[off];
    if (v > 127) {
        v -= 256;
    }
    *out_v = v;
    return HL2M_OK;
}

static hl2m_fixed hl2m_fixed_from_ieee32(hl2m_u32 bits)
{
    int sign;
    int exp;
    hl2m_u32 mant;
    int e;
    int shift;
    hl2m_u32 mag;
    int out_v;

    sign = (bits & 0x80000000u) ? -1 : 1;
    exp = (int)((bits >> 23) & 255u);
    mant = bits & 0x007fffffu;

    if (exp == 255) {
        return (sign < 0) ? (hl2m_fixed)0x80000000u : (hl2m_fixed)0x7fffffffu;
    }
    if (exp == 0 && mant == 0u) {
        return 0;
    }
    if (exp == 0) {
        e = -126;
    } else {
        mant |= 0x00800000u;
        e = exp - 127;
    }

    shift = e - 7;
    if (shift >= 0) {
        if (shift > 7) {
            return (sign < 0) ? (hl2m_fixed)0x80000000u : (hl2m_fixed)0x7fffffffu;
        }
        mag = mant << shift;
        if (mag > 0x7fffffffu) {
            return (sign < 0) ? (hl2m_fixed)0x80000000u : (hl2m_fixed)0x7fffffffu;
        }
        out_v = (int)mag;
    } else {
        shift = -shift;
        if (shift >= 31) {
            out_v = 0;
        } else {
            out_v = (int)(mant >> shift);
        }
    }

    if (sign < 0) {
        out_v = -out_v;
    }
    return out_v;
}

static int hl2m_read_fixed_ieee32(const hl2m_u8 *data, unsigned int size, unsigned int off, hl2m_fixed *out_v)
{
    if (!hl2m_in_range(off, 4u, size)) {
        return HL2M_ERR_RANGE;
    }
    *out_v = hl2m_fixed_from_ieee32(hl2m_le_u32(data + off));
    return HL2M_OK;
}

static int hl2m_read_vec3_ieee32(const hl2m_u8 *data, unsigned int size, unsigned int off, hl2m_vec3 *out_v)
{
    int r;
    r = hl2m_read_fixed_ieee32(data, size, off, &out_v->x);
    if (r != HL2M_OK) return r;
    r = hl2m_read_fixed_ieee32(data, size, off + 4u, &out_v->y);
    if (r != HL2M_OK) return r;
    return hl2m_read_fixed_ieee32(data, size, off + 8u, &out_v->z);
}

static int hl2m_read_vec2_ieee32(const hl2m_u8 *data, unsigned int size, unsigned int off, hl2m_vec2 *out_v)
{
    int r;
    r = hl2m_read_fixed_ieee32(data, size, off, &out_v->x);
    if (r != HL2M_OK) return r;
    return hl2m_read_fixed_ieee32(data, size, off + 4u, &out_v->y);
}

static int hl2m_read_vec4_ieee32(const hl2m_u8 *data, unsigned int size, unsigned int off, hl2m_vec4 *out_v)
{
    int r;
    r = hl2m_read_fixed_ieee32(data, size, off, &out_v->x);
    if (r != HL2M_OK) return r;
    r = hl2m_read_fixed_ieee32(data, size, off + 4u, &out_v->y);
    if (r != HL2M_OK) return r;
    r = hl2m_read_fixed_ieee32(data, size, off + 8u, &out_v->z);
    if (r != HL2M_OK) return r;
    return hl2m_read_fixed_ieee32(data, size, off + 12u, &out_v->w);
}

static void hl2m_copy_z(char *dst, unsigned int dst_size, const char *src, unsigned int src_avail)
{
    unsigned int i;
    if (dst_size == 0u) {
        return;
    }
    i = 0u;
    while (i + 1u < dst_size && i < src_avail && src[i] != 0) {
        dst[i] = src[i];
        ++i;
    }
    dst[i] = 0;
}

static void hl2m_copy_token(char *dst, unsigned int dst_size, const char *src)
{
    unsigned int i;
    if (dst_size == 0u) {
        return;
    }
    i = 0u;
    while (i + 1u < dst_size && src[i] != 0) {
        dst[i] = src[i];
        ++i;
    }
    dst[i] = 0;
}

static int hl2m_copy_offset_string(const hl2m_u8 *data, unsigned int size, unsigned int base, int rel, char *dst, unsigned int dst_size)
{
    unsigned int off;
    if (rel < 0) {
        if (dst_size != 0u) dst[0] = 0;
        return HL2M_ERR_RANGE;
    }
    off = base + (unsigned int)rel;
    if (off >= size) {
        if (dst_size != 0u) dst[0] = 0;
        return HL2M_ERR_RANGE;
    }
    hl2m_copy_z(dst, dst_size, (const char *)(data + off), size - off);
    return HL2M_OK;
}

static int hl2m_text_eq(const char *a, const char *b)
{
    while (*a != 0 && *b != 0) {
        if (*a != *b) {
            return 0;
        }
        ++a;
        ++b;
    }
    return (*a == 0 && *b == 0);
}

static int hl2m_is_space(char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static const char *hl2m_skip_spaces(const char *p)
{
    while (*p != 0 && hl2m_is_space(*p)) {
        ++p;
    }
    return p;
}

static int hl2m_parse_int_ptr(const char **pp, int *out_v)
{
    const char *p;
    int sign;
    int v;
    int got;
    p = hl2m_skip_spaces(*pp);
    sign = 1;
    if (*p == '-') {
        sign = -1;
        ++p;
    } else if (*p == '+') {
        ++p;
    }
    v = 0;
    got = 0;
    while (*p >= '0' && *p <= '9') {
        got = 1;
        v = v * 10 + (int)(*p - '0');
        ++p;
    }
    if (!got) {
        return HL2M_ERR_PARSE;
    }
    *out_v = v * sign;
    *pp = p;
    return HL2M_OK;
}

static int hl2m_parse_fixed_ptr(const char **pp, hl2m_fixed *out_v)
{
    const char *p;
    int sign;
    int whole;
    int frac;
    int scale;
    int digits;
    int bit;
    int remainder;
    int frac_fixed;
    hl2m_fixed v;
    p = hl2m_skip_spaces(*pp);
    sign = 1;
    if (*p == '-') {
        sign = -1;
        ++p;
    } else if (*p == '+') {
        ++p;
    }
    whole = 0;
    digits = 0;
    while (*p >= '0' && *p <= '9') {
        whole = whole * 10 + (int)(*p - '0');
        ++p;
        ++digits;
    }
    frac = 0;
    scale = 1;
    if (*p == '.') {
        ++p;
        while (*p >= '0' && *p <= '9' && scale < 1000000) {
            frac = frac * 10 + (int)(*p - '0');
            scale = scale * 10;
            ++p;
            ++digits;
        }
        while (*p >= '0' && *p <= '9') {
            ++p;
        }
    }
    if (digits == 0) {
        return HL2M_ERR_PARSE;
    }
    if (whole > 32767) {
        v = (hl2m_fixed)0x7fffffffu;
    } else {
        v = (hl2m_fixed)(whole * HL2M_FIXED_ONE);
        if (scale > 1) {
            /* Q16 fractional division without 32-bit intermediate overflow.
               0 <= remainder < scale <= 1000000. */
            remainder = frac;
            frac_fixed = 0;
            for (bit = 0; bit < 16; ++bit) {
                remainder *= 2;
                frac_fixed *= 2;
                if (remainder >= scale) {
                    remainder -= scale;
                    ++frac_fixed;
                }
            }
            v += (hl2m_fixed)frac_fixed;
        }
    }
    if (sign < 0) {
        v = -v;
    }
    *out_v = v;
    *pp = p;
    return HL2M_OK;
}

static int hl2m_parse_word_ptr(const char **pp, char *out_text, unsigned int out_size)
{
    const char *p;
    unsigned int i;
    char quote;
    p = hl2m_skip_spaces(*pp);
    if (*p == 0) {
        if (out_size != 0u) out_text[0] = 0;
        return HL2M_ERR_PARSE;
    }
    i = 0u;
    if (*p == '"') {
        quote = *p;
        ++p;
        while (*p != 0 && *p != quote) {
            if (i + 1u < out_size) out_text[i++] = *p;
            ++p;
        }
        if (*p == quote) ++p;
    } else {
        while (*p != 0 && !hl2m_is_space(*p)) {
            if (i + 1u < out_size) out_text[i++] = *p;
            ++p;
        }
    }
    if (out_size != 0u) out_text[i] = 0;
    *pp = p;
    return HL2M_OK;
}

void hl2m_clear_mdl(hl2m_mdl *out_model)
{
    if (out_model != 0) hl2m_memzero(out_model, (unsigned int)sizeof(*out_model));
}

void hl2m_clear_vvd(hl2m_vvd *out_vvd)
{
    if (out_vvd != 0) hl2m_memzero(out_vvd, (unsigned int)sizeof(*out_vvd));
}

void hl2m_clear_vtx(hl2m_vtx *out_vtx)
{
    if (out_vtx != 0) hl2m_memzero(out_vtx, (unsigned int)sizeof(*out_vtx));
}

void hl2m_clear_smd(hl2m_smd *out_smd)
{
    if (out_smd != 0) hl2m_memzero(out_smd, (unsigned int)sizeof(*out_smd));
}

void hl2m_clear_vmt(hl2m_vmt *out_vmt)
{
    if (out_vmt != 0) hl2m_memzero(out_vmt, (unsigned int)sizeof(*out_vmt));
}

void hl2m_clear_phy(hl2m_phy *out_phy)
{
    if (out_phy != 0) hl2m_memzero(out_phy, (unsigned int)sizeof(*out_phy));
}

void hl2m_clear_ani(hl2m_ani *out_ani)
{
    if (out_ani != 0) hl2m_memzero(out_ani, (unsigned int)sizeof(*out_ani));
}

void hl2m_clear_material_desc(hl2m_material_desc *out_desc)
{
    if (out_desc != 0) hl2m_memzero(out_desc, (unsigned int)sizeof(*out_desc));
}

hl2m_fixed hl2m_fixed_from_int(int value)
{
    return value * HL2M_FIXED_ONE;
}

int hl2m_fixed_to_int(hl2m_fixed value)
{
    return value / HL2M_FIXED_ONE;
}

void hl2m_fixed_to_string(hl2m_fixed value, char *out_text, unsigned int out_size, int frac_digits)
{
    /* Base 65536 fractional expansion, no overflow and no float/64-bit math. */
    char tmp[32];
    char rev[32];
    unsigned int ti, ri;
    unsigned int mag, whole, frac;
    int neg, d;
    if (out_text == 0 || out_size == 0u) return;
    if (out_size == 1u) { out_text[0] = 0; return; }
    if (frac_digits < 0) frac_digits = 0;
    if (frac_digits > 5) frac_digits = 5;
    neg = value < 0;
    mag = neg ? 0u - (unsigned int)value : (unsigned int)value;
    whole = mag / (unsigned int)HL2M_FIXED_ONE;
    frac = mag % (unsigned int)HL2M_FIXED_ONE;
    ti = 0u;
    if (neg) tmp[ti++] = '-';
    ri = 0u;
    do {
        rev[ri++] = (char)('0' + (whole % 10u));
        whole /= 10u;
    } while (whole > 0u && ri < (unsigned int)sizeof(rev));
    while (ri > 0u) tmp[ti++] = rev[--ri];
    if (frac_digits > 0) {
        tmp[ti++] = '.';
        for (d = 0; d < frac_digits; ++d) {
            /* frac * 10 cannot exceed 655350, safely inside signed 32-bit. */
            frac *= 10u;
            tmp[ti++] = (char)('0' + frac / (unsigned int)HL2M_FIXED_ONE);
            frac %= (unsigned int)HL2M_FIXED_ONE;
        }
    }
    tmp[ti] = 0;
    hl2m_copy_token(out_text, out_size, tmp);
}

/* Format-specific vendor module; private helper ABI intentionally local. */
#include "../vendors/source/formats/mdl_codec89.inc"

/* Format-specific vendor module; private helper ABI intentionally local. */
#include "../vendors/source/formats/vvd_codec89.inc"

/* Format-specific vendor module; private helper ABI intentionally local. */
#include "../vendors/source/formats/vtx_codec89.inc"

typedef struct hl2m_stream_s {
    const hl2m_u8 *data;
    unsigned int size;
    unsigned int pos;
} hl2m_stream;

static int hl2m_stream_line(hl2m_stream *s, char *line, unsigned int line_size)
{
    unsigned int i;
    if (s->pos >= s->size) return 0;
    i = 0u;
    while (s->pos < s->size) {
        char c;
        c = (char)s->data[s->pos++];
        if (c == '\n') break;
        if (c != '\r' && i + 1u < line_size) {
            line[i++] = c;
        }
    }
    line[i] = 0;
    return 1;
}

int hl2m_check_mdl_vvd_vtx_sync(const hl2m_mdl *mdl, const hl2m_vvd *vvd, const hl2m_vtx *vtx)
{
    if (mdl == 0 || vvd == 0 || vtx == 0) return HL2M_ERR_NULL;
    if (mdl->checksum != vvd->checksum) return HL2M_ERR_MAGIC;
    if (mdl->checksum != vtx->checksum) return HL2M_ERR_MAGIC;
    return HL2M_OK;
}

static char hl2m_lower_ascii(char c)
{
    if (c >= 'A' && c <= 'Z') return (char)(c - 'A' + 'a');
    return c;
}

static int hl2m_text_eq_ci(const char *a, const char *b)
{
    while (*a != 0 && *b != 0) {
        if (hl2m_lower_ascii(*a) != hl2m_lower_ascii(*b)) return 0;
        ++a;
        ++b;
    }
    return (*a == 0 && *b == 0);
}

void hl2m_clear_qc(hl2m_qc *out_qc)
{
    if (out_qc != 0) hl2m_memzero(out_qc, (unsigned int)sizeof(*out_qc));
}

void hl2m_clear_engine_model(hl2m_engine_model *out_model)
{
    if (out_model != 0) hl2m_memzero(out_model, (unsigned int)sizeof(*out_model));
}

void hl2m_clear_flex_controller_state(hl2m_flex_controller_state *out_state)
{
    if (out_state != 0) hl2m_memzero(out_state, (unsigned int)sizeof(*out_state));
}

void hl2m_clear_convex_decomp(hl2m_convex_decomp *out_decomp)
{
    if (out_decomp != 0) hl2m_memzero(out_decomp, (unsigned int)sizeof(*out_decomp));
}


void hl2m_clear_flex_eval_report(hl2m_flex_eval_report *out_report)
{
    if (out_report != 0) hl2m_memzero(out_report, (unsigned int)sizeof(*out_report));
}

void hl2m_clear_ik_pose(hl2m_ik_pose *out_pose)
{
    if (out_pose != 0) hl2m_memzero(out_pose, (unsigned int)sizeof(*out_pose));
}

void hl2m_clear_ik_report(hl2m_ik_report *out_report)
{
    if (out_report != 0) hl2m_memzero(out_report, (unsigned int)sizeof(*out_report));
}

void hl2m_clear_anim_sample_report(hl2m_anim_sample_report *out_report)
{
    if (out_report != 0) hl2m_memzero(out_report, (unsigned int)sizeof(*out_report));
}

void hl2m_clear_pose_matrices(hl2m_pose_matrices *out_pose)
{
    if (out_pose != 0) hl2m_memzero(out_pose, (unsigned int)sizeof(*out_pose));
}

void hl2m_clear_skin_report(hl2m_skin_report *out_report)
{
    if (out_report != 0) hl2m_memzero(out_report, (unsigned int)sizeof(*out_report));
}

void hl2m_clear_event_cursor(hl2m_event_cursor *out_cursor)
{
    if (out_cursor != 0) hl2m_memzero(out_cursor, (unsigned int)sizeof(*out_cursor));
}

void hl2m_clear_lod_select_report(hl2m_lod_select_report *out_report)
{
    if (out_report != 0) hl2m_memzero(out_report, (unsigned int)sizeof(*out_report));
}

void hl2m_clear_hitbox_build_report(hl2m_hitbox_build_report *out_report)
{
    if (out_report != 0) hl2m_memzero(out_report, (unsigned int)sizeof(*out_report));
}

void hl2m_clear_attachment_report(hl2m_attachment_report *out_report)
{
    if (out_report != 0) hl2m_memzero(out_report, (unsigned int)sizeof(*out_report));
}

void hl2m_clear_hitbox_transform_report(hl2m_hitbox_transform_report *out_report)
{
    if (out_report != 0) hl2m_memzero(out_report, (unsigned int)sizeof(*out_report));
}

void hl2m_clear_draw_filter_report(hl2m_draw_filter_report *out_report)
{
    if (out_report != 0) hl2m_memzero(out_report, (unsigned int)sizeof(*out_report));
}

void hl2m_clear_sequence_runtime_report(hl2m_sequence_runtime_report *out_report)
{
    if (out_report != 0) hl2m_memzero(out_report, (unsigned int)sizeof(*out_report));
}

void hl2m_clear_studiomdl_parity_report(hl2m_studiomdl_parity_report *out_report)
{
    if (out_report != 0) hl2m_memzero(out_report, (unsigned int)sizeof(*out_report));
}

static int hl2m_out_append_char(char *out_text, unsigned int out_size, unsigned int *used, char c)
{
    if (out_text != 0 && out_size != 0u && *used + 1u < out_size) {
        out_text[*used] = c;
    }
    ++(*used);
    if (out_text != 0 && out_size != 0u) {
        if (*used < out_size) out_text[*used] = 0;
        else out_text[out_size - 1u] = 0;
    }
    return (*used + 1u < out_size || out_size == 0u) ? HL2M_OK : HL2M_ERR_LIMIT;
}

static int hl2m_out_append_text(char *out_text, unsigned int out_size, unsigned int *used, const char *text)
{
    int ret;
    ret = HL2M_OK;
    while (*text != 0) {
        if (hl2m_out_append_char(out_text, out_size, used, *text) != HL2M_OK) ret = HL2M_ERR_LIMIT;
        ++text;
    }
    return ret;
}

static int hl2m_append_fixed_qc(char *out_text, unsigned int out_size, unsigned int *used, hl2m_fixed v)
{
    char num[64];
    hl2m_fixed_to_string(v, num, (unsigned int)sizeof(num), 4);
    return hl2m_out_append_text(out_text, out_size, used, num);
}


static int hl2m_out_append_int(char *out_text, unsigned int out_size, unsigned int *used, int value)
{
    char tmp[32];
    char rev[32];
    unsigned int ri;
    unsigned int ti;
    int neg;
    int v;
    neg = 0;
    if (value < 0) {
        neg = 1;
        v = -value;
    } else {
        v = value;
    }
    ri = 0u;
    if (v == 0) rev[ri++] = '0';
    while (v > 0 && ri < (unsigned int)sizeof(rev)) {
        rev[ri++] = (char)('0' + (v % 10));
        v /= 10;
    }
    ti = 0u;
    if (neg) tmp[ti++] = '-';
    while (ri > 0u && ti + 1u < (unsigned int)sizeof(tmp)) tmp[ti++] = rev[--ri];
    tmp[ti] = 0;
    return hl2m_out_append_text(out_text, out_size, used, tmp);
}

/* One vendored format implementation per file, sharing legacy text helpers. */
#include "../vendors/source/formats/smd_codec89.inc"
#include "../vendors/source/formats/qc_codec89.inc"
int hl2m_decompile_mdl_to_qc(const hl2m_mdl *mdl, char *out_text, unsigned int out_size, unsigned int *out_used)
{
    unsigned int used;
    int ret;
    int i;
    int bp;
    int m;
    int model_index;
    if (mdl == 0) return HL2M_ERR_NULL;
    used = 0u;
    ret = HL2M_OK;
    if (out_text != 0 && out_size != 0u) out_text[0] = 0;
    if (hl2m_out_append_text(out_text, out_size, &used, "// hl2modelib cleanroom QC decompile manifest\n") != HL2M_OK) ret = HL2M_ERR_LIMIT;
    if (hl2m_out_append_text(out_text, out_size, &used, "$modelname ") != HL2M_OK) ret = HL2M_ERR_LIMIT;
    if (hl2m_out_append_qc_arg(out_text, out_size, &used, mdl->name) != HL2M_OK) ret = HL2M_ERR_LIMIT;
    if (hl2m_out_append_char(out_text, out_size, &used, '\n') != HL2M_OK) ret = HL2M_ERR_LIMIT;
    for (i = 0; i < mdl->cdtexture_count; ++i) {
        if (hl2m_out_append_text(out_text, out_size, &used, "$cdmaterials ") != HL2M_OK) ret = HL2M_ERR_LIMIT;
        if (hl2m_out_append_qc_arg(out_text, out_size, &used, mdl->cdtextures[i]) != HL2M_OK) ret = HL2M_ERR_LIMIT;
        if (hl2m_out_append_char(out_text, out_size, &used, '\n') != HL2M_OK) ret = HL2M_ERR_LIMIT;
    }
    if (hl2m_out_append_text(out_text, out_size, &used, "$eyeposition ") != HL2M_OK) ret = HL2M_ERR_LIMIT;
    if (hl2m_append_fixed_qc(out_text, out_size, &used, mdl->eyeposition.x) != HL2M_OK) ret = HL2M_ERR_LIMIT;
    if (hl2m_out_append_char(out_text, out_size, &used, ' ') != HL2M_OK) ret = HL2M_ERR_LIMIT;
    if (hl2m_append_fixed_qc(out_text, out_size, &used, mdl->eyeposition.y) != HL2M_OK) ret = HL2M_ERR_LIMIT;
    if (hl2m_out_append_char(out_text, out_size, &used, ' ') != HL2M_OK) ret = HL2M_ERR_LIMIT;
    if (hl2m_append_fixed_qc(out_text, out_size, &used, mdl->eyeposition.z) != HL2M_OK) ret = HL2M_ERR_LIMIT;
    if (hl2m_out_append_char(out_text, out_size, &used, '\n') != HL2M_OK) ret = HL2M_ERR_LIMIT;
    for (bp = 0; bp < mdl->bodypart_count; ++bp) {
        for (m = 0; m < mdl->bodyparts[bp].nummodels; ++m) {
            model_index = mdl->bodyparts[bp].first_model + m;
            if (model_index < 0 || model_index >= mdl->model_count) continue;
            if (hl2m_out_append_text(out_text, out_size, &used, "$body ") != HL2M_OK) ret = HL2M_ERR_LIMIT;
            if (hl2m_out_append_qc_arg(out_text, out_size, &used, mdl->bodyparts[bp].name) != HL2M_OK) ret = HL2M_ERR_LIMIT;
            if (hl2m_out_append_char(out_text, out_size, &used, ' ') != HL2M_OK) ret = HL2M_ERR_LIMIT;
            if (hl2m_out_append_qc_arg(out_text, out_size, &used, mdl->models[model_index].name) != HL2M_OK) ret = HL2M_ERR_LIMIT;
            if (hl2m_out_append_char(out_text, out_size, &used, '\n') != HL2M_OK) ret = HL2M_ERR_LIMIT;
        }
    }
    if (mdl->hitboxset_count > 0) {
        if (hl2m_out_append_text(out_text, out_size, &used, "// hitbox sets found; use hl2m_mdl.hitboxes for exact fixed-point bounds\n") != HL2M_OK) ret = HL2M_ERR_LIMIT;
    }
    if (out_used != 0) *out_used = used;
    return ret;
}

/* Format-specific vendor module; private helper ABI intentionally local. */
#include "../vendors/source/mesher/hl2m_mesher_impl89.inc"

int hl2m_build_engine_model_from_smd(const hl2m_smd *smd, hl2m_engine_model *out_model)
{
    int t;
    int material;
    int first_vertex;
    int first_index;
    int draw_index;
    int r;
    if (smd == 0 || out_model == 0) return HL2M_ERR_NULL;
    hl2m_clear_engine_model(out_model);
    t = 0;
    while (t < smd->triangle_count) {
        material = smd->triangles[t].material;
        if (out_model->draw_count >= HL2M_MAX_ENGINE_DRAWS) return HL2M_ERR_LIMIT;
        draw_index = out_model->draw_count;
        first_vertex = out_model->vertex_count;
        first_index = out_model->index_count;
        ++out_model->draw_count;
        while (t < smd->triangle_count && smd->triangles[t].material == material) {
            int k;
            int first;
            first = smd->triangles[t].first_vertex;
            if (first < 0 || first + 2 >= smd->vertex_count) return HL2M_ERR_RANGE;
            for (k = 0; k < 3; ++k) {
                r = hl2m_engine_append_smd_vertex(&smd->vertices[first + k], material, out_model);
                if (r != HL2M_OK) return r;
            }
            ++t;
        }
        out_model->draws[draw_index].material = material;
        if (material >= 0 && material < smd->material_count)
            hl2m_copy_token(out_model->draws[draw_index].material_name,
                            HL2M_PATH_MAX, smd->materials[material]);
        out_model->draws[draw_index].primitive = HL2M_PRIM_TRIANGLES;
        out_model->draws[draw_index].first_index = first_index;
        out_model->draws[draw_index].num_indices = out_model->index_count - first_index;
        out_model->draws[draw_index].first_vertex = first_vertex;
        out_model->draws[draw_index].num_vertices = out_model->vertex_count - first_vertex;
        out_model->draws[draw_index].bodypart = 0;
        out_model->draws[draw_index].model = 0;
        out_model->draws[draw_index].mesh = draw_index;
        out_model->draws[draw_index].lod = 0;
    }
    return HL2M_OK;
}


static hl2m_fixed hl2m_fixed_mul_unit_approx(hl2m_fixed a, hl2m_fixed b)
{
    return (hl2m_fixed)((a / 256) * (b / 256));
}

int hl2m_engine_apply_vta_flex(const hl2m_engine_model *base_model, const hl2m_vta *vta, int flex_index, hl2m_fixed weight, hl2m_engine_model *out_model)
{
    int i;
    int j;
    const hl2m_vta_flex *fx;
    if (base_model == 0 || vta == 0 || out_model == 0) return HL2M_ERR_NULL;
    if (flex_index < 0 || flex_index >= vta->flex_count) return HL2M_ERR_RANGE;
    *out_model = *base_model;
    fx = &vta->flexes[flex_index];
    for (i = 0; i < fx->num_vertices; ++i) {
        int vta_index;
        vta_index = fx->first_vertex + i;
        if (vta_index < 0 || vta_index >= vta->vertex_count) continue;
        for (j = 0; j < out_model->vertex_count; ++j) {
            int source_index;
            source_index = out_model->vertices[j].source_vvd_vertex;
            if (source_index < 0) source_index = out_model->vertices[j].source_vtx_vertex;
            if (source_index < 0) source_index = j;
            if (source_index == vta->vertices[vta_index].source_vertex) {
                hl2m_vec3 target;
                target = vta->vertices[vta_index].position;
                out_model->vertices[j].position.x += hl2m_fixed_mul_unit_approx(target.x - base_model->vertices[j].position.x, weight);
                out_model->vertices[j].position.y += hl2m_fixed_mul_unit_approx(target.y - base_model->vertices[j].position.y, weight);
                out_model->vertices[j].position.z += hl2m_fixed_mul_unit_approx(target.z - base_model->vertices[j].position.z, weight);
                out_model->vertices[j].normal.x += hl2m_fixed_mul_unit_approx(vta->vertices[vta_index].normal.x - base_model->vertices[j].normal.x, weight);
                out_model->vertices[j].normal.y += hl2m_fixed_mul_unit_approx(vta->vertices[vta_index].normal.y - base_model->vertices[j].normal.y, weight);
                out_model->vertices[j].normal.z += hl2m_fixed_mul_unit_approx(vta->vertices[vta_index].normal.z - base_model->vertices[j].normal.z, weight);
            }
        }
    }
    return HL2M_OK;
}

static int hl2m_smd_find_node_index(const hl2m_smd *smd, const char *name)
{
    int i;
    if (smd == 0 || name == 0) return -1;
    for (i = 0; i < smd->node_count; ++i) {
        if (hl2m_text_eq_ci(smd->nodes[i].name, name)) return i;
    }
    return -1;
}

static int hl2m_state_find_controller(const hl2m_flex_controller_state *state, const char *name)
{
    int i;
    if (state == 0 || name == 0) return -1;
    for (i = 0; i < state->controller_count; ++i) {
        if (hl2m_text_eq_ci(state->names[i], name)) return i;
    }
    return -1;
}

int hl2m_flex_state_from_qc(const hl2m_qc *qc, hl2m_flex_controller_state *out_state)
{
    int i;
    if (qc == 0 || out_state == 0) return HL2M_ERR_NULL;
    hl2m_clear_flex_controller_state(out_state);
    out_state->controller_count = qc->flexcontroller_count;
    if (out_state->controller_count > HL2M_MAX_QC_FLEXCONTROLLERS) out_state->controller_count = HL2M_MAX_QC_FLEXCONTROLLERS;
    for (i = 0; i < out_state->controller_count; ++i) {
        hl2m_copy_token(out_state->names[i], HL2M_NAME_MAX, qc->flexcontrollers[i].name);
        out_state->values[i] = qc->flexcontrollers[i].min_value;
    }
    return HL2M_OK;
}

int hl2m_flex_state_set(hl2m_flex_controller_state *state, const char *name, hl2m_fixed value)
{
    int idx;
    if (state == 0 || name == 0) return HL2M_ERR_NULL;
    idx = hl2m_state_find_controller(state, name);
    if (idx < 0) return HL2M_ERR_RANGE;
    state->values[idx] = value;
    return HL2M_OK;
}


static hl2m_fixed hl2m_fixed_clamp_value(hl2m_fixed v, hl2m_fixed mn, hl2m_fixed mx)
{
    if (mn > mx) {
        hl2m_fixed t;
        t = mn;
        mn = mx;
        mx = t;
    }
    if (v < mn) return mn;
    if (v > mx) return mx;
    return v;
}

static hl2m_fixed hl2m_fixed_div_unit_approx(hl2m_fixed a, hl2m_fixed b)
{
    int q;
    if (b == 0) return 0;
    q = b / 256;
    if (q == 0) return (a >= 0) ? HL2M_FIXED_ONE : -HL2M_FIXED_ONE;
    return (hl2m_fixed)((a / q) * 256);
}

static hl2m_fixed hl2m_flex_state_value_or_missing(const hl2m_flex_controller_state *state, const char *name, int *missing)
{
    int idx;
    const char *p;
    if (state == 0 || name == 0) {
        if (missing != 0) ++(*missing);
        return 0;
    }
    p = name;
    if (*p == '%') ++p;
    idx = hl2m_state_find_controller(state, p);
    if (idx >= 0) return state->values[idx];
    if (missing != 0) ++(*missing);
    return 0;
}

int hl2m_flex_state_clamp(const hl2m_qc *qc, hl2m_flex_controller_state *state, hl2m_flex_eval_report *out_report)
{
    int i;
    int q;
    if (qc == 0 || state == 0) return HL2M_ERR_NULL;
    if (out_report != 0) hl2m_clear_flex_eval_report(out_report);
    for (i = 0; i < state->controller_count; ++i) {
        for (q = 0; q < qc->flexcontroller_count; ++q) {
            if (hl2m_text_eq_ci(state->names[i], qc->flexcontrollers[q].name)) {
                hl2m_fixed oldv;
                oldv = state->values[i];
                state->values[i] = hl2m_fixed_clamp_value(state->values[i], qc->flexcontrollers[q].min_value, qc->flexcontrollers[q].max_value);
                if (oldv != state->values[i] && out_report != 0) ++out_report->clamped_count;
                break;
            }
        }
    }
    if (out_report != 0) out_report->flex_count = qc->flexcontroller_count;
    return HL2M_OK;
}

static int hl2m_flex_token_is_number(const char *text)
{
    if (text == 0 || *text == 0) return 0;
    if (*text == '-' || *text == '+') ++text;
    if (*text >= '0' && *text <= '9') return 1;
    if (*text == '.') return 1;
    return 0;
}

static int hl2m_flex_eval_simple_expression(const hl2m_flex_controller_state *state, const char *expr, hl2m_fixed *out_value, hl2m_flex_eval_report *report)
{
    const char *p;
    char tok[HL2M_TOKEN_MAX];
    int brace;
    hl2m_fixed stack[HL2M_FLEX_EVAL_STACK_MAX];
    int sp;
    int missing;
    int unsupported;
    if (state == 0 || expr == 0 || out_value == 0) return HL2M_ERR_NULL;
    p = expr;
    sp = 0;
    missing = 0;
    unsupported = 0;
    while (hl2m_qc_next_token(&p, tok, HL2M_TOKEN_MAX, &brace)) {
        if (brace != 0) continue;
        if (tok[0] == 0) continue;
        if (hl2m_text_eq_ci(tok, "=")) continue;
        if (hl2m_text_eq_ci(tok, "localvar")) continue;
        if (hl2m_text_eq_ci(tok, "controller")) continue;
        if (hl2m_text_eq_ci(tok, "+") || hl2m_text_eq_ci(tok, "add")) {
            if (sp >= 2) { stack[sp - 2] = stack[sp - 2] + stack[sp - 1]; --sp; }
            else unsupported = 1;
        } else if (hl2m_text_eq_ci(tok, "-") || hl2m_text_eq_ci(tok, "sub")) {
            if (sp >= 2) { stack[sp - 2] = stack[sp - 2] - stack[sp - 1]; --sp; }
            else unsupported = 1;
        } else if (hl2m_text_eq_ci(tok, "*") || hl2m_text_eq_ci(tok, "mul")) {
            if (sp >= 2) { stack[sp - 2] = hl2m_fixed_mul_unit_approx(stack[sp - 2], stack[sp - 1]); --sp; }
            else unsupported = 1;
        } else if (hl2m_text_eq_ci(tok, "/") || hl2m_text_eq_ci(tok, "div")) {
            if (sp >= 2) { stack[sp - 2] = hl2m_fixed_div_unit_approx(stack[sp - 2], stack[sp - 1]); --sp; }
            else unsupported = 1;
        } else if (hl2m_text_eq_ci(tok, "max")) {
            if (sp >= 2) { if (stack[sp - 1] > stack[sp - 2]) stack[sp - 2] = stack[sp - 1]; --sp; }
            else unsupported = 1;
        } else if (hl2m_text_eq_ci(tok, "min")) {
            if (sp >= 2) { if (stack[sp - 1] < stack[sp - 2]) stack[sp - 2] = stack[sp - 1]; --sp; }
            else unsupported = 1;
        } else if (hl2m_text_eq_ci(tok, "neg")) {
            if (sp >= 1) stack[sp - 1] = -stack[sp - 1];
            else unsupported = 1;
        } else if (hl2m_text_eq_ci(tok, "clamp")) {
            if (sp >= 3) {
                stack[sp - 3] = hl2m_fixed_clamp_value(stack[sp - 3], stack[sp - 2], stack[sp - 1]);
                sp -= 2;
                if (report != 0) ++report->clamped_count;
            } else unsupported = 1;
        } else if (hl2m_flex_token_is_number(tok)) {
            hl2m_fixed v;
            if (hl2m_parse_fixed_text(tok, &v) == HL2M_OK) {
                if (sp < HL2M_FLEX_EVAL_STACK_MAX) stack[sp++] = v;
                else unsupported = 1;
            } else unsupported = 1;
        } else {
            hl2m_fixed v;
            v = hl2m_flex_state_value_or_missing(state, tok, &missing);
            if (sp < HL2M_FLEX_EVAL_STACK_MAX) stack[sp++] = v;
            else unsupported = 1;
        }
    }
    if (missing != 0 && report != 0) report->missing_controller_count += missing;
    if (unsupported != 0) {
        if (report != 0) ++report->unsupported_expression_count;
        *out_value = (sp > 0) ? stack[sp - 1] : 0;
        return HL2M_OK;
    }
    if (sp <= 0) {
        *out_value = 0;
    } else {
        *out_value = stack[sp - 1];
        if (report != 0) ++report->simple_expression_count;
    }
    return HL2M_OK;
}

static hl2m_fixed hl2m_flex_weight_for_name(const hl2m_qc *qc, const hl2m_flex_controller_state *state, const char *flex_name, hl2m_flex_eval_report *report)
{
    int i;
    int idx;
    if (state == 0 || flex_name == 0) return 0;
    if (qc != 0) {
        for (i = 0; i < qc->flexrule_count; ++i) {
            if (hl2m_text_eq_ci(qc->flexrules[i].flex_name, flex_name)) {
                hl2m_fixed v;
                if (report != 0) ++report->rules_evaluated;
                if (qc->flexrules[i].controller_index >= 0 && qc->flexrules[i].expression[0] == 0) {
                    idx = hl2m_state_find_controller(state, qc->flexcontrollers[qc->flexrules[i].controller_index].name);
                    if (idx >= 0) return state->values[idx];
                }
                if (hl2m_flex_eval_simple_expression(state, qc->flexrules[i].expression, &v, report) == HL2M_OK) return v;
            }
        }
    }
    idx = hl2m_state_find_controller(state, flex_name);
    if (idx >= 0) return state->values[idx];
    if (report != 0) ++report->missing_controller_count;
    return 0;
}

int hl2m_flex_eval_weights(const hl2m_qc *qc, const hl2m_vta *vta, const hl2m_flex_controller_state *state, hl2m_fixed out_weights[HL2M_MAX_VTA_FLEXES], hl2m_flex_eval_report *out_report)
{
    int i;
    hl2m_flex_eval_report local_report;
    if (vta == 0 || state == 0 || out_weights == 0) return HL2M_ERR_NULL;
    hl2m_clear_flex_eval_report(&local_report);
    local_report.flex_count = vta->flex_count;
    for (i = 0; i < HL2M_MAX_VTA_FLEXES; ++i) out_weights[i] = 0;
    for (i = 0; i < vta->flex_count && i < HL2M_MAX_VTA_FLEXES; ++i) {
        hl2m_fixed w;
        w = hl2m_flex_weight_for_name(qc, state, vta->flexes[i].name, &local_report);
        if (w != 0) ++local_report.active_flex_count;
        out_weights[i] = w;
    }
    if (out_report != 0) *out_report = local_report;
    return HL2M_OK;
}

int hl2m_engine_apply_flex_weights(const hl2m_engine_model *base_model, const hl2m_vta *vta, const hl2m_fixed weights[HL2M_MAX_VTA_FLEXES], hl2m_engine_model *out_model, hl2m_flex_eval_report *out_report)
{
    int f;
    int i;
    int j;
    if (base_model == 0 || vta == 0 || weights == 0 || out_model == 0) return HL2M_ERR_NULL;
    *out_model = *base_model;
    if (out_report != 0) out_report->flex_count = vta->flex_count;
    for (f = 0; f < vta->flex_count && f < HL2M_MAX_VTA_FLEXES; ++f) {
        const hl2m_vta_flex *fx;
        hl2m_fixed weight;
        fx = &vta->flexes[f];
        weight = weights[f];
        if (weight == 0) continue;
        for (i = 0; i < fx->num_vertices; ++i) {
            int vta_index;
            vta_index = fx->first_vertex + i;
            if (vta_index < 0 || vta_index >= vta->vertex_count) continue;
            for (j = 0; j < out_model->vertex_count; ++j) {
                int source_index;
                source_index = out_model->vertices[j].source_vvd_vertex;
                if (source_index < 0) source_index = out_model->vertices[j].source_vtx_vertex;
                if (source_index < 0) source_index = j;
                if (source_index == vta->vertices[vta_index].source_vertex) {
                    hl2m_vec3 target;
                    target = vta->vertices[vta_index].position;
                    out_model->vertices[j].position.x += hl2m_fixed_mul_unit_approx(target.x - base_model->vertices[j].position.x, weight);
                    out_model->vertices[j].position.y += hl2m_fixed_mul_unit_approx(target.y - base_model->vertices[j].position.y, weight);
                    out_model->vertices[j].position.z += hl2m_fixed_mul_unit_approx(target.z - base_model->vertices[j].position.z, weight);
                    out_model->vertices[j].normal.x += hl2m_fixed_mul_unit_approx(vta->vertices[vta_index].normal.x - base_model->vertices[j].normal.x, weight);
                    out_model->vertices[j].normal.y += hl2m_fixed_mul_unit_approx(vta->vertices[vta_index].normal.y - base_model->vertices[j].normal.y, weight);
                    out_model->vertices[j].normal.z += hl2m_fixed_mul_unit_approx(vta->vertices[vta_index].normal.z - base_model->vertices[j].normal.z, weight);
                    if (out_report != 0) ++out_report->applied_vertex_count;
                }
            }
        }
    }
    return HL2M_OK;
}

int hl2m_engine_apply_flex_controllers(const hl2m_engine_model *base_model, const hl2m_vta *vta, const hl2m_qc *qc, const hl2m_flex_controller_state *state, hl2m_engine_model *out_model)
{
    hl2m_fixed weights[HL2M_MAX_VTA_FLEXES];
    hl2m_flex_eval_report eval_report;
    int r;
    if (base_model == 0 || vta == 0 || state == 0 || out_model == 0) return HL2M_ERR_NULL;
    r = hl2m_flex_eval_weights(qc, vta, state, weights, &eval_report); if (r != HL2M_OK) return r;
    return hl2m_engine_apply_flex_weights(base_model, vta, weights, out_model, &eval_report);
}

static int hl2m_smd_group_bounds(const hl2m_smd *smd, int mode, int bone, int material, hl2m_vec3 *out_min, hl2m_vec3 *out_max, int *out_has)
{
    int i;
    int has;
    hl2m_vec3 mn;
    hl2m_vec3 mx;
    if (smd == 0 || out_min == 0 || out_max == 0 || out_has == 0) return HL2M_ERR_NULL;
    has = 0;
    mn.x = mn.y = mn.z = 0;
    mx = mn;
    for (i = 0; i < smd->vertex_count; ++i) {
        const hl2m_smd_vertex *v;
        int use_v;
        v = &smd->vertices[i];
        use_v = 1;
        if (mode == HL2M_COLLISION_PER_BONE_AABB && v->parent_bone != bone) use_v = 0;
        if (mode == HL2M_COLLISION_PER_MATERIAL_AABB && v->material != material) use_v = 0;
        if (mode == HL2M_COLLISION_BONE_MATERIAL_AABB && (v->parent_bone != bone || v->material != material)) use_v = 0;
        if (!use_v) continue;
        if (!has) {
            mn = v->position;
            mx = v->position;
            has = 1;
        } else {
            if (v->position.x < mn.x) mn.x = v->position.x;
            if (v->position.y < mn.y) mn.y = v->position.y;
            if (v->position.z < mn.z) mn.z = v->position.z;
            if (v->position.x > mx.x) mx.x = v->position.x;
            if (v->position.y > mx.y) mx.y = v->position.y;
            if (v->position.z > mx.z) mx.z = v->position.z;
        }
    }
    *out_min = mn;
    *out_max = mx;
    *out_has = has;
    return HL2M_OK;
}

static int hl2m_convex_add_box(hl2m_convex_decomp *out_decomp, int bone, int material, hl2m_vec3 bbmin, hl2m_vec3 bbmax)
{
    hl2m_convex_hull *h;
    int base;
    if (out_decomp == 0) return HL2M_ERR_NULL;
    if (out_decomp->hull_count >= HL2M_MAX_CONVEX_HULLS) return HL2M_ERR_LIMIT;
    if (out_decomp->vertex_count + 8 > HL2M_MAX_CONVEX_VERTICES) return HL2M_ERR_LIMIT;
    base = out_decomp->vertex_count;
    h = &out_decomp->hulls[out_decomp->hull_count];
    h->bone = bone;
    h->material = material;
    h->first_vertex = base;
    h->vertex_count = 8;
    h->triangle_count = 12;
    h->bbmin = bbmin;
    h->bbmax = bbmax;
    out_decomp->vertices[base + 0].x = bbmin.x; out_decomp->vertices[base + 0].y = bbmin.y; out_decomp->vertices[base + 0].z = bbmin.z;
    out_decomp->vertices[base + 1].x = bbmax.x; out_decomp->vertices[base + 1].y = bbmin.y; out_decomp->vertices[base + 1].z = bbmin.z;
    out_decomp->vertices[base + 2].x = bbmax.x; out_decomp->vertices[base + 2].y = bbmax.y; out_decomp->vertices[base + 2].z = bbmin.z;
    out_decomp->vertices[base + 3].x = bbmin.x; out_decomp->vertices[base + 3].y = bbmax.y; out_decomp->vertices[base + 3].z = bbmin.z;
    out_decomp->vertices[base + 4].x = bbmin.x; out_decomp->vertices[base + 4].y = bbmin.y; out_decomp->vertices[base + 4].z = bbmax.z;
    out_decomp->vertices[base + 5].x = bbmax.x; out_decomp->vertices[base + 5].y = bbmin.y; out_decomp->vertices[base + 5].z = bbmax.z;
    out_decomp->vertices[base + 6].x = bbmax.x; out_decomp->vertices[base + 6].y = bbmax.y; out_decomp->vertices[base + 6].z = bbmax.z;
    out_decomp->vertices[base + 7].x = bbmin.x; out_decomp->vertices[base + 7].y = bbmax.y; out_decomp->vertices[base + 7].z = bbmax.z;
    out_decomp->vertex_count += 8;
    ++out_decomp->hull_count;
    return HL2M_OK;
}

int hl2m_build_convex_decomp_from_smd(const hl2m_smd *smd, int mode, hl2m_convex_decomp *out_decomp)
{
    int b;
    int m;
    int r;
    if (smd == 0 || out_decomp == 0) return HL2M_ERR_NULL;
    hl2m_clear_convex_decomp(out_decomp);
    if (mode == HL2M_COLLISION_AABB) {
        hl2m_vec3 mn;
        hl2m_vec3 mx;
        r = hl2m_smd_bounds(smd, &mn, &mx); if (r != HL2M_OK) return r;
        return hl2m_convex_add_box(out_decomp, -1, -1, mn, mx);
    }
    if (mode == HL2M_COLLISION_PER_MATERIAL_AABB) {
        for (m = 0; m < smd->material_count && out_decomp->hull_count < HL2M_MAX_CONVEX_HULLS; ++m) {
            hl2m_vec3 mn;
            hl2m_vec3 mx;
            int has;
            r = hl2m_smd_group_bounds(smd, mode, -1, m, &mn, &mx, &has); if (r != HL2M_OK) return r;
            if (has) { r = hl2m_convex_add_box(out_decomp, -1, m, mn, mx); if (r != HL2M_OK) return r; }
        }
    } else if (mode == HL2M_COLLISION_BONE_MATERIAL_AABB) {
        for (b = 0; b < smd->node_count && out_decomp->hull_count < HL2M_MAX_CONVEX_HULLS; ++b) {
            for (m = 0; m < smd->material_count && out_decomp->hull_count < HL2M_MAX_CONVEX_HULLS; ++m) {
                hl2m_vec3 mn;
                hl2m_vec3 mx;
                int has;
                r = hl2m_smd_group_bounds(smd, mode, b, m, &mn, &mx, &has); if (r != HL2M_OK) return r;
                if (has) { r = hl2m_convex_add_box(out_decomp, b, m, mn, mx); if (r != HL2M_OK) return r; }
            }
        }
    } else {
        for (b = 0; b < smd->node_count && out_decomp->hull_count < HL2M_MAX_CONVEX_HULLS; ++b) {
            hl2m_vec3 mn;
            hl2m_vec3 mx;
            int has;
            r = hl2m_smd_group_bounds(smd, HL2M_COLLISION_PER_BONE_AABB, b, -1, &mn, &mx, &has); if (r != HL2M_OK) return r;
            if (has) { r = hl2m_convex_add_box(out_decomp, b, -1, mn, mx); if (r != HL2M_OK) return r; }
        }
    }
    if (out_decomp->hull_count == 0) {
        hl2m_vec3 mn;
        hl2m_vec3 mx;
        r = hl2m_smd_bounds(smd, &mn, &mx); if (r != HL2M_OK) return r;
        return hl2m_convex_add_box(out_decomp, -1, -1, mn, mx);
    }
    return HL2M_OK;
}


int hl2m_build_hitboxes_from_qc_smd(const hl2m_qc *qc, const hl2m_smd *smd, hl2m_mdl_hitbox *out_hitboxes, int out_cap, int *out_count, hl2m_hitbox_build_report *out_report)
{
    int i;
    int count;
    hl2m_hitbox_build_report local_report;
    if (smd == 0 || out_count == 0) return HL2M_ERR_NULL;
    if (out_cap < 0) return HL2M_ERR_RANGE;
    if (out_report != 0) hl2m_clear_hitbox_build_report(out_report);
    hl2m_clear_hitbox_build_report(&local_report);
    count = 0;
    if (qc != 0 && qc->qc_hitbox_count > 0) {
        for (i = 0; i < qc->qc_hitbox_count; ++i) {
            int bone;
            bone = hl2m_smd_find_node_index(smd, qc->qc_hitboxes[i].bone);
            if (bone < 0) {
                bone = 0;
                ++local_report.missing_bone_count;
            }
            if (count < out_cap && out_hitboxes != 0) {
                out_hitboxes[count].bone = bone;
                out_hitboxes[count].group = qc->qc_hitboxes[i].group;
                out_hitboxes[count].bbmin = qc->qc_hitboxes[i].bbmin;
                out_hitboxes[count].bbmax = qc->qc_hitboxes[i].bbmax;
                if (qc->qc_hitboxes[i].name[0] != 0) hl2m_copy_token(out_hitboxes[count].name, HL2M_NAME_MAX, qc->qc_hitboxes[i].name);
                else hl2m_copy_token(out_hitboxes[count].name, HL2M_NAME_MAX, qc->qc_hitboxes[i].bone);
            } else {
                ++local_report.clamped_count;
            }
            ++count;
        }
    } else {
        for (i = 0; i < smd->node_count; ++i) {
            hl2m_vec3 mn;
            hl2m_vec3 mx;
            int has;
            int r;
            r = hl2m_smd_group_bounds(smd, HL2M_COLLISION_PER_BONE_AABB, i, -1, &mn, &mx, &has); if (r != HL2M_OK) return r;
            if (!has) continue;
            if (count < out_cap && out_hitboxes != 0) {
                out_hitboxes[count].bone = i;
                out_hitboxes[count].group = 0;
                out_hitboxes[count].bbmin = mn;
                out_hitboxes[count].bbmax = mx;
                hl2m_copy_token(out_hitboxes[count].name, HL2M_NAME_MAX, smd->nodes[i].name);
            } else {
                ++local_report.clamped_count;
            }
            ++local_report.autogenerated_count;
            ++count;
        }
        if (count == 0) {
            hl2m_vec3 mn;
            hl2m_vec3 mx;
            int r;
            r = hl2m_smd_bounds(smd, &mn, &mx); if (r != HL2M_OK) return r;
            if (out_cap > 0 && out_hitboxes != 0) {
                out_hitboxes[0].bone = 0;
                out_hitboxes[0].group = 0;
                out_hitboxes[0].bbmin = mn;
                out_hitboxes[0].bbmax = mx;
                hl2m_copy_token(out_hitboxes[0].name, HL2M_NAME_MAX, "autobox");
            } else {
                ++local_report.clamped_count;
            }
            ++local_report.autogenerated_count;
            count = 1;
        }
    }
    local_report.hitbox_count = count;
    *out_count = count;
    if (out_report != 0) *out_report = local_report;
    return HL2M_OK;
}

int hl2m_collect_qc_sequence_events(const hl2m_qc *qc, const char *sequence_name, int first_frame, int last_frame, hl2m_sequence_event_out *out_events, int out_cap, int *out_count)
{
    int i;
    int count;
    int filter_sequence;
    if (qc == 0 || out_count == 0) return HL2M_ERR_NULL;
    if (out_cap < 0) return HL2M_ERR_RANGE;
    count = 0;
    filter_sequence = (sequence_name != 0 && sequence_name[0] != 0);
    for (i = 0; i < qc->sequence_event_count; ++i) {
        const hl2m_qc_sequence_event *ev;
        int use_event;
        ev = &qc->sequence_events[i];
        use_event = 1;
        if (filter_sequence) {
            if (!hl2m_text_eq_ci(ev->sequence_name, sequence_name)) use_event = 0;
        }
        if (first_frame >= 0 && ev->frame < first_frame) use_event = 0;
        if (last_frame >= 0 && ev->frame > last_frame) use_event = 0;
        if (!use_event) continue;
        if (count < out_cap && out_events != 0) {
            out_events[count].sequence_index = ev->sequence_index;
            hl2m_copy_token(out_events[count].sequence_name, HL2M_NAME_MAX, ev->sequence_name);
            out_events[count].event_id = ev->event_id;
            out_events[count].frame = ev->frame;
            out_events[count].cycle = ev->cycle;
            out_events[count].type = ev->type;
            hl2m_copy_token(out_events[count].options, (unsigned int)sizeof(out_events[count].options), ev->options);
            hl2m_copy_token(out_events[count].event_name, HL2M_NAME_MAX, ev->event_name);
        }
        ++count;
    }
    *out_count = count;
    return (count > out_cap && out_events != 0) ? HL2M_ERR_LIMIT : HL2M_OK;
}

int hl2m_ik_pose_from_smd_frame(const hl2m_smd *smd, int frame, hl2m_ik_pose *out_pose)
{
    int i;
    int p;
    if (smd == 0 || out_pose == 0) return HL2M_ERR_NULL;
    hl2m_clear_ik_pose(out_pose);
    out_pose->bone_count = smd->node_count;
    if (out_pose->bone_count > HL2M_MAX_BONES) out_pose->bone_count = HL2M_MAX_BONES;
    for (i = 0; i < out_pose->bone_count; ++i) {
        hl2m_copy_token(out_pose->bone_names[i], HL2M_NAME_MAX, smd->nodes[i].name);
        out_pose->parent[i] = smd->nodes[i].parent;
        out_pose->pos[i].x = out_pose->pos[i].y = out_pose->pos[i].z = 0;
        out_pose->rot[i].x = out_pose->rot[i].y = out_pose->rot[i].z = 0;
        for (p = 0; p < smd->pose_count; ++p) {
            if (smd->poses[p].node == i && smd->poses[p].time == frame) {
                out_pose->pos[i] = smd->poses[p].pos;
                out_pose->rot[i] = smd->poses[p].rot;
                break;
            }
        }
        if (p >= smd->pose_count && frame != 0) {
            for (p = 0; p < smd->pose_count; ++p) {
                if (smd->poses[p].node == i && smd->poses[p].time == 0) {
                    out_pose->pos[i] = smd->poses[p].pos;
                    out_pose->rot[i] = smd->poses[p].rot;
                    break;
                }
            }
        }
    }
    return HL2M_OK;
}

static int hl2m_ik_pose_find_bone(const hl2m_ik_pose *pose, const char *name)
{
    int i;
    if (pose == 0 || name == 0) return -1;
    for (i = 0; i < pose->bone_count; ++i) {
        if (hl2m_text_eq_ci(pose->bone_names[i], name)) return i;
    }
    return -1;
}

int hl2m_solve_ik_rules(const hl2m_qc *qc, const hl2m_ik_pose *in_pose, int frame, hl2m_ik_pose *out_pose, hl2m_ik_report *out_report)
{
    int i;
    hl2m_ik_report local_report;
    if (qc == 0 || in_pose == 0 || out_pose == 0) return HL2M_ERR_NULL;
    hl2m_clear_ik_report(&local_report);
    local_report.chain_count = qc->ikchain_count;
    local_report.rule_count = qc->ikrule_count;
    *out_pose = *in_pose;
    for (i = 0; i < qc->ikrule_count; ++i) {
        int chain;
        int bone;
        const char *bone_name;
        const hl2m_qc_ikrule *rule;
        rule = &qc->ikrules[i];
        if (rule->frame > 0 && frame >= 0 && rule->frame != frame) {
            ++local_report.frame_filtered_count;
            continue;
        }
        chain = hl2m_qc_find_ikchain(qc, rule->chain);
        if (chain < 0) {
            ++local_report.missing_chain_count;
            bone_name = rule->bone;
        } else {
            bone_name = rule->bone[0] != 0 ? rule->bone : qc->ikchains[chain].bone;
        }
        bone = hl2m_ik_pose_find_bone(in_pose, bone_name);
        if (bone < 0) {
            ++local_report.missing_bone_count;
            continue;
        }
        if (rule->contact != 0) {
            out_pose->pos[bone].z = rule->contact;
        }
        if (rule->drop != 0 || rule->top != 0) {
            hl2m_fixed mn;
            hl2m_fixed mx;
            mn = out_pose->pos[bone].z - rule->drop;
            mx = out_pose->pos[bone].z + rule->top;
            out_pose->pos[bone].z = hl2m_fixed_clamp_value(out_pose->pos[bone].z, mn, mx);
            ++local_report.clamped_count;
        }
        if (chain >= 0) {
            out_pose->pos[bone].x += qc->ikchains[chain].knee.x / 16;
            out_pose->pos[bone].y += qc->ikchains[chain].knee.y / 16;
            out_pose->pos[bone].z += qc->ikchains[chain].knee.z / 16;
        }
        ++local_report.solved_count;
    }
    if (out_report != 0) *out_report = local_report;
    return HL2M_OK;
}

static int hl2m_convex_add_split_recursive(hl2m_convex_decomp *out_decomp, const hl2m_convex_hull *src, hl2m_vec3 mn, hl2m_vec3 mx, int depth)
{
    hl2m_fixed dx;
    hl2m_fixed dy;
    hl2m_fixed dz;
    hl2m_fixed mid;
    hl2m_vec3 a_mn;
    hl2m_vec3 a_mx;
    hl2m_vec3 b_mn;
    hl2m_vec3 b_mx;
    int axis;
    if (depth <= 0) return hl2m_convex_add_box(out_decomp, src->bone, src->material, mn, mx);
    if (out_decomp->hull_count + 2 > HL2M_MAX_CONVEX_HULLS) return hl2m_convex_add_box(out_decomp, src->bone, src->material, mn, mx);
    dx = mx.x - mn.x; if (dx < 0) dx = -dx;
    dy = mx.y - mn.y; if (dy < 0) dy = -dy;
    dz = mx.z - mn.z; if (dz < 0) dz = -dz;
    axis = 0;
    if (dy > dx && dy >= dz) axis = 1;
    else if (dz > dx && dz >= dy) axis = 2;
    a_mn = mn; a_mx = mx; b_mn = mn; b_mx = mx;
    if (axis == 0) {
        mid = mn.x + (mx.x - mn.x) / 2;
        a_mx.x = mid;
        b_mn.x = mid;
    } else if (axis == 1) {
        mid = mn.y + (mx.y - mn.y) / 2;
        a_mx.y = mid;
        b_mn.y = mid;
    } else {
        mid = mn.z + (mx.z - mn.z) / 2;
        a_mx.z = mid;
        b_mn.z = mid;
    }
    if (hl2m_convex_add_split_recursive(out_decomp, src, a_mn, a_mx, depth - 1) != HL2M_OK) return HL2M_ERR_LIMIT;
    return hl2m_convex_add_split_recursive(out_decomp, src, b_mn, b_mx, depth - 1);
}


int hl2m_smd_frame_range(const hl2m_smd *smd, int *out_first_frame, int *out_last_frame, int *out_frame_count)
{
    int i;
    int j;
    int first;
    int last;
    int count;
    if (smd == 0) return HL2M_ERR_NULL;
    first = 0;
    last = 0;
    count = 0;
    for (i = 0; i < smd->pose_count; ++i) {
        int t;
        int seen;
        t = smd->poses[i].time;
        if (count == 0 || t < first) first = t;
        if (count == 0 || t > last) last = t;
        seen = 0;
        for (j = 0; j < i; ++j) {
            if (smd->poses[j].time == t) { seen = 1; break; }
        }
        if (!seen) ++count;
    }
    if (out_first_frame != 0) *out_first_frame = first;
    if (out_last_frame != 0) *out_last_frame = last;
    if (out_frame_count != 0) *out_frame_count = count;
    return HL2M_OK;
}

static const hl2m_smd_pose *hl2m_smd_find_pose_for_node_frame(const hl2m_smd *smd, int node, int frame)
{
    int i;
    if (smd == 0) return 0;
    for (i = 0; i < smd->pose_count; ++i) {
        if (smd->poses[i].node == node && smd->poses[i].time == frame) return &smd->poses[i];
    }
    return 0;
}

static const hl2m_smd_pose *hl2m_smd_find_any_pose_for_node(const hl2m_smd *smd, int node)
{
    int i;
    if (smd == 0) return 0;
    for (i = 0; i < smd->pose_count; ++i) {
        if (smd->poses[i].node == node) return &smd->poses[i];
    }
    return 0;
}

static hl2m_fixed hl2m_fixed_lerp_value(hl2m_fixed a, hl2m_fixed b, hl2m_fixed blend)
{
    return a + hl2m_fixed_mul_unit_approx(b - a, blend);
}

static hl2m_vec3 hl2m_vec3_lerp_value(hl2m_vec3 a, hl2m_vec3 b, hl2m_fixed blend)
{
    hl2m_vec3 out_v;
    out_v.x = hl2m_fixed_lerp_value(a.x, b.x, blend);
    out_v.y = hl2m_fixed_lerp_value(a.y, b.y, blend);
    out_v.z = hl2m_fixed_lerp_value(a.z, b.z, blend);
    return out_v;
}

int hl2m_sample_smd_pose_lerp(const hl2m_smd *smd, int frame_a, int frame_b, hl2m_fixed blend, hl2m_ik_pose *out_pose, hl2m_anim_sample_report *out_report)
{
    int i;
    hl2m_anim_sample_report local_report;
    int first;
    int last;
    int frames;
    if (smd == 0 || out_pose == 0) return HL2M_ERR_NULL;
    hl2m_clear_ik_pose(out_pose);
    hl2m_clear_anim_sample_report(&local_report);
    (void)hl2m_smd_frame_range(smd, &first, &last, &frames);
    if (blend < 0) { blend = 0; local_report.warning_flags |= HL2M_ANIM_WARN_CLAMPED_TIME; }
    if (blend > HL2M_FIXED_ONE) { blend = HL2M_FIXED_ONE; local_report.warning_flags |= HL2M_ANIM_WARN_CLAMPED_TIME; }
    local_report.first_frame = first;
    local_report.last_frame = last;
    local_report.frame_count = frames;
    local_report.frame_a = frame_a;
    local_report.frame_b = frame_b;
    local_report.blend = blend;
    out_pose->bone_count = smd->node_count;
    if (out_pose->bone_count > HL2M_MAX_BONES) out_pose->bone_count = HL2M_MAX_BONES;
    for (i = 0; i < out_pose->bone_count; ++i) {
        const hl2m_smd_pose *pa;
        const hl2m_smd_pose *pb;
        int node_id;
        node_id = smd->nodes[i].id;
        hl2m_copy_token(out_pose->bone_names[i], HL2M_NAME_MAX, smd->nodes[i].name);
        out_pose->parent[i] = smd->nodes[i].parent;
        pa = hl2m_smd_find_pose_for_node_frame(smd, node_id, frame_a);
        pb = hl2m_smd_find_pose_for_node_frame(smd, node_id, frame_b);
        if (pa == 0) pa = hl2m_smd_find_pose_for_node_frame(smd, i, frame_a);
        if (pb == 0) pb = hl2m_smd_find_pose_for_node_frame(smd, i, frame_b);
        if (pa == 0) pa = hl2m_smd_find_any_pose_for_node(smd, node_id);
        if (pb == 0) pb = hl2m_smd_find_any_pose_for_node(smd, node_id);
        if (pa == 0) pa = hl2m_smd_find_any_pose_for_node(smd, i);
        if (pb == 0) pb = hl2m_smd_find_any_pose_for_node(smd, i);
        if (pa != 0 && pb != 0) {
            out_pose->pos[i] = hl2m_vec3_lerp_value(pa->pos, pb->pos, blend);
            out_pose->rot[i] = hl2m_vec3_lerp_value(pa->rot, pb->rot, blend);
            ++local_report.interpolated_count;
        } else if (pa != 0) {
            out_pose->pos[i] = pa->pos;
            out_pose->rot[i] = pa->rot;
            ++local_report.fallback_count;
            local_report.warning_flags |= HL2M_ANIM_WARN_NODE_FALLBACK;
        } else if (pb != 0) {
            out_pose->pos[i] = pb->pos;
            out_pose->rot[i] = pb->rot;
            ++local_report.fallback_count;
            local_report.warning_flags |= HL2M_ANIM_WARN_NODE_FALLBACK;
        } else {
            out_pose->pos[i].x = out_pose->pos[i].y = out_pose->pos[i].z = 0;
            out_pose->rot[i].x = out_pose->rot[i].y = out_pose->rot[i].z = 0;
            ++local_report.fallback_count;
            local_report.warning_flags |= HL2M_ANIM_WARN_MISSING_FRAME | HL2M_ANIM_WARN_NODE_FALLBACK;
        }
    }
    local_report.pose_count = out_pose->bone_count;
    if (out_report != 0) *out_report = local_report;
    return HL2M_OK;
}

int hl2m_sample_smd_pose_time(const hl2m_smd *smd, hl2m_fixed frame_time, hl2m_ik_pose *out_pose, hl2m_anim_sample_report *out_report)
{
    int first;
    int last;
    int count;
    int frame_a;
    int frame_b;
    hl2m_fixed blend;
    hl2m_anim_sample_report local_report;
    int r;
    if (smd == 0 || out_pose == 0) return HL2M_ERR_NULL;
    r = hl2m_smd_frame_range(smd, &first, &last, &count); if (r != HL2M_OK) return r;
    hl2m_clear_anim_sample_report(&local_report);
    if (count <= 0) return HL2M_ERR_PARSE;
    if (frame_time < first * HL2M_FIXED_ONE) { frame_time = first * HL2M_FIXED_ONE; local_report.warning_flags |= HL2M_ANIM_WARN_CLAMPED_TIME; }
    if (frame_time > last * HL2M_FIXED_ONE) { frame_time = last * HL2M_FIXED_ONE; local_report.warning_flags |= HL2M_ANIM_WARN_CLAMPED_TIME; }
    frame_a = frame_time / HL2M_FIXED_ONE;
    blend = frame_time - frame_a * HL2M_FIXED_ONE;
    frame_b = frame_a + 1;
    if (frame_b > last) frame_b = frame_a;
    r = hl2m_sample_smd_pose_lerp(smd, frame_a, frame_b, blend, out_pose, &local_report);
    local_report.first_frame = first;
    local_report.last_frame = last;
    local_report.frame_count = count;
    if (out_report != 0) *out_report = local_report;
    return r;
}

#define HL2M_FIXED_PI 205887
#define HL2M_FIXED_TWO_PI 411775
#define HL2M_FIXED_HALF_PI 102943

static hl2m_fixed hl2m_fixed_wrap_pi(hl2m_fixed x)
{
    while (x > HL2M_FIXED_PI) x -= HL2M_FIXED_TWO_PI;
    while (x < -HL2M_FIXED_PI) x += HL2M_FIXED_TWO_PI;
    return x;
}

/* Source RadianEuler uses radians; reduce to the first quadrant BEFORE
   Taylor approximation.  The old polynomial over [-pi,pi] produced
   sin(pi) ~= 0.52 and non-orthogonal bone matrices at large angles.
   All arithmetic remains signed 32-bit fixed 16.16 (strict C89). */
static hl2m_fixed hl2m_fixed_sin_rad(hl2m_fixed x)
{
    hl2m_fixed x2, x3, x5, x7, x9;
    x = hl2m_fixed_wrap_pi(x);
    if (x > HL2M_FIXED_HALF_PI) x = HL2M_FIXED_PI - x;
    else if (x < -HL2M_FIXED_HALF_PI) x = -HL2M_FIXED_PI - x;
    x2 = hl2m_fixed_mul_unit_approx(x, x);
    x3 = hl2m_fixed_mul_unit_approx(x2, x);
    x5 = hl2m_fixed_mul_unit_approx(x3, x2);
    x7 = hl2m_fixed_mul_unit_approx(x5, x2);
    x9 = hl2m_fixed_mul_unit_approx(x7, x2);
    return x - x3 / 6 + x5 / 120 - x7 / 5040 + x9 / 362880;
}
static hl2m_fixed hl2m_fixed_cos_rad(hl2m_fixed x)
{
    return hl2m_fixed_sin_rad(HL2M_FIXED_HALF_PI - x);
}

static void hl2m_mat_identity(hl2m_mat3x4 *m)
{
    int r;
    int c;
    for (r = 0; r < 3; ++r) {
        for (c = 0; c < 4; ++c) m->m[r][c] = 0;
    }
    m->m[0][0] = HL2M_FIXED_ONE;
    m->m[1][1] = HL2M_FIXED_ONE;
    m->m[2][2] = HL2M_FIXED_ONE;
}

static void hl2m_mat_from_pos_rot(hl2m_vec3 pos, hl2m_vec3 rot, int include_rotation, hl2m_mat3x4 *m)
{
    hl2m_fixed sx;
    hl2m_fixed cx;
    hl2m_fixed sy;
    hl2m_fixed cy;
    hl2m_fixed sz;
    hl2m_fixed cz;
    if (!include_rotation) {
        hl2m_mat_identity(m);
    } else {
        sx = hl2m_fixed_sin_rad(rot.x); cx = hl2m_fixed_cos_rad(rot.x);
        sy = hl2m_fixed_sin_rad(rot.y); cy = hl2m_fixed_cos_rad(rot.y);
        sz = hl2m_fixed_sin_rad(rot.z); cz = hl2m_fixed_cos_rad(rot.z);
        m->m[0][0] = hl2m_fixed_mul_unit_approx(cz, cy);
        m->m[0][1] = hl2m_fixed_mul_unit_approx(cz, hl2m_fixed_mul_unit_approx(sy, sx)) - hl2m_fixed_mul_unit_approx(sz, cx);
        m->m[0][2] = hl2m_fixed_mul_unit_approx(cz, hl2m_fixed_mul_unit_approx(sy, cx)) + hl2m_fixed_mul_unit_approx(sz, sx);
        m->m[1][0] = hl2m_fixed_mul_unit_approx(sz, cy);
        m->m[1][1] = hl2m_fixed_mul_unit_approx(sz, hl2m_fixed_mul_unit_approx(sy, sx)) + hl2m_fixed_mul_unit_approx(cz, cx);
        m->m[1][2] = hl2m_fixed_mul_unit_approx(sz, hl2m_fixed_mul_unit_approx(sy, cx)) - hl2m_fixed_mul_unit_approx(cz, sx);
        m->m[2][0] = -sy;
        m->m[2][1] = hl2m_fixed_mul_unit_approx(cy, sx);
        m->m[2][2] = hl2m_fixed_mul_unit_approx(cy, cx);
    }
    m->m[0][3] = pos.x;
    m->m[1][3] = pos.y;
    m->m[2][3] = pos.z;
}

static void hl2m_mat_mul(const hl2m_mat3x4 *a, const hl2m_mat3x4 *b, hl2m_mat3x4 *out_m)
{
    int r;
    int c;
    int k;
    hl2m_mat3x4 tmp;
    for (r = 0; r < 3; ++r) {
        for (c = 0; c < 3; ++c) {
            hl2m_fixed v;
            v = 0;
            for (k = 0; k < 3; ++k) v += hl2m_fixed_mul_unit_approx(a->m[r][k], b->m[k][c]);
            tmp.m[r][c] = v;
        }
        tmp.m[r][3] = a->m[r][3];
        for (k = 0; k < 3; ++k) tmp.m[r][3] += hl2m_fixed_mul_unit_approx(a->m[r][k], b->m[k][3]);
    }
    *out_m = tmp;
}

static hl2m_vec3 hl2m_mat_transform_point(const hl2m_mat3x4 *m, hl2m_vec3 v)
{
    hl2m_vec3 out_v;
    out_v.x = hl2m_fixed_mul_unit_approx(m->m[0][0], v.x) + hl2m_fixed_mul_unit_approx(m->m[0][1], v.y) + hl2m_fixed_mul_unit_approx(m->m[0][2], v.z) + m->m[0][3];
    out_v.y = hl2m_fixed_mul_unit_approx(m->m[1][0], v.x) + hl2m_fixed_mul_unit_approx(m->m[1][1], v.y) + hl2m_fixed_mul_unit_approx(m->m[1][2], v.z) + m->m[1][3];
    out_v.z = hl2m_fixed_mul_unit_approx(m->m[2][0], v.x) + hl2m_fixed_mul_unit_approx(m->m[2][1], v.y) + hl2m_fixed_mul_unit_approx(m->m[2][2], v.z) + m->m[2][3];
    return out_v;
}

static hl2m_vec3 hl2m_mat_transform_vector(const hl2m_mat3x4 *m, hl2m_vec3 v)
{
    hl2m_vec3 out_v;
    out_v.x = hl2m_fixed_mul_unit_approx(m->m[0][0], v.x) + hl2m_fixed_mul_unit_approx(m->m[0][1], v.y) + hl2m_fixed_mul_unit_approx(m->m[0][2], v.z);
    out_v.y = hl2m_fixed_mul_unit_approx(m->m[1][0], v.x) + hl2m_fixed_mul_unit_approx(m->m[1][1], v.y) + hl2m_fixed_mul_unit_approx(m->m[1][2], v.z);
    out_v.z = hl2m_fixed_mul_unit_approx(m->m[2][0], v.x) + hl2m_fixed_mul_unit_approx(m->m[2][1], v.y) + hl2m_fixed_mul_unit_approx(m->m[2][2], v.z);
    return out_v;
}

static void hl2m_mat_inverse_rigid(const hl2m_mat3x4 *m, hl2m_mat3x4 *out_m)
{
    int r;
    int c;
    hl2m_vec3 t;
    for (r = 0; r < 3; ++r) {
        for (c = 0; c < 3; ++c) out_m->m[r][c] = m->m[c][r];
    }
    t.x = m->m[0][3];
    t.y = m->m[1][3];
    t.z = m->m[2][3];
    out_m->m[0][3] = -(hl2m_fixed_mul_unit_approx(out_m->m[0][0], t.x) + hl2m_fixed_mul_unit_approx(out_m->m[0][1], t.y) + hl2m_fixed_mul_unit_approx(out_m->m[0][2], t.z));
    out_m->m[1][3] = -(hl2m_fixed_mul_unit_approx(out_m->m[1][0], t.x) + hl2m_fixed_mul_unit_approx(out_m->m[1][1], t.y) + hl2m_fixed_mul_unit_approx(out_m->m[1][2], t.z));
    out_m->m[2][3] = -(hl2m_fixed_mul_unit_approx(out_m->m[2][0], t.x) + hl2m_fixed_mul_unit_approx(out_m->m[2][1], t.y) + hl2m_fixed_mul_unit_approx(out_m->m[2][2], t.z));
}

int hl2m_pose_matrices_from_pose(const hl2m_ik_pose *pose, int include_rotation, hl2m_pose_matrices *out_matrices)
{
    int i;
    if (pose == 0 || out_matrices == 0) return HL2M_ERR_NULL;
    hl2m_clear_pose_matrices(out_matrices);
    out_matrices->bone_count = pose->bone_count;
    if (out_matrices->bone_count > HL2M_MAX_POSE_MATRICES) out_matrices->bone_count = HL2M_MAX_POSE_MATRICES;
    for (i = 0; i < out_matrices->bone_count; ++i) {
        int parent;
        hl2m_copy_token(out_matrices->bone_names[i], HL2M_NAME_MAX, pose->bone_names[i]);
        out_matrices->parent[i] = pose->parent[i];
        hl2m_mat_from_pos_rot(pose->pos[i], pose->rot[i], include_rotation, &out_matrices->local[i]);
        parent = pose->parent[i];
        if (parent >= 0 && parent < i) hl2m_mat_mul(&out_matrices->world[parent], &out_matrices->local[i], &out_matrices->world[i]);
        else out_matrices->world[i] = out_matrices->local[i];
    }
    return HL2M_OK;
}

int hl2m_pose_inverse_bind_from_pose(const hl2m_ik_pose *bind_pose, int include_rotation, hl2m_pose_matrices *out_inverse_bind)
{
    hl2m_pose_matrices bind_world;
    int i;
    int r;
    if (bind_pose == 0 || out_inverse_bind == 0) return HL2M_ERR_NULL;
    r = hl2m_pose_matrices_from_pose(bind_pose, include_rotation, &bind_world); if (r != HL2M_OK) return r;
    hl2m_clear_pose_matrices(out_inverse_bind);
    out_inverse_bind->bone_count = bind_world.bone_count;
    for (i = 0; i < bind_world.bone_count; ++i) {
        hl2m_copy_token(out_inverse_bind->bone_names[i], HL2M_NAME_MAX, bind_world.bone_names[i]);
        out_inverse_bind->parent[i] = bind_world.parent[i];
        hl2m_mat_inverse_rigid(&bind_world.world[i], &out_inverse_bind->world[i]);
        out_inverse_bind->local[i] = out_inverse_bind->world[i];
    }
    return HL2M_OK;
}


static int hl2m_pose_find_bone_index(const hl2m_ik_pose *pose, const char *name)
{
    int i;
    if (pose == 0 || name == 0) return -1;
    for (i = 0; i < pose->bone_count; ++i) {
        if (hl2m_text_eq_ci(pose->bone_names[i], name)) return i;
    }
    return -1;
}

static int hl2m_matrices_find_bone_index(const hl2m_pose_matrices *pose, const char *name)
{
    int i;
    if (pose == 0 || name == 0) return -1;
    for (i = 0; i < pose->bone_count; ++i) {
        if (hl2m_text_eq_ci(pose->bone_names[i], name)) return i;
    }
    return -1;
}

int hl2m_blend_poses(const hl2m_ik_pose *pose_a, const hl2m_ik_pose *pose_b, hl2m_fixed blend, hl2m_ik_pose *out_pose, hl2m_anim_sample_report *out_report)
{
    int i;
    hl2m_anim_sample_report local_report;
    if (pose_a == 0 || pose_b == 0 || out_pose == 0) return HL2M_ERR_NULL;
    hl2m_clear_ik_pose(out_pose);
    hl2m_clear_anim_sample_report(&local_report);
    if (blend < 0) { blend = 0; local_report.warning_flags |= HL2M_ANIM_WARN_CLAMPED_TIME; }
    if (blend > HL2M_FIXED_ONE) { blend = HL2M_FIXED_ONE; local_report.warning_flags |= HL2M_ANIM_WARN_CLAMPED_TIME; }
    out_pose->bone_count = pose_a->bone_count;
    if (out_pose->bone_count > HL2M_MAX_BONES) out_pose->bone_count = HL2M_MAX_BONES;
    local_report.blend = blend;
    for (i = 0; i < out_pose->bone_count; ++i) {
        int j;
        j = hl2m_pose_find_bone_index(pose_b, pose_a->bone_names[i]);
        if (j < 0 && i < pose_b->bone_count) j = i;
        hl2m_copy_token(out_pose->bone_names[i], HL2M_NAME_MAX, pose_a->bone_names[i]);
        out_pose->parent[i] = pose_a->parent[i];
        if (j >= 0 && j < pose_b->bone_count) {
            out_pose->pos[i] = hl2m_vec3_lerp_value(pose_a->pos[i], pose_b->pos[j], blend);
            out_pose->rot[i] = hl2m_vec3_lerp_value(pose_a->rot[i], pose_b->rot[j], blend);
            ++local_report.interpolated_count;
        } else {
            out_pose->pos[i] = pose_a->pos[i];
            out_pose->rot[i] = pose_a->rot[i];
            ++local_report.fallback_count;
            local_report.warning_flags |= HL2M_ANIM_WARN_NODE_FALLBACK;
        }
    }
    local_report.pose_count = out_pose->bone_count;
    if (out_report != 0) *out_report = local_report;
    return HL2M_OK;
}

int hl2m_apply_additive_pose(const hl2m_ik_pose *base_pose, const hl2m_ik_pose *reference_pose, const hl2m_ik_pose *add_pose, hl2m_fixed weight, hl2m_ik_pose *out_pose, hl2m_anim_sample_report *out_report)
{
    int i;
    hl2m_anim_sample_report local_report;
    if (base_pose == 0 || reference_pose == 0 || add_pose == 0 || out_pose == 0) return HL2M_ERR_NULL;
    hl2m_clear_ik_pose(out_pose);
    hl2m_clear_anim_sample_report(&local_report);
    if (weight < 0) { weight = 0; local_report.warning_flags |= HL2M_ANIM_WARN_CLAMPED_TIME; }
    if (weight > HL2M_FIXED_ONE) { weight = HL2M_FIXED_ONE; local_report.warning_flags |= HL2M_ANIM_WARN_CLAMPED_TIME; }
    out_pose->bone_count = base_pose->bone_count;
    if (out_pose->bone_count > HL2M_MAX_BONES) out_pose->bone_count = HL2M_MAX_BONES;
    local_report.blend = weight;
    for (i = 0; i < out_pose->bone_count; ++i) {
        int r;
        int a;
        hl2m_vec3 dp;
        hl2m_vec3 dr;
        r = hl2m_pose_find_bone_index(reference_pose, base_pose->bone_names[i]);
        a = hl2m_pose_find_bone_index(add_pose, base_pose->bone_names[i]);
        if (r < 0 && i < reference_pose->bone_count) r = i;
        if (a < 0 && i < add_pose->bone_count) a = i;
        hl2m_copy_token(out_pose->bone_names[i], HL2M_NAME_MAX, base_pose->bone_names[i]);
        out_pose->parent[i] = base_pose->parent[i];
        if (r >= 0 && r < reference_pose->bone_count && a >= 0 && a < add_pose->bone_count) {
            dp.x = add_pose->pos[a].x - reference_pose->pos[r].x;
            dp.y = add_pose->pos[a].y - reference_pose->pos[r].y;
            dp.z = add_pose->pos[a].z - reference_pose->pos[r].z;
            dr.x = add_pose->rot[a].x - reference_pose->rot[r].x;
            dr.y = add_pose->rot[a].y - reference_pose->rot[r].y;
            dr.z = add_pose->rot[a].z - reference_pose->rot[r].z;
            out_pose->pos[i].x = base_pose->pos[i].x + hl2m_fixed_mul_unit_approx(dp.x, weight);
            out_pose->pos[i].y = base_pose->pos[i].y + hl2m_fixed_mul_unit_approx(dp.y, weight);
            out_pose->pos[i].z = base_pose->pos[i].z + hl2m_fixed_mul_unit_approx(dp.z, weight);
            out_pose->rot[i].x = base_pose->rot[i].x + hl2m_fixed_mul_unit_approx(dr.x, weight);
            out_pose->rot[i].y = base_pose->rot[i].y + hl2m_fixed_mul_unit_approx(dr.y, weight);
            out_pose->rot[i].z = base_pose->rot[i].z + hl2m_fixed_mul_unit_approx(dr.z, weight);
            ++local_report.interpolated_count;
        } else {
            out_pose->pos[i] = base_pose->pos[i];
            out_pose->rot[i] = base_pose->rot[i];
            ++local_report.fallback_count;
            local_report.warning_flags |= HL2M_ANIM_WARN_NODE_FALLBACK;
        }
    }
    local_report.pose_count = out_pose->bone_count;
    if (out_report != 0) *out_report = local_report;
    return HL2M_OK;
}

static void hl2m_vec3_minmax_init(hl2m_vec3 p, hl2m_vec3 *mn, hl2m_vec3 *mx)
{
    *mn = p;
    *mx = p;
}

static void hl2m_vec3_minmax_include(hl2m_vec3 p, hl2m_vec3 *mn, hl2m_vec3 *mx)
{
    if (p.x < mn->x) mn->x = p.x;
    if (p.y < mn->y) mn->y = p.y;
    if (p.z < mn->z) mn->z = p.z;
    if (p.x > mx->x) mx->x = p.x;
    if (p.y > mx->y) mx->y = p.y;
    if (p.z > mx->z) mx->z = p.z;
}

int hl2m_build_attachment_poses_from_qc(const hl2m_qc *qc, const hl2m_pose_matrices *pose_world, hl2m_attachment_pose *out_attachments, int out_cap, int *out_count, hl2m_attachment_report *out_report)
{
    int i;
    int count;
    hl2m_attachment_report local_report;
    if (qc == 0 || pose_world == 0 || out_count == 0) return HL2M_ERR_NULL;
    if (out_cap < 0) return HL2M_ERR_RANGE;
    hl2m_clear_attachment_report(&local_report);
    count = 0;
    for (i = 0; i < qc->attachment_count; ++i) {
        int bone;
        hl2m_mat3x4 local;
        hl2m_mat3x4 world;
        bone = hl2m_matrices_find_bone_index(pose_world, qc->attachments[i].bone);
        hl2m_mat_from_pos_rot(qc->attachments[i].pos, qc->attachments[i].rot, qc->attachments[i].has_rotate, &local);
        if (bone >= 0 && bone < pose_world->bone_count) {
            hl2m_mat_mul(&pose_world->world[bone], &local, &world);
            ++local_report.resolved_count;
        } else {
            world = local;
            ++local_report.missing_bone_count;
            local_report.warning_flags |= HL2M_ATTACHMENT_WARN_MISSING_BONE;
        }
        if (out_attachments != 0 && count < out_cap) {
            hl2m_copy_token(out_attachments[count].name, HL2M_NAME_MAX, qc->attachments[i].name);
            hl2m_copy_token(out_attachments[count].bone_name, HL2M_NAME_MAX, qc->attachments[i].bone);
            out_attachments[count].bone = bone;
            out_attachments[count].local_pos = qc->attachments[i].pos;
            out_attachments[count].world = world;
            out_attachments[count].world_pos.x = world.m[0][3];
            out_attachments[count].world_pos.y = world.m[1][3];
            out_attachments[count].world_pos.z = world.m[2][3];
        }
        ++count;
    }
    local_report.attachment_count = count;
    *out_count = count;
    if (out_report != 0) *out_report = local_report;
    if (out_attachments != 0 && count > out_cap) return HL2M_ERR_LIMIT;
    return HL2M_OK;
}

int hl2m_transform_hitboxes_to_world(const hl2m_mdl_hitbox *hitboxes, int hitbox_count, const hl2m_pose_matrices *pose_world, hl2m_world_hitbox *out_hitboxes, int out_cap, int *out_count, hl2m_hitbox_transform_report *out_report)
{
    int i;
    int count;
    hl2m_hitbox_transform_report local_report;
    if (hitboxes == 0 || pose_world == 0 || out_count == 0) return HL2M_ERR_NULL;
    if (hitbox_count < 0 || out_cap < 0) return HL2M_ERR_RANGE;
    hl2m_clear_hitbox_transform_report(&local_report);
    count = 0;
    for (i = 0; i < hitbox_count; ++i) {
        int bone;
        int c;
        hl2m_vec3 mn;
        hl2m_vec3 mx;
        hl2m_vec3 p;
        bone = hitboxes[i].bone;
        if (bone < 0 || bone >= pose_world->bone_count) {
            ++local_report.missing_bone_count;
            local_report.warning_flags |= HL2M_HITBOX_WARN_MISSING_BONE;
            mn = hitboxes[i].bbmin;
            mx = hitboxes[i].bbmax;
        } else {
            for (c = 0; c < 8; ++c) {
                p.x = (c & 1) ? hitboxes[i].bbmax.x : hitboxes[i].bbmin.x;
                p.y = (c & 2) ? hitboxes[i].bbmax.y : hitboxes[i].bbmin.y;
                p.z = (c & 4) ? hitboxes[i].bbmax.z : hitboxes[i].bbmin.z;
                p = hl2m_mat_transform_point(&pose_world->world[bone], p);
                if (c == 0) hl2m_vec3_minmax_init(p, &mn, &mx);
                else hl2m_vec3_minmax_include(p, &mn, &mx);
            }
            ++local_report.transformed_count;
        }
        if (out_hitboxes != 0 && count < out_cap) {
            hl2m_copy_token(out_hitboxes[count].name, HL2M_NAME_MAX, hitboxes[i].name);
            out_hitboxes[count].bone = bone;
            out_hitboxes[count].group = hitboxes[i].group;
            out_hitboxes[count].bbmin = mn;
            out_hitboxes[count].bbmax = mx;
        }
        ++count;
    }
    local_report.hitbox_count = count;
    *out_count = count;
    if (out_report != 0) *out_report = local_report;
    if (out_hitboxes != 0 && count > out_cap) return HL2M_ERR_LIMIT;
    return HL2M_OK;
}

/* Format-specific vendor module; private helper ABI intentionally local. */
#include "../vendors/source/animator/hl2m_skinner_impl89.inc"

int hl2m_event_cursor_init(const char *sequence_name, int start_frame, int last_frame, hl2m_event_cursor *out_cursor)
{
    if (out_cursor == 0) return HL2M_ERR_NULL;
    hl2m_clear_event_cursor(out_cursor);
    if (sequence_name != 0) hl2m_copy_token(out_cursor->sequence_name, HL2M_NAME_MAX, sequence_name);
    out_cursor->previous_frame = start_frame;
    out_cursor->current_frame = start_frame;
    out_cursor->last_frame = last_frame;
    return HL2M_OK;
}

static int hl2m_event_cursor_append_range(const hl2m_qc *qc, const char *sequence_name, int first_frame, int last_frame, hl2m_sequence_event_out *out_events, int out_cap, int *io_count)
{
    hl2m_sequence_event_out tmp[HL2M_EVENT_WINDOW_MAX];
    int tmp_count;
    int i;
    int r;
    if (io_count == 0) return HL2M_ERR_NULL;
    tmp_count = 0;
    if (first_frame > last_frame) return HL2M_OK;
    r = hl2m_collect_qc_sequence_events(qc, sequence_name, first_frame, last_frame, tmp, HL2M_EVENT_WINDOW_MAX, &tmp_count);
    if (r != HL2M_OK) return r;
    for (i = 0; i < tmp_count; ++i) {
        if (*io_count < out_cap && out_events != 0) out_events[*io_count] = tmp[i];
        ++(*io_count);
    }
    return HL2M_OK;
}

int hl2m_event_cursor_advance(const hl2m_qc *qc, hl2m_event_cursor *cursor, int next_frame, hl2m_sequence_event_out *out_events, int out_cap, int *out_count)
{
    int count;
    int r;
    if (qc == 0 || cursor == 0 || out_count == 0) return HL2M_ERR_NULL;
    if (out_cap < 0) return HL2M_ERR_RANGE;
    count = 0;
    cursor->previous_frame = cursor->current_frame;
    if (next_frame >= cursor->current_frame) {
        r = hl2m_event_cursor_append_range(qc, cursor->sequence_name, cursor->current_frame + 1, next_frame, out_events, out_cap, &count);
        if (r != HL2M_OK) return r;
    } else {
        r = hl2m_event_cursor_append_range(qc, cursor->sequence_name, cursor->current_frame + 1, cursor->last_frame, out_events, out_cap, &count);
        if (r != HL2M_OK) return r;
        r = hl2m_event_cursor_append_range(qc, cursor->sequence_name, 0, next_frame, out_events, out_cap, &count);
        if (r != HL2M_OK) return r;
        ++cursor->loop_count;
    }
    cursor->current_frame = next_frame;
    *out_count = count;
    return (count > out_cap) ? HL2M_ERR_LIMIT : HL2M_OK;
}

int hl2m_select_lod_from_distance(const hl2m_compile_options *opt, hl2m_fixed distance, int *out_lod, hl2m_lod_select_report *out_report)
{
    int i;
    int lod;
    hl2m_lod_select_report local_report;
    if (opt == 0 || out_lod == 0) return HL2M_ERR_NULL;
    hl2m_clear_lod_select_report(&local_report);
    lod = 0;
    for (i = 0; i < opt->lod_count && i < HL2M_MAX_COMPILE_LODS; ++i) {
        if (distance >= opt->lods[i].switch_point) lod = i + 1;
    }
    if (lod > opt->lod_count) lod = opt->lod_count;
    local_report.selected_lod = lod;
    local_report.distance = distance;
    if (lod > 0) {
        int idx;
        idx = lod - 1;
        local_report.switch_point = opt->lods[idx].switch_point;
        if (opt->lods[idx].replace_from[0] != 0 && opt->lods[idx].replace_to[0] != 0) local_report.replacement_count = 1;
    }
    *out_lod = lod;
    if (out_report != 0) *out_report = local_report;
    return HL2M_OK;
}


int hl2m_engine_filter_draws_for_lod(const hl2m_engine_model *model, int lod, hl2m_engine_model *out_model, hl2m_draw_filter_report *out_report)
{
    int i;
    int write_i;
    int match_count;
    int used_lod;
    hl2m_draw_filter_report local_report;
    if (model == 0 || out_model == 0) return HL2M_ERR_NULL;
    hl2m_clear_draw_filter_report(&local_report);
    local_report.requested_lod = lod;
    match_count = 0;
    for (i = 0; i < model->draw_count; ++i) {
        if (model->draws[i].lod == lod) ++match_count;
    }
    used_lod = lod;
    if (match_count == 0 && lod != 0) {
        used_lod = 0;
        local_report.warning_flags |= HL2M_DRAW_FILTER_WARN_FALLBACK_LOD;
        for (i = 0; i < model->draw_count; ++i) {
            if (model->draws[i].lod == 0) ++match_count;
        }
    }
    if (out_model != model) *out_model = *model;
    write_i = 0;
    for (i = 0; i < model->draw_count; ++i) {
        if (model->draws[i].lod == used_lod || (match_count == 0 && used_lod == lod)) {
            if (write_i < HL2M_MAX_ENGINE_DRAWS) {
                out_model->draws[write_i] = model->draws[i];
                local_report.kept_index_count += model->draws[i].num_indices;
                ++write_i;
            }
        } else {
            ++local_report.skipped_draw_count;
        }
    }
    out_model->draw_count = write_i;
    local_report.used_lod = used_lod;
    local_report.kept_draw_count = write_i;
    if (out_report != 0) *out_report = local_report;
    return HL2M_OK;
}

int hl2m_runtime_eval_sequence(const hl2m_qc *qc, const hl2m_smd *bind_smd, const hl2m_smd *anim_smd, const hl2m_engine_model *base_model, const hl2m_vta *vta, const hl2m_flex_controller_state *flex_state, const hl2m_compile_options *opt, const char *sequence_name, hl2m_fixed frame_time, hl2m_fixed distance, int include_rotation, hl2m_engine_model *out_model, hl2m_sequence_event_out *out_events, int event_cap, int *out_event_count, hl2m_sequence_runtime_report *out_report)
{
    hl2m_ik_pose bind_pose;
    hl2m_ik_pose sample_pose;
    hl2m_ik_pose solved_pose;
    hl2m_pose_matrices inv_bind;
    hl2m_pose_matrices pose_world;
    hl2m_anim_sample_report sample_report;
    hl2m_ik_report ik_report;
    hl2m_skin_report skin_report;
    hl2m_flex_eval_report flex_report;
    hl2m_fixed flex_weights[HL2M_MAX_VTA_FLEXES];
    hl2m_lod_select_report lod_report;
    hl2m_draw_filter_report draw_report;
    hl2m_attachment_report attachment_report;
    hl2m_sequence_runtime_report local_report;
    int r;
    int lod;
    int event_count;
    int attachment_count;
    if (qc == 0 || bind_smd == 0 || anim_smd == 0 || base_model == 0 || out_model == 0) return HL2M_ERR_NULL;
    if (event_cap < 0) return HL2M_ERR_RANGE;
    hl2m_clear_sequence_runtime_report(&local_report);
    if (out_event_count != 0) *out_event_count = 0;
    r = hl2m_sample_smd_pose_lerp(bind_smd, 0, 0, 0, &bind_pose, &sample_report);
    if (r != HL2M_OK) return r;
    r = hl2m_sample_smd_pose_time(anim_smd, frame_time, &sample_pose, &sample_report);
    if (r != HL2M_OK) {
        local_report.warning_flags |= HL2M_RUNTIME_WARN_SAMPLE;
        return r;
    }
    local_report.sampled_bones = sample_pose.bone_count;
    r = hl2m_solve_ik_rules(qc, &sample_pose, sample_report.frame_a, &solved_pose, &ik_report);
    if (r != HL2M_OK) {
        local_report.warning_flags |= HL2M_RUNTIME_WARN_IK;
        solved_pose = sample_pose;
    }
    local_report.ik_solved_count = ik_report.solved_count;
    r = hl2m_pose_inverse_bind_from_pose(&bind_pose, include_rotation, &inv_bind); if (r != HL2M_OK) return r;
    r = hl2m_pose_matrices_from_pose(&solved_pose, include_rotation, &pose_world); if (r != HL2M_OK) return r;
    if (vta != 0 && flex_state != 0 && vta->flex_count > 0) {
        r = hl2m_engine_apply_flex_controllers(base_model, vta, qc, flex_state, out_model);
        if (r != HL2M_OK) {
            local_report.warning_flags |= HL2M_RUNTIME_WARN_FLEX;
            *out_model = *base_model;
        } else {
            r = hl2m_flex_eval_weights(qc, vta, flex_state, flex_weights, &flex_report);
            if (r == HL2M_OK) local_report.active_flex_count = flex_report.active_flex_count;
        }
    } else {
        *out_model = *base_model;
    }
    r = hl2m_engine_cpu_skin(out_model, &inv_bind, &pose_world, out_model, &skin_report);
    if (r != HL2M_OK) {
        local_report.warning_flags |= HL2M_RUNTIME_WARN_SKIN;
        return r;
    }
    local_report.skinned_vertices = skin_report.skinned_count;
    event_count = 0;
    if (out_events != 0 && event_cap > 0) {
        r = hl2m_collect_qc_sequence_events(qc, sequence_name, sample_report.frame_a, sample_report.frame_b, out_events, event_cap, &event_count);
    } else {
        r = hl2m_collect_qc_sequence_events(qc, sequence_name, sample_report.frame_a, sample_report.frame_b, 0, 0, &event_count);
    }
    if (r != HL2M_OK && r != HL2M_ERR_LIMIT) local_report.warning_flags |= HL2M_RUNTIME_WARN_EVENTS;
    local_report.event_count = event_count;
    if (out_event_count != 0) *out_event_count = event_count;
    lod = 0;
    if (opt != 0) {
        r = hl2m_select_lod_from_distance(opt, distance, &lod, &lod_report);
        if (r != HL2M_OK) local_report.warning_flags |= HL2M_RUNTIME_WARN_LOD;
        else local_report.selected_lod = lod;
        r = hl2m_engine_filter_draws_for_lod(out_model, lod, out_model, &draw_report);
        if (r != HL2M_OK) local_report.warning_flags |= HL2M_RUNTIME_WARN_LOD;
    }
    attachment_count = 0;
    r = hl2m_build_attachment_poses_from_qc(qc, &pose_world, 0, 0, &attachment_count, &attachment_report);
    if (r == HL2M_OK || r == HL2M_ERR_LIMIT) local_report.attachment_count = attachment_count;
    local_report.hitbox_count = qc->qc_hitbox_count;
    if (out_report != 0) *out_report = local_report;
    return HL2M_OK;
}

int hl2m_convex_decomp_split_long_axes(const hl2m_convex_decomp *in_decomp, int max_depth, hl2m_convex_decomp *out_decomp)
{
    int i;
    int depth;
    int r;
    if (in_decomp == 0 || out_decomp == 0) return HL2M_ERR_NULL;
    hl2m_clear_convex_decomp(out_decomp);
    depth = max_depth;
    if (depth < 0) depth = 0;
    if (depth > 4) depth = 4;
    for (i = 0; i < in_decomp->hull_count; ++i) {
        r = hl2m_convex_add_split_recursive(out_decomp, &in_decomp->hulls[i], in_decomp->hulls[i].bbmin, in_decomp->hulls[i].bbmax, depth);
        if (r != HL2M_OK) return r;
        if (out_decomp->hull_count >= HL2M_MAX_CONVEX_HULLS) break;
    }
    return HL2M_OK;
}

int hl2m_build_studiomdl_parity_report(const hl2m_qc *qc, const hl2m_smd *smd, const hl2m_vta *vta, const hl2m_mdl *mdl, const hl2m_vtx *vtx, const hl2m_phy *phy, const hl2m_ani *ani, hl2m_studiomdl_parity_report *out_report)
{
    int all_flags;
    if (out_report == 0) return HL2M_ERR_NULL;
    hl2m_clear_studiomdl_parity_report(out_report);
    all_flags = HL2M_PARITY_QC_STRUCTURED | HL2M_PARITY_BINARY_WRITERS | HL2M_PARITY_VTA_FLEX |
                HL2M_PARITY_FLEX_RULE_EVAL | HL2M_PARITY_IK_METADATA | HL2M_PARITY_IK_RUNTIME |
                HL2M_PARITY_SEQUENCE_EVENTS | HL2M_PARITY_HITBOXES | HL2M_PARITY_PHY_CONVEX |
                HL2M_PARITY_LOD_WRITER | HL2M_PARITY_ANIM_BLOCKS | HL2M_PARITY_MATERIAL_REPLACEMENTS |
                HL2M_PARITY_ANIM_SAMPLER | HL2M_PARITY_CPU_SKINNING |
                HL2M_PARITY_EVENT_CURSOR | HL2M_PARITY_LOD_RUNTIME |
                HL2M_PARITY_POSE_BLEND | HL2M_PARITY_ATTACHMENT_RUNTIME |
                HL2M_PARITY_HITBOX_RUNTIME | HL2M_PARITY_DRAW_FILTER |
                HL2M_PARITY_SEQUENCE_EVAL;
    if (qc != 0) {
        out_report->qc_command_count = qc->command_count;
        out_report->qc_structured_items = qc->body_count + qc->sequence_count + qc->material_dir_count + qc->lod_count +
                                          qc->attachment_count + qc->qc_hitbox_count + qc->definebone_count +
                                          qc->poseparameter_count + qc->material_replace_count + qc->collision_count +
                                          qc->flexcontroller_count + qc->flexrule_count + qc->sequence_event_count +
                                          qc->ikchain_count + qc->ikrule_count;
        if (out_report->qc_structured_items > 0) out_report->feature_flags |= HL2M_PARITY_QC_STRUCTURED;
        if (qc->flexcontroller_count > 0 || qc->flexrule_count > 0) out_report->feature_flags |= HL2M_PARITY_FLEX_RULE_EVAL;
        if (qc->ikchain_count > 0 || qc->ikrule_count > 0) out_report->feature_flags |= HL2M_PARITY_IK_METADATA | HL2M_PARITY_IK_RUNTIME;
        if (qc->sequence_event_count > 0) out_report->feature_flags |= HL2M_PARITY_SEQUENCE_EVENTS | HL2M_PARITY_EVENT_CURSOR | HL2M_PARITY_SEQUENCE_EVAL;
        if (qc->attachment_count > 0) out_report->feature_flags |= HL2M_PARITY_ATTACHMENT_RUNTIME;
        if (qc->qc_hitbox_count > 0) out_report->feature_flags |= HL2M_PARITY_HITBOXES | HL2M_PARITY_HITBOX_RUNTIME;
        if (qc->lod_count > 0) out_report->feature_flags |= HL2M_PARITY_LOD_WRITER | HL2M_PARITY_LOD_RUNTIME;
        if (qc->material_replace_count > 0) out_report->feature_flags |= HL2M_PARITY_MATERIAL_REPLACEMENTS;
    }
    if (smd != 0) {
        out_report->feature_flags |= HL2M_PARITY_BINARY_WRITERS | HL2M_PARITY_ANIM_SAMPLER | HL2M_PARITY_CPU_SKINNING | HL2M_PARITY_POSE_BLEND | HL2M_PARITY_SEQUENCE_EVAL | HL2M_PARITY_DRAW_FILTER;
    }
    if (vta != 0 && vta->flex_count > 0) {
        out_report->feature_flags |= HL2M_PARITY_VTA_FLEX;
    }
    if (mdl != 0) {
        out_report->mdl_hitboxes = mdl->hitbox_count;
        out_report->mdl_events = mdl->event_count;
        out_report->mdl_ikrules = mdl->ikrule_count;
        out_report->binary_table_count += (mdl->bone_count > 0) ? 1 : 0;
        out_report->binary_table_count += (mdl->texture_count > 0) ? 1 : 0;
        out_report->binary_table_count += (mdl->bodypart_count > 0) ? 1 : 0;
        out_report->binary_table_count += (mdl->mesh_count > 0) ? 1 : 0;
        out_report->binary_table_count += (mdl->hitbox_count > 0) ? 1 : 0;
        out_report->binary_table_count += (mdl->event_count > 0) ? 1 : 0;
        out_report->binary_table_count += (mdl->ikrule_count > 0) ? 1 : 0;
        if (mdl->hitbox_count > 0) out_report->feature_flags |= HL2M_PARITY_HITBOXES | HL2M_PARITY_HITBOX_RUNTIME;
        if (mdl->event_count > 0) out_report->feature_flags |= HL2M_PARITY_SEQUENCE_EVENTS;
        if (mdl->ikrule_count > 0) out_report->feature_flags |= HL2M_PARITY_IK_METADATA;
        if (mdl->animblock_count > 0) out_report->feature_flags |= HL2M_PARITY_ANIM_BLOCKS;
    }
    if (vtx != 0) {
        out_report->vtx_lods = vtx->lod_count;
        out_report->binary_table_count += (vtx->index_count > 0) ? 1 : 0;
        if (vtx->lod_count > 0) out_report->feature_flags |= HL2M_PARITY_LOD_WRITER;
        if (vtx->material_replacement_count > 0) out_report->feature_flags |= HL2M_PARITY_MATERIAL_REPLACEMENTS;
    }
    if (phy != 0) {
        out_report->phy_solids = phy->solid_count;
        if (phy->solid_count > 0) out_report->feature_flags |= HL2M_PARITY_PHY_CONVEX;
    }
    if (ani != 0) {
        out_report->ani_blocks = ani->animblock_count;
        if (ani->animblock_count > 0 || ani->scanned_channel_count > 0) out_report->feature_flags |= HL2M_PARITY_ANIM_BLOCKS;
    }
    if ((out_report->feature_flags & HL2M_PARITY_FLEX_RULE_EVAL) != 0) ++out_report->runtime_bridge_count;
    if ((out_report->feature_flags & HL2M_PARITY_IK_RUNTIME) != 0) ++out_report->runtime_bridge_count;
    if ((out_report->feature_flags & HL2M_PARITY_SEQUENCE_EVENTS) != 0) ++out_report->runtime_bridge_count;
    if ((out_report->feature_flags & HL2M_PARITY_POSE_BLEND) != 0) ++out_report->runtime_bridge_count;
    if ((out_report->feature_flags & HL2M_PARITY_ATTACHMENT_RUNTIME) != 0) ++out_report->runtime_bridge_count;
    if ((out_report->feature_flags & HL2M_PARITY_HITBOX_RUNTIME) != 0) ++out_report->runtime_bridge_count;
    if ((out_report->feature_flags & HL2M_PARITY_DRAW_FILTER) != 0) ++out_report->runtime_bridge_count;
    if ((out_report->feature_flags & HL2M_PARITY_SEQUENCE_EVAL) != 0) ++out_report->runtime_bridge_count;
    out_report->missing_flags = all_flags & ~out_report->feature_flags;
    if (out_report->missing_flags != 0) out_report->warning_flags |= HL2M_COMPILE_WARN_STUDIOMDL_GAP;
    return HL2M_OK;
}

int hl2m_write_phy_from_convex_decomp(const hl2m_convex_decomp *decomp, const hl2m_compile_options *opt, hl2m_u8 *out_data, unsigned int out_cap, unsigned int *out_size)
{
    hl2m_compile_options o;
    unsigned int off;
    unsigned int kv_off;
    unsigned int kv_len;
    int r;
    int solid;
    int solid_count;
    if (decomp == 0 || out_data == 0 || out_size == 0) return HL2M_ERR_NULL;
    hl2m_compile_get_options(opt, &o);
    solid_count = decomp->hull_count;
    if (solid_count <= 0) return HL2M_ERR_RANGE;
    if (solid_count > HL2M_MAX_PHY_SOLIDS) solid_count = HL2M_MAX_PHY_SOLIDS;
    kv_len = 64u;
    if (!hl2m_put_range(0u, HL2M_PHY_HEADER_SIZE + (unsigned int)solid_count * HL2M_PHY_COMPACT_SURFACE_HEADER_SIZE + kv_len, out_cap)) return HL2M_ERR_RANGE;
    hl2m_memzero(out_data, HL2M_PHY_HEADER_SIZE + (unsigned int)solid_count * HL2M_PHY_COMPACT_SURFACE_HEADER_SIZE + kv_len);
    r = hl2m_put_i32(out_data, out_cap, 0u, HL2M_PHY_HEADER_SIZE); if (r != HL2M_OK) return r;
    r = hl2m_put_i32(out_data, out_cap, 4u, (int)HL2M_PHY_MAGIC_VPHY); if (r != HL2M_OK) return r;
    r = hl2m_put_i32(out_data, out_cap, 8u, solid_count); if (r != HL2M_OK) return r;
    r = hl2m_put_i32(out_data, out_cap, 12u, o.checksum); if (r != HL2M_OK) return r;
    off = HL2M_PHY_HEADER_SIZE;
    for (solid = 0; solid < solid_count; ++solid) {
        const hl2m_convex_hull *h;
        hl2m_vec3 extent;
        h = &decomp->hulls[solid];
        extent.x = (h->bbmax.x - h->bbmin.x) / 2;
        extent.y = (h->bbmax.y - h->bbmin.y) / 2;
        extent.z = (h->bbmax.z - h->bbmin.z) / 2;
        if (extent.x < 0) extent.x = -extent.x;
        if (extent.y < 0) extent.y = -extent.y;
        if (extent.z < 0) extent.z = -extent.z;
        r = hl2m_put_i32(out_data, out_cap, off + 0u, (int)HL2M_PHY_COMPACT_SURFACE_HEADER_SIZE); if (r != HL2M_OK) return r;
        r = hl2m_put_i32(out_data, out_cap, off + 4u, (int)HL2M_PHY_MAGIC_VPHY); if (r != HL2M_OK) return r;
        r = hl2m_put_u16(out_data, out_cap, off + 8u, 16); if (r != HL2M_OK) return r;
        r = hl2m_put_u16(out_data, out_cap, off + 10u, h->bone >= 0 ? h->bone : 0); if (r != HL2M_OK) return r;
        r = hl2m_put_i32(out_data, out_cap, off + 12u, 0); if (r != HL2M_OK) return r;
        r = hl2m_put_vec3(out_data, out_cap, off + 16u, extent); if (r != HL2M_OK) return r;
        r = hl2m_put_i32(out_data, out_cap, off + 28u, solid); if (r != HL2M_OK) return r;
        off += HL2M_PHY_COMPACT_SURFACE_HEADER_SIZE;
    }
    kv_off = off;
    r = hl2m_put_z(out_data, out_cap, kv_off, "solid { cooked hl2modelib_convex_multi_aabb }", kv_len); if (r != HL2M_OK) return r;
    *out_size = hl2m_align4u(kv_off + kv_len);
    return HL2M_OK;
}

int hl2m_engine_model_bounds(const hl2m_engine_model *model, hl2m_vec3 *out_min, hl2m_vec3 *out_max)
{
    int i;
    hl2m_vec3 mn;
    hl2m_vec3 mx;
    if (model == 0 || out_min == 0 || out_max == 0) return HL2M_ERR_NULL;
    if (model->vertex_count <= 0) return HL2M_ERR_RANGE;
    mn = model->vertices[0].position;
    mx = model->vertices[0].position;
    for (i = 1; i < model->vertex_count; ++i) {
        const hl2m_vec3 *p;
        p = &model->vertices[i].position;
        if (p->x < mn.x) mn.x = p->x;
        if (p->y < mn.y) mn.y = p->y;
        if (p->z < mn.z) mn.z = p->z;
        if (p->x > mx.x) mx.x = p->x;
        if (p->y > mx.y) mx.y = p->y;
        if (p->z > mx.z) mx.z = p->z;
    }
    *out_min = mn;
    *out_max = mx;
    return HL2M_OK;
}

int hl2m_engine_model_validate(const hl2m_engine_model *model)
{
    int i;
    int d;
    if (model == 0) return HL2M_ERR_NULL;
    if (model->vertex_count < 0 || model->vertex_count > HL2M_MAX_ENGINE_VERTICES) return HL2M_ERR_RANGE;
    if (model->index_count < 0 || model->index_count > HL2M_MAX_ENGINE_INDICES) return HL2M_ERR_RANGE;
    if (model->draw_count < 0 || model->draw_count > HL2M_MAX_ENGINE_DRAWS) return HL2M_ERR_RANGE;
    for (i = 0; i < model->index_count; ++i) {
        if (model->indices[i] < 0 || model->indices[i] >= model->vertex_count) return HL2M_ERR_RANGE;
    }
    for (d = 0; d < model->draw_count; ++d) {
        if (model->draws[d].first_index < 0) return HL2M_ERR_RANGE;
        if (model->draws[d].num_indices < 0) return HL2M_ERR_RANGE;
        if (model->draws[d].first_index + model->draws[d].num_indices > model->index_count) return HL2M_ERR_RANGE;
        if (model->draws[d].first_vertex < 0) return HL2M_ERR_RANGE;
        if (model->draws[d].num_vertices < 0) return HL2M_ERR_RANGE;
        if (model->draws[d].first_vertex + model->draws[d].num_vertices > model->vertex_count) return HL2M_ERR_RANGE;
    }
    return HL2M_OK;
}


int hl2m_engine_model_write_obj(const hl2m_engine_model *model, const char *object_name, char *out_text, unsigned int out_size, unsigned int *out_used)
{
    unsigned int used;
    int ret;
    int i;
    if (model == 0) return HL2M_ERR_NULL;
    ret = hl2m_engine_model_validate(model);
    if (ret != HL2M_OK) return ret;
    used = 0u;
    ret = HL2M_OK;
    if (out_text != 0 && out_size != 0u) out_text[0] = 0;
    if (object_name == 0 || object_name[0] == 0) object_name = "hl2m_engine_model";
    if (hl2m_out_append_text(out_text, out_size, &used, "o ") != HL2M_OK) ret = HL2M_ERR_LIMIT;
    if (hl2m_out_append_text(out_text, out_size, &used, object_name) != HL2M_OK) ret = HL2M_ERR_LIMIT;
    if (hl2m_out_append_char(out_text, out_size, &used, '\n') != HL2M_OK) ret = HL2M_ERR_LIMIT;
    for (i = 0; i < model->vertex_count; ++i) {
        if (hl2m_out_append_text(out_text, out_size, &used, "v ") != HL2M_OK) ret = HL2M_ERR_LIMIT;
        if (hl2m_append_fixed_qc(out_text, out_size, &used, model->vertices[i].position.x) != HL2M_OK) ret = HL2M_ERR_LIMIT;
        if (hl2m_out_append_char(out_text, out_size, &used, ' ') != HL2M_OK) ret = HL2M_ERR_LIMIT;
        if (hl2m_append_fixed_qc(out_text, out_size, &used, model->vertices[i].position.y) != HL2M_OK) ret = HL2M_ERR_LIMIT;
        if (hl2m_out_append_char(out_text, out_size, &used, ' ') != HL2M_OK) ret = HL2M_ERR_LIMIT;
        if (hl2m_append_fixed_qc(out_text, out_size, &used, model->vertices[i].position.z) != HL2M_OK) ret = HL2M_ERR_LIMIT;
        if (hl2m_out_append_char(out_text, out_size, &used, '\n') != HL2M_OK) ret = HL2M_ERR_LIMIT;
    }
    for (i = 0; i < model->vertex_count; ++i) {
        if (hl2m_out_append_text(out_text, out_size, &used, "vt ") != HL2M_OK) ret = HL2M_ERR_LIMIT;
        if (hl2m_append_fixed_qc(out_text, out_size, &used, model->vertices[i].texcoord.x) != HL2M_OK) ret = HL2M_ERR_LIMIT;
        if (hl2m_out_append_char(out_text, out_size, &used, ' ') != HL2M_OK) ret = HL2M_ERR_LIMIT;
        if (hl2m_append_fixed_qc(out_text, out_size, &used, model->vertices[i].texcoord.y) != HL2M_OK) ret = HL2M_ERR_LIMIT;
        if (hl2m_out_append_char(out_text, out_size, &used, '\n') != HL2M_OK) ret = HL2M_ERR_LIMIT;
    }
    for (i = 0; i < model->vertex_count; ++i) {
        if (hl2m_out_append_text(out_text, out_size, &used, "vn ") != HL2M_OK) ret = HL2M_ERR_LIMIT;
        if (hl2m_append_fixed_qc(out_text, out_size, &used, model->vertices[i].normal.x) != HL2M_OK) ret = HL2M_ERR_LIMIT;
        if (hl2m_out_append_char(out_text, out_size, &used, ' ') != HL2M_OK) ret = HL2M_ERR_LIMIT;
        if (hl2m_append_fixed_qc(out_text, out_size, &used, model->vertices[i].normal.y) != HL2M_OK) ret = HL2M_ERR_LIMIT;
        if (hl2m_out_append_char(out_text, out_size, &used, ' ') != HL2M_OK) ret = HL2M_ERR_LIMIT;
        if (hl2m_append_fixed_qc(out_text, out_size, &used, model->vertices[i].normal.z) != HL2M_OK) ret = HL2M_ERR_LIMIT;
        if (hl2m_out_append_char(out_text, out_size, &used, '\n') != HL2M_OK) ret = HL2M_ERR_LIMIT;
    }
    for (i = 0; i + 2 < model->index_count; i += 3) {
        int a;
        int b;
        int c;
        a = model->indices[i] + 1;
        b = model->indices[i + 1] + 1;
        c = model->indices[i + 2] + 1;
        if (hl2m_out_append_text(out_text, out_size, &used, "f ") != HL2M_OK) ret = HL2M_ERR_LIMIT;
        if (hl2m_out_append_int(out_text, out_size, &used, a) != HL2M_OK) ret = HL2M_ERR_LIMIT;
        if (hl2m_out_append_char(out_text, out_size, &used, '/') != HL2M_OK) ret = HL2M_ERR_LIMIT;
        if (hl2m_out_append_int(out_text, out_size, &used, a) != HL2M_OK) ret = HL2M_ERR_LIMIT;
        if (hl2m_out_append_char(out_text, out_size, &used, '/') != HL2M_OK) ret = HL2M_ERR_LIMIT;
        if (hl2m_out_append_int(out_text, out_size, &used, a) != HL2M_OK) ret = HL2M_ERR_LIMIT;
        if (hl2m_out_append_char(out_text, out_size, &used, ' ') != HL2M_OK) ret = HL2M_ERR_LIMIT;
        if (hl2m_out_append_int(out_text, out_size, &used, b) != HL2M_OK) ret = HL2M_ERR_LIMIT;
        if (hl2m_out_append_char(out_text, out_size, &used, '/') != HL2M_OK) ret = HL2M_ERR_LIMIT;
        if (hl2m_out_append_int(out_text, out_size, &used, b) != HL2M_OK) ret = HL2M_ERR_LIMIT;
        if (hl2m_out_append_char(out_text, out_size, &used, '/') != HL2M_OK) ret = HL2M_ERR_LIMIT;
        if (hl2m_out_append_int(out_text, out_size, &used, b) != HL2M_OK) ret = HL2M_ERR_LIMIT;
        if (hl2m_out_append_char(out_text, out_size, &used, ' ') != HL2M_OK) ret = HL2M_ERR_LIMIT;
        if (hl2m_out_append_int(out_text, out_size, &used, c) != HL2M_OK) ret = HL2M_ERR_LIMIT;
        if (hl2m_out_append_char(out_text, out_size, &used, '/') != HL2M_OK) ret = HL2M_ERR_LIMIT;
        if (hl2m_out_append_int(out_text, out_size, &used, c) != HL2M_OK) ret = HL2M_ERR_LIMIT;
        if (hl2m_out_append_char(out_text, out_size, &used, '/') != HL2M_OK) ret = HL2M_ERR_LIMIT;
        if (hl2m_out_append_int(out_text, out_size, &used, c) != HL2M_OK) ret = HL2M_ERR_LIMIT;
        if (hl2m_out_append_char(out_text, out_size, &used, '\n') != HL2M_OK) ret = HL2M_ERR_LIMIT;
    }
    if (out_used != 0) *out_used = used;
    return ret;
}


/* Format-specific vendor module; private helper ABI intentionally local. */
#include "../vendors/source/mesher/hl2m_mesher_build89.inc"

static int hl2m_starts_ci_v05(const char *text, const char *prefix)
{
    while (*prefix != 0) {
        if (*text == 0) return 0;
        if (hl2m_lower_ascii(*text) != hl2m_lower_ascii(*prefix)) return 0;
        ++text;
        ++prefix;
    }
    return 1;
}

void hl2m_clear_vta(hl2m_vta *out_vta)
{
    if (out_vta != 0) hl2m_memzero(out_vta, (unsigned int)sizeof(*out_vta));
}

void hl2m_clear_compile_options(hl2m_compile_options *out_options)
{
    int i;
    if (out_options == 0) return;
    hl2m_memzero(out_options, (unsigned int)sizeof(*out_options));
    out_options->checksum = 0x484c3250;
    out_options->emit_phy = 1;
    out_options->emit_ani = 1;
    out_options->emit_vta_flexes = 1;
    out_options->normalize_weights = 1;
    out_options->emit_material_replacements = 1;
    out_options->emit_flex_rules = 1;
    out_options->emit_sequence_events = 1;
    out_options->emit_ik_rules = 1;
    out_options->collision_mode = HL2M_COLLISION_AABB;
    out_options->convex_max_hulls = HL2M_MAX_CONVEX_HULLS;
    out_options->lod_count = 1;
    hl2m_copy_token(out_options->body_name, HL2M_NAME_MAX, "body");
    hl2m_copy_token(out_options->surfaceprop, HL2M_NAME_MAX, "default");
    for (i = 0; i < HL2M_MAX_COMPILE_LODS; ++i) {
        out_options->lods[i].switch_point = (hl2m_fixed)(i * 10 * HL2M_FIXED_ONE);
        out_options->lods[i].triangle_step = i + 1;
    }
}

void hl2m_clear_compile_result(hl2m_compile_result *out_result)
{
    if (out_result != 0) hl2m_memzero(out_result, (unsigned int)sizeof(*out_result));
}

void hl2m_clear_bundle_report(hl2m_bundle_report *out_report)
{
    if (out_report != 0) hl2m_memzero(out_report, (unsigned int)sizeof(*out_report));
}

int hl2m_make_compile_options_from_qc(const hl2m_qc *qc, hl2m_compile_options *out_options)
{
    int i;
    int lod_i;
    if (out_options == 0) return HL2M_ERR_NULL;
    hl2m_clear_compile_options(out_options);
    if (qc == 0) return HL2M_OK;
    if (qc->modelname[0] != 0) hl2m_copy_token(out_options->model_name, HL2M_PATH_MAX, qc->modelname);
    if (qc->body_count > 0) hl2m_copy_token(out_options->body_name, HL2M_NAME_MAX, qc->bodies[0].name);
    for (i = 0; i < qc->command_count; ++i) {
        if (qc->commands[i].kind == HL2M_QC_SURFACEPROP && qc->commands[i].argc > 0) {
            hl2m_copy_token(out_options->surfaceprop, HL2M_NAME_MAX, qc->commands[i].args[0]);
        }
    }
    if (qc->lod_count > 0) {
        out_options->lod_count = qc->lod_count + 1;
        if (out_options->lod_count > HL2M_MAX_COMPILE_LODS) out_options->lod_count = HL2M_MAX_COMPILE_LODS;
        out_options->lods[0].switch_point = 0;
        out_options->lods[0].triangle_step = 1;
        for (lod_i = 1; lod_i < out_options->lod_count; ++lod_i) {
            out_options->lods[lod_i].switch_point = qc->lods[lod_i - 1].switch_point;
            out_options->lods[lod_i].triangle_step = lod_i + 1;
        }
    }
    for (i = 0; i < qc->collision_count; ++i) {
        out_options->emit_phy = 1;
        if (qc->collisions[i].joints != 0) out_options->collision_mode = HL2M_COLLISION_BONE_MATERIAL_AABB;
    }
    if (qc->material_replace_count <= 0) out_options->emit_material_replacements = 0;
    return HL2M_OK;
}

int hl2m_validate_compile_inputs(const hl2m_qc *qc, const hl2m_smd *reference_smd, const hl2m_smd *animation_smd, const hl2m_vta *vta, hl2m_compile_result *out_result)
{
    int i;
    int j;
    int total;
    hl2m_compile_result r;
    hl2m_clear_compile_result(&r);
    if (reference_smd == 0) return HL2M_ERR_NULL;
    r.bone_count = reference_smd->node_count;
    r.material_count = reference_smd->material_count;
    r.vertex_count = reference_smd->vertex_count;
    r.triangle_count = reference_smd->triangle_count;
    if (qc != 0) {
        r.qc_body_count = qc->body_count;
        r.hitbox_count = qc->qc_hitbox_count;
        r.flex_controller_count = qc->flexcontroller_count;
        r.flex_rule_count = qc->flexrule_count;
        r.sequence_event_count = qc->sequence_event_count;
        r.ik_chain_count = qc->ikchain_count;
        r.ik_rule_count = qc->ikrule_count;
        if (qc->flexrule_count > 0) r.warning_flags |= HL2M_COMPILE_WARN_FLEX_RULE_SIMPLE;
        if (qc->sequence_event_count > 0) r.warning_flags |= HL2M_COMPILE_WARN_SEQUENCE_EVENTS;
        if (qc->ikrule_count > 0 || qc->ikchain_count > 0) r.warning_flags |= HL2M_COMPILE_WARN_IK_METADATA;
        if (qc->modelname[0] == 0 || qc->body_count <= 0) r.warning_flags |= HL2M_COMPILE_WARN_QC_INCOMPLETE;
    } else {
        r.warning_flags |= HL2M_COMPILE_WARN_QC_INCOMPLETE;
    }
    if (animation_smd != 0) {
        r.anim_frame_count = hl2m_smd_frame_count(animation_smd);
        if (animation_smd->node_count != reference_smd->node_count) r.warning_flags |= HL2M_COMPILE_WARN_ANIM_NODE_MISMATCH;
    }
    if (vta != 0) {
        r.flex_count = vta->flex_count;
        for (i = 0; i < vta->vertex_count; ++i) {
            if (vta->vertices[i].source_vertex < 0 || vta->vertices[i].source_vertex >= reference_smd->vertex_count) {
                ++r.vta_range_count;
            }
        }
        if (r.vta_range_count > 0) r.warning_flags |= HL2M_COMPILE_WARN_VTA_RANGE;
    }
    for (i = 0; i < reference_smd->vertex_count; ++i) {
        const hl2m_smd_vertex *v;
        v = &reference_smd->vertices[i];
        if (v->parent_bone < 0 || v->parent_bone >= reference_smd->node_count) ++r.bad_bone_ref_count;
        if (v->weight_count <= 0) {
            ++r.weight_repair_count;
        } else {
            total = 0;
            for (j = 0; j < v->weight_count && j < HL2M_MAX_BONES_PER_VERT; ++j) {
                if (v->weights[j].bone < 0 || v->weights[j].bone >= reference_smd->node_count) ++r.bad_bone_ref_count;
                total += v->weights[j].weight;
            }
            if (total < HL2M_FIXED_ONE - 512 || total > HL2M_FIXED_ONE + 512) ++r.weight_repair_count;
        }
    }
    if (r.bad_bone_ref_count > 0) r.warning_flags |= HL2M_COMPILE_WARN_BAD_BONE_REF;
    if (r.weight_repair_count > 0) r.warning_flags |= HL2M_COMPILE_WARN_WEIGHT_REPAIRED;
    if (out_result != 0) *out_result = r;
    return HL2M_OK;
}

int hl2m_validate_binary_bundle(const hl2m_u8 *mdl_data, unsigned int mdl_size, const hl2m_u8 *vvd_data, unsigned int vvd_size, const hl2m_u8 *vtx_data, unsigned int vtx_size, const hl2m_u8 *phy_data, unsigned int phy_size, const hl2m_u8 *ani_data, unsigned int ani_size, hl2m_bundle_report *out_report)
{
    hl2m_bundle_report rep;
    hl2m_clear_bundle_report(&rep);
    rep.mdl_size = mdl_size;
    rep.vvd_size = vvd_size;
    rep.vtx_size = vtx_size;
    rep.phy_size = phy_size;
    rep.ani_size = ani_size;
    if (out_report == 0) return HL2M_ERR_NULL;
    if (mdl_data == 0 || mdl_size < HL2M_STUDIOHDR_MIN) {
        rep.warning_flags |= HL2M_BUNDLE_WARN_MDL_BAD_MAGIC;
    } else {
        if (hl2m_le_u32(mdl_data + 0u) != HL2M_MDL_MAGIC) rep.warning_flags |= HL2M_BUNDLE_WARN_MDL_BAD_MAGIC;
        rep.mdl_version = hl2m_le_i32(mdl_data + 4u);
        rep.mdl_checksum = hl2m_le_i32(mdl_data + 8u);
        rep.mdl_declared_length = hl2m_le_i32(mdl_data + 76u);
        if (rep.mdl_declared_length != (int)mdl_size) rep.warning_flags |= HL2M_BUNDLE_WARN_MDL_LENGTH_MISMATCH;
    }
    if (vvd_data == 0 || vvd_size < HL2M_VVD_HEADER_SIZE) {
        rep.warning_flags |= HL2M_BUNDLE_WARN_VVD_BAD_MAGIC;
    } else {
        if (hl2m_le_u32(vvd_data + 0u) != HL2M_VVD_MAGIC) rep.warning_flags |= HL2M_BUNDLE_WARN_VVD_BAD_MAGIC;
        rep.vvd_version = hl2m_le_i32(vvd_data + 4u);
        rep.vvd_checksum = hl2m_le_i32(vvd_data + 8u);
    }
    if (vtx_data == 0 || vtx_size < HL2M_VTX_FILE_SIZE) {
        rep.warning_flags |= HL2M_BUNDLE_WARN_VTX_BAD_VERSION;
    } else {
        rep.vtx_version = hl2m_le_i32(vtx_data + 0u);
        rep.vtx_checksum = hl2m_le_i32(vtx_data + 16u);
        if (rep.vtx_version != HL2M_VTX_VERSION) rep.warning_flags |= HL2M_BUNDLE_WARN_VTX_BAD_VERSION;
    }
    if (rep.mdl_checksum != rep.vvd_checksum || rep.mdl_checksum != rep.vtx_checksum) rep.warning_flags |= HL2M_BUNDLE_WARN_CHECKSUM_MISMATCH;
    if (phy_data != 0 && phy_size >= HL2M_PHY_HEADER_SIZE) {
        rep.phy_checksum = hl2m_le_i32(phy_data + 12u);
        if (rep.phy_checksum != rep.mdl_checksum) rep.warning_flags |= HL2M_BUNDLE_WARN_PHY_CHECKSUM_MISMATCH;
    } else {
        rep.warning_flags |= HL2M_BUNDLE_WARN_MISSING_OPTIONAL;
    }
    if (ani_data != 0 && ani_size >= HL2M_STUDIOHDR_MIN) {
        rep.ani_checksum = hl2m_le_i32(ani_data + 8u);
        if (rep.ani_checksum != rep.mdl_checksum) rep.warning_flags |= HL2M_BUNDLE_WARN_ANI_CHECKSUM_MISMATCH;
    } else {
        rep.warning_flags |= HL2M_BUNDLE_WARN_MISSING_OPTIONAL;
    }
    *out_report = rep;
    return HL2M_OK;
}

/* Standalone VTA format implementation, core-private text helpers. */
#include "../vendors/source/formats/vta_codec89.inc"
static unsigned int hl2m_align4u(unsigned int v)
{
    return (v + 3u) & ~3u;
}

static int hl2m_put_range(unsigned int off, unsigned int need, unsigned int cap)
{
    if (off > cap) return 0;
    if (need > cap - off) return 0;
    return 1;
}

static int hl2m_put_u8(hl2m_u8 *data, unsigned int cap, unsigned int off, int v)
{
    if (!hl2m_put_range(off, 1u, cap)) return HL2M_ERR_RANGE;
    data[off] = (hl2m_u8)(v & 255);
    return HL2M_OK;
}

static int hl2m_put_u16(hl2m_u8 *data, unsigned int cap, unsigned int off, int v)
{
    if (!hl2m_put_range(off, 2u, cap)) return HL2M_ERR_RANGE;
    data[off + 0u] = (hl2m_u8)(v & 255);
    data[off + 1u] = (hl2m_u8)((v >> 8) & 255);
    return HL2M_OK;
}

static int hl2m_put_i32(hl2m_u8 *data, unsigned int cap, unsigned int off, int v)
{
    hl2m_u32 u;
    if (!hl2m_put_range(off, 4u, cap)) return HL2M_ERR_RANGE;
    u = (hl2m_u32)v;
    data[off + 0u] = (hl2m_u8)(u & 255u);
    data[off + 1u] = (hl2m_u8)((u >> 8) & 255u);
    data[off + 2u] = (hl2m_u8)((u >> 16) & 255u);
    data[off + 3u] = (hl2m_u8)((u >> 24) & 255u);
    return HL2M_OK;
}

static hl2m_u32 hl2m_ieee32_from_fixed(hl2m_fixed value)
{
    hl2m_u32 sign;
    hl2m_u32 mag;
    int p;
    int exp;
    hl2m_u32 sig;
    hl2m_u32 mant;

    if (value == 0) return 0u;
    sign = 0u;
    if (value < 0) {
        sign = 0x80000000u;
        if (value == (hl2m_fixed)0x80000000u) mag = 0x80000000u;
        else mag = (hl2m_u32)(-value);
    } else {
        mag = (hl2m_u32)value;
    }
    p = 31;
    while (p > 0 && ((mag & ((hl2m_u32)1u << p)) == 0u)) --p;
    exp = p - 16 + 127;
    if (exp <= 0) return sign;
    if (exp >= 255) return sign | 0x7f800000u;
    if (p >= 23) sig = mag >> (p - 23);
    else sig = mag << (23 - p);
    mant = sig & 0x007fffffu;
    return sign | ((hl2m_u32)exp << 23) | mant;
}

static int hl2m_put_fixed(hl2m_u8 *data, unsigned int cap, unsigned int off, hl2m_fixed v)
{
    return hl2m_put_i32(data, cap, off, (int)hl2m_ieee32_from_fixed(v));
}

static int hl2m_put_vec3(hl2m_u8 *data, unsigned int cap, unsigned int off, hl2m_vec3 v)
{
    int r;
    r = hl2m_put_fixed(data, cap, off + 0u, v.x); if (r != HL2M_OK) return r;
    r = hl2m_put_fixed(data, cap, off + 4u, v.y); if (r != HL2M_OK) return r;
    r = hl2m_put_fixed(data, cap, off + 8u, v.z); if (r != HL2M_OK) return r;
    return HL2M_OK;
}

static int hl2m_put_vec4(hl2m_u8 *data, unsigned int cap, unsigned int off, hl2m_vec4 v)
{
    int r;
    r = hl2m_put_fixed(data, cap, off + 0u, v.x); if (r != HL2M_OK) return r;
    r = hl2m_put_fixed(data, cap, off + 4u, v.y); if (r != HL2M_OK) return r;
    r = hl2m_put_fixed(data, cap, off + 8u, v.z); if (r != HL2M_OK) return r;
    r = hl2m_put_fixed(data, cap, off + 12u, v.w); if (r != HL2M_OK) return r;
    return HL2M_OK;
}

static int hl2m_put_z(hl2m_u8 *data, unsigned int cap, unsigned int off, const char *text, unsigned int max_chars)
{
    unsigned int i;
    if (!hl2m_put_range(off, max_chars, cap)) return HL2M_ERR_RANGE;
    for (i = 0u; i < max_chars; ++i) data[off + i] = 0u;
    if (text == 0) return HL2M_OK;
    i = 0u;
    while (text[i] != 0 && i + 1u < max_chars) {
        data[off + i] = (hl2m_u8)text[i];
        ++i;
    }
    return HL2M_OK;
}

static int hl2m_append_z(hl2m_u8 *data, unsigned int cap, unsigned int *io_off, const char *text, unsigned int *out_off)
{
    unsigned int start;
    unsigned int i;
    if (data == 0 || io_off == 0 || out_off == 0) return HL2M_ERR_NULL;
    start = *io_off;
    i = 0u;
    while (text != 0 && text[i] != 0) ++i;
    if (!hl2m_put_range(start, i + 1u, cap)) return HL2M_ERR_RANGE;
    if (text != 0) {
        for (i = 0u; text[i] != 0; ++i) data[start + i] = (hl2m_u8)text[i];
    } else {
        i = 0u;
    }
    data[start + i] = 0u;
    *io_off = hl2m_align4u(start + i + 1u);
    while (start + i + 1u < *io_off && start + i + 1u < cap) {
        ++i;
        data[start + i] = 0u;
    }
    *out_off = start;
    return HL2M_OK;
}

static void hl2m_compile_get_options(const hl2m_compile_options *in_opt, hl2m_compile_options *out_opt)
{
    if (in_opt != 0) {
        *out_opt = *in_opt;
        if (out_opt->checksum == 0) out_opt->checksum = 0x484c3250;
        if (out_opt->lod_count <= 0) out_opt->lod_count = 1;
        if (out_opt->convex_max_hulls <= 0) out_opt->convex_max_hulls = HL2M_MAX_CONVEX_HULLS;
    } else {
        hl2m_clear_compile_options(out_opt);
    }
    if (out_opt->lod_count > HL2M_MAX_COMPILE_LODS) out_opt->lod_count = HL2M_MAX_COMPILE_LODS;
    if (out_opt->convex_max_hulls > HL2M_MAX_CONVEX_HULLS) out_opt->convex_max_hulls = HL2M_MAX_CONVEX_HULLS;
    if (out_opt->body_name[0] == 0) hl2m_copy_token(out_opt->body_name, HL2M_NAME_MAX, "body");
    if (out_opt->surfaceprop[0] == 0) hl2m_copy_token(out_opt->surfaceprop, HL2M_NAME_MAX, "default");
}

static int hl2m_smd_frame_count(const hl2m_smd *smd)
{
    int i;
    int max_time;
    if (smd == 0) return 0;
    max_time = -1;
    for (i = 0; i < smd->pose_count; ++i) {
        if (smd->poses[i].time > max_time) max_time = smd->poses[i].time;
    }
    return max_time + 1;
}

static int hl2m_smd_bounds(const hl2m_smd *smd, hl2m_vec3 *out_min, hl2m_vec3 *out_max)
{
    int i;
    if (smd == 0 || out_min == 0 || out_max == 0) return HL2M_ERR_NULL;
    if (smd->vertex_count <= 0) {
        out_min->x = out_min->y = out_min->z = 0;
        out_max->x = out_max->y = out_max->z = 0;
        return HL2M_OK;
    }
    *out_min = smd->vertices[0].position;
    *out_max = smd->vertices[0].position;
    for (i = 1; i < smd->vertex_count; ++i) {
        hl2m_vec3 p;
        p = smd->vertices[i].position;
        if (p.x < out_min->x) out_min->x = p.x;
        if (p.y < out_min->y) out_min->y = p.y;
        if (p.z < out_min->z) out_min->z = p.z;
        if (p.x > out_max->x) out_max->x = p.x;
        if (p.y > out_max->y) out_max->y = p.y;
        if (p.z > out_max->z) out_max->z = p.z;
    }
    return HL2M_OK;
}


static int hl2m_qc_surfaceprop(const hl2m_qc *qc, char *out_text, unsigned int out_size)
{
    int i;
    if (out_text == 0 || out_size == 0u) return HL2M_ERR_NULL;
    out_text[0] = 0;
    if (qc == 0) return HL2M_ERR_PARSE;
    for (i = 0; i < qc->command_count; ++i) {
        if (qc->commands[i].kind == HL2M_QC_SURFACEPROP && qc->commands[i].argc > 0) {
            hl2m_copy_token(out_text, out_size, qc->commands[i].args[0]);
            return HL2M_OK;
        }
    }
    return HL2M_ERR_PARSE;
}

static int hl2m_qc_staticprop(const hl2m_qc *qc)
{
    int i;
    if (qc == 0) return 0;
    for (i = 0; i < qc->command_count; ++i) {
        if (qc->commands[i].kind == HL2M_QC_STATICPROP) return 1;
    }
    return 0;
}

int hl2m_write_vvd_from_smd(const hl2m_smd *smd, const hl2m_compile_options *opt, hl2m_u8 *out_data, unsigned int out_cap, unsigned int *out_size)
{
    hl2m_compile_options o;
    unsigned int vertex_off;
    unsigned int tangent_off;
    unsigned int size_needed;
    int i;
    int b;
    int r;

    if (smd == 0 || out_data == 0 || out_size == 0) return HL2M_ERR_NULL;
    if (smd->vertex_count < 0 || smd->vertex_count > HL2M_MAX_VVD_VERTS) return HL2M_ERR_LIMIT;
    hl2m_compile_get_options(opt, &o);
    vertex_off = HL2M_VVD_HEADER_SIZE;
    tangent_off = vertex_off + (unsigned int)smd->vertex_count * HL2M_VVD_VERTEX_SIZE;
    size_needed = tangent_off + (unsigned int)smd->vertex_count * 16u;
    if (!hl2m_put_range(0u, size_needed, out_cap)) return HL2M_ERR_RANGE;
    hl2m_memzero(out_data, size_needed);
    r = hl2m_put_i32(out_data, out_cap, 0u, (int)HL2M_VVD_MAGIC); if (r != HL2M_OK) return r;
    r = hl2m_put_i32(out_data, out_cap, 4u, HL2M_VVD_VERSION); if (r != HL2M_OK) return r;
    r = hl2m_put_i32(out_data, out_cap, 8u, o.checksum); if (r != HL2M_OK) return r;
    r = hl2m_put_i32(out_data, out_cap, 12u, o.lod_count); if (r != HL2M_OK) return r;
    for (i = 0; i < HL2M_MAX_LODS; ++i) {
        int count;
        count = smd->vertex_count;
        if (i >= o.lod_count) count = 0;
        r = hl2m_put_i32(out_data, out_cap, 16u + (unsigned int)i * 4u, count); if (r != HL2M_OK) return r;
    }
    r = hl2m_put_i32(out_data, out_cap, 48u, 0); if (r != HL2M_OK) return r;
    r = hl2m_put_i32(out_data, out_cap, 52u, 0); if (r != HL2M_OK) return r;
    r = hl2m_put_i32(out_data, out_cap, 56u, (int)vertex_off); if (r != HL2M_OK) return r;
    r = hl2m_put_i32(out_data, out_cap, 60u, (int)tangent_off); if (r != HL2M_OK) return r;
    for (i = 0; i < smd->vertex_count; ++i) {
        unsigned int off;
        hl2m_smd_vertex v;
        off = vertex_off + (unsigned int)i * HL2M_VVD_VERTEX_SIZE;
        v = smd->vertices[i];
        if (v.weight_count <= 0) {
            v.weight_count = 1;
            v.weights[0].bone = v.parent_bone;
            v.weights[0].weight = HL2M_FIXED_ONE;
        }
        for (b = 0; b < HL2M_MAX_BONES_PER_VERT; ++b) {
            hl2m_fixed w;
            w = (b < v.weight_count) ? v.weights[b].weight : 0;
            r = hl2m_put_fixed(out_data, out_cap, off + (unsigned int)b * 4u, w); if (r != HL2M_OK) return r;
        }
        for (b = 0; b < HL2M_MAX_BONES_PER_VERT; ++b) {
            int bone;
            bone = (b < v.weight_count) ? v.weights[b].bone : 0;
            r = hl2m_put_u8(out_data, out_cap, off + 12u + (unsigned int)b, bone); if (r != HL2M_OK) return r;
        }
        r = hl2m_put_u8(out_data, out_cap, off + 15u, v.weight_count); if (r != HL2M_OK) return r;
        r = hl2m_put_vec3(out_data, out_cap, off + 16u, v.position); if (r != HL2M_OK) return r;
        r = hl2m_put_vec3(out_data, out_cap, off + 28u, v.normal); if (r != HL2M_OK) return r;
        r = hl2m_put_fixed(out_data, out_cap, off + 40u, v.texcoord.x); if (r != HL2M_OK) return r;
        r = hl2m_put_fixed(out_data, out_cap, off + 44u, v.texcoord.y); if (r != HL2M_OK) return r;
        r = hl2m_put_fixed(out_data, out_cap, tangent_off + (unsigned int)i * 16u + 0u, HL2M_FIXED_ONE); if (r != HL2M_OK) return r;
        r = hl2m_put_fixed(out_data, out_cap, tangent_off + (unsigned int)i * 16u + 4u, 0); if (r != HL2M_OK) return r;
        r = hl2m_put_fixed(out_data, out_cap, tangent_off + (unsigned int)i * 16u + 8u, 0); if (r != HL2M_OK) return r;
        r = hl2m_put_fixed(out_data, out_cap, tangent_off + (unsigned int)i * 16u + 12u, HL2M_FIXED_ONE); if (r != HL2M_OK) return r;
    }
    *out_size = size_needed;
    return HL2M_OK;
}

static int hl2m_count_triangles_for_material_lod(const hl2m_smd *smd, int material, int step)
{
    int t;
    int c;
    int seen;
    if (step <= 0) step = 1;
    c = 0;
    seen = 0;
    for (t = 0; t < smd->triangle_count; ++t) {
        if (smd->triangles[t].material == material) {
            if ((seen % step) == 0) ++c;
            ++seen;
        }
    }
    return c;
}

static int hl2m_write_vtx_stripgroup_for_material(const hl2m_smd *smd, int material, int step, hl2m_u8 *data, unsigned int cap, unsigned int sg_off, unsigned int *out_used)
{
    int numtri;
    int numverts;
    int numindices;
    unsigned int vert_off_rel;
    unsigned int index_off_rel;
    unsigned int strip_off_rel;
    unsigned int cursor;
    int t;
    int seen;
    int local_index;
    int r;

    numtri = hl2m_count_triangles_for_material_lod(smd, material, step);
    numverts = numtri * 3;
    numindices = numtri * 3;
    if (numverts > 65535 || numindices > 65535) return HL2M_ERR_LIMIT;
    vert_off_rel = HL2M_VTX_STRIPGROUP_SIZE;
    index_off_rel = vert_off_rel + (unsigned int)numverts * HL2M_VTX_VERTEX_SIZE;
    strip_off_rel = index_off_rel + (unsigned int)numindices * 2u;
    cursor = strip_off_rel + HL2M_VTX_STRIP_SIZE;
    if (!hl2m_put_range(sg_off, cursor, cap)) return HL2M_ERR_RANGE;
    r = hl2m_put_i32(data, cap, sg_off + 0u, numverts); if (r != HL2M_OK) return r;
    r = hl2m_put_i32(data, cap, sg_off + 4u, (int)vert_off_rel); if (r != HL2M_OK) return r;
    r = hl2m_put_i32(data, cap, sg_off + 8u, numindices); if (r != HL2M_OK) return r;
    r = hl2m_put_i32(data, cap, sg_off + 12u, (int)index_off_rel); if (r != HL2M_OK) return r;
    r = hl2m_put_i32(data, cap, sg_off + 16u, (numindices > 0) ? 1 : 0); if (r != HL2M_OK) return r;
    r = hl2m_put_i32(data, cap, sg_off + 20u, (int)strip_off_rel); if (r != HL2M_OK) return r;
    r = hl2m_put_u8(data, cap, sg_off + 24u, 0); if (r != HL2M_OK) return r;

    seen = 0;
    local_index = 0;
    for (t = 0; t < smd->triangle_count; ++t) {
        if (smd->triangles[t].material == material) {
            if ((seen % step) == 0) {
                int k;
                int first;
                first = smd->triangles[t].first_vertex;
                for (k = 0; k < 3; ++k) {
                    unsigned int vo;
                    int src_vertex;
                    src_vertex = first + k;
                    vo = sg_off + vert_off_rel + (unsigned int)local_index * HL2M_VTX_VERTEX_SIZE;
                    r = hl2m_put_u8(data, cap, vo + 0u, 0); if (r != HL2M_OK) return r;
                    r = hl2m_put_u8(data, cap, vo + 1u, 1); if (r != HL2M_OK) return r;
                    r = hl2m_put_u8(data, cap, vo + 2u, 2); if (r != HL2M_OK) return r;
                    r = hl2m_put_u8(data, cap, vo + 3u, smd->vertices[src_vertex].weight_count > 0 ? smd->vertices[src_vertex].weight_count : 1); if (r != HL2M_OK) return r;
                    r = hl2m_put_u16(data, cap, vo + 4u, src_vertex); if (r != HL2M_OK) return r;
                    r = hl2m_put_u8(data, cap, vo + 6u, smd->vertices[src_vertex].parent_bone); if (r != HL2M_OK) return r;
                    r = hl2m_put_u8(data, cap, vo + 7u, 0); if (r != HL2M_OK) return r;
                    r = hl2m_put_u8(data, cap, vo + 8u, 0); if (r != HL2M_OK) return r;
                    r = hl2m_put_u16(data, cap, sg_off + index_off_rel + (unsigned int)local_index * 2u, local_index); if (r != HL2M_OK) return r;
                    ++local_index;
                }
            }
            ++seen;
        }
    }
    if (numindices > 0) {
        unsigned int so;
        so = sg_off + strip_off_rel;
        r = hl2m_put_i32(data, cap, so + 0u, numindices); if (r != HL2M_OK) return r;
        r = hl2m_put_i32(data, cap, so + 4u, 0); if (r != HL2M_OK) return r;
        r = hl2m_put_i32(data, cap, so + 8u, numverts); if (r != HL2M_OK) return r;
        r = hl2m_put_i32(data, cap, so + 12u, 0); if (r != HL2M_OK) return r;
        r = hl2m_put_u16(data, cap, so + 16u, 1); if (r != HL2M_OK) return r;
        r = hl2m_put_u8(data, cap, so + 18u, 1); if (r != HL2M_OK) return r;
        r = hl2m_put_i32(data, cap, so + 19u, 0); if (r != HL2M_OK) return r;
    }
    *out_used = cursor;
    return HL2M_OK;
}

int hl2m_write_vtx_from_smd(const hl2m_smd *smd, const hl2m_qc *qc, const hl2m_compile_options *opt, hl2m_u8 *out_data, unsigned int out_cap, unsigned int *out_size)
{
    hl2m_compile_options o;
    unsigned int cursor;
    unsigned int material_list_off;
    unsigned int bodypart_off;
    unsigned int model_off;
    unsigned int lod_off;
    unsigned int mesh_base_off;
    int lod;
    int mat;
    int material_count;
    int replacements;
    int r;

    if (smd == 0 || out_data == 0 || out_size == 0) return HL2M_ERR_NULL;
    if (smd->vertex_count > 65535) return HL2M_ERR_LIMIT;
    hl2m_compile_get_options(opt, &o);
    hl2m_memzero(out_data, out_cap);
    material_count = smd->material_count;
    if (material_count <= 0) material_count = 1;
    replacements = 0;
    if (qc != 0 && o.emit_material_replacements != 0) replacements = qc->material_replace_count;
    cursor = HL2M_VTX_FILE_SIZE;
    material_list_off = 0u;
    if (replacements > 0) {
        material_list_off = cursor;
        cursor += (unsigned int)o.lod_count * HL2M_VTX_MATERIAL_REPLACEMENT_LIST_SIZE;
        for (lod = 0; lod < o.lod_count; ++lod) {
            int count;
            unsigned int reps_off;
            count = replacements;
            reps_off = cursor;
            /* Source expects an array of fixed-size replacement headers,
               followed by name strings, not interleaved header/name pairs. */
            if ((unsigned int)replacements > (out_cap - cursor) / HL2M_VTX_MATERIAL_REPLACEMENT_SIZE)
                return HL2M_ERR_LIMIT;
            cursor += (unsigned int)replacements * HL2M_VTX_MATERIAL_REPLACEMENT_SIZE;
            r = hl2m_put_i32(out_data, out_cap, material_list_off + (unsigned int)lod * HL2M_VTX_MATERIAL_REPLACEMENT_LIST_SIZE + 0u, count); if (r != HL2M_OK) return r;
            r = hl2m_put_i32(out_data, out_cap, material_list_off + (unsigned int)lod * HL2M_VTX_MATERIAL_REPLACEMENT_LIST_SIZE + 4u, (int)(reps_off - (material_list_off + (unsigned int)lod * HL2M_VTX_MATERIAL_REPLACEMENT_LIST_SIZE))); if (r != HL2M_OK) return r;
            for (mat = 0; mat < replacements; ++mat) {
                unsigned int ro;
                unsigned int name_off;
                int material_id;
                material_id = 0;
                if (qc != 0) {
                    int sm;
                    for (sm = 0; sm < smd->material_count; ++sm) {
                        if (hl2m_text_eq_ci(smd->materials[sm], qc->material_replaces[mat].from_material)) material_id = sm;
                    }
                }
                ro = reps_off + (unsigned int)mat * HL2M_VTX_MATERIAL_REPLACEMENT_SIZE;
                r = hl2m_append_z(out_data, out_cap, &cursor, qc->material_replaces[mat].to_material, &name_off); if (r != HL2M_OK) return r;
                r = hl2m_put_u16(out_data, out_cap, ro + 0u, material_id); if (r != HL2M_OK) return r;
                r = hl2m_put_i32(out_data, out_cap, ro + 2u, (int)(name_off - ro)); if (r != HL2M_OK) return r;
            }
        }
    }
    cursor = hl2m_align4u(cursor);
    bodypart_off = cursor;
    model_off = bodypart_off + HL2M_VTX_BODYPART_SIZE;
    lod_off = model_off + HL2M_VTX_MODEL_SIZE;
    mesh_base_off = lod_off + (unsigned int)o.lod_count * HL2M_VTX_LOD_SIZE;
    cursor = mesh_base_off + (unsigned int)o.lod_count * (unsigned int)material_count * HL2M_VTX_MESH_SIZE;
    if (!hl2m_put_range(0u, cursor, out_cap)) return HL2M_ERR_RANGE;
    r = hl2m_put_i32(out_data, out_cap, 0u, HL2M_VTX_VERSION); if (r != HL2M_OK) return r;
    r = hl2m_put_i32(out_data, out_cap, 4u, 0); if (r != HL2M_OK) return r;
    r = hl2m_put_u16(out_data, out_cap, 8u, 64); if (r != HL2M_OK) return r;
    r = hl2m_put_u16(out_data, out_cap, 10u, 64); if (r != HL2M_OK) return r;
    r = hl2m_put_i32(out_data, out_cap, 12u, HL2M_MAX_BONES_PER_VERT); if (r != HL2M_OK) return r;
    r = hl2m_put_i32(out_data, out_cap, 16u, o.checksum); if (r != HL2M_OK) return r;
    r = hl2m_put_i32(out_data, out_cap, 20u, o.lod_count); if (r != HL2M_OK) return r;
    r = hl2m_put_i32(out_data, out_cap, 24u, (int)material_list_off); if (r != HL2M_OK) return r;
    r = hl2m_put_i32(out_data, out_cap, 28u, 1); if (r != HL2M_OK) return r;
    r = hl2m_put_i32(out_data, out_cap, 32u, (int)bodypart_off); if (r != HL2M_OK) return r;
    r = hl2m_put_i32(out_data, out_cap, bodypart_off + 0u, 1); if (r != HL2M_OK) return r;
    r = hl2m_put_i32(out_data, out_cap, bodypart_off + 4u, (int)(model_off - bodypart_off)); if (r != HL2M_OK) return r;
    r = hl2m_put_i32(out_data, out_cap, model_off + 0u, o.lod_count); if (r != HL2M_OK) return r;
    r = hl2m_put_i32(out_data, out_cap, model_off + 4u, (int)(lod_off - model_off)); if (r != HL2M_OK) return r;
    for (lod = 0; lod < o.lod_count; ++lod) {
        unsigned int lo;
        unsigned int mesh_off;
        int step;
        lo = lod_off + (unsigned int)lod * HL2M_VTX_LOD_SIZE;
        mesh_off = mesh_base_off + (unsigned int)lod * (unsigned int)material_count * HL2M_VTX_MESH_SIZE;
        step = o.lods[lod].triangle_step;
        if (step <= 0) step = lod + 1;
        r = hl2m_put_i32(out_data, out_cap, lo + 0u, material_count); if (r != HL2M_OK) return r;
        r = hl2m_put_i32(out_data, out_cap, lo + 4u, (int)(mesh_off - lo)); if (r != HL2M_OK) return r;
        r = hl2m_put_fixed(out_data, out_cap, lo + 8u, o.lods[lod].switch_point); if (r != HL2M_OK) return r;
        for (mat = 0; mat < material_count; ++mat) {
            unsigned int mo;
            unsigned int sg_off;
            unsigned int used;
            int numtri;
            numtri = hl2m_count_triangles_for_material_lod(smd, mat, step);
            mo = mesh_off + (unsigned int)mat * HL2M_VTX_MESH_SIZE;
            sg_off = cursor;
            r = hl2m_put_i32(out_data, out_cap, mo + 0u, 1); if (r != HL2M_OK) return r;
            r = hl2m_put_i32(out_data, out_cap, mo + 4u, (int)(sg_off - mo)); if (r != HL2M_OK) return r;
            r = hl2m_put_u8(out_data, out_cap, mo + 8u, 0); if (r != HL2M_OK) return r;
            r = hl2m_write_vtx_stripgroup_for_material(smd, mat, step, out_data, out_cap, sg_off, &used); if (r != HL2M_OK) return r;
            cursor = hl2m_align4u(sg_off + used);
            if (numtri == 0) {
                ;
            }
        }
    }
    *out_size = cursor;
    return HL2M_OK;
}

int hl2m_write_phy_from_smd(const hl2m_smd *smd, const hl2m_compile_options *opt, hl2m_u8 *out_data, unsigned int out_cap, unsigned int *out_size)
{
    hl2m_compile_options o;
    hl2m_convex_decomp decomp;
    int r;
    if (smd == 0 || out_data == 0 || out_size == 0) return HL2M_ERR_NULL;
    hl2m_compile_get_options(opt, &o);
    r = hl2m_build_convex_decomp_from_smd(smd, o.collision_mode, &decomp);
    if (r != HL2M_OK) return r;
    return hl2m_write_phy_from_convex_decomp(&decomp, &o, out_data, out_cap, out_size);
}

static int hl2m_write_mdl_anim_channel_bytes(hl2m_u8 *data, unsigned int cap, unsigned int off, const hl2m_smd *anim_smd);

int hl2m_write_ani_from_smd(const hl2m_smd *anim_smd, const hl2m_compile_options *opt, hl2m_u8 *out_data, unsigned int out_cap, unsigned int *out_size)
{
    hl2m_compile_options o;
    unsigned int animblock_off;
    unsigned int data_off;
    unsigned int payload_off;
    unsigned int cursor;
    unsigned int channel_bytes;
    int frame_count;
    int i;
    int r;

    if (anim_smd == 0 || out_data == 0 || out_size == 0) return HL2M_ERR_NULL;
    hl2m_compile_get_options(opt, &o);
    frame_count = hl2m_smd_frame_count(anim_smd);
    animblock_off = HL2M_STUDIOHDR_MIN;
    data_off = animblock_off + HL2M_STUDIO_ANIMBLOCK_SIZE * 2u;
    channel_bytes = (unsigned int)(anim_smd->node_count + 1) * 4u;
    payload_off = data_off + channel_bytes;
    cursor = payload_off + 16u + (unsigned int)anim_smd->pose_count * 32u;
    if (!hl2m_put_range(0u, cursor, out_cap)) return HL2M_ERR_RANGE;
    hl2m_memzero(out_data, cursor);
    r = hl2m_put_i32(out_data, out_cap, 0u, (int)HL2M_MDL_MAGIC); if (r != HL2M_OK) return r;
    r = hl2m_put_i32(out_data, out_cap, 4u, HL2M_MDL_STUDIO_VERSION); if (r != HL2M_OK) return r;
    r = hl2m_put_i32(out_data, out_cap, 8u, o.checksum); if (r != HL2M_OK) return r;
    r = hl2m_put_z(out_data, out_cap, 12u, o.model_name[0] != 0 ? o.model_name : "hl2m_anim.ani", 64u); if (r != HL2M_OK) return r;
    r = hl2m_put_i32(out_data, out_cap, 352u, 2); if (r != HL2M_OK) return r;
    r = hl2m_put_i32(out_data, out_cap, 356u, (int)animblock_off); if (r != HL2M_OK) return r;
    r = hl2m_put_i32(out_data, out_cap, animblock_off + 0u, 0); if (r != HL2M_OK) return r;
    r = hl2m_put_i32(out_data, out_cap, animblock_off + 4u, 0); if (r != HL2M_OK) return r;
    r = hl2m_put_i32(out_data, out_cap, animblock_off + 8u, (int)data_off); if (r != HL2M_OK) return r;
    r = hl2m_put_i32(out_data, out_cap, animblock_off + 12u, (int)cursor); if (r != HL2M_OK) return r;
    r = hl2m_write_mdl_anim_channel_bytes(out_data, out_cap, data_off, anim_smd); if (r != HL2M_OK) return r;
    r = hl2m_put_i32(out_data, out_cap, payload_off + 0u, (int)0x32414c48); if (r != HL2M_OK) return r;
    r = hl2m_put_i32(out_data, out_cap, payload_off + 4u, frame_count); if (r != HL2M_OK) return r;
    r = hl2m_put_i32(out_data, out_cap, payload_off + 8u, anim_smd->node_count); if (r != HL2M_OK) return r;
    r = hl2m_put_i32(out_data, out_cap, payload_off + 12u, anim_smd->pose_count); if (r != HL2M_OK) return r;
    cursor = payload_off + 16u;
    for (i = 0; i < anim_smd->pose_count; ++i) {
        r = hl2m_put_i32(out_data, out_cap, cursor + 0u, anim_smd->poses[i].time); if (r != HL2M_OK) return r;
        r = hl2m_put_i32(out_data, out_cap, cursor + 4u, anim_smd->poses[i].node); if (r != HL2M_OK) return r;
        r = hl2m_put_vec3(out_data, out_cap, cursor + 8u, anim_smd->poses[i].pos); if (r != HL2M_OK) return r;
        r = hl2m_put_vec3(out_data, out_cap, cursor + 20u, anim_smd->poses[i].rot); if (r != HL2M_OK) return r;
        cursor += 32u;
    }
    r = hl2m_put_i32(out_data, out_cap, 76u, (int)cursor); if (r != HL2M_OK) return r;
    *out_size = cursor;
    return HL2M_OK;
}

static int hl2m_write_mdl_anim_channel_bytes(hl2m_u8 *data, unsigned int cap, unsigned int off, const hl2m_smd *anim_smd)
{
    int b;
    int r;
    unsigned int cur;
    if (anim_smd == 0 || anim_smd->node_count <= 0) {
        r = hl2m_put_u8(data, cap, off + 0u, 255); if (r != HL2M_OK) return r;
        r = hl2m_put_u8(data, cap, off + 1u, 0); if (r != HL2M_OK) return r;
        r = hl2m_put_u16(data, cap, off + 2u, 0); if (r != HL2M_OK) return r;
        return HL2M_OK;
    }
    cur = off;
    for (b = 0; b < anim_smd->node_count && b < HL2M_MAX_BONES; ++b) {
        r = hl2m_put_u8(data, cap, cur + 0u, b); if (r != HL2M_OK) return r;
        r = hl2m_put_u8(data, cap, cur + 1u, 0x3f); if (r != HL2M_OK) return r;
        r = hl2m_put_u16(data, cap, cur + 2u, 4); if (r != HL2M_OK) return r;
        cur += 4u;
    }
    r = hl2m_put_u8(data, cap, cur + 0u, 255); if (r != HL2M_OK) return r;
    r = hl2m_put_u8(data, cap, cur + 1u, 0); if (r != HL2M_OK) return r;
    r = hl2m_put_u16(data, cap, cur + 2u, 0); if (r != HL2M_OK) return r;
    return HL2M_OK;
}

int hl2m_write_mdl_from_qc_smd(const hl2m_qc *qc, const hl2m_smd *smd, const hl2m_smd *anim_smd, const hl2m_vta *vta, const hl2m_compile_options *opt, hl2m_u8 *out_data, unsigned int out_cap, unsigned int *out_size)
{
    hl2m_compile_options o;
    hl2m_vec3 bbmin;
    hl2m_vec3 bbmax;
    hl2m_vec4 qident;
    unsigned int cursor;
    unsigned int bone_off;
    unsigned int hitset_off;
    unsigned int hitbox_off;
    unsigned int animdesc_off;
    unsigned int seqdesc_off;
    unsigned int tex_off;
    unsigned int cdtable_off;
    unsigned int bodypart_off;
    unsigned int model_off;
    unsigned int mesh_off;
    unsigned int surface_off;
    unsigned int animblockname_off;
    unsigned int animblock_off;
    unsigned int animbytes_off;
    unsigned int keyvalue_off;
    unsigned int event_off;
    unsigned int ikrule_off;
    int bone_count;
    int material_count;
    int hitbox_count;
    int event_count;
    int ikrule_count;
    int frame_count;
    int i;
    int r;
    char surface[HL2M_NAME_MAX];

    if (smd == 0 || out_data == 0 || out_size == 0) return HL2M_ERR_NULL;
    hl2m_compile_get_options(opt, &o);
    if (qc != 0 && qc->modelname[0] != 0 && o.model_name[0] == 0) hl2m_copy_token(o.model_name, HL2M_PATH_MAX, qc->modelname);
    if (o.model_name[0] == 0) hl2m_copy_token(o.model_name, HL2M_PATH_MAX, "models/hl2modelib/generated.mdl");
    hl2m_copy_token(surface, HL2M_NAME_MAX, o.surfaceprop);
    if (qc != 0 && hl2m_qc_surfaceprop(qc, surface, HL2M_NAME_MAX) != HL2M_OK) {
        ;
    }
    bone_count = smd->node_count > 0 ? smd->node_count : 1;
    if (bone_count > HL2M_MAX_BONES) return HL2M_ERR_LIMIT;
    material_count = smd->material_count > 0 ? smd->material_count : 1;
    if (material_count > HL2M_MAX_TEXTURES) return HL2M_ERR_LIMIT;
    hitbox_count = (qc != 0 && qc->qc_hitbox_count > 0) ? qc->qc_hitbox_count : 1;
    if (hitbox_count > HL2M_MAX_HITBOXES) hitbox_count = HL2M_MAX_HITBOXES;
    event_count = (qc != 0 && anim_smd != 0 && o.emit_sequence_events) ? qc->sequence_event_count : 0;
    if (event_count > HL2M_MAX_MDL_EVENTS) event_count = HL2M_MAX_MDL_EVENTS;
    ikrule_count = (qc != 0 && anim_smd != 0 && o.emit_ik_rules) ? qc->ikrule_count : 0;
    if (ikrule_count > HL2M_MAX_MDL_IKRULES) ikrule_count = HL2M_MAX_MDL_IKRULES;
    frame_count = (anim_smd != 0) ? hl2m_smd_frame_count(anim_smd) : 0;
    r = hl2m_smd_bounds(smd, &bbmin, &bbmax); if (r != HL2M_OK) return r;
    qident.x = 0; qident.y = 0; qident.z = 0; qident.w = HL2M_FIXED_ONE;
    cursor = 356u;
    bone_off = cursor; cursor += (unsigned int)bone_count * HL2M_STUDIO_BONE_SIZE;
    hitset_off = cursor; cursor += HL2M_STUDIO_HITBOXSET_SIZE;
    hitbox_off = cursor; cursor += (unsigned int)hitbox_count * HL2M_STUDIO_HITBOX_SIZE;
    animdesc_off = cursor; if (anim_smd != 0) cursor += HL2M_STUDIO_ANIMDESC_SIZE;
    seqdesc_off = cursor; if (anim_smd != 0) cursor += HL2M_STUDIO_SEQDESC_SIZE;
    event_off = cursor; if (anim_smd != 0 && event_count > 0) cursor += (unsigned int)event_count * HL2M_STUDIO_EVENT_SIZE;
    ikrule_off = cursor; if (anim_smd != 0 && ikrule_count > 0) cursor += (unsigned int)ikrule_count * HL2M_STUDIO_IKRULE_SIZE;
    tex_off = cursor; cursor += (unsigned int)material_count * HL2M_STUDIO_TEXTURE_SIZE;
    cdtable_off = cursor; cursor += 4u;
    bodypart_off = cursor; cursor += HL2M_STUDIO_BODYPART_SIZE;
    model_off = cursor; cursor += HL2M_STUDIO_MODEL_SIZE;
    mesh_off = cursor; cursor += (unsigned int)material_count * HL2M_STUDIO_MESH_SIZE;
    cursor = hl2m_align4u(cursor);
    if (!hl2m_put_range(0u, cursor + 1024u, out_cap)) return HL2M_ERR_RANGE;
    hl2m_memzero(out_data, cursor + 1024u);
    r = hl2m_put_i32(out_data, out_cap, 0u, (int)HL2M_MDL_MAGIC); if (r != HL2M_OK) return r;
    r = hl2m_put_i32(out_data, out_cap, 4u, HL2M_MDL_STUDIO_VERSION); if (r != HL2M_OK) return r;
    r = hl2m_put_i32(out_data, out_cap, 8u, o.checksum); if (r != HL2M_OK) return r;
    r = hl2m_put_z(out_data, out_cap, 12u, o.model_name, 64u); if (r != HL2M_OK) return r;
    r = hl2m_put_vec3(out_data, out_cap, 80u, bbmax); if (r != HL2M_OK) return r;
    r = hl2m_put_vec3(out_data, out_cap, 92u, bbmax); if (r != HL2M_OK) return r;
    r = hl2m_put_vec3(out_data, out_cap, 104u, bbmin); if (r != HL2M_OK) return r;
    r = hl2m_put_vec3(out_data, out_cap, 116u, bbmax); if (r != HL2M_OK) return r;
    r = hl2m_put_vec3(out_data, out_cap, 128u, bbmin); if (r != HL2M_OK) return r;
    r = hl2m_put_vec3(out_data, out_cap, 140u, bbmax); if (r != HL2M_OK) return r;
    r = hl2m_put_i32(out_data, out_cap, 152u, hl2m_qc_staticprop(qc) ? 0x10 : 0); if (r != HL2M_OK) return r;
    r = hl2m_put_i32(out_data, out_cap, 156u, bone_count); if (r != HL2M_OK) return r;
    r = hl2m_put_i32(out_data, out_cap, 160u, (int)bone_off); if (r != HL2M_OK) return r;
    r = hl2m_put_i32(out_data, out_cap, 172u, 1); if (r != HL2M_OK) return r;
    r = hl2m_put_i32(out_data, out_cap, 176u, (int)hitset_off); if (r != HL2M_OK) return r;
    r = hl2m_put_i32(out_data, out_cap, 180u, anim_smd != 0 ? 1 : 0); if (r != HL2M_OK) return r;
    r = hl2m_put_i32(out_data, out_cap, 184u, anim_smd != 0 ? (int)animdesc_off : 0); if (r != HL2M_OK) return r;
    r = hl2m_put_i32(out_data, out_cap, 188u, anim_smd != 0 ? 1 : 0); if (r != HL2M_OK) return r;
    r = hl2m_put_i32(out_data, out_cap, 192u, anim_smd != 0 ? (int)seqdesc_off : 0); if (r != HL2M_OK) return r;
    r = hl2m_put_i32(out_data, out_cap, 204u, material_count); if (r != HL2M_OK) return r;
    r = hl2m_put_i32(out_data, out_cap, 208u, (int)tex_off); if (r != HL2M_OK) return r;
    r = hl2m_put_i32(out_data, out_cap, 212u, 1); if (r != HL2M_OK) return r;
    r = hl2m_put_i32(out_data, out_cap, 216u, (int)cdtable_off); if (r != HL2M_OK) return r;
    r = hl2m_put_i32(out_data, out_cap, 232u, 1); if (r != HL2M_OK) return r;
    r = hl2m_put_i32(out_data, out_cap, 236u, (int)bodypart_off); if (r != HL2M_OK) return r;
    r = hl2m_append_z(out_data, out_cap, &cursor, surface, &surface_off); if (r != HL2M_OK) return r;
    r = hl2m_put_i32(out_data, out_cap, 308u, (int)surface_off); if (r != HL2M_OK) return r;
    keyvalue_off = 0u;
    if (vta != 0 && vta->flex_count > 0) {
        r = hl2m_append_z(out_data, out_cap, &cursor, "hl2modelib { flexes baked_from_vta 1 }", &keyvalue_off); if (r != HL2M_OK) return r;
        r = hl2m_put_i32(out_data, out_cap, 312u, (int)keyvalue_off); if (r != HL2M_OK) return r;
        r = hl2m_put_i32(out_data, out_cap, 316u, 38); if (r != HL2M_OK) return r;
    }
    r = hl2m_put_fixed(out_data, out_cap, 328u, HL2M_FIXED_ONE); if (r != HL2M_OK) return r;
    r = hl2m_put_i32(out_data, out_cap, 332u, 0); if (r != HL2M_OK) return r;
    animblockname_off = 0u;
    animblock_off = 0u;
    if (anim_smd != 0) {
        r = hl2m_append_z(out_data, out_cap, &cursor, "generated.ani", &animblockname_off); if (r != HL2M_OK) return r;
        animblock_off = cursor; cursor += HL2M_STUDIO_ANIMBLOCK_SIZE * 2u;
        animbytes_off = cursor; cursor += (unsigned int)(bone_count + 1) * 4u;
        r = hl2m_put_i32(out_data, out_cap, 348u, (int)animblockname_off); if (r != HL2M_OK) return r;
        r = hl2m_put_i32(out_data, out_cap, 352u, 2); if (r != HL2M_OK) return r;
        r = hl2m_put_i32(out_data, out_cap, 356u, (int)animblock_off); if (r != HL2M_OK) return r;
        r = hl2m_put_i32(out_data, out_cap, animblock_off + 0u, 0); if (r != HL2M_OK) return r;
        r = hl2m_put_i32(out_data, out_cap, animblock_off + 4u, 0); if (r != HL2M_OK) return r;
        r = hl2m_put_i32(out_data, out_cap, animblock_off + 8u, (int)(HL2M_STUDIOHDR_MIN + HL2M_STUDIO_ANIMBLOCK_SIZE * 2u)); if (r != HL2M_OK) return r;
        r = hl2m_put_i32(out_data, out_cap, animblock_off + 12u, (int)(HL2M_STUDIOHDR_MIN + HL2M_STUDIO_ANIMBLOCK_SIZE * 2u + (unsigned int)(bone_count + 1) * 4u)); if (r != HL2M_OK) return r;
        r = hl2m_write_mdl_anim_channel_bytes(out_data, out_cap, animbytes_off, anim_smd); if (r != HL2M_OK) return r;
    }
    for (i = 0; i < bone_count; ++i) {
        unsigned int bo;
        unsigned int nameoff;
        hl2m_vec3 zero3;
        zero3.x = zero3.y = zero3.z = 0;
        bo = bone_off + (unsigned int)i * HL2M_STUDIO_BONE_SIZE;
        r = hl2m_append_z(out_data, out_cap, &cursor, (i < smd->node_count) ? smd->nodes[i].name : "root", &nameoff); if (r != HL2M_OK) return r;
        r = hl2m_put_i32(out_data, out_cap, bo + 0u, (int)(nameoff - bo)); if (r != HL2M_OK) return r;
        r = hl2m_put_i32(out_data, out_cap, bo + 4u, (i < smd->node_count) ? smd->nodes[i].parent : -1); if (r != HL2M_OK) return r;
        r = hl2m_put_vec3(out_data, out_cap, bo + 32u, zero3); if (r != HL2M_OK) return r;
        r = hl2m_put_vec4(out_data, out_cap, bo + 44u, qident); if (r != HL2M_OK) return r;
        r = hl2m_put_vec3(out_data, out_cap, bo + 60u, zero3); if (r != HL2M_OK) return r;
        r = hl2m_put_vec3(out_data, out_cap, bo + 72u, zero3); if (r != HL2M_OK) return r;
        r = hl2m_put_vec3(out_data, out_cap, bo + 84u, zero3); if (r != HL2M_OK) return r;
        r = hl2m_put_i32(out_data, out_cap, bo + 172u, i); if (r != HL2M_OK) return r;
        r = hl2m_put_i32(out_data, out_cap, bo + 180u, 0); if (r != HL2M_OK) return r;
    }
    {
        unsigned int hname;
        int h;
        r = hl2m_append_z(out_data, out_cap, &cursor, "default", &hname); if (r != HL2M_OK) return r;
        r = hl2m_put_i32(out_data, out_cap, hitset_off + 0u, (int)(hname - hitset_off)); if (r != HL2M_OK) return r;
        r = hl2m_put_i32(out_data, out_cap, hitset_off + 4u, hitbox_count); if (r != HL2M_OK) return r;
        r = hl2m_put_i32(out_data, out_cap, hitset_off + 8u, (int)(hitbox_off - hitset_off)); if (r != HL2M_OK) return r;
        for (h = 0; h < hitbox_count; ++h) {
            unsigned int ho;
            int bone_idx;
            int group_idx;
            hl2m_vec3 hmin;
            hl2m_vec3 hmax;
            const char *hbname;
            ho = hitbox_off + (unsigned int)h * HL2M_STUDIO_HITBOX_SIZE;
            bone_idx = 0;
            group_idx = 0;
            hmin = bbmin;
            hmax = bbmax;
            hbname = "";
            if (qc != 0 && h < qc->qc_hitbox_count) {
                int found;
                found = hl2m_smd_find_node_index(smd, qc->qc_hitboxes[h].bone);
                if (found >= 0) bone_idx = found;
                group_idx = qc->qc_hitboxes[h].group;
                hmin = qc->qc_hitboxes[h].bbmin;
                hmax = qc->qc_hitboxes[h].bbmax;
                hbname = qc->qc_hitboxes[h].name;
            }
            r = hl2m_put_i32(out_data, out_cap, ho + 0u, bone_idx); if (r != HL2M_OK) return r;
            r = hl2m_put_i32(out_data, out_cap, ho + 4u, group_idx); if (r != HL2M_OK) return r;
            r = hl2m_put_vec3(out_data, out_cap, ho + 8u, hmin); if (r != HL2M_OK) return r;
            r = hl2m_put_vec3(out_data, out_cap, ho + 20u, hmax); if (r != HL2M_OK) return r;
            if (hbname != 0 && hbname[0] != 0) {
                unsigned int nboff;
                r = hl2m_append_z(out_data, out_cap, &cursor, hbname, &nboff); if (r != HL2M_OK) return r;
                r = hl2m_put_i32(out_data, out_cap, ho + 32u, (int)(nboff - ho)); if (r != HL2M_OK) return r;
            }
        }
    }
    if (anim_smd != 0) {
        unsigned int aname;
        unsigned int sname;
        const char *seq_name;
        seq_name = (qc != 0 && qc->sequence_count > 0) ? qc->sequences[0].name : "idle";
        r = hl2m_append_z(out_data, out_cap, &cursor, seq_name, &aname); if (r != HL2M_OK) return r;
        r = hl2m_put_i32(out_data, out_cap, animdesc_off + 4u, (int)(aname - animdesc_off)); if (r != HL2M_OK) return r;
        r = hl2m_put_fixed(out_data, out_cap, animdesc_off + 8u, 30 * HL2M_FIXED_ONE); if (r != HL2M_OK) return r;
        r = hl2m_put_i32(out_data, out_cap, animdesc_off + 16u, frame_count); if (r != HL2M_OK) return r;
        r = hl2m_put_i32(out_data, out_cap, animdesc_off + 52u, 1); if (r != HL2M_OK) return r;
        r = hl2m_put_i32(out_data, out_cap, animdesc_off + 56u, 0); if (r != HL2M_OK) return r;
        if (ikrule_count > 0) {
            int ir;
            r = hl2m_put_i32(out_data, out_cap, animdesc_off + 60u, ikrule_count); if (r != HL2M_OK) return r;
            r = hl2m_put_i32(out_data, out_cap, animdesc_off + 64u, (int)(ikrule_off - animdesc_off)); if (r != HL2M_OK) return r;
            for (ir = 0; ir < ikrule_count; ++ir) {
                unsigned int io;
                int chain_idx;
                int bone_idx;
                const char *bone_name;
                io = ikrule_off + (unsigned int)ir * HL2M_STUDIO_IKRULE_SIZE;
                chain_idx = hl2m_qc_find_ikchain(qc, qc->ikrules[ir].chain);
                bone_name = qc->ikrules[ir].bone;
                if ((bone_name == 0 || bone_name[0] == 0) && chain_idx >= 0) bone_name = qc->ikchains[chain_idx].bone;
                bone_idx = hl2m_smd_find_node_index(smd, bone_name);
                if (bone_idx < 0) bone_idx = 0;
                r = hl2m_put_i32(out_data, out_cap, io + 0u, ir); if (r != HL2M_OK) return r;
                r = hl2m_put_i32(out_data, out_cap, io + 4u, qc->ikrules[ir].type); if (r != HL2M_OK) return r;
                r = hl2m_put_i32(out_data, out_cap, io + 8u, chain_idx >= 0 ? chain_idx : 0); if (r != HL2M_OK) return r;
                r = hl2m_put_i32(out_data, out_cap, io + 12u, bone_idx); if (r != HL2M_OK) return r;
                r = hl2m_put_i32(out_data, out_cap, io + 16u, chain_idx >= 0 ? chain_idx : ir); if (r != HL2M_OK) return r;
                r = hl2m_put_fixed(out_data, out_cap, io + 76u, qc->ikrules[ir].start); if (r != HL2M_OK) return r;
                r = hl2m_put_fixed(out_data, out_cap, io + 80u, qc->ikrules[ir].peak); if (r != HL2M_OK) return r;
                r = hl2m_put_fixed(out_data, out_cap, io + 84u, qc->ikrules[ir].tail); if (r != HL2M_OK) return r;
                r = hl2m_put_fixed(out_data, out_cap, io + 88u, qc->ikrules[ir].end); if (r != HL2M_OK) return r;
                r = hl2m_put_fixed(out_data, out_cap, io + 96u, qc->ikrules[ir].contact); if (r != HL2M_OK) return r;
                r = hl2m_put_fixed(out_data, out_cap, io + 100u, qc->ikrules[ir].drop); if (r != HL2M_OK) return r;
                r = hl2m_put_fixed(out_data, out_cap, io + 104u, qc->ikrules[ir].top); if (r != HL2M_OK) return r;
                if (qc->ikrules[ir].attachment[0] != 0) {
                    unsigned int iaoff;
                    r = hl2m_append_z(out_data, out_cap, &cursor, qc->ikrules[ir].attachment, &iaoff); if (r != HL2M_OK) return r;
                    r = hl2m_put_i32(out_data, out_cap, io + 120u, (int)(iaoff - io)); if (r != HL2M_OK) return r;
                }
            }
        }
        r = hl2m_append_z(out_data, out_cap, &cursor, seq_name, &sname); if (r != HL2M_OK) return r;
        r = hl2m_put_i32(out_data, out_cap, seqdesc_off + 4u, (int)(sname - seqdesc_off)); if (r != HL2M_OK) return r;
        r = hl2m_put_vec3(out_data, out_cap, seqdesc_off + 32u, bbmin); if (r != HL2M_OK) return r;
        r = hl2m_put_vec3(out_data, out_cap, seqdesc_off + 44u, bbmax); if (r != HL2M_OK) return r;
        r = hl2m_put_i32(out_data, out_cap, seqdesc_off + 56u, 1); if (r != HL2M_OK) return r;
        if (event_count > 0) {
            int ev;
            r = hl2m_put_i32(out_data, out_cap, seqdesc_off + 24u, event_count); if (r != HL2M_OK) return r;
            r = hl2m_put_i32(out_data, out_cap, seqdesc_off + 28u, (int)(event_off - seqdesc_off)); if (r != HL2M_OK) return r;
            for (ev = 0; ev < event_count; ++ev) {
                unsigned int eo;
                hl2m_fixed cycle;
                eo = event_off + (unsigned int)ev * HL2M_STUDIO_EVENT_SIZE;
                cycle = 0;
                if (frame_count > 1) cycle = (hl2m_fixed)((qc->sequence_events[ev].frame * HL2M_FIXED_ONE) / (frame_count - 1));
                r = hl2m_put_fixed(out_data, out_cap, eo + 0u, cycle); if (r != HL2M_OK) return r;
                r = hl2m_put_i32(out_data, out_cap, eo + 4u, qc->sequence_events[ev].event_id); if (r != HL2M_OK) return r;
                r = hl2m_put_i32(out_data, out_cap, eo + 8u, qc->sequence_events[ev].type); if (r != HL2M_OK) return r;
                r = hl2m_put_z(out_data, out_cap, eo + 12u, qc->sequence_events[ev].options, 64u); if (r != HL2M_OK) return r;
                if (qc->sequence_events[ev].event_name[0] != 0) {
                    unsigned int enoff;
                    r = hl2m_append_z(out_data, out_cap, &cursor, qc->sequence_events[ev].event_name, &enoff); if (r != HL2M_OK) return r;
                    r = hl2m_put_i32(out_data, out_cap, eo + 76u, (int)(enoff - eo)); if (r != HL2M_OK) return r;
                }
            }
        }
    }
    for (i = 0; i < material_count; ++i) {
        unsigned int nameoff;
        unsigned int to;
        const char *matname;
        matname = (i < smd->material_count) ? smd->materials[i] : "material";
        to = tex_off + (unsigned int)i * HL2M_STUDIO_TEXTURE_SIZE;
        r = hl2m_append_z(out_data, out_cap, &cursor, matname, &nameoff); if (r != HL2M_OK) return r;
        r = hl2m_put_i32(out_data, out_cap, to + 0u, (int)(nameoff - to)); if (r != HL2M_OK) return r;
        r = hl2m_put_i32(out_data, out_cap, to + 8u, 1); if (r != HL2M_OK) return r;
    }
    {
        unsigned int cdoff;
        r = hl2m_append_z(out_data, out_cap, &cursor, qc != 0 && qc->material_dir_count > 0 ? qc->material_dirs[0] : "models/", &cdoff); if (r != HL2M_OK) return r;
        r = hl2m_put_i32(out_data, out_cap, cdtable_off + 0u, (int)cdoff); if (r != HL2M_OK) return r;
    }
    {
        unsigned int bpname;
        r = hl2m_append_z(out_data, out_cap, &cursor, o.body_name, &bpname); if (r != HL2M_OK) return r;
        r = hl2m_put_i32(out_data, out_cap, bodypart_off + 0u, (int)(bpname - bodypart_off)); if (r != HL2M_OK) return r;
        r = hl2m_put_i32(out_data, out_cap, bodypart_off + 4u, 1); if (r != HL2M_OK) return r;
        r = hl2m_put_i32(out_data, out_cap, bodypart_off + 8u, 1); if (r != HL2M_OK) return r;
        r = hl2m_put_i32(out_data, out_cap, bodypart_off + 12u, (int)(model_off - bodypart_off)); if (r != HL2M_OK) return r;
    }
    r = hl2m_put_z(out_data, out_cap, model_off + 0u, "studio", 64u); if (r != HL2M_OK) return r;
    r = hl2m_put_i32(out_data, out_cap, model_off + 64u, 0); if (r != HL2M_OK) return r;
    r = hl2m_put_fixed(out_data, out_cap, model_off + 68u, HL2M_FIXED_ONE); if (r != HL2M_OK) return r;
    r = hl2m_put_i32(out_data, out_cap, model_off + 72u, material_count); if (r != HL2M_OK) return r;
    r = hl2m_put_i32(out_data, out_cap, model_off + 76u, (int)(mesh_off - model_off)); if (r != HL2M_OK) return r;
    r = hl2m_put_i32(out_data, out_cap, model_off + 80u, smd->vertex_count); if (r != HL2M_OK) return r;
    r = hl2m_put_i32(out_data, out_cap, model_off + 84u, 0); if (r != HL2M_OK) return r;
    r = hl2m_put_i32(out_data, out_cap, model_off + 88u, 0); if (r != HL2M_OK) return r;
    for (i = 0; i < material_count; ++i) {
        unsigned int mo;
        mo = mesh_off + (unsigned int)i * HL2M_STUDIO_MESH_SIZE;
        r = hl2m_put_i32(out_data, out_cap, mo + 0u, i); if (r != HL2M_OK) return r;
        r = hl2m_put_i32(out_data, out_cap, mo + 8u, smd->vertex_count); if (r != HL2M_OK) return r;
        r = hl2m_put_i32(out_data, out_cap, mo + 12u, 0); if (r != HL2M_OK) return r;
        r = hl2m_put_i32(out_data, out_cap, mo + 16u, vta != 0 ? vta->flex_count : 0); if (r != HL2M_OK) return r;
        r = hl2m_put_i32(out_data, out_cap, mo + 32u, i); if (r != HL2M_OK) return r;
        r = hl2m_put_vec3(out_data, out_cap, mo + 36u, bbmax); if (r != HL2M_OK) return r;
    }
    cursor = hl2m_align4u(cursor);
    r = hl2m_put_i32(out_data, out_cap, 76u, (int)cursor); if (r != HL2M_OK) return r;
    *out_size = cursor;
    return HL2M_OK;
}

int hl2m_compile_qc_smd_to_binary(const hl2m_qc *qc, const hl2m_smd *reference_smd, const hl2m_smd *animation_smd, const hl2m_vta *vta, const hl2m_compile_options *opt, hl2m_u8 *mdl_data, unsigned int mdl_cap, unsigned int *mdl_size, hl2m_u8 *vvd_data, unsigned int vvd_cap, unsigned int *vvd_size, hl2m_u8 *vtx_data, unsigned int vtx_cap, unsigned int *vtx_size, hl2m_u8 *phy_data, unsigned int phy_cap, unsigned int *phy_size, hl2m_u8 *ani_data, unsigned int ani_cap, unsigned int *ani_size, hl2m_compile_result *out_result)
{
    hl2m_compile_options o;
    hl2m_compile_options qc_o;
    hl2m_compile_result local_result;
    hl2m_compile_result validation;
    hl2m_bundle_report bundle_report;
    int r;
    int i;
    if (reference_smd == 0) return HL2M_ERR_NULL;
    hl2m_compile_get_options(opt, &o);
    if (qc != 0) {
        r = hl2m_make_compile_options_from_qc(qc, &qc_o); if (r != HL2M_OK) return r;
        if (o.model_name[0] == 0 && qc_o.model_name[0] != 0) hl2m_copy_token(o.model_name, HL2M_PATH_MAX, qc_o.model_name);
        if (o.body_name[0] == 0 || hl2m_text_eq_ci(o.body_name, "body")) {
            if (qc_o.body_name[0] != 0) hl2m_copy_token(o.body_name, HL2M_NAME_MAX, qc_o.body_name);
        }
        if (o.surfaceprop[0] == 0 || hl2m_text_eq_ci(o.surfaceprop, "default")) {
            if (qc_o.surfaceprop[0] != 0) hl2m_copy_token(o.surfaceprop, HL2M_NAME_MAX, qc_o.surfaceprop);
        }
        if ((opt == 0 || opt->lod_count <= 1) && qc_o.lod_count > 1) {
            o.lod_count = qc_o.lod_count;
            for (i = 0; i < o.lod_count; ++i) o.lods[i] = qc_o.lods[i];
        }
        if (qc_o.collision_mode != HL2M_COLLISION_AABB) o.collision_mode = qc_o.collision_mode;
    }
    r = hl2m_validate_compile_inputs(qc, reference_smd, animation_smd, vta, &validation); if (r != HL2M_OK) return r;
    hl2m_clear_compile_result(&local_result);
    local_result.warning_flags = validation.warning_flags;
    local_result.bad_bone_ref_count = validation.bad_bone_ref_count;
    local_result.weight_repair_count = validation.weight_repair_count;
    local_result.vta_range_count = validation.vta_range_count;
    local_result.qc_body_count = validation.qc_body_count;
    local_result.bone_count = reference_smd->node_count;
    local_result.material_count = reference_smd->material_count;
    local_result.vertex_count = reference_smd->vertex_count;
    local_result.triangle_count = reference_smd->triangle_count;
    local_result.lod_count = o.lod_count;
    local_result.flex_count = (vta != 0) ? vta->flex_count : 0;
    local_result.flex_controller_count = (qc != 0) ? qc->flexcontroller_count : 0;
    local_result.flex_rule_count = (qc != 0) ? qc->flexrule_count : 0;
    local_result.sequence_event_count = (qc != 0) ? qc->sequence_event_count : 0;
    local_result.ik_chain_count = (qc != 0) ? qc->ikchain_count : 0;
    local_result.ik_rule_count = (qc != 0) ? qc->ikrule_count : 0;
    local_result.hitbox_count = (qc != 0) ? qc->qc_hitbox_count : 0;
    local_result.anim_frame_count = (animation_smd != 0) ? hl2m_smd_frame_count(animation_smd) : 0;
    if (o.emit_phy) {
        hl2m_convex_decomp tmp_decomp;
        if (hl2m_build_convex_decomp_from_smd(reference_smd, o.collision_mode, &tmp_decomp) == HL2M_OK) {
            local_result.collision_solid_count = tmp_decomp.hull_count;
            local_result.convex_hull_count = tmp_decomp.hull_count;
        } else {
            local_result.collision_solid_count = 1;
            local_result.convex_hull_count = 1;
        }
    } else {
        local_result.collision_solid_count = 0;
        local_result.convex_hull_count = 0;
    }
    if (local_result.collision_solid_count > HL2M_MAX_PHY_SOLIDS) local_result.collision_solid_count = HL2M_MAX_PHY_SOLIDS;
    if (animation_smd == 0) local_result.warning_flags |= HL2M_COMPILE_WARN_NO_ANIM;
    if (vta == 0) local_result.warning_flags |= HL2M_COMPILE_WARN_NO_VTA;
    if (o.emit_phy && o.collision_mode == HL2M_COLLISION_AABB) local_result.warning_flags |= HL2M_COMPILE_WARN_PHY_BOX_ONLY;
    if (o.lod_count > 1) local_result.warning_flags |= HL2M_COMPILE_WARN_LOD_DECIMATED;
    r = hl2m_write_mdl_from_qc_smd(qc, reference_smd, animation_smd, vta, &o, mdl_data, mdl_cap, mdl_size); if (r != HL2M_OK) return r;
    local_result.mdl_size = (mdl_size != 0) ? *mdl_size : 0u;
    r = hl2m_write_vvd_from_smd(reference_smd, &o, vvd_data, vvd_cap, vvd_size); if (r != HL2M_OK) return r;
    local_result.vvd_size = (vvd_size != 0) ? *vvd_size : 0u;
    r = hl2m_write_vtx_from_smd(reference_smd, qc, &o, vtx_data, vtx_cap, vtx_size); if (r != HL2M_OK) return r;
    local_result.vtx_size = (vtx_size != 0) ? *vtx_size : 0u;
    if (o.emit_phy && phy_data != 0 && phy_size != 0) {
        r = hl2m_write_phy_from_smd(reference_smd, &o, phy_data, phy_cap, phy_size); if (r != HL2M_OK) return r;
        local_result.phy_size = *phy_size;
    } else if (phy_size != 0) {
        *phy_size = 0u;
    }
    if (o.emit_ani && animation_smd != 0 && ani_data != 0 && ani_size != 0) {
        r = hl2m_write_ani_from_smd(animation_smd, &o, ani_data, ani_cap, ani_size); if (r != HL2M_OK) return r;
        local_result.ani_size = *ani_size;
    } else if (ani_size != 0) {
        *ani_size = 0u;
    }
    r = hl2m_validate_binary_bundle(mdl_data, local_result.mdl_size, vvd_data, local_result.vvd_size, vtx_data, local_result.vtx_size, phy_data, local_result.phy_size, ani_data, local_result.ani_size, &bundle_report);
    if (r == HL2M_OK) {
        local_result.bundle_warning_flags = bundle_report.warning_flags;
        if (bundle_report.warning_flags != 0) local_result.warning_flags |= HL2M_COMPILE_WARN_BUNDLE_MISMATCH;
    }
    if (out_result != 0) *out_result = local_result;
    return HL2M_OK;
}

static int hl2m_path_has_suffix_ci(const char *path, const char *suffix)
{
    unsigned int n;
    unsigned int m;
    if (path == 0 || suffix == 0) return 0;
    n = 0u;
    m = 0u;
    while (path[n] != 0) ++n;
    while (suffix[m] != 0) ++m;
    if (n < m) return 0;
    return hl2m_text_eq_ci(path + n - m, suffix);
}

int hl2m_guess_format(const hl2m_u8 *data, unsigned int size, const char *path)
{
    if (path != 0) {
        if (hl2m_path_has_suffix_ci(path, ".mdl")) return HL2M_FORMAT_MDL;
        if (hl2m_path_has_suffix_ci(path, ".ani")) return HL2M_FORMAT_ANI;
        if (hl2m_path_has_suffix_ci(path, ".phy")) return HL2M_FORMAT_PHY;
        if (hl2m_path_has_suffix_ci(path, ".vvd")) return HL2M_FORMAT_VVD;
        if (hl2m_path_has_suffix_ci(path, ".vtx")) return HL2M_FORMAT_VTX;
        if (hl2m_path_has_suffix_ci(path, ".dx90.vtx")) return HL2M_FORMAT_VTX;
        if (hl2m_path_has_suffix_ci(path, ".dx80.vtx")) return HL2M_FORMAT_VTX;
        if (hl2m_path_has_suffix_ci(path, ".sw.vtx")) return HL2M_FORMAT_VTX;
        if (hl2m_path_has_suffix_ci(path, ".smd")) return HL2M_FORMAT_SMD;
        if (hl2m_path_has_suffix_ci(path, ".vmt")) return HL2M_FORMAT_VMT;
        if (hl2m_path_has_suffix_ci(path, ".vtf")) return HL2M_FORMAT_VTF;
        if (hl2m_path_has_suffix_ci(path, ".vta")) return HL2M_FORMAT_VTA;
        if (hl2m_path_has_suffix_ci(path, ".qc")) return HL2M_FORMAT_QC;
    }
    if (data != 0 && size >= 4u) {
        hl2m_u32 magic;
        magic = hl2m_le_u32(data);
        if (magic == HL2M_MDL_MAGIC) return HL2M_FORMAT_MDL;
        if (magic == HL2M_VVD_MAGIC || magic == HL2M_VVD_LEGACY_MAGIC) return HL2M_FORMAT_VVD;
        if (data[0] == 'V' && data[1] == 'T' && data[2] == 'F' && data[3] == 0u) return HL2M_FORMAT_VTF;
        if (data[0] == 'T' && data[1] == 'F' && data[2] == 0u && data[3] == 7u) return HL2M_FORMAT_VTF;
        if (data[0] == 'D' && data[1] == 'S' && data[2] == 'T') return HL2M_FORMAT_MDL;
        if (data[0] == 'D' && data[1] == 'S' && data[2] == 'V') return HL2M_FORMAT_VVD;
        if (size >= 8u && magic == 7u) return HL2M_FORMAT_VTX;
    }
    if (data != 0 && size >= 7u) {
        if (data[0] == 'v' && data[1] == 'e' && data[2] == 'r' && data[3] == 's' && data[4] == 'i' && data[5] == 'o' && data[6] == 'n') return HL2M_FORMAT_SMD;
    }
    return HL2M_FORMAT_UNKNOWN;
}

const char *hl2m_error_name(int code)
{
    if (code == HL2M_OK) return "HL2M_OK";
    if (code == HL2M_ERR_NULL) return "HL2M_ERR_NULL";
    if (code == HL2M_ERR_RANGE) return "HL2M_ERR_RANGE";
    if (code == HL2M_ERR_MAGIC) return "HL2M_ERR_MAGIC";
    if (code == HL2M_ERR_VERSION) return "HL2M_ERR_VERSION";
    if (code == HL2M_ERR_LIMIT) return "HL2M_ERR_LIMIT";
    if (code == HL2M_ERR_PARSE) return "HL2M_ERR_PARSE";
    if (code == HL2M_ERR_UNSUPPORTED) return "HL2M_ERR_UNSUPPORTED";
    return "HL2M_ERR_UNKNOWN";
}
