#include "u3d3d.h"

static int u3d3d_is_digit(char c)
{
    return c >= '0' && c <= '9';
}

static char u3d3d_upper(char c)
{
    if (c >= 'a' && c <= 'z') {
        return (char)(c - ('a' - 'A'));
    }
    return c;
}

static const char *u3d3d_find_ci_limited(const char *p, const char *end, const char *needle)
{
    const char *it;
    const char *n;
    const char *q;
    int ok;

    it = p;
    while (it < end && *it != '\0') {
        n = needle;
        q = it;
        ok = 1;
        while (*n != '\0') {
            if (q >= end || *q == '\0' || u3d3d_upper(*q) != u3d3d_upper(*n)) {
                ok = 0;
                break;
            }
            ++q;
            ++n;
        }
        if (ok) {
            return it;
        }
        ++it;
    }
    return (const char *)0;
}

static const char *u3d3d_line_end(const char *p, const char *end)
{
    while (p < end && *p != '\0' && *p != '\r' && *p != '\n') {
        ++p;
    }
    return p;
}

static const char *u3d3d_skip_spaces_no_nl(const char *p, const char *end)
{
    while (p < end && (*p == ' ' || *p == '\t')) {
        ++p;
    }
    return p;
}

static int u3d3d_parse_uint_token(const char *p, const char *end, u3d3d_u16 *out_value)
{
    u3d3d_u32 v;
    int any;

    if (out_value == (u3d3d_u16 *)0) {
        return U3D3D_ERR_NULL;
    }
    v = 0u;
    any = 0;
    while (p < end && u3d3d_is_digit(*p)) {
        any = 1;
        v = (v * 10u) + (u3d3d_u32)(*p - '0');
        if (v > 65535u) {
            return U3D3D_ERR_RANGE;
        }
        ++p;
    }
    if (!any) {
        return U3D3D_ERR_PARSE;
    }
    *out_value = (u3d3d_u16)v;
    return U3D3D_OK;
}

static int u3d3d_parse_fixed_token(const char *p, const char *end, u3d3d_fx *out_value)
{
    int neg;
    u3d3d_u32 whole;
    u3d3d_u32 frac;
    u3d3d_u32 scale;
    u3d3d_u32 frac_fx;
    int any;
    u3d3d_fx result;

    if (out_value == (u3d3d_fx *)0) {
        return U3D3D_ERR_NULL;
    }
    neg = 0;
    whole = 0u;
    frac = 0u;
    scale = 1u;
    any = 0;

    if (p < end && (*p == '-' || *p == '+')) {
        if (*p == '-') {
            neg = 1;
        }
        ++p;
    }

    while (p < end && u3d3d_is_digit(*p)) {
        any = 1;
        whole = (whole * 10u) + (u3d3d_u32)(*p - '0');
        if (whole > 32767u) {
            return U3D3D_ERR_RANGE;
        }
        ++p;
    }

    if (p < end && *p == '.') {
        ++p;
        while (p < end && u3d3d_is_digit(*p) && scale < 1000000u) {
            any = 1;
            frac = (frac * 10u) + (u3d3d_u32)(*p - '0');
            scale *= 10u;
            ++p;
        }
        while (p < end && u3d3d_is_digit(*p)) {
            ++p;
        }
    }

    if (!any) {
        return U3D3D_ERR_PARSE;
    }

    frac_fx = (frac * 65536u + (scale / 2u)) / scale;
    result = (u3d3d_fx)((whole << 16) + frac_fx);
    if (neg) {
        result = (u3d3d_fx)(-result);
    }
    *out_value = result;
    return U3D3D_OK;
}

void u3d3d_uc_info_init(u3d3d_uc_info *info)
{
    u3d3d_u16 i;

    if (info == (u3d3d_uc_info *)0) {
        return;
    }
    info->scale_x = U3D3D_FX_ONE;
    info->scale_y = U3D3D_FX_ONE;
    info->scale_z = U3D3D_FX_ONE;
    info->has_scale_x = 0u;
    info->has_scale_y = 0u;
    info->has_scale_z = 0u;
    info->texture_slot_count = 0u;
    for (i = 0u; i < 256u; ++i) {
        info->texture_slots_seen[i] = 0u;
    }
}

static void u3d3d_mark_texture_slot(u3d3d_uc_info *info, u3d3d_u16 slot)
{
    if (slot > 255u) {
        return;
    }
    if (info->texture_slots_seen[slot] == 0u) {
        info->texture_slots_seen[slot] = 1u;
        ++info->texture_slot_count;
    }
}

static void u3d3d_parse_scale_line(const char *line, const char *line_end, u3d3d_uc_info *info)
{
    const char *p;
    u3d3d_fx value;

    p = line;
    while (p < line_end) {
        p = u3d3d_skip_spaces_no_nl(p, line_end);
        if (p + 2 <= line_end && u3d3d_upper(p[0]) == 'X' && p[1] == '=') {
            if (u3d3d_parse_fixed_token(p + 2, line_end, &value) == U3D3D_OK) {
                info->scale_x = value;
                info->has_scale_x = 1u;
            }
        } else if (p + 2 <= line_end && u3d3d_upper(p[0]) == 'Y' && p[1] == '=') {
            if (u3d3d_parse_fixed_token(p + 2, line_end, &value) == U3D3D_OK) {
                info->scale_y = value;
                info->has_scale_y = 1u;
            }
        } else if (p + 2 <= line_end && u3d3d_upper(p[0]) == 'Z' && p[1] == '=') {
            if (u3d3d_parse_fixed_token(p + 2, line_end, &value) == U3D3D_OK) {
                info->scale_z = value;
                info->has_scale_z = 1u;
            }
        }
        ++p;
    }
}

static void u3d3d_parse_settexture_line(const char *line, const char *line_end, u3d3d_uc_info *info)
{
    const char *p;
    u3d3d_u16 slot;

    p = u3d3d_find_ci_limited(line, line_end, "NUM=");
    if (p != (const char *)0) {
        if (u3d3d_parse_uint_token(p + 4, line_end, &slot) == U3D3D_OK) {
            u3d3d_mark_texture_slot(info, slot);
        }
    }
}

int u3d3d_parse_uc_info(const char *text, u3d3d_u32 text_size, u3d3d_uc_info *info)
{
    const char *p;
    const char *end;
    const char *le;

    if (text == (const char *)0 || info == (u3d3d_uc_info *)0) {
        return U3D3D_ERR_NULL;
    }

    p = text;
    end = text + text_size;
    while (p < end && *p != '\0') {
        le = u3d3d_line_end(p, end);
        if (u3d3d_find_ci_limited(p, le, "MESHMAP") != (const char *)0) {
            if (u3d3d_find_ci_limited(p, le, "SCALE") != (const char *)0) {
                u3d3d_parse_scale_line(p, le, info);
            } else if (u3d3d_find_ci_limited(p, le, "SETTEXTURE") != (const char *)0) {
                u3d3d_parse_settexture_line(p, le, info);
            }
        }
        p = le;
        while (p < end && (*p == '\r' || *p == '\n')) {
            ++p;
        }
    }
    return U3D3D_OK;
}
