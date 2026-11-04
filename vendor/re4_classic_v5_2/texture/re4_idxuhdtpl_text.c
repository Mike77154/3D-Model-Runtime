#include "re4c3d/re4_texture.h"
#include "re4c3d/re4_base.h"
#include <string.h>
#include <stdio.h>

static re4_strref re4t_trim(re4_strref s)
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

static re4_bool re4t_eq_nocase(re4_strref s, const char *z)
{
    re4_size i;
    if (z == 0) return RE4_FALSE;
    for (i = 0UL; i < s.len; ++i) {
        re4_u8 a;
        re4_u8 b;
        if (z[i] == 0) return RE4_FALSE;
        a = re4_ascii_tolower_u8((re4_u8)s.ptr[i]);
        b = re4_ascii_tolower_u8((re4_u8)z[i]);
        if (a != b) return RE4_FALSE;
    }
    return z[s.len] == 0 ? RE4_TRUE : RE4_FALSE;
}

static re4_bool re4t_starts_nocase(re4_strref s, const char *z)
{
    re4_size i;
    if (z == 0) return RE4_FALSE;
    for (i = 0UL; z[i] != 0; ++i) {
        re4_u8 a;
        re4_u8 b;
        if (i >= s.len) return RE4_FALSE;
        a = re4_ascii_tolower_u8((re4_u8)s.ptr[i]);
        b = re4_ascii_tolower_u8((re4_u8)z[i]);
        if (a != b) return RE4_FALSE;
    }
    return RE4_TRUE;
}

static re4_bool re4t_is_digit(char c)
{
    return (c >= '0' && c <= '9') ? RE4_TRUE : RE4_FALSE;
}

static re4_u8 re4t_hexval(char c)
{
    if (c >= '0' && c <= '9') return (re4_u8)(c - '0');
    if (c >= 'a' && c <= 'f') return (re4_u8)(10 + c - 'a');
    if (c >= 'A' && c <= 'F') return (re4_u8)(10 + c - 'A');
    return 255U;
}

static re4_err re4t_parse_u32_dec(re4_strref s, re4_u32 *out)
{
    re4_size i;
    re4_u32 v;
    if (out == 0) return RE4_ERR_NULL;
    s = re4t_trim(s);
    if (s.len == 0UL) return RE4_ERR_BAD_FORMAT;
    v = 0U;
    for (i = 0UL; i < s.len; ++i) {
        re4_u32 d;
        if (!re4t_is_digit(s.ptr[i])) return RE4_ERR_BAD_FORMAT;
        d = (re4_u32)(s.ptr[i] - '0');
        if (v > 429496729U) return RE4_ERR_RANGE;
        v = (v * 10U) + d;
    }
    *out = v;
    return RE4_OK;
}

static re4_err re4t_parse_u32_hex(re4_strref s, re4_u32 *out)
{
    re4_size i;
    re4_u32 v;
    if (out == 0) return RE4_ERR_NULL;
    s = re4t_trim(s);
    if (s.len == 0UL) return RE4_ERR_BAD_FORMAT;
    if (s.len > 2UL && s.ptr[0] == '0' && (s.ptr[1] == 'x' || s.ptr[1] == 'X')) {
        s.ptr += 2;
        s.len -= 2;
    }
    v = 0U;
    for (i = 0UL; i < s.len; ++i) {
        re4_u8 d;
        d = re4t_hexval(s.ptr[i]);
        if (d == 255U) return RE4_ERR_BAD_FORMAT;
        v = (v << 4) | (re4_u32)d;
    }
    *out = v;
    return RE4_OK;
}

static re4_err re4t_parse_q16_text(re4_strref s, re4_q16 *out)
{
    re4_size i;
    re4_bool neg;
    re4_u32 whole;
    re4_u32 frac;
    re4_u32 denom;
    re4_u32 digits;
    re4_u32 mag;
    if (out == 0) return RE4_ERR_NULL;
    s = re4t_trim(s);
    if (s.len == 0UL) return RE4_ERR_BAD_FORMAT;
    i = 0UL;
    neg = RE4_FALSE;
    if (s.ptr[i] == '-') {
        neg = RE4_TRUE;
        ++i;
    }
    if (i >= s.len || !re4t_is_digit(s.ptr[i])) return RE4_ERR_BAD_FORMAT;
    whole = 0U;
    while (i < s.len && re4t_is_digit(s.ptr[i])) {
        if (whole > 32767U) return RE4_ERR_RANGE;
        whole = (whole * 10U) + (re4_u32)(s.ptr[i] - '0');
        ++i;
    }
    frac = 0U;
    denom = 1U;
    digits = 0U;
    if (i < s.len && s.ptr[i] == '.') {
        ++i;
        while (i < s.len && re4t_is_digit(s.ptr[i])) {
            if (digits < 4U) {
                frac = (frac * 10U) + (re4_u32)(s.ptr[i] - '0');
                denom *= 10U;
                ++digits;
            }
            ++i;
        }
    }
    if (i != s.len) return RE4_ERR_BAD_FORMAT;
    mag = (whole << 16) + ((frac * 65536U + (denom / 2U)) / denom);
    *out = re4_clamp_i64_like_q16(mag, neg);
    return RE4_OK;
}

static re4_err re4t_split_key_value(re4_strref line, re4_strref *key, re4_strref *value)
{
    re4_size i;
    if (key == 0 || value == 0) return RE4_ERR_NULL;
    for (i = 0UL; i < line.len; ++i) {
        if (line.ptr[i] == ':') {
            key->ptr = line.ptr;
            key->len = i;
            value->ptr = line.ptr + i + 1UL;
            value->len = line.len - i - 1UL;
            *key = re4t_trim(*key);
            *value = re4t_trim(*value);
            return RE4_OK;
        }
    }
    return RE4_ERR_BAD_FORMAT;
}

static re4_bool re4t_tpl_line_id(re4_strref line, re4_u32 *out_id)
{
    re4_strref tail;
    re4_size i;
    re4_u32 v;
    line = re4t_trim(line);
    if (!re4t_starts_nocase(line, "TPL_")) return RE4_FALSE;
    if (line.len <= 4UL) return RE4_FALSE;
    tail.ptr = line.ptr + 4UL;
    tail.len = line.len - 4UL;
    v = 0U;
    for (i = 0UL; i < tail.len; ++i) {
        if (!re4t_is_digit(tail.ptr[i])) return RE4_FALSE;
        v = (v * 10U) + (re4_u32)(tail.ptr[i] - '0');
    }
    if (out_id != 0) *out_id = v;
    return RE4_TRUE;
}

static void re4_idxuhdtpl_entry_defaults(re4_idxuhdtpl_entry *e)
{
    memset(e, 0, sizeof(*e));
    e->width = 1U;
    e->height = 1U;
    e->wrap_s = 1U;
    e->wrap_t = 1U;
    e->min_filter = 1U;
    e->mag_filter = 1U;
}

static re4_strref re4t_next_line(re4_span text, re4_size *pos)
{
    re4_size start;
    re4_strref line;
    start = *pos;
    while (*pos < text.size && text.data[*pos] != (re4_u8)'\n') ++(*pos);
    line.ptr = (const char *)text.data + start;
    line.len = *pos - start;
    if (*pos < text.size && text.data[*pos] == (re4_u8)'\n') ++(*pos);
    if (line.len != 0UL && line.ptr[line.len - 1UL] == '\r') --line.len;
    return line;
}

re4_err re4_idxuhdtpl_read_text(re4_span text, re4_arena *arena, re4_idxuhdtpl *out)
{
    re4_size pos;
    re4_u32 count;
    re4_u32 current;
    re4_idxuhdtpl_entry *entries;
    if (arena == 0 || out == 0) return RE4_ERR_NULL;
    memset(out, 0, sizeof(*out));
    pos = 0UL;
    count = 0U;
    while (pos < text.size) {
        re4_strref line;
        re4_u32 ignored;
        line = re4t_next_line(text, &pos);
        line = re4t_trim(line);
        if (line.len != 0UL && line.ptr[0] != '#' && re4t_tpl_line_id(line, &ignored)) ++count;
    }
    entries = (re4_idxuhdtpl_entry *)re4_arena_push(arena, (re4_size)count * sizeof(re4_idxuhdtpl_entry), sizeof(re4_u32));
    if (count != 0U && entries == 0) return RE4_ERR_NO_SPACE;
    pos = 0UL;
    current = RE4_U32_MAX;
    count = 0U;
    while (pos < text.size) {
        re4_strref line;
        re4_u32 id;
        line = re4t_next_line(text, &pos);
        line = re4t_trim(line);
        if (line.len == 0UL || line.ptr[0] == '#') continue;
        if (re4t_tpl_line_id(line, &id)) {
            re4_idxuhdtpl_entry_defaults(&entries[count]);
            entries[count].id = id;
            current = count;
            ++count;
        } else if (current != RE4_U32_MAX) {
            re4_strref key;
            re4_strref value;
            re4_u32 v;
            re4_err er;
            er = re4t_split_key_value(line, &key, &value);
            if (er != RE4_OK) continue;
            if (re4t_eq_nocase(key, "PackID")) {
                er = re4t_parse_u32_hex(value, &v);
                if (er != RE4_OK) return er;
                entries[current].pack_id = v;
                entries[current].seen_mask |= RE4_IDXUHDTPL_SEEN_PACK_ID;
            } else if (re4t_eq_nocase(key, "TextureID")) {
                er = re4t_parse_u32_dec(value, &v);
                if (er != RE4_OK) return er;
                entries[current].texture_id = v;
                entries[current].seen_mask |= RE4_IDXUHDTPL_SEEN_TEXTURE_ID;
            } else if (re4t_eq_nocase(key, "PixelFormatType")) {
                er = re4t_parse_u32_hex(value, &v);
                if (er != RE4_OK) return er;
                entries[current].pixel_format_type = v;
                entries[current].seen_mask |= RE4_IDXUHDTPL_SEEN_PIXEL_FORMAT;
            } else if (re4t_eq_nocase(key, "width")) {
                er = re4t_parse_u32_dec(value, &v);
                if (er != RE4_OK) return er;
                entries[current].width = v;
                entries[current].seen_mask |= RE4_IDXUHDTPL_SEEN_WIDTH;
            } else if (re4t_eq_nocase(key, "height")) {
                er = re4t_parse_u32_dec(value, &v);
                if (er != RE4_OK) return er;
                entries[current].height = v;
                entries[current].seen_mask |= RE4_IDXUHDTPL_SEEN_HEIGHT;
            } else if (re4t_eq_nocase(key, "wrap_s")) {
                er = re4t_parse_u32_dec(value, &entries[current].wrap_s);
                if (er != RE4_OK) return er;
            } else if (re4t_eq_nocase(key, "wrap_t")) {
                er = re4t_parse_u32_dec(value, &entries[current].wrap_t);
                if (er != RE4_OK) return er;
            } else if (re4t_eq_nocase(key, "min_filter")) {
                er = re4t_parse_u32_dec(value, &entries[current].min_filter);
                if (er != RE4_OK) return er;
            } else if (re4t_eq_nocase(key, "mag_filter")) {
                er = re4t_parse_u32_dec(value, &entries[current].mag_filter);
                if (er != RE4_OK) return er;
            } else if (re4t_eq_nocase(key, "lod_bias")) {
                er = re4t_parse_q16_text(value, &entries[current].lod_bias_q16);
                if (er != RE4_OK) return er;
            } else if (re4t_eq_nocase(key, "enable_lod")) {
                er = re4t_parse_u32_dec(value, &v);
                if (er != RE4_OK) return er;
                entries[current].enable_lod = (re4_u8)v;
            } else if (re4t_eq_nocase(key, "min_lod")) {
                er = re4t_parse_u32_dec(value, &v);
                if (er != RE4_OK) return er;
                entries[current].min_lod = (re4_u8)v;
            } else if (re4t_eq_nocase(key, "max_lod")) {
                er = re4t_parse_u32_dec(value, &v);
                if (er != RE4_OK) return er;
                entries[current].max_lod = (re4_u8)v;
            } else if (re4t_eq_nocase(key, "is_compressed")) {
                er = re4t_parse_u32_dec(value, &v);
                if (er != RE4_OK) return er;
                entries[current].is_compressed = (re4_u8)v;
            }
        }
    }
    out->entry_count = count;
    out->entries = entries;
    return RE4_OK;
}

re4_err re4_idxuhdtpl_find(const re4_idxuhdtpl *idx, re4_u32 pack_id, re4_u32 texture_id, re4_u32 *out_index)
{
    re4_u32 i;
    if (idx == 0 || out_index == 0) return RE4_ERR_NULL;
    for (i = 0U; i < idx->entry_count; ++i) {
        if (idx->entries[i].pack_id == pack_id && idx->entries[i].texture_id == texture_id) {
            *out_index = i;
            return RE4_OK;
        }
    }
    return RE4_ERR_RANGE;
}

static re4_err re4t_emit_cstr(re4_emit_fn emit, void *user, const char *z)
{
    re4_size n;
    if (emit == 0 || z == 0) return RE4_ERR_NULL;
    n = 0UL;
    while (z[n] != 0) ++n;
    return emit(user, z, n);
}

re4_err re4_idxuhdtpl_entry_emit_text(const re4_idxuhdtpl_entry *entry, re4_emit_fn emit, void *emit_user)
{
    char buf[512];
    if (entry == 0 || emit == 0) return RE4_ERR_NULL;
    sprintf(buf,
            "TPL_%03u\nPackID:%08X\nTextureID:%04u\nPixelFormatType:%02X\nwidth:%u\nheight:%u\nwrap_s:%u\nwrap_t:%u\nmin_filter:%u\nmag_filter:%u\nenable_lod:%u\nmin_lod:%u\nmax_lod:%u\nis_compressed:%u\n\n",
            (unsigned)entry->id,
            (unsigned)entry->pack_id,
            (unsigned)entry->texture_id,
            (unsigned)entry->pixel_format_type,
            (unsigned)entry->width,
            (unsigned)entry->height,
            (unsigned)entry->wrap_s,
            (unsigned)entry->wrap_t,
            (unsigned)entry->min_filter,
            (unsigned)entry->mag_filter,
            (unsigned)entry->enable_lod,
            (unsigned)entry->min_lod,
            (unsigned)entry->max_lod,
            (unsigned)entry->is_compressed);
    return re4t_emit_cstr(emit, emit_user, buf);
}

re4_err re4_idxuhdtpl_emit_text(const re4_idxuhdtpl *idx, re4_emit_fn emit, void *emit_user)
{
    re4_u32 i;
    re4_err er;
    if (idx == 0 || emit == 0) return RE4_ERR_NULL;
    for (i = 0U; i < idx->entry_count; ++i) {
        er = re4_idxuhdtpl_entry_emit_text(&idx->entries[i], emit, emit_user);
        if (er != RE4_OK) return er;
    }
    return RE4_OK;
}

static re4_err re4t_parse_bool_text(re4_strref s, re4_bool *out)
{
    if (out == 0) return RE4_ERR_NULL;
    s = re4t_trim(s);
    if (re4t_eq_nocase(s, "true") || re4t_eq_nocase(s, "yes") || re4t_eq_nocase(s, "1")) {
        *out = RE4_TRUE;
        return RE4_OK;
    }
    if (re4t_eq_nocase(s, "false") || re4t_eq_nocase(s, "no") || re4t_eq_nocase(s, "0")) {
        *out = RE4_FALSE;
        return RE4_OK;
    }
    return RE4_ERR_BAD_FORMAT;
}

static re4_err re4t_parse_u32_auto(re4_strref s, re4_u32 *out)
{
    re4_size i;
    s = re4t_trim(s);
    if (s.len > 2UL && s.ptr[0] == '0' && (s.ptr[1] == 'x' || s.ptr[1] == 'X')) return re4t_parse_u32_hex(s, out);
    for (i = 0UL; i < s.len; ++i) {
        if ((s.ptr[i] >= 'A' && s.ptr[i] <= 'F') || (s.ptr[i] >= 'a' && s.ptr[i] <= 'f')) return re4t_parse_u32_hex(s, out);
    }
    return re4t_parse_u32_dec(s, out);
}

re4_err re4_idxps2tpl_read_text(re4_span text, re4_idxps2tpl *out)
{
    re4_size pos;
    if (out == 0) return RE4_ERR_NULL;
    memset(out, 0, sizeof(*out));
    pos = 0UL;
    while (pos < text.size) {
        re4_strref line;
        re4_strref key;
        re4_strref value;
        re4_err er;
        line = re4t_next_line(text, &pos);
        line = re4t_trim(line);
        if (line.len == 0UL || line.ptr[0] == '#') continue;
        er = re4t_split_key_value(line, &key, &value);
        if (er != RE4_OK) continue;
        if (re4t_eq_nocase(key, "ImageFlipY")) {
            er = re4t_parse_bool_text(value, &out->image_flip_y);
            if (er != RE4_OK) return er;
            out->seen_mask |= RE4_IDXPS2TPL_SEEN_IMAGE_FLIP_Y;
        } else if (re4t_eq_nocase(key, "ImageFolder")) {
            out->image_folder = value;
            out->seen_mask |= RE4_IDXPS2TPL_SEEN_IMAGE_FOLDER;
        } else if (re4t_eq_nocase(key, "ImageFormat")) {
            out->image_format = value;
            out->seen_mask |= RE4_IDXPS2TPL_SEEN_IMAGE_FORMAT;
        }
    }
    return RE4_OK;
}

re4_err re4_idxtpl_header_read_text(re4_span text, re4_idxtpl_header *out)
{
    re4_size pos;
    if (out == 0) return RE4_ERR_NULL;
    memset(out, 0, sizeof(*out));
    pos = 0UL;
    while (pos < text.size) {
        re4_strref line;
        re4_strref key;
        re4_strref value;
        re4_u32 v;
        re4_err er;
        line = re4t_next_line(text, &pos);
        line = re4t_trim(line);
        if (line.len == 0UL || line.ptr[0] == '#') continue;
        er = re4t_split_key_value(line, &key, &value);
        if (er != RE4_OK) continue;
        if (re4t_eq_nocase(key, "Bitdepth")) {
            er = re4t_parse_u32_auto(value, &v); if (er != RE4_OK) return er;
            out->bitdepth = v; out->seen_mask |= RE4_IDXTPL_HEADER_SEEN_BITDEPTH;
        } else if (re4t_eq_nocase(key, "Interlace")) {
            er = re4t_parse_u32_auto(value, &v); if (er != RE4_OK) return er;
            out->interlace = v; out->seen_mask |= RE4_IDXTPL_HEADER_SEEN_INTERLACE;
        } else if (re4t_eq_nocase(key, "Width")) {
            er = re4t_parse_u32_dec(value, &v); if (er != RE4_OK) return er;
            out->width = v; out->seen_mask |= RE4_IDXTPL_HEADER_SEEN_WIDTH;
        } else if (re4t_eq_nocase(key, "Height")) {
            er = re4t_parse_u32_dec(value, &v); if (er != RE4_OK) return er;
            out->height = v; out->seen_mask |= RE4_IDXTPL_HEADER_SEEN_HEIGHT;
        } else if (re4t_eq_nocase(key, "Next")) {
            er = re4t_parse_u32_auto(value, &v); if (er != RE4_OK) return er;
            out->next = v; out->seen_mask |= RE4_IDXTPL_HEADER_SEEN_NEXT;
        } else if (re4t_eq_nocase(key, "MipmapStatus") || re4t_eq_nocase(key, "MipMapStatus")) {
            er = re4t_parse_u32_auto(value, &v); if (er != RE4_OK) return er;
            out->mipmap_status = v; out->seen_mask |= RE4_IDXTPL_HEADER_SEEN_MIPMAP_STATUS;
        } else if (re4t_eq_nocase(key, "Qwc")) {
            er = re4t_parse_u32_auto(value, &v); if (er != RE4_OK) return er;
            out->qwc = v; out->seen_mask |= RE4_IDXTPL_HEADER_SEEN_QWC;
        } else if (re4t_eq_nocase(key, "Ref")) {
            er = re4t_parse_u32_auto(value, &v); if (er != RE4_OK) return er;
            out->ref = v; out->seen_mask |= RE4_IDXTPL_HEADER_SEEN_REF;
        } else if (re4t_eq_nocase(key, "MipmapHeader1Offset") || re4t_eq_nocase(key, "Mipmap1Offset")) {
            er = re4t_parse_u32_auto(value, &v); if (er != RE4_OK) return er;
            out->mipmap_header1_offset = v; out->seen_mask |= RE4_IDXTPL_HEADER_SEEN_MIP1OFF;
        } else if (re4t_eq_nocase(key, "MipmapHeader2Offset") || re4t_eq_nocase(key, "Mipmap2Offset")) {
            er = re4t_parse_u32_auto(value, &v); if (er != RE4_OK) return er;
            out->mipmap_header2_offset = v; out->seen_mask |= RE4_IDXTPL_HEADER_SEEN_MIP2OFF;
        } else if (re4t_eq_nocase(key, "GsMip")) {
            er = re4t_parse_u32_auto(value, &v); if (er != RE4_OK) return er;
            out->gs_mip_lo = v; out->seen_mask |= RE4_IDXTPL_HEADER_SEEN_GSMIP;
        } else if (re4t_eq_nocase(key, "GsMipHi")) {
            er = re4t_parse_u32_auto(value, &v); if (er != RE4_OK) return er;
            out->gs_mip_hi = v; out->seen_mask |= RE4_IDXTPL_HEADER_SEEN_GSMIP;
        } else if (re4t_eq_nocase(key, "IndexesOffset") || re4t_eq_nocase(key, "IndexOffset")) {
            er = re4t_parse_u32_auto(value, &v); if (er != RE4_OK) return er;
            out->indexes_offset = v; out->seen_mask |= RE4_IDXTPL_HEADER_SEEN_INDEXES_OFFSET;
        } else if (re4t_eq_nocase(key, "PaletteOffset")) {
            er = re4t_parse_u32_auto(value, &v); if (er != RE4_OK) return er;
            out->palette_offset = v; out->seen_mask |= RE4_IDXTPL_HEADER_SEEN_PALETTE_OFFSET;
        } else if (re4t_eq_nocase(key, "GsTex")) {
            er = re4t_parse_u32_auto(value, &v); if (er != RE4_OK) return er;
            out->gs_tex = v; out->seen_mask |= RE4_IDXTPL_HEADER_SEEN_GSTEX;
        } else if (re4t_eq_nocase(key, "GsTexHi")) {
            er = re4t_parse_u32_auto(value, &v); if (er != RE4_OK) return er;
            out->gs_tex_hi = v; out->seen_mask |= RE4_IDXTPL_HEADER_SEEN_GSTEX;
        }
    }
    return RE4_OK;
}

static re4_err re4_idxtpl_emit_cstr(re4_emit_fn emit, void *user, const char *text)
{
    re4_size n;
    if (emit == 0 || text == 0) return RE4_ERR_NULL;
    n = 0UL;
    while (text[n] != 0) ++n;
    return emit(user, text, n);
}

re4_err re4_idxtpl_header_emit_text(const re4_idxtpl_header *h, re4_emit_fn emit, void *emit_user)
{
    char buf[512];
    re4_err er;
    if (h == 0 || emit == 0) return RE4_ERR_NULL;
    sprintf(buf,
            "Width:%u\nHeight:%u\nBitdepth:%u\nInterlace:%02X\nNext:%04X\nMipmapStatus:%04X\nQwc:%04X\nRef:%04X\nMipmapHeader1Offset:%08X\nMipmapHeader2Offset:%08X\nGsMip:%08X\nGsMipHi:%08X\nIndexesOffset:%08X\nPaletteOffset:%08X\nGsTex:%08X\nGsTexHi:%08X\n",
            (unsigned)h->width,
            (unsigned)h->height,
            (unsigned)h->bitdepth,
            (unsigned)h->interlace,
            (unsigned)h->next,
            (unsigned)h->mipmap_status,
            (unsigned)h->qwc,
            (unsigned)h->ref,
            (unsigned)h->mipmap_header1_offset,
            (unsigned)h->mipmap_header2_offset,
            (unsigned)h->gs_mip_lo,
            (unsigned)h->gs_mip_hi,
            (unsigned)h->indexes_offset,
            (unsigned)h->palette_offset,
            (unsigned)h->gs_tex,
            (unsigned)h->gs_tex_hi);
    er = re4_idxtpl_emit_cstr(emit, emit_user, buf);
    return er;
}

re4_bool re4_idxtpl_header_has_interlace(const re4_idxtpl_header *h, re4_u32 flag)
{
    if (h == 0) return RE4_FALSE;
    return (h->interlace & flag) != 0U ? RE4_TRUE : RE4_FALSE;
}

re4_bool re4_idxtpl_header_has_mipmaps(const re4_idxtpl_header *h)
{
    if (h == 0) return RE4_FALSE;
    if (h->mipmap_status == RE4_PS2TPL_MIPMAP_TWO) return RE4_TRUE;
    if (h->mipmap_header1_offset != 0U || h->mipmap_header2_offset != 0U) return RE4_TRUE;
    return RE4_FALSE;
}

re4_u32 re4_ps2tpl_bitdepth_palette_limit(re4_u32 bitdepth)
{
    if (bitdepth == 9U) return 256U;
    if (bitdepth == 8U) return 16U;
    return 0U;
}

re4_err re4_idxtpl_header_validate_basic(const re4_idxtpl_header *h, re4_u32 *out_warnings)
{
    re4_u32 w;
    if (h == 0 || out_warnings == 0) return RE4_ERR_NULL;
    w = 0U;
    if (h->width == 0U || h->height == 0U) w |= RE4_IDXTPL_WARN_ZERO_DIM;
    if (h->width < h->height) w |= RE4_IDXTPL_WARN_WIDTH_LT_HEIGHT;
    if (!(h->bitdepth == 6U || h->bitdepth == 8U || h->bitdepth == 9U)) w |= RE4_IDXTPL_WARN_BAD_BITDEPTH;
    if ((h->interlace & ~(RE4_PS2TPL_INTERLACE_ROTATE_IMAGE | RE4_PS2TPL_INTERLACE_SWIZZLE)) != 0U) w |= RE4_IDXTPL_WARN_BAD_INTERLACE;
    if ((h->seen_mask & RE4_IDXTPL_HEADER_SEEN_QWC) != 0U && h->qwc == 0U) w |= RE4_IDXTPL_WARN_ZERO_QWC;
    if (!(h->mipmap_status == RE4_PS2TPL_MIPMAP_NONE || h->mipmap_status == RE4_PS2TPL_MIPMAP_TWO)) w |= RE4_IDXTPL_WARN_BAD_MIPMAP;
    if (h->mipmap_status == RE4_PS2TPL_MIPMAP_TWO && (h->mipmap_header1_offset == 0U || h->mipmap_header2_offset == 0U)) w |= RE4_IDXTPL_WARN_MIP_OFFSET_GAP;
    *out_warnings = w;
    return RE4_OK;
}

const char *re4_idxtpl_warning_label(re4_u32 warning)
{
    switch (warning) {
    case RE4_IDXTPL_WARN_ZERO_DIM: return "zero dimensions";
    case RE4_IDXTPL_WARN_WIDTH_LT_HEIGHT: return "width lower than height";
    case RE4_IDXTPL_WARN_BAD_BITDEPTH: return "unexpected bitdepth";
    case RE4_IDXTPL_WARN_BAD_INTERLACE: return "unexpected interlace bits";
    case RE4_IDXTPL_WARN_ZERO_QWC: return "zero qwc";
    case RE4_IDXTPL_WARN_BAD_MIPMAP: return "unexpected mipmap status";
    case RE4_IDXTPL_WARN_MIP_OFFSET_GAP: return "missing mipmap offset";
    default: return "unknown warning";
    }
}
