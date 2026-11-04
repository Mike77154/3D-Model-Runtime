#include "re4c3d/re4_model.h"
#include "re4c3d/re4_texture.h"
#include "re4c3d/re4_base.h"
#include <string.h>
#include <stdio.h>

static re4_strref re4m_trim(re4_strref s)
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

static re4_bool re4m_is_digit(char c)
{
    return (c >= '0' && c <= '9') ? RE4_TRUE : RE4_FALSE;
}

static re4_u8 re4m_hexval(char c)
{
    if (c >= '0' && c <= '9') return (re4_u8)(c - '0');
    if (c >= 'a' && c <= 'f') return (re4_u8)(10 + c - 'a');
    if (c >= 'A' && c <= 'F') return (re4_u8)(10 + c - 'A');
    return 255U;
}

static re4_bool re4m_eq_nocase(re4_strref s, const char *z)
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

static re4_bool re4m_starts_nocase(re4_strref s, const char *z)
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

static re4_err re4m_parse_u32_dec(re4_strref s, re4_u32 *out)
{
    re4_size i;
    re4_u32 v;
    if (out == 0) return RE4_ERR_NULL;
    s = re4m_trim(s);
    if (s.len == 0UL) return RE4_ERR_BAD_FORMAT;
    v = 0U;
    for (i = 0UL; i < s.len; ++i) {
        re4_u32 d;
        if (!re4m_is_digit(s.ptr[i])) return RE4_ERR_BAD_FORMAT;
        d = (re4_u32)(s.ptr[i] - '0');
        if (v > 429496729U) return RE4_ERR_RANGE;
        v = (v * 10U) + d;
    }
    *out = v;
    return RE4_OK;
}

static re4_err re4m_parse_u32_hex(re4_strref s, re4_u32 *out)
{
    re4_size i;
    re4_u32 v;
    if (out == 0) return RE4_ERR_NULL;
    s = re4m_trim(s);
    if (s.len == 0UL) return RE4_ERR_BAD_FORMAT;
    if (s.len > 2UL && s.ptr[0] == '0' && (s.ptr[1] == 'x' || s.ptr[1] == 'X')) {
        s.ptr += 2;
        s.len -= 2;
    }
    v = 0U;
    for (i = 0UL; i < s.len; ++i) {
        re4_u8 d;
        d = re4m_hexval(s.ptr[i]);
        if (d == 255U) return RE4_ERR_BAD_FORMAT;
        v = (v << 4) | (re4_u32)d;
    }
    *out = v;
    return RE4_OK;
}

static re4_err re4m_split_key_value(re4_strref line, re4_strref *key, re4_strref *value)
{
    re4_size i;
    if (key == 0 || value == 0) return RE4_ERR_NULL;
    for (i = 0UL; i < line.len; ++i) {
        if (line.ptr[i] == ':') {
            key->ptr = line.ptr;
            key->len = i;
            value->ptr = line.ptr + i + 1UL;
            value->len = line.len - i - 1UL;
            *key = re4m_trim(*key);
            *value = re4m_trim(*value);
            return RE4_OK;
        }
    }
    return RE4_ERR_BAD_FORMAT;
}

static re4_strref re4m_next_line(re4_span text, re4_size *pos)
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

static void re4m_core_defaults(re4_bin_material_core *m)
{
    memset(m, 0, sizeof(*m));
    m->variant = RE4_BIN_MATERIAL_CORE_UHD_24;
    m->bump_map = 255U;
    m->opacity_map = 255U;
    m->generic_specular_map = 255U;
    m->custom_specular_map = 255U;
}

static re4_bool re4m_usematerial_line(re4_strref line, re4_strref *name)
{
    re4_strref key;
    re4_strref value;
    if (re4m_split_key_value(line, &key, &value) != RE4_OK) return RE4_FALSE;
    if (!re4m_eq_nocase(key, "UseMaterial")) return RE4_FALSE;
    if (name != 0) *name = value;
    return RE4_TRUE;
}

re4_err re4_idxmaterial_read_text(re4_span text, re4_arena *arena, re4_idxmaterial *out)
{
    re4_size pos;
    re4_u32 count;
    re4_u32 current;
    re4_idxmaterial_entry *entries;
    if (arena == 0 || out == 0) return RE4_ERR_NULL;
    memset(out, 0, sizeof(*out));
    pos = 0UL;
    count = 0U;
    while (pos < text.size) {
        re4_strref line;
        line = re4m_next_line(text, &pos);
        line = re4m_trim(line);
        if (line.len != 0UL && line.ptr[0] != '#' && re4m_usematerial_line(line, 0)) ++count;
    }
    entries = (re4_idxmaterial_entry *)re4_arena_push(arena, (re4_size)count * sizeof(re4_idxmaterial_entry), sizeof(re4_u32));
    if (count != 0U && entries == 0) return RE4_ERR_NO_SPACE;
    pos = 0UL;
    count = 0U;
    current = RE4_U32_MAX;
    while (pos < text.size) {
        re4_strref line;
        re4_strref name;
        line = re4m_next_line(text, &pos);
        line = re4m_trim(line);
        if (line.len == 0UL || line.ptr[0] == '#') continue;
        if (re4m_usematerial_line(line, &name)) {
            memset(&entries[count], 0, sizeof(entries[count]));
            entries[count].name = name;
            entries[count].seen_mask |= RE4_IDXMATERIAL_SEEN_NAME;
            re4m_core_defaults(&entries[count].core);
            current = count;
            ++count;
        } else if (current != RE4_U32_MAX) {
            re4_strref key;
            re4_strref value;
            re4_u32 v;
            re4_err er;
            er = re4m_split_key_value(line, &key, &value);
            if (er != RE4_OK) continue;
            if (re4m_eq_nocase(key, "material_flag")) {
                er = re4m_parse_u32_hex(value, &v);
                if (er != RE4_OK) return er;
                entries[current].core.material_flag = (re4_u8)v;
                entries[current].seen_mask |= RE4_IDXMATERIAL_SEEN_FLAG;
            } else if (re4m_eq_nocase(key, "diffuse_map")) {
                er = re4m_parse_u32_dec(value, &v);
                if (er != RE4_OK) return er;
                entries[current].core.diffuse_map = (re4_u8)v;
                entries[current].seen_mask |= RE4_IDXMATERIAL_SEEN_DIFFUSE;
            } else if (re4m_eq_nocase(key, "bump_map")) {
                er = re4m_parse_u32_dec(value, &v);
                if (er != RE4_OK) return er;
                entries[current].core.bump_map = (re4_u8)v;
                entries[current].seen_mask |= RE4_IDXMATERIAL_SEEN_BUMP;
            } else if (re4m_eq_nocase(key, "opacity_map")) {
                er = re4m_parse_u32_dec(value, &v);
                if (er != RE4_OK) return er;
                entries[current].core.opacity_map = (re4_u8)v;
                entries[current].seen_mask |= RE4_IDXMATERIAL_SEEN_OPACITY;
            } else if (re4m_eq_nocase(key, "generic_specular_map")) {
                er = re4m_parse_u32_dec(value, &v);
                if (er != RE4_OK) return er;
                entries[current].core.generic_specular_map = (re4_u8)v;
                entries[current].seen_mask |= RE4_IDXMATERIAL_SEEN_GENERIC_SPECULAR;
            } else if (re4m_eq_nocase(key, "custom_specular_map")) {
                er = re4m_parse_u32_dec(value, &v);
                if (er != RE4_OK) return er;
                entries[current].core.custom_specular_map = (re4_u8)v;
                entries[current].seen_mask |= RE4_IDXMATERIAL_SEEN_CUSTOM_SPECULAR;
            } else if (re4m_eq_nocase(key, "intensity_specular_r")) {
                er = re4m_parse_u32_dec(value, &v);
                if (er != RE4_OK) return er;
                entries[current].core.specular_rgb[0] = (re4_u8)v;
            } else if (re4m_eq_nocase(key, "intensity_specular_g")) {
                er = re4m_parse_u32_dec(value, &v);
                if (er != RE4_OK) return er;
                entries[current].core.specular_rgb[1] = (re4_u8)v;
            } else if (re4m_eq_nocase(key, "intensity_specular_b")) {
                er = re4m_parse_u32_dec(value, &v);
                if (er != RE4_OK) return er;
                entries[current].core.specular_rgb[2] = (re4_u8)v;
            } else if (re4m_eq_nocase(key, "unk_08")) {
                er = re4m_parse_u32_dec(value, &v);
                if (er != RE4_OK) return er;
                entries[current].core.unknown_08 = (re4_u8)v;
            } else if (re4m_eq_nocase(key, "unk_09")) {
                er = re4m_parse_u32_dec(value, &v);
                if (er != RE4_OK) return er;
                entries[current].core.unknown_09 = (re4_u8)v;
            } else if (re4m_eq_nocase(key, "specular_scale")) {
                er = re4m_parse_u32_hex(value, &v);
                if (er != RE4_OK) return er;
                entries[current].core.specular_scale = (re4_u8)v;
            } else if (re4m_eq_nocase(key, "unk_11")) {
                er = re4m_parse_u32_dec(value, &v);
                if (er != RE4_OK) return er;
                entries[current].core.unknown_11 = (re4_u8)v;
            } else if (re4m_starts_nocase(key, "unk_min_")) {
                re4_strref nref;
                re4_u32 which;
                nref.ptr = key.ptr + 8UL;
                nref.len = key.len - 8UL;
                er = re4m_parse_u32_dec(nref, &which);
                if (er == RE4_OK && which >= 1U && which <= 11U) {
                    er = re4m_parse_u32_dec(value, &v);
                    if (er != RE4_OK) return er;
                    entries[current].core.unknown_min[11U - which] = (re4_u8)v;
                }
            }
        }
    }
    out->entry_count = count;
    out->entries = entries;
    return RE4_OK;
}

static re4_err re4m_emit_cstr(re4_emit_fn emit, void *user, const char *z)
{
    re4_size n;
    if (emit == 0 || z == 0) return RE4_ERR_NULL;
    n = 0UL;
    while (z[n] != 0) ++n;
    return emit(user, z, n);
}

static re4_err re4m_emit_ref(re4_emit_fn emit, void *user, re4_strref s)
{
    if (emit == 0) return RE4_ERR_NULL;
    if (s.ptr == 0) return RE4_ERR_NULL;
    return emit(user, s.ptr, s.len);
}

re4_err re4_idxmaterial_emit_text(const re4_idxmaterial *idx, re4_emit_fn emit, void *emit_user)
{
    re4_u32 i;
    char buf[512];
    re4_err er;
    if (idx == 0 || emit == 0) return RE4_ERR_NULL;
    for (i = 0U; i < idx->entry_count; ++i) {
        er = re4m_emit_cstr(emit, emit_user, "UseMaterial:");
        if (er != RE4_OK) return er;
        er = re4m_emit_ref(emit, emit_user, idx->entries[i].name);
        if (er != RE4_OK) return er;
        sprintf(buf,
                "\nmaterial_flag:%02X\ndiffuse_map:%u\nbump_map:%u\nopacity_map:%u\ngeneric_specular_map:%u\nintensity_specular_r:%u\nintensity_specular_g:%u\nintensity_specular_b:%u\nunk_08:%u\nunk_09:%u\nspecular_scale:%02X\nunk_11:%u\ncustom_specular_map:%u\n\n",
                (unsigned)idx->entries[i].core.material_flag,
                (unsigned)idx->entries[i].core.diffuse_map,
                (unsigned)idx->entries[i].core.bump_map,
                (unsigned)idx->entries[i].core.opacity_map,
                (unsigned)idx->entries[i].core.generic_specular_map,
                (unsigned)idx->entries[i].core.specular_rgb[0],
                (unsigned)idx->entries[i].core.specular_rgb[1],
                (unsigned)idx->entries[i].core.specular_rgb[2],
                (unsigned)idx->entries[i].core.unknown_08,
                (unsigned)idx->entries[i].core.unknown_09,
                (unsigned)idx->entries[i].core.specular_scale,
                (unsigned)idx->entries[i].core.unknown_11,
                (unsigned)idx->entries[i].core.custom_specular_map);
        er = re4m_emit_cstr(emit, emit_user, buf);
        if (er != RE4_OK) return er;
    }
    return RE4_OK;
}

static re4_strref re4m_next_token(re4_strref s, re4_size *pos)
{
    re4_size start;
    re4_strref t;
    while (*pos < s.len && (s.ptr[*pos] == ' ' || s.ptr[*pos] == '\t')) ++(*pos);
    start = *pos;
    while (*pos < s.len && s.ptr[*pos] != ' ' && s.ptr[*pos] != '\t') ++(*pos);
    t.ptr = s.ptr + start;
    t.len = *pos - start;
    return t;
}

static re4_bool re4m_token_to_units(re4_strref tok, re4_u8 *out)
{
    re4_u32 v;
    re4_size i;
    if (out == 0) return RE4_FALSE;
    tok = re4m_trim(tok);
    if (tok.len == 0UL || !re4m_is_digit(tok.ptr[0])) return RE4_FALSE;
    v = 0U;
    i = 0UL;
    while (i < tok.len && re4m_is_digit(tok.ptr[i])) {
        v = (v * 10U) + (re4_u32)(tok.ptr[i] - '0');
        ++i;
    }
    if (v < 1U) v = 1U;
    if (v > 16U) v = 16U;
    *out = (re4_u8)v;
    return RE4_TRUE;
}

static re4_bool re4m_ext_is(re4_strref path, const char *ext)
{
    re4_size le;
    re4_size i;
    if (ext == 0) return RE4_FALSE;
    le = 0UL;
    while (ext[le] != 0) ++le;
    if (path.len < le) return RE4_FALSE;
    for (i = 0UL; i < le; ++i) {
        re4_u8 a;
        re4_u8 b;
        a = re4_ascii_tolower_u8((re4_u8)path.ptr[path.len - le + i]);
        b = re4_ascii_tolower_u8((re4_u8)ext[i]);
        if (a != b) return RE4_FALSE;
    }
    return RE4_TRUE;
}

static re4_err re4m_parse_texture_path(re4_strref path, re4_mtl_texture_ref *out)
{
    re4_size dot;
    re4_size tex_start;
    re4_size pack_end;
    re4_size pack_start;
    re4_strref tex;
    re4_strref pack;
    re4_err er;
    re4_u32 v;
    path = re4m_trim(path);
    out->path = path;
    out->has_pack = 0U;
    out->has_texture = 0U;
    out->ext = RE4_MTL_EXT_UNKNOWN;
    if (re4m_ext_is(path, ".dds")) out->ext = RE4_MTL_EXT_DDS;
    if (re4m_ext_is(path, ".tga")) out->ext = RE4_MTL_EXT_TGA;
    if (out->ext == RE4_MTL_EXT_UNKNOWN) return RE4_ERR_BAD_FORMAT;
    dot = path.len;
    while (dot != 0UL && path.ptr[dot - 1UL] != '.') --dot;
    if (dot == 0UL) return RE4_ERR_BAD_FORMAT;
    --dot;
    tex_start = dot;
    while (tex_start != 0UL && path.ptr[tex_start - 1UL] != '/' && path.ptr[tex_start - 1UL] != '\\') --tex_start;
    tex.ptr = path.ptr + tex_start;
    tex.len = dot - tex_start;
    er = re4m_parse_u32_dec(tex, &v);
    if (er != RE4_OK) return er;
    out->texture_id = v;
    out->has_texture = 1U;
    if (tex_start == 0UL) return RE4_OK;
    pack_end = tex_start - 1UL;
    while (pack_end != 0UL && (path.ptr[pack_end - 1UL] == '/' || path.ptr[pack_end - 1UL] == '\\')) --pack_end;
    pack_start = pack_end;
    while (pack_start != 0UL && path.ptr[pack_start - 1UL] != '/' && path.ptr[pack_start - 1UL] != '\\') --pack_start;
    if (pack_end > pack_start) {
        pack.ptr = path.ptr + pack_start;
        pack.len = pack_end - pack_start;
        if (pack.len == 8UL) {
            er = re4m_parse_u32_hex(pack, &v);
            if (er == RE4_OK) {
                out->pack_id = v;
                out->has_pack = 1U;
            }
        }
    }
    return RE4_OK;
}

re4_err re4_mtl_texture_ref_parse(re4_strref line, re4_mtl_texture_ref *out)
{
    re4_size pos;
    re4_strref cmd;
    re4_strref tok;
    re4_strref path;
    if (out == 0) return RE4_ERR_NULL;
    memset(out, 0, sizeof(*out));
    line = re4m_trim(line);
    if (line.len == 0UL || line.ptr[0] == '#') return RE4_ERR_BAD_FORMAT;
    pos = 0UL;
    cmd = re4m_next_token(line, &pos);
    if (cmd.len == 0UL) return RE4_ERR_BAD_FORMAT;
    out->command = cmd;
    if (re4m_eq_nocase(cmd, "map_Kd")) out->role = RE4_MTL_TEXROLE_DIFFUSE;
    else if (re4m_eq_nocase(cmd, "map_Bump") || re4m_eq_nocase(cmd, "Bump")) out->role = RE4_MTL_TEXROLE_BUMP;
    else if (re4m_eq_nocase(cmd, "map_d")) out->role = RE4_MTL_TEXROLE_OPACITY;
    else if (re4m_eq_nocase(cmd, "map_Ns")) out->role = RE4_MTL_TEXROLE_SPECULAR;
    else return RE4_ERR_BAD_FORMAT;
    path.ptr = 0;
    path.len = 0UL;
    out->scale_x_units = 1U;
    out->scale_y_units = 1U;
    while (pos < line.len) {
        tok = re4m_next_token(line, &pos);
        if (tok.len == 0UL) break;
        if (re4m_eq_nocase(tok, "-s")) {
            re4_strref sx;
            re4_strref sy;
            re4_strref sz;
            sx = re4m_next_token(line, &pos);
            sy = re4m_next_token(line, &pos);
            sz = re4m_next_token(line, &pos);
            (void)sz;
            if (re4m_token_to_units(sx, &out->scale_x_units) && re4m_token_to_units(sy, &out->scale_y_units)) out->has_scale = 1U;
        } else {
            path = tok;
        }
    }
    if (path.len == 0UL) return RE4_ERR_BAD_FORMAT;
    return re4m_parse_texture_path(path, out);
}

re4_u8 re4_mtl_specular_scale_pack(re4_u32 x_units, re4_u32 y_units)
{
    if (x_units < 1U) x_units = 1U;
    if (y_units < 1U) y_units = 1U;
    if (x_units > 16U) x_units = 16U;
    if (y_units > 16U) y_units = 16U;
    return (re4_u8)(((x_units - 1U) << 4) | (y_units - 1U));
}

void re4_mtl_specular_scale_unpack(re4_u8 packed, re4_u32 *x_units, re4_u32 *y_units)
{
    if (x_units != 0) *x_units = ((re4_u32)(packed >> 4) & 15U) + 1U;
    if (y_units != 0) *y_units = ((re4_u32)packed & 15U) + 1U;
}

re4_u32 re4_mtl_pixel_format_type_for_ref(const re4_mtl_texture_ref *ref)
{
    if (ref == 0) return 0U;
    if (ref->role == RE4_MTL_TEXROLE_DIFFUSE && ref->ext == RE4_MTL_EXT_DDS) return 0x0EU;
    if (ref->role == RE4_MTL_TEXROLE_OPACITY && ref->ext == RE4_MTL_EXT_TGA) return 0x00U;
    if (ref->role == RE4_MTL_TEXROLE_BUMP && ref->ext == RE4_MTL_EXT_TGA) return 0x03U;
    if (ref->role == RE4_MTL_TEXROLE_SPECULAR) {
        if (ref->has_pack && ref->pack_id == 0x07000000U && ref->ext == RE4_MTL_EXT_DDS) return 0x0EU;
        if (ref->ext == RE4_MTL_EXT_TGA) return 0x06U;
    }
    return 0U;
}

static re4_err re4m_find_marker_dec(re4_strref name, const char *marker, re4_u32 *out)
{
    re4_size i;
    re4_size mlen;
    if (out == 0 || marker == 0) return RE4_ERR_NULL;
    mlen = 0UL;
    while (marker[mlen] != 0) ++mlen;
    for (i = 0UL; i + mlen < name.len; ++i) {
        re4_strref at;
        at.ptr = name.ptr + i;
        at.len = name.len - i;
        if (re4m_starts_nocase(at, marker)) {
            re4_size j;
            re4_u32 v;
            j = i + mlen;
            if (j >= name.len || !re4m_is_digit(name.ptr[j])) continue;
            v = 0U;
            while (j < name.len && re4m_is_digit(name.ptr[j])) {
                v = (v * 10U) + (re4_u32)(name.ptr[j] - '0');
                ++j;
            }
            *out = v;
            return RE4_OK;
        }
    }
    return RE4_ERR_BAD_FORMAT;
}

static re4_err re4m_find_marker_hex(re4_strref name, const char *marker, re4_u32 *out)
{
    re4_size i;
    re4_size mlen;
    if (out == 0 || marker == 0) return RE4_ERR_NULL;
    mlen = 0UL;
    while (marker[mlen] != 0) ++mlen;
    for (i = 0UL; i + mlen < name.len; ++i) {
        re4_strref at;
        at.ptr = name.ptr + i;
        at.len = name.len - i;
        if (re4m_starts_nocase(at, marker)) {
            re4_size j;
            re4_u32 v;
            j = i + mlen;
            if (j >= name.len || re4m_hexval(name.ptr[j]) == 255U) continue;
            v = 0U;
            while (j < name.len && re4m_hexval(name.ptr[j]) != 255U) {
                v = (v << 4) | (re4_u32)re4m_hexval(name.ptr[j]);
                ++j;
            }
            *out = v;
            return RE4_OK;
        }
    }
    return RE4_ERR_BAD_FORMAT;
}

const char *re4_scenario_prefix_name(re4_scenario_prefix prefix)
{
    switch (prefix) {
    case RE4_SCENARIO_PREFIX_UHD: return "UHDSCENARIO";
    case RE4_SCENARIO_PREFIX_UU: return "UUSCENARIO";
    case RE4_SCENARIO_PREFIX_GG: return "GGSCENARIO";
    case RE4_SCENARIO_PREFIX_MAIN: return "MAINSCENARIO";
    case RE4_SCENARIO_PREFIX_FILE: return "FILE";
    default: return "unknown";
    }
}

re4_err re4_scenario_group_name_parse(re4_strref name, re4_scenario_group_ref *out)
{
    re4_err er;
    re4_u32 v;
    if (out == 0) return RE4_ERR_NULL;
    memset(out, 0, sizeof(*out));
    name = re4m_trim(name);
    if (re4m_starts_nocase(name, "UHDSCENARIO")) out->prefix = RE4_SCENARIO_PREFIX_UHD;
    else if (re4m_starts_nocase(name, "UUSCENARIO")) out->prefix = RE4_SCENARIO_PREFIX_UU;
    else if (re4m_starts_nocase(name, "GGSCENARIO")) out->prefix = RE4_SCENARIO_PREFIX_GG;
    else if (re4m_starts_nocase(name, "MAINSCENARIO")) out->prefix = RE4_SCENARIO_PREFIX_MAIN;
    else if (re4m_starts_nocase(name, "FILE_")) out->prefix = RE4_SCENARIO_PREFIX_FILE;
    else out->prefix = RE4_SCENARIO_PREFIX_UNKNOWN;
    er = re4m_find_marker_dec(name, "FILE_", &v);
    if (er == RE4_OK) { out->file_id = v; out->seen_mask |= RE4_SCENARIO_GROUP_SEEN_FILE; }
    er = re4m_find_marker_dec(name, "SMD_", &v);
    if (er == RE4_OK) { out->smd_id = v; out->seen_mask |= RE4_SCENARIO_GROUP_SEEN_SMD; }
    er = re4m_find_marker_dec(name, "SMX_", &v);
    if (er == RE4_OK) { out->smx_id = v; out->seen_mask |= RE4_SCENARIO_GROUP_SEEN_SMX; }
    er = re4m_find_marker_hex(name, "TYPE_", &v);
    if (er == RE4_OK) { out->object_status = v; out->seen_mask |= RE4_SCENARIO_GROUP_SEEN_TYPE; }
    er = re4m_find_marker_dec(name, "BIN_", &v);
    if (er == RE4_OK) { out->bin_id = v; out->seen_mask |= RE4_SCENARIO_GROUP_SEEN_BIN; }
    if (out->prefix == RE4_SCENARIO_PREFIX_UNKNOWN) return RE4_ERR_BAD_FORMAT;
    if ((out->seen_mask & RE4_SCENARIO_GROUP_SEEN_SMD) == 0U) return RE4_ERR_BAD_FORMAT;
    return RE4_OK;
}

const char *re4_scenario_status_flag_label(re4_u32 flag)
{
    switch (flag) {
    case RE4_SCENARIO_STATUS_EXE_SCRIPTED: return "exe_scripted";
    case RE4_SCENARIO_STATUS_UNKNOWN_02: return "unknown_02";
    case RE4_SCENARIO_STATUS_ASSIGN_SMX_GROUP: return "assign_smx_group";
    case RE4_SCENARIO_STATUS_ENDS_SMX_GROUP: return "ends_smx_group";
    case RE4_SCENARIO_STATUS_USE_SHARED_BIN: return "use_shared_bin";
    case RE4_SCENARIO_STATUS_USE_SHARED_TPL: return "use_shared_tpl";
    case RE4_SCENARIO_STATUS_USE_SHARED_MOT: return "use_shared_mot";
    case RE4_SCENARIO_STATUS_UNKNOWN_80: return "unknown_80";
    default: return "unknown";
    }
}

static re4_err re4m_parse_bool(re4_strref s, re4_bool *out)
{
    if (out == 0) return RE4_ERR_NULL;
    s = re4m_trim(s);
    if (re4m_eq_nocase(s, "true") || re4m_eq_nocase(s, "yes") || re4m_eq_nocase(s, "1")) { *out = RE4_TRUE; return RE4_OK; }
    if (re4m_eq_nocase(s, "false") || re4m_eq_nocase(s, "no") || re4m_eq_nocase(s, "0")) { *out = RE4_FALSE; return RE4_OK; }
    return RE4_ERR_BAD_FORMAT;
}

re4_err re4_scenario_idx_options_read_text(re4_span text, re4_scenario_idx_options *out)
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
        re4_bool b;
        re4_err er;
        line = re4m_next_line(text, &pos);
        line = re4m_trim(line);
        if (line.len == 0UL || line.ptr[0] == '#') continue;
        er = re4m_split_key_value(line, &key, &value);
        if (er != RE4_OK) continue;
        if (re4m_eq_nocase(key, "Magic")) {
            er = re4m_parse_u32_hex(value, &v);
            if (er != RE4_OK) return er;
            out->magic = v;
            out->seen_mask |= RE4_SCENARIO_IDX_SEEN_MAGIC;
        } else if (re4m_eq_nocase(key, "SmdFileName")) {
            out->smd_file_name = value;
            out->seen_mask |= RE4_SCENARIO_IDX_SEEN_SMD_FILE_NAME;
        } else if (re4m_eq_nocase(key, "TplFileName")) {
            out->tpl_file_name = value;
            out->seen_mask |= RE4_SCENARIO_IDX_SEEN_TPL_FILE_NAME;
        } else if (re4m_eq_nocase(key, "BinFolder")) {
            out->bin_folder = value;
            out->seen_mask |= RE4_SCENARIO_IDX_SEEN_BIN_FOLDER;
        } else if (re4m_eq_nocase(key, "SharedFileName")) {
            out->shared_file_name = value;
            out->seen_mask |= RE4_SCENARIO_IDX_SEEN_SHARED_FILE_NAME;
        } else if (re4m_eq_nocase(key, "UseIdxUhdTpl")) {
            er = re4m_parse_bool(value, &b);
            if (er != RE4_OK) return er;
            out->use_idxuhdtpl = b;
            out->seen_mask |= RE4_SCENARIO_IDX_SEEN_USE_IDXUHDTPL;
        } else if (re4m_eq_nocase(key, "UseIdxMaterial")) {
            er = re4m_parse_bool(value, &b);
            if (er != RE4_OK) return er;
            out->use_idxmaterial = b;
            out->seen_mask |= RE4_SCENARIO_IDX_SEEN_USE_IDXMATERIAL;
        } else if (re4m_eq_nocase(key, "EnableVertexColor")) {
            er = re4m_parse_bool(value, &b);
            if (er != RE4_OK) return er;
            out->enable_vertex_color = b;
            out->seen_mask |= RE4_SCENARIO_IDX_SEEN_ENABLE_VERTEX_COLOR;
        } else if (re4m_eq_nocase(key, "EnableDinamicVertexColor") || re4m_eq_nocase(key, "EnableDynamicVertexColor")) {
            er = re4m_parse_bool(value, &b);
            if (er != RE4_OK) return er;
            out->enable_dynamic_vertex_color = b;
            out->seen_mask |= RE4_SCENARIO_IDX_SEEN_ENABLE_DYNAMIC_COLOR;
        }
    }
    return RE4_OK;
}


static re4_bool re4m_line_command(re4_strref line, const char *cmd, re4_strref *rest)
{
    re4_size pos;
    re4_strref first;
    line = re4m_trim(line);
    pos = 0UL;
    first = re4m_next_token(line, &pos);
    if (!re4m_eq_nocase(first, cmd)) return RE4_FALSE;
    if (rest != 0) {
        rest->ptr = line.ptr + pos;
        rest->len = line.len - pos;
        *rest = re4m_trim(*rest);
    }
    return RE4_TRUE;
}

static re4_err re4m_decimal_01_to_u8(re4_strref s, re4_u8 *out)
{
    re4_size i;
    re4_u32 whole;
    re4_u32 frac;
    re4_u32 denom;
    re4_u32 digits;
    re4_u32 mag;
    if (out == 0) return RE4_ERR_NULL;
    s = re4m_trim(s);
    if (s.len == 0UL) return RE4_ERR_BAD_FORMAT;
    if (s.ptr[0] == '-') {
        *out = 0U;
        return RE4_OK;
    }
    i = 0UL;
    if (i >= s.len || !re4m_is_digit(s.ptr[i])) return RE4_ERR_BAD_FORMAT;
    whole = 0U;
    while (i < s.len && re4m_is_digit(s.ptr[i])) {
        whole = (whole * 10U) + (re4_u32)(s.ptr[i] - '0');
        ++i;
    }
    if (whole >= 1U) {
        *out = 255U;
        return RE4_OK;
    }
    frac = 0U;
    denom = 1U;
    digits = 0U;
    if (i < s.len && s.ptr[i] == '.') {
        ++i;
        while (i < s.len && re4m_is_digit(s.ptr[i])) {
            if (digits < 6U) {
                frac = (frac * 10U) + (re4_u32)(s.ptr[i] - '0');
                denom *= 10U;
                ++digits;
            }
            ++i;
        }
    }
    if (i != s.len) return RE4_ERR_BAD_FORMAT;
    mag = ((whole * denom + frac) * 255U + (denom / 2U)) / denom;
    if (mag > 255U) mag = 255U;
    *out = (re4_u8)mag;
    return RE4_OK;
}

static re4_err re4m_parse_ks_rgb(re4_strref line, re4_u8 rgb[3])
{
    re4_size pos;
    re4_strref cmd;
    re4_strref r;
    re4_strref g;
    re4_strref b;
    re4_err er;
    if (rgb == 0) return RE4_ERR_NULL;
    line = re4m_trim(line);
    pos = 0UL;
    cmd = re4m_next_token(line, &pos);
    if (!re4m_eq_nocase(cmd, "Ks")) return RE4_ERR_BAD_FORMAT;
    r = re4m_next_token(line, &pos);
    g = re4m_next_token(line, &pos);
    b = re4m_next_token(line, &pos);
    if (r.len == 0UL || g.len == 0UL || b.len == 0UL) return RE4_ERR_BAD_FORMAT;
    er = re4m_decimal_01_to_u8(r, &rgb[0]);
    if (er != RE4_OK) return er;
    er = re4m_decimal_01_to_u8(g, &rgb[1]);
    if (er != RE4_OK) return er;
    er = re4m_decimal_01_to_u8(b, &rgb[2]);
    if (er != RE4_OK) return er;
    return RE4_OK;
}

re4_err re4_mtl_read_text(re4_span text, re4_arena *arena, re4_mtl *out)
{
    re4_size pos;
    re4_u32 count;
    re4_u32 current;
    re4_mtl_material *materials;
    if (arena == 0 || out == 0) return RE4_ERR_NULL;
    memset(out, 0, sizeof(*out));
    pos = 0UL;
    count = 0U;
    while (pos < text.size) {
        re4_strref line;
        re4_strref name;
        line = re4m_next_line(text, &pos);
        line = re4m_trim(line);
        if (line.len == 0UL || line.ptr[0] == '#') continue;
        if (re4m_line_command(line, "newmtl", &name) && name.len != 0UL) ++count;
    }
    materials = (re4_mtl_material *)re4_arena_push(arena, (re4_size)count * sizeof(re4_mtl_material), sizeof(re4_u32));
    if (count != 0U && materials == 0) return RE4_ERR_NO_SPACE;
    pos = 0UL;
    count = 0U;
    current = RE4_U32_MAX;
    while (pos < text.size) {
        re4_strref line;
        re4_strref name;
        line = re4m_next_line(text, &pos);
        line = re4m_trim(line);
        if (line.len == 0UL || line.ptr[0] == '#') continue;
        if (re4m_line_command(line, "newmtl", &name) && name.len != 0UL) {
            memset(&materials[count], 0, sizeof(materials[count]));
            materials[count].name = name;
            materials[count].seen_mask = RE4_MTL_MATERIAL_SEEN_NAME;
            current = count;
            ++count;
        } else if (current != RE4_U32_MAX) {
            re4_mtl_texture_ref ref;
            if (re4m_parse_ks_rgb(line, materials[current].specular_rgb) == RE4_OK) {
                materials[current].seen_mask |= RE4_MTL_MATERIAL_SEEN_KS;
            } else if (re4_mtl_texture_ref_parse(line, &ref) == RE4_OK) {
                if (ref.role == RE4_MTL_TEXROLE_DIFFUSE) {
                    materials[current].diffuse = ref;
                    materials[current].seen_mask |= RE4_MTL_MATERIAL_SEEN_DIFFUSE;
                } else if (ref.role == RE4_MTL_TEXROLE_BUMP) {
                    if (re4m_eq_nocase(ref.command, "Bump")) {
                        materials[current].bump = ref;
                        materials[current].seen_mask |= RE4_MTL_MATERIAL_SEEN_BUMP | RE4_MTL_MATERIAL_BUMP_IS_BUMP;
                    } else if ((materials[current].seen_mask & RE4_MTL_MATERIAL_BUMP_IS_BUMP) == 0U) {
                        materials[current].bump = ref;
                        materials[current].seen_mask |= RE4_MTL_MATERIAL_SEEN_BUMP;
                    }
                } else if (ref.role == RE4_MTL_TEXROLE_OPACITY) {
                    materials[current].opacity = ref;
                    materials[current].seen_mask |= RE4_MTL_MATERIAL_SEEN_OPACITY;
                } else if (ref.role == RE4_MTL_TEXROLE_SPECULAR) {
                    materials[current].specular = ref;
                    materials[current].seen_mask |= RE4_MTL_MATERIAL_SEEN_SPECULAR;
                }
            }
        }
    }
    out->material_count = count;
    out->materials = materials;
    return RE4_OK;
}

static re4_bool re4m_mtl_entry_has(const re4_idxuhdtpl_entry *entries, re4_u32 count, re4_u32 pack_id, re4_u32 texture_id)
{
    re4_u32 i;
    for (i = 0U; i < count; ++i) {
        if (entries[i].pack_id == pack_id && entries[i].texture_id == texture_id) return RE4_TRUE;
    }
    return RE4_FALSE;
}

static void re4m_mtl_push_ref_if_new(re4_idxuhdtpl_entry *entries, re4_u32 *count, const re4_mtl_texture_ref *ref, re4_u32 width, re4_u32 height)
{
    re4_idxuhdtpl_entry *e;
    if (entries == 0 || count == 0 || ref == 0) return;
    if (!ref->has_pack || !ref->has_texture) return;
    if (re4m_mtl_entry_has(entries, *count, ref->pack_id, ref->texture_id)) return;
    e = &entries[*count];
    memset(e, 0, sizeof(*e));
    e->id = *count;
    e->pack_id = ref->pack_id;
    e->texture_id = ref->texture_id;
    e->pixel_format_type = re4_mtl_pixel_format_type_for_ref(ref);
    e->width = width == 0U ? 1U : width;
    e->height = height == 0U ? 1U : height;
    e->wrap_s = 1U;
    e->wrap_t = 1U;
    e->min_filter = 1U;
    e->mag_filter = 1U;
    e->seen_mask = RE4_IDXUHDTPL_SEEN_PACK_ID | RE4_IDXUHDTPL_SEEN_TEXTURE_ID | RE4_IDXUHDTPL_SEEN_PIXEL_FORMAT | RE4_IDXUHDTPL_SEEN_WIDTH | RE4_IDXUHDTPL_SEEN_HEIGHT;
    ++(*count);
}

re4_err re4_mtl_collect_idxuhdtpl(const re4_mtl *mtl, re4_u32 width, re4_u32 height, re4_arena *arena, struct re4_idxuhdtpl_s *out)
{
    re4_u32 cap;
    re4_u32 i;
    re4_u32 count;
    re4_idxuhdtpl_entry *entries;
    if (mtl == 0 || arena == 0 || out == 0) return RE4_ERR_NULL;
    memset(out, 0, sizeof(*out));
    if (mtl->material_count > 16384U) return RE4_ERR_BAD_FORMAT;
    cap = mtl->material_count * 4U;
    entries = (re4_idxuhdtpl_entry *)re4_arena_push(arena, (re4_size)cap * sizeof(re4_idxuhdtpl_entry), sizeof(re4_u32));
    if (cap != 0U && entries == 0) return RE4_ERR_NO_SPACE;
    count = 0U;
    for (i = 0U; i < mtl->material_count; ++i) {
        const re4_mtl_material *m;
        m = &mtl->materials[i];
        if ((m->seen_mask & RE4_MTL_MATERIAL_SEEN_DIFFUSE) != 0U) re4m_mtl_push_ref_if_new(entries, &count, &m->diffuse, width, height);
        if ((m->seen_mask & RE4_MTL_MATERIAL_SEEN_BUMP) != 0U) re4m_mtl_push_ref_if_new(entries, &count, &m->bump, width, height);
        if ((m->seen_mask & RE4_MTL_MATERIAL_SEEN_OPACITY) != 0U) re4m_mtl_push_ref_if_new(entries, &count, &m->opacity, width, height);
        if ((m->seen_mask & RE4_MTL_MATERIAL_SEEN_SPECULAR) != 0U) re4m_mtl_push_ref_if_new(entries, &count, &m->specular, width, height);
    }
    out->entry_count = count;
    out->entries = entries;
    return RE4_OK;
}

static re4_err re4m_mtl_resolve_tpl_id(const struct re4_idxuhdtpl_s *tpl_order, const re4_mtl_texture_ref *ref, re4_u8 *out_id)
{
    re4_u32 index;
    re4_u32 id;
    re4_err er;
    if (tpl_order == 0 || ref == 0 || out_id == 0) return RE4_ERR_NULL;
    if (!ref->has_pack || !ref->has_texture) return RE4_ERR_BAD_FORMAT;
    er = re4_idxuhdtpl_find(tpl_order, ref->pack_id, ref->texture_id, &index);
    if (er != RE4_OK) return er;
    id = tpl_order->entries[index].id;
    if (id > 255U) return RE4_ERR_RANGE;
    *out_id = (re4_u8)id;
    return RE4_OK;
}

re4_err re4_mtl_material_make_core(const re4_mtl_material *mat, const struct re4_idxuhdtpl_s *tpl_order, re4_bin_material_core *out)
{
    re4_err er;
    if (mat == 0 || out == 0) return RE4_ERR_NULL;
    re4m_core_defaults(out);
    if ((mat->seen_mask & RE4_MTL_MATERIAL_SEEN_KS) != 0U) {
        out->specular_rgb[0] = mat->specular_rgb[0];
        out->specular_rgb[1] = mat->specular_rgb[1];
        out->specular_rgb[2] = mat->specular_rgb[2];
    }
    if ((mat->seen_mask & RE4_MTL_MATERIAL_SEEN_DIFFUSE) != 0U) {
        er = re4m_mtl_resolve_tpl_id(tpl_order, &mat->diffuse, &out->diffuse_map);
        if (er != RE4_OK) return er;
    }
    if ((mat->seen_mask & RE4_MTL_MATERIAL_SEEN_BUMP) != 0U) {
        er = re4m_mtl_resolve_tpl_id(tpl_order, &mat->bump, &out->bump_map);
        if (er != RE4_OK) return er;
        out->material_flag = (re4_u8)(out->material_flag | RE4_BIN_MATERIAL_F_BUMP_MAP);
    }
    if ((mat->seen_mask & RE4_MTL_MATERIAL_SEEN_OPACITY) != 0U) {
        er = re4m_mtl_resolve_tpl_id(tpl_order, &mat->opacity, &out->opacity_map);
        if (er != RE4_OK) return er;
        out->material_flag = (re4_u8)(out->material_flag | RE4_BIN_MATERIAL_F_OPACITY_MAP);
    }
    if ((mat->seen_mask & RE4_MTL_MATERIAL_SEEN_SPECULAR) != 0U) {
        if (mat->specular.has_pack && mat->specular.pack_id == 0x07000000U && mat->specular.ext == RE4_MTL_EXT_DDS) {
            if (mat->specular.texture_id > 255U) return RE4_ERR_RANGE;
            out->generic_specular_map = (re4_u8)mat->specular.texture_id;
            out->material_flag = (re4_u8)(out->material_flag | RE4_BIN_MATERIAL_F_GENERIC_SPECULAR);
        } else {
            er = re4m_mtl_resolve_tpl_id(tpl_order, &mat->specular, &out->custom_specular_map);
            if (er != RE4_OK) return er;
            out->generic_specular_map = 0U;
            out->material_flag = (re4_u8)(out->material_flag | RE4_BIN_MATERIAL_F_CUSTOM_SPECULAR);
        }
        if (mat->specular.has_scale) {
            out->specular_scale = re4_mtl_specular_scale_pack(mat->specular.scale_x_units, mat->specular.scale_y_units);
        }
    }
    return RE4_OK;
}

re4_err re4_idxmaterial_from_mtl(const re4_mtl *mtl, const struct re4_idxuhdtpl_s *tpl_order, re4_arena *arena, re4_idxmaterial *out)
{
    re4_u32 i;
    re4_idxmaterial_entry *entries;
    if (mtl == 0 || tpl_order == 0 || arena == 0 || out == 0) return RE4_ERR_NULL;
    memset(out, 0, sizeof(*out));
    if (mtl->material_count > 16384U) return RE4_ERR_BAD_FORMAT;
    entries = (re4_idxmaterial_entry *)re4_arena_push(arena, (re4_size)mtl->material_count * sizeof(re4_idxmaterial_entry), sizeof(re4_u32));
    if (mtl->material_count != 0U && entries == 0) return RE4_ERR_NO_SPACE;
    for (i = 0U; i < mtl->material_count; ++i) {
        re4_err er;
        memset(&entries[i], 0, sizeof(entries[i]));
        entries[i].name = mtl->materials[i].name;
        entries[i].seen_mask = RE4_IDXMATERIAL_SEEN_NAME;
        er = re4_mtl_material_make_core(&mtl->materials[i], tpl_order, &entries[i].core);
        if (er != RE4_OK) return er;
        entries[i].seen_mask |= RE4_IDXMATERIAL_SEEN_FLAG | RE4_IDXMATERIAL_SEEN_DIFFUSE;
        if ((mtl->materials[i].seen_mask & RE4_MTL_MATERIAL_SEEN_BUMP) != 0U) entries[i].seen_mask |= RE4_IDXMATERIAL_SEEN_BUMP;
        if ((mtl->materials[i].seen_mask & RE4_MTL_MATERIAL_SEEN_OPACITY) != 0U) entries[i].seen_mask |= RE4_IDXMATERIAL_SEEN_OPACITY;
        if ((entries[i].core.material_flag & RE4_BIN_MATERIAL_F_GENERIC_SPECULAR) != 0U) entries[i].seen_mask |= RE4_IDXMATERIAL_SEEN_GENERIC_SPECULAR;
        if ((entries[i].core.material_flag & RE4_BIN_MATERIAL_F_CUSTOM_SPECULAR) != 0U) entries[i].seen_mask |= RE4_IDXMATERIAL_SEEN_CUSTOM_SPECULAR;
    }
    out->entry_count = mtl->material_count;
    out->entries = entries;
    return RE4_OK;
}


static re4_err re4m_parse_q16_text(re4_strref s, re4_q16 *out)
{
    re4_size i;
    re4_bool neg;
    re4_u32 whole;
    re4_u32 frac;
    re4_u32 denom;
    re4_u32 mag;
    re4_u32 frac_mag;
    if (out == 0) return RE4_ERR_NULL;
    s = re4m_trim(s);
    if (s.len == 0UL) return RE4_ERR_BAD_FORMAT;
    i = 0UL;
    neg = RE4_FALSE;
    if (s.ptr[i] == '-') { neg = RE4_TRUE; ++i; }
    else if (s.ptr[i] == '+') { ++i; }
    if (i >= s.len || !re4m_is_digit(s.ptr[i])) return RE4_ERR_BAD_FORMAT;
    whole = 0U;
    while (i < s.len && re4m_is_digit(s.ptr[i])) {
        if (whole > 32767U) {
            *out = neg ? RE4_I32_MIN : RE4_I32_MAX;
            return RE4_OK;
        }
        whole = (whole * 10U) + (re4_u32)(s.ptr[i] - '0');
        ++i;
    }
    frac = 0U;
    denom = 1U;
    if (i < s.len && s.ptr[i] == '.') {
        re4_u32 digits;
        ++i;
        digits = 0U;
        while (i < s.len && re4m_is_digit(s.ptr[i])) {
            if (digits < 4U) {
                frac = (frac * 10U) + (re4_u32)(s.ptr[i] - '0');
                denom *= 10U;
                ++digits;
            }
            ++i;
        }
    }
    if (i != s.len) return RE4_ERR_BAD_FORMAT;
    if (whole > 32767U) {
        *out = neg ? RE4_I32_MIN : RE4_I32_MAX;
        return RE4_OK;
    }
    frac_mag = ((frac * 65536U) + (denom / 2U)) / denom;
    mag = (whole << 16) + frac_mag;
    *out = re4_clamp_i64_like_q16(mag, neg);
    return RE4_OK;
}

static int re4m_cmp_strref_nocase_order(re4_strref a, re4_strref b)
{
    re4_size i;
    re4_size n;
    n = a.len < b.len ? a.len : b.len;
    for (i = 0UL; i < n; ++i) {
        re4_u8 ca;
        re4_u8 cb;
        ca = re4_ascii_tolower_u8((re4_u8)a.ptr[i]);
        cb = re4_ascii_tolower_u8((re4_u8)b.ptr[i]);
        if (ca < cb) return -1;
        if (ca > cb) return 1;
    }
    if (a.len < b.len) return -1;
    if (a.len > b.len) return 1;
    return 0;
}

static re4_bool re4m_scenario_entry_header_parse(re4_strref line, re4_scenario_idx_entry *out)
{
    re4_u32 v;
    if (out == 0) return RE4_FALSE;
    memset(out, 0, sizeof(*out));
    out->scale[0] = (re4_q16)65536;
    out->scale[1] = (re4_q16)65536;
    out->scale[2] = (re4_q16)65536;
    if (re4m_find_marker_dec(line, "FILE_", &v) == RE4_OK) {
        out->file_id = v;
        out->seen_mask |= RE4_SCENARIO_ENTRY_SEEN_FILE;
    }
    if (re4m_find_marker_dec(line, "SMD_", &v) == RE4_OK) {
        out->smd_id = v;
        out->seen_mask |= RE4_SCENARIO_ENTRY_SEEN_SMD;
    }
    return (out->seen_mask & RE4_SCENARIO_ENTRY_SEEN_SMD) != 0U ? RE4_TRUE : RE4_FALSE;
}

static re4_bool re4m_key_starts_extra_file(re4_strref key)
{
    if (re4m_starts_nocase(key, "ExtraSmdFileName_")) return RE4_TRUE;
    if (re4m_starts_nocase(key, "ExtraFileName_")) return RE4_TRUE;
    return RE4_FALSE;
}

re4_err re4_scenario_idx_read_text(re4_span text, re4_arena *arena, re4_scenario_idx_doc *out)
{
    re4_size pos;
    re4_u32 count;
    re4_u32 cur;
    re4_scenario_idx_entry *entries;
    re4_err er;
    if (arena == 0 || out == 0) return RE4_ERR_NULL;
    memset(out, 0, sizeof(*out));
    er = re4_scenario_idx_options_read_text(text, &out->options);
    if (er != RE4_OK) return er;
    pos = 0UL;
    count = 0U;
    while (pos < text.size) {
        re4_strref line;
        re4_strref key;
        re4_strref value;
        re4_scenario_idx_entry tmp;
        line = re4m_next_line(text, &pos);
        line = re4m_trim(line);
        if (line.len == 0UL || line.ptr[0] == '#') continue;
        if (re4m_split_key_value(line, &key, &value) == RE4_OK) {
            if (re4m_starts_nocase(key, "ExtraParameter_")) ++out->extra_parameter_count;
            else if (re4m_key_starts_extra_file(key)) ++out->extra_file_name_count;
            else if (re4m_starts_nocase(key, "ExtraTplFileName_")) ++out->extra_tpl_file_name_count;
            continue;
        }
        if (re4m_scenario_entry_header_parse(line, &tmp)) ++count;
    }
    entries = (re4_scenario_idx_entry *)re4_arena_push(arena, (re4_size)count * sizeof(re4_scenario_idx_entry), sizeof(re4_u32));
    if (count != 0U && entries == 0) return RE4_ERR_NO_SPACE;
    pos = 0UL;
    count = 0U;
    cur = RE4_U32_MAX;
    while (pos < text.size) {
        re4_strref line;
        re4_strref key;
        re4_strref value;
        line = re4m_next_line(text, &pos);
        line = re4m_trim(line);
        if (line.len == 0UL || line.ptr[0] == '#') continue;
        if (re4m_split_key_value(line, &key, &value) != RE4_OK) {
            re4_scenario_idx_entry tmp;
            if (re4m_scenario_entry_header_parse(line, &tmp)) {
                entries[count] = tmp;
                cur = count;
                ++count;
            }
        } else if (cur != RE4_U32_MAX) {
            re4_u32 v;
            re4_q16 q;
            if (re4m_eq_nocase(key, "PositionX")) {
                er = re4m_parse_q16_text(value, &q); if (er != RE4_OK) return er;
                entries[cur].position[0] = q; entries[cur].seen_mask |= RE4_SCENARIO_ENTRY_SEEN_POS_X;
            } else if (re4m_eq_nocase(key, "PositionY")) {
                er = re4m_parse_q16_text(value, &q); if (er != RE4_OK) return er;
                entries[cur].position[1] = q; entries[cur].seen_mask |= RE4_SCENARIO_ENTRY_SEEN_POS_Y;
            } else if (re4m_eq_nocase(key, "PositionZ")) {
                er = re4m_parse_q16_text(value, &q); if (er != RE4_OK) return er;
                entries[cur].position[2] = q; entries[cur].seen_mask |= RE4_SCENARIO_ENTRY_SEEN_POS_Z;
            } else if (re4m_eq_nocase(key, "AngleX")) {
                er = re4m_parse_q16_text(value, &q); if (er != RE4_OK) return er;
                entries[cur].angle[0] = q; entries[cur].seen_mask |= RE4_SCENARIO_ENTRY_SEEN_ANG_X;
            } else if (re4m_eq_nocase(key, "AngleY")) {
                er = re4m_parse_q16_text(value, &q); if (er != RE4_OK) return er;
                entries[cur].angle[1] = q; entries[cur].seen_mask |= RE4_SCENARIO_ENTRY_SEEN_ANG_Y;
            } else if (re4m_eq_nocase(key, "AngleZ")) {
                er = re4m_parse_q16_text(value, &q); if (er != RE4_OK) return er;
                entries[cur].angle[2] = q; entries[cur].seen_mask |= RE4_SCENARIO_ENTRY_SEEN_ANG_Z;
            } else if (re4m_eq_nocase(key, "ScaleX")) {
                er = re4m_parse_q16_text(value, &q); if (er != RE4_OK) return er;
                entries[cur].scale[0] = q; entries[cur].seen_mask |= RE4_SCENARIO_ENTRY_SEEN_SCALE_X;
            } else if (re4m_eq_nocase(key, "ScaleY")) {
                er = re4m_parse_q16_text(value, &q); if (er != RE4_OK) return er;
                entries[cur].scale[1] = q; entries[cur].seen_mask |= RE4_SCENARIO_ENTRY_SEEN_SCALE_Y;
            } else if (re4m_eq_nocase(key, "ScaleZ")) {
                er = re4m_parse_q16_text(value, &q); if (er != RE4_OK) return er;
                entries[cur].scale[2] = q; entries[cur].seen_mask |= RE4_SCENARIO_ENTRY_SEEN_SCALE_Z;
            } else if (re4m_eq_nocase(key, "TplFileID")) {
                er = re4m_parse_u32_dec(value, &v); if (er != RE4_OK) return er;
                entries[cur].tpl_file_id = v; entries[cur].seen_mask |= RE4_SCENARIO_ENTRY_SEEN_TPL_FILE;
            } else if (re4m_eq_nocase(key, "BinFileID")) {
                er = re4m_parse_u32_dec(value, &v); if (er != RE4_OK) return er;
                entries[cur].bin_file_id = v; entries[cur].seen_mask |= RE4_SCENARIO_ENTRY_SEEN_BIN_FILE;
            } else if (re4m_eq_nocase(key, "SmxID")) {
                er = re4m_parse_u32_dec(value, &v); if (er != RE4_OK) return er;
                entries[cur].smx_id = v; entries[cur].seen_mask |= RE4_SCENARIO_ENTRY_SEEN_SMX;
            } else if (re4m_eq_nocase(key, "ObjectStatus")) {
                er = re4m_parse_u32_hex(value, &v); if (er != RE4_OK) return er;
                entries[cur].object_status = v; entries[cur].seen_mask |= RE4_SCENARIO_ENTRY_SEEN_STATUS;
            } else if (re4m_eq_nocase(key, "FixedFF")) {
                er = re4m_parse_u32_hex(value, &v); if (er != RE4_OK) return er;
                entries[cur].fixed_ff = v; entries[cur].seen_mask |= RE4_SCENARIO_ENTRY_SEEN_FIXED_FF;
            } else if (re4m_starts_nocase(key, "Unused")) {
                re4_strref which_ref;
                re4_u32 which;
                which_ref.ptr = key.ptr + 6UL;
                which_ref.len = key.len - 6UL;
                er = re4m_parse_u32_dec(which_ref, &which);
                if (er == RE4_OK && which >= 1U && which <= 7U) {
                    er = re4m_parse_u32_hex(value, &v); if (er != RE4_OK) return er;
                    entries[cur].unused[which - 1U] = v;
                    entries[cur].seen_mask |= (RE4_SCENARIO_ENTRY_SEEN_UNUSED_BASE << (which - 1U));
                }
            }
        }
    }
    out->entry_count = count;
    out->entries = entries;
    return RE4_OK;
}

re4_bool re4_scenario_idx_entry_has_status(const re4_scenario_idx_entry *entry, re4_u32 flag)
{
    if (entry == 0) return RE4_FALSE;
    return (entry->object_status & flag) != 0U ? RE4_TRUE : RE4_FALSE;
}

re4_bool re4_idxmaterial_entry_is_transparent(const re4_idxmaterial_entry *entry)
{
    if (entry == 0) return RE4_FALSE;
    if ((entry->core.material_flag & RE4_BIN_MATERIAL_F_OPACITY_MAP) != 0U) return RE4_TRUE;
    if (entry->core.opacity_map != 255U) return RE4_TRUE;
    return RE4_FALSE;
}

re4_err re4_idxmaterial_validate_order(const re4_idxmaterial *idx, re4_idxmaterial_order_report *out)
{
    re4_u32 i;
    re4_bool seen_transparent;
    if (idx == 0 || out == 0) return RE4_ERR_NULL;
    memset(out, 0, sizeof(*out));
    seen_transparent = RE4_FALSE;
    for (i = 0U; i < idx->entry_count; ++i) {
        re4_bool transparent;
        transparent = re4_idxmaterial_entry_is_transparent(&idx->entries[i]);
        if (transparent) {
            seen_transparent = RE4_TRUE;
            ++out->transparent_count;
        } else if (seen_transparent && (out->flags & RE4_IDXMATERIAL_ORDER_OPAQUE_AFTER_A) == 0U) {
            out->flags |= RE4_IDXMATERIAL_ORDER_OPAQUE_AFTER_A;
            out->first_bad_index = i;
        }
        if (i != 0U) {
            if (re4m_cmp_strref_nocase_order(idx->entries[i - 1U].name, idx->entries[i].name) > 0) {
                if ((out->flags & RE4_IDXMATERIAL_ORDER_ALPHA_BREAK) == 0U) out->first_bad_index = i;
                out->flags |= RE4_IDXMATERIAL_ORDER_ALPHA_BREAK;
            }
        }
    }
    return RE4_OK;
}

static re4_bool re4m_idxmaterial_less(const re4_idxmaterial *idx, re4_u32 a, re4_u32 b)
{
    re4_bool ta;
    re4_bool tb;
    int c;
    ta = re4_idxmaterial_entry_is_transparent(&idx->entries[a]);
    tb = re4_idxmaterial_entry_is_transparent(&idx->entries[b]);
    if (ta != tb) return ta ? RE4_FALSE : RE4_TRUE;
    c = re4m_cmp_strref_nocase_order(idx->entries[a].name, idx->entries[b].name);
    if (c < 0) return RE4_TRUE;
    if (c > 0) return RE4_FALSE;
    return a < b ? RE4_TRUE : RE4_FALSE;
}

re4_err re4_idxmaterial_make_order_plan(const re4_idxmaterial *idx, re4_arena *arena, re4_u32 **out_indices, re4_u32 *out_count)
{
    re4_u32 i;
    re4_u32 *order;
    if (idx == 0 || arena == 0 || out_indices == 0 || out_count == 0) return RE4_ERR_NULL;
    *out_indices = 0;
    *out_count = 0U;
    order = (re4_u32 *)re4_arena_push(arena, (re4_size)idx->entry_count * sizeof(re4_u32), sizeof(re4_u32));
    if (idx->entry_count != 0U && order == 0) return RE4_ERR_NO_SPACE;
    for (i = 0U; i < idx->entry_count; ++i) order[i] = i;
    for (i = 0U; i < idx->entry_count; ++i) {
        re4_u32 best;
        re4_u32 j;
        best = i;
        for (j = i + 1U; j < idx->entry_count; ++j) {
            if (re4m_idxmaterial_less(idx, order[j], order[best])) best = j;
        }
        if (best != i) {
            re4_u32 t;
            t = order[i]; order[i] = order[best]; order[best] = t;
        }
    }
    *out_indices = order;
    *out_count = idx->entry_count;
    return RE4_OK;
}

static re4_bool re4m_value_truth(const re4_idxuubin_kv *kv)
{
    if ((kv->flags & RE4_IDXUUBIN_KV_HAS_BOOL) != 0U) return (kv->flags & RE4_IDXUUBIN_KV_BOOL_TRUE) != 0U ? RE4_TRUE : RE4_FALSE;
    if ((kv->flags & RE4_IDXUUBIN_KV_HAS_U32) != 0U) return kv->value_u32 != 0U ? RE4_TRUE : RE4_FALSE;
    return RE4_FALSE;
}

re4_err re4_idxuubin_read_text(re4_span text, re4_arena *arena, re4_idxuubin *out)
{
    re4_size pos;
    re4_u32 count;
    re4_idxuubin_kv *kvs;
    re4_strref section;
    if (arena == 0 || out == 0) return RE4_ERR_NULL;
    memset(out, 0, sizeof(*out));
    pos = 0UL;
    count = 0U;
    while (pos < text.size) {
        re4_strref line;
        re4_strref key;
        re4_strref value;
        line = re4m_next_line(text, &pos);
        line = re4m_trim(line);
        if (line.len == 0UL || line.ptr[0] == '#' || line.ptr[0] == '/' || line.ptr[0] == '\\') continue;
        if (re4m_split_key_value(line, &key, &value) == RE4_OK) ++count;
    }
    kvs = (re4_idxuubin_kv *)re4_arena_push(arena, (re4_size)count * sizeof(re4_idxuubin_kv), sizeof(re4_u32));
    if (count != 0U && kvs == 0) return RE4_ERR_NO_SPACE;
    section.ptr = 0;
    section.len = 0UL;
    pos = 0UL;
    count = 0U;
    while (pos < text.size) {
        re4_strref line;
        re4_strref key;
        re4_strref value;
        re4_u32 line_no;
        line_no = 1U;
        {
            re4_size scan;
            scan = 0UL;
            while (scan < pos) { if (text.data[scan] == (re4_u8)'\n') ++line_no; ++scan; }
        }
        line = re4m_next_line(text, &pos);
        line = re4m_trim(line);
        if (line.len == 0UL || line.ptr[0] == '#' || line.ptr[0] == '/' || line.ptr[0] == '\\') continue;
        if (re4m_split_key_value(line, &key, &value) == RE4_OK) {
            re4_idxuubin_kv *kv;
            re4_u32 v;
            re4_q16 q;
            re4_bool b;
            re4_err er;
            kv = &kvs[count];
            memset(kv, 0, sizeof(*kv));
            kv->section = section;
            kv->key = key;
            kv->value = value;
            kv->line_no = line_no;
            er = re4m_parse_u32_dec(value, &v);
            if (er == RE4_OK) { kv->value_u32 = v; kv->flags |= RE4_IDXUUBIN_KV_HAS_U32; }
            er = re4m_parse_q16_text(value, &q);
            if (er == RE4_OK) { kv->value_q16 = q; kv->flags |= RE4_IDXUUBIN_KV_HAS_Q16; }
            er = re4m_parse_bool(value, &b);
            if (er == RE4_OK) {
                kv->flags |= RE4_IDXUUBIN_KV_HAS_BOOL;
                if (b) kv->flags |= RE4_IDXUUBIN_KV_BOOL_TRUE;
            }
            if (re4m_eq_nocase(key, "UseVertexColor") || re4m_eq_nocase(key, "EnableVertexColor")) {
                out->use_vertex_color = re4m_value_truth(kv);
                out->seen_mask |= RE4_IDXUUBIN_SEEN_USE_VERTEX_COLOR;
            } else if (re4m_eq_nocase(key, "MaterialCount") || re4m_eq_nocase(key, "Materials")) {
                if ((kv->flags & RE4_IDXUUBIN_KV_HAS_U32) != 0U) { out->material_count = kv->value_u32; out->seen_mask |= RE4_IDXUUBIN_SEEN_MATERIAL_COUNT; }
            } else if (re4m_eq_nocase(key, "BoneCount") || re4m_eq_nocase(key, "Bones")) {
                if ((kv->flags & RE4_IDXUUBIN_KV_HAS_U32) != 0U) { out->bone_count = kv->value_u32; out->seen_mask |= RE4_IDXUUBIN_SEEN_BONE_COUNT; }
            } else if (re4m_eq_nocase(key, "WeightMapCount") || re4m_eq_nocase(key, "WeightmapCount") || re4m_eq_nocase(key, "WeightMaps")) {
                if ((kv->flags & RE4_IDXUUBIN_KV_HAS_U32) != 0U) { out->weightmap_count = kv->value_u32; out->seen_mask |= RE4_IDXUUBIN_SEEN_WEIGHTMAP_COUNT; }
            } else if (re4m_eq_nocase(key, "TexCount") || re4m_eq_nocase(key, "TextureCount") || re4m_eq_nocase(key, "Textures")) {
                if ((kv->flags & RE4_IDXUUBIN_KV_HAS_U32) != 0U) { out->texture_count = kv->value_u32; out->seen_mask |= RE4_IDXUUBIN_SEEN_TEX_COUNT; }
            } else if (re4m_eq_nocase(key, "MorphCount") || re4m_eq_nocase(key, "Morphs")) {
                if ((kv->flags & RE4_IDXUUBIN_KV_HAS_U32) != 0U) { out->morph_count = kv->value_u32; out->seen_mask |= RE4_IDXUUBIN_SEEN_MORPH; }
            }
            ++count;
        } else {
            section = line;
        }
    }
    out->kv_count = count;
    out->kvs = kvs;
    return RE4_OK;
}

re4_err re4_idxuubin_find_first(const re4_idxuubin *doc, const char *key, re4_idxuubin_kv *out)
{
    re4_u32 i;
    if (doc == 0 || key == 0 || out == 0) return RE4_ERR_NULL;
    for (i = 0U; i < doc->kv_count; ++i) {
        if (re4m_eq_nocase(doc->kvs[i].key, key)) {
            *out = doc->kvs[i];
            return RE4_OK;
        }
    }
    return RE4_ERR_BAD_FORMAT;
}

re4_err re4_mtl_ps2_texture_id_from_path(re4_strref path, re4_u32 *out_texture_id)
{
    re4_size end;
    re4_size start;
    re4_strref stem;
    if (out_texture_id == 0) return RE4_ERR_NULL;
    path = re4m_trim(path);
    if (path.len == 0UL) return RE4_ERR_BAD_FORMAT;
    end = path.len;
    while (end != 0UL && path.ptr[end - 1UL] != '/' && path.ptr[end - 1UL] != '\\') {
        if (path.ptr[end - 1UL] == '.') break;
        --end;
    }
    if (end == 0UL || path.ptr[end - 1UL] == '/' || path.ptr[end - 1UL] == '\\') end = path.len;
    start = end;
    while (start != 0UL && path.ptr[start - 1UL] != '/' && path.ptr[start - 1UL] != '\\') --start;
    while (end > start && path.ptr[end - 1UL] == '.') --end;
    stem.ptr = path.ptr + start;
    stem.len = end - start;
    if (stem.len == 0UL) return RE4_ERR_BAD_FORMAT;
    return re4m_parse_u32_dec(stem, out_texture_id);
}

re4_u32 re4_scenario_alignment_for_prefix(re4_scenario_prefix prefix)
{
    if (prefix == RE4_SCENARIO_PREFIX_GG) return 32U;
    return 16U;
}

static re4_bool re4m_key_suffix_dec(re4_strref key, const char *prefix, re4_u32 *out_id)
{
    re4_size n;
    re4_strref tail;
    n = 0UL;
    while (prefix[n] != 0) ++n;
    if (!re4m_starts_nocase(key, prefix)) return RE4_FALSE;
    if (key.len <= n) return RE4_FALSE;
    tail.ptr = key.ptr + n;
    tail.len = key.len - n;
    if (re4m_parse_u32_dec(tail, out_id) != RE4_OK) return RE4_FALSE;
    return RE4_TRUE;
}

re4_err re4_scenario_r100_read_text(re4_span text, re4_arena *arena, re4_scenario_r100 *out)
{
    re4_size pos;
    re4_u32 ep_count;
    re4_u32 es_count;
    re4_u32 et_count;
    re4_u32 ep_i;
    re4_u32 es_i;
    re4_u32 et_i;
    if (arena == 0 || out == 0) return RE4_ERR_NULL;
    memset(out, 0, sizeof(*out));
    pos = 0UL;
    ep_count = es_count = et_count = 0U;
    while (pos < text.size) {
        re4_strref line;
        re4_strref key;
        re4_strref value;
        re4_u32 id;
        line = re4m_next_line(text, &pos);
        line = re4m_trim(line);
        if (line.len == 0UL || line.ptr[0] == '#') continue;
        if (re4m_split_key_value(line, &key, &value) != RE4_OK) continue;
        if (re4m_key_suffix_dec(key, "ExtraParameter_", &id)) ++ep_count;
        else if (re4m_key_suffix_dec(key, "ExtraSmdFileName_", &id) || re4m_key_suffix_dec(key, "ExtraFileName_", &id)) ++es_count;
        else if (re4m_key_suffix_dec(key, "ExtraTplFileName_", &id)) ++et_count;
    }
    out->extra_parameters = (re4_scenario_r100_extra_parameter *)re4_arena_push(arena, (re4_size)ep_count * sizeof(re4_scenario_r100_extra_parameter), sizeof(re4_u32));
    out->extra_smd_files = (re4_scenario_r100_extra_name *)re4_arena_push(arena, (re4_size)es_count * sizeof(re4_scenario_r100_extra_name), sizeof(re4_u32));
    out->extra_tpl_files = (re4_scenario_r100_extra_name *)re4_arena_push(arena, (re4_size)et_count * sizeof(re4_scenario_r100_extra_name), sizeof(re4_u32));
    if ((ep_count != 0U && out->extra_parameters == 0) || (es_count != 0U && out->extra_smd_files == 0) || (et_count != 0U && out->extra_tpl_files == 0)) return RE4_ERR_NO_SPACE;
    pos = 0UL;
    ep_i = es_i = et_i = 0U;
    while (pos < text.size) {
        re4_strref line;
        re4_strref key;
        re4_strref value;
        re4_u32 id;
        re4_u32 v;
        re4_err er;
        line = re4m_next_line(text, &pos);
        line = re4m_trim(line);
        if (line.len == 0UL || line.ptr[0] == '#') continue;
        if (re4m_split_key_value(line, &key, &value) != RE4_OK) continue;
        if (re4m_key_suffix_dec(key, "ExtraParameter_", &id)) {
            er = re4m_parse_u32_dec(value, &v); if (er != RE4_OK) return er;
            out->extra_parameters[ep_i].id = id;
            out->extra_parameters[ep_i].value = v;
            ++ep_i;
        } else if (re4m_key_suffix_dec(key, "ExtraSmdFileName_", &id) || re4m_key_suffix_dec(key, "ExtraFileName_", &id)) {
            out->extra_smd_files[es_i].id = id;
            out->extra_smd_files[es_i].name = value;
            ++es_i;
        } else if (re4m_key_suffix_dec(key, "ExtraTplFileName_", &id)) {
            out->extra_tpl_files[et_i].id = id;
            out->extra_tpl_files[et_i].name = value;
            ++et_i;
        }
    }
    out->extra_parameter_count = ep_i;
    out->extra_smd_count = es_i;
    out->extra_tpl_count = et_i;
    return RE4_OK;
}

re4_err re4_idxps2bin_read_text(re4_span text, re4_arena *arena, re4_idxps2bin *out)
{
    re4_size pos;
    re4_u32 count;
    re4_idxuubin_kv *kvs;
    re4_strref section;
    if (arena == 0 || out == 0) return RE4_ERR_NULL;
    memset(out, 0, sizeof(*out));
    pos = 0UL;
    count = 0U;
    while (pos < text.size) {
        re4_strref line;
        re4_strref key;
        re4_strref value;
        line = re4m_next_line(text, &pos);
        line = re4m_trim(line);
        if (line.len == 0UL || line.ptr[0] == '#' || line.ptr[0] == '/' || line.ptr[0] == '\\') continue;
        if (re4m_split_key_value(line, &key, &value) == RE4_OK) ++count;
    }
    kvs = (re4_idxuubin_kv *)re4_arena_push(arena, (re4_size)count * sizeof(re4_idxuubin_kv), sizeof(re4_u32));
    if (count != 0U && kvs == 0) return RE4_ERR_NO_SPACE;
    section.ptr = 0;
    section.len = 0UL;
    pos = 0UL;
    count = 0U;
    while (pos < text.size) {
        re4_strref line;
        re4_strref key;
        re4_strref value;
        line = re4m_next_line(text, &pos);
        line = re4m_trim(line);
        if (line.len == 0UL || line.ptr[0] == '#' || line.ptr[0] == '/' || line.ptr[0] == '\\') continue;
        if (re4m_split_key_value(line, &key, &value) == RE4_OK) {
            re4_idxuubin_kv *kv;
            re4_u32 v;
            re4_q16 q;
            re4_bool b;
            re4_err er;
            kv = &kvs[count];
            memset(kv, 0, sizeof(*kv));
            kv->section = section;
            kv->key = key;
            kv->value = value;
            kv->line_no = count + 1U;
            er = re4m_parse_u32_dec(value, &v);
            if (er == RE4_OK) { kv->value_u32 = v; kv->flags |= RE4_IDXUUBIN_KV_HAS_U32; }
            er = re4m_parse_q16_text(value, &q);
            if (er == RE4_OK) { kv->value_q16 = q; kv->flags |= RE4_IDXUUBIN_KV_HAS_Q16; }
            er = re4m_parse_bool(value, &b);
            if (er == RE4_OK) {
                kv->flags |= RE4_IDXUUBIN_KV_HAS_BOOL;
                if (b) kv->flags |= RE4_IDXUUBIN_KV_BOOL_TRUE;
            }
            if (re4m_eq_nocase(key, "UseIdxMaterial")) {
                out->use_idxmaterial = re4m_value_truth(kv);
                out->seen_mask |= RE4_IDXPS2BIN_SEEN_USE_IDXMATERIAL;
            } else if (re4m_eq_nocase(key, "AutoCalcBoundingBox")) {
                out->auto_calc_bounding_box = re4m_value_truth(kv);
                out->seen_mask |= RE4_IDXPS2BIN_SEEN_AUTOCALC_BBOX;
            } else if (re4m_eq_nocase(key, "MaterialCount") || re4m_eq_nocase(key, "Materials")) {
                if ((kv->flags & RE4_IDXUUBIN_KV_HAS_U32) != 0U) { out->material_count = kv->value_u32; out->seen_mask |= RE4_IDXPS2BIN_SEEN_MATERIAL_COUNT; }
            } else if (re4m_eq_nocase(key, "BoneCount") || re4m_eq_nocase(key, "Bones")) {
                if ((kv->flags & RE4_IDXUUBIN_KV_HAS_U32) != 0U) { out->bone_count = kv->value_u32; out->seen_mask |= RE4_IDXPS2BIN_SEEN_BONE_COUNT; }
            } else if (re4m_eq_nocase(key, "WeightMapCount") || re4m_eq_nocase(key, "WeightmapCount") || re4m_eq_nocase(key, "WeightMaps")) {
                if ((kv->flags & RE4_IDXUUBIN_KV_HAS_U32) != 0U) { out->weightmap_count = kv->value_u32; out->seen_mask |= RE4_IDXPS2BIN_SEEN_WEIGHTMAP_COUNT; }
            }
            ++count;
        } else {
            section = line;
        }
    }
    out->kv_count = count;
    out->kvs = kvs;
    return RE4_OK;
}

re4_err re4_idxps2bin_find_first(const re4_idxps2bin *doc, const char *key, re4_idxuubin_kv *out)
{
    re4_u32 i;
    if (doc == 0 || key == 0 || out == 0) return RE4_ERR_NULL;
    for (i = 0U; i < doc->kv_count; ++i) {
        if (re4m_eq_nocase(doc->kvs[i].key, key)) {
            *out = doc->kvs[i];
            return RE4_OK;
        }
    }
    return RE4_ERR_BAD_FORMAT;
}
