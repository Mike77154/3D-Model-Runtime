#include "re4c3d/re4_model.h"
#include "re4c3d/re4_base.h"
#include <string.h>

static re4_bool re4_is_digit_u8(re4_u8 c)
{
    return (c >= (re4_u8)'0' && c <= (re4_u8)'9') ? RE4_TRUE : RE4_FALSE;
}

static re4_strref re4_trim_ref(re4_strref s)
{
    while (s.len != 0UL && (s.ptr[0] == ' ' || s.ptr[0] == '\t' || s.ptr[0] == '\r' || s.ptr[0] == '\n')) {
        ++s.ptr;
        --s.len;
    }
    while (s.len != 0UL) {
        char c;
        c = s.ptr[s.len - 1UL];
        if (!(c == ' ' || c == '\t' || c == '\r' || c == '\n')) break;
        --s.len;
    }
    return s;
}

static re4_bool re4_match_bone_marker(re4_strref name, re4_size at)
{
    re4_u8 b0;
    re4_u8 b1;
    re4_u8 b2;
    re4_u8 b3;
    re4_u8 b4;
    if (at + 5UL > name.len) return RE4_FALSE;
    b0 = re4_ascii_tolower_u8((re4_u8)name.ptr[at + 0UL]);
    b1 = re4_ascii_tolower_u8((re4_u8)name.ptr[at + 1UL]);
    b2 = re4_ascii_tolower_u8((re4_u8)name.ptr[at + 2UL]);
    b3 = re4_ascii_tolower_u8((re4_u8)name.ptr[at + 3UL]);
    b4 = (re4_u8)name.ptr[at + 4UL];
    if (b0 != (re4_u8)'b') return RE4_FALSE;
    if (b1 != (re4_u8)'o' && b1 != (re4_u8)'0') return RE4_FALSE;
    if (b2 != (re4_u8)'n') return RE4_FALSE;
    if (b3 != (re4_u8)'e') return RE4_FALSE;
    if (b4 != (re4_u8)'_') return RE4_FALSE;
    return RE4_TRUE;
}

re4_err re4_smd_bone_id_from_name(re4_strref name, re4_u32 *out_id)
{
    re4_size i;
    if (out_id == 0) return RE4_ERR_NULL;
    name = re4_trim_ref(name);
    for (i = 0UL; i < name.len; ++i) {
        if (re4_match_bone_marker(name, i)) {
            re4_size j;
            re4_u32 v;
            j = i + 5UL;
            if (j >= name.len || !re4_is_digit_u8((re4_u8)name.ptr[j])) continue;
            v = 0U;
            while (j < name.len && re4_is_digit_u8((re4_u8)name.ptr[j])) {
                re4_u32 d;
                d = (re4_u32)((re4_u8)name.ptr[j] - (re4_u8)'0');
                if (v > 429496729U) return RE4_ERR_RANGE;
                v = (v * 10U) + d;
                ++j;
            }
            *out_id = v;
            return RE4_OK;
        }
    }
    return RE4_ERR_BAD_FORMAT;
}

static re4_err re4_parse_i32_ref(re4_strref s, re4_i32 *out)
{
    re4_size i;
    re4_bool neg;
    re4_u32 v;
    if (out == 0) return RE4_ERR_NULL;
    s = re4_trim_ref(s);
    if (s.len == 0UL) return RE4_ERR_BAD_FORMAT;
    i = 0UL;
    neg = RE4_FALSE;
    if (s.ptr[0] == '-') {
        neg = RE4_TRUE;
        i = 1UL;
    }
    if (i >= s.len || !re4_is_digit_u8((re4_u8)s.ptr[i])) return RE4_ERR_BAD_FORMAT;
    v = 0U;
    while (i < s.len) {
        re4_u32 d;
        if (!re4_is_digit_u8((re4_u8)s.ptr[i])) return RE4_ERR_BAD_FORMAT;
        d = (re4_u32)((re4_u8)s.ptr[i] - (re4_u8)'0');
        if (v > 214748364U) return RE4_ERR_RANGE;
        v = (v * 10U) + d;
        ++i;
    }
    if (neg) {
        if (v > 2147483648U) return RE4_ERR_RANGE;
        if (v == 2147483648U) *out = RE4_I32_MIN;
        else *out = (re4_i32)(0 - (re4_i32)v);
    } else {
        if (v > 2147483647U) return RE4_ERR_RANGE;
        *out = (re4_i32)v;
    }
    return RE4_OK;
}

static re4_err re4_split_replacebone(re4_strref line, re4_strref parts[4])
{
    re4_size start;
    re4_size i;
    re4_u32 count;
    start = 0UL;
    count = 0U;
    for (i = 0UL; i <= line.len; ++i) {
        if (i == line.len || line.ptr[i] == '|') {
            if (count >= 4U) return RE4_ERR_BAD_FORMAT;
            parts[count].ptr = line.ptr + start;
            parts[count].len = i - start;
            parts[count] = re4_trim_ref(parts[count]);
            ++count;
            start = i + 1UL;
        }
    }
    if (count != 4U) return RE4_ERR_BAD_FORMAT;
    return RE4_OK;
}

re4_err re4_smd_replacebone_line_parse(re4_strref line, re4_smd_bone_rule *out)
{
    re4_strref parts[4];
    re4_err er;
    if (out == 0) return RE4_ERR_NULL;
    memset(out, 0, sizeof(*out));
    line = re4_trim_ref(line);
    if (line.len == 0UL) return RE4_ERR_BAD_FORMAT;
    if (line.ptr[0] == '#') return RE4_ERR_BAD_FORMAT;
    er = re4_split_replacebone(line, parts);
    if (er != RE4_OK) return er;
    out->old_name = parts[0];
    out->new_name = parts[2];
    er = re4_parse_i32_ref(parts[1], &out->new_id);
    if (er != RE4_OK) return er;
    er = re4_parse_i32_ref(parts[3], &out->parent_id);
    if (er != RE4_OK) return er;
    return RE4_OK;
}
