#include "re4c3d/re4_model.h"
#include "re4c3d/re4_base.h"
#include <string.h>
#include <stdio.h>

static void re4_bin_material_core_zero(re4_bin_material_core *out)
{
    memset(out, 0, sizeof(*out));
    out->bump_map = 255U;
    out->opacity_map = 255U;
    out->generic_specular_map = 255U;
    out->custom_specular_map = 255U;
}

re4_err re4_bin_material_core_read_uhd24(re4_span src, re4_bin_material_core *out)
{
    re4_size i;
    if (out == 0) return RE4_ERR_NULL;
    if (!re4_span_ok(src, 0UL, 24UL)) return RE4_ERR_RANGE;
    re4_bin_material_core_zero(out);
    out->variant = RE4_BIN_MATERIAL_CORE_UHD_24;
    for (i = 0UL; i < 11UL; ++i) out->unknown_min[i] = src.data[i];
    out->material_flag = src.data[11UL];
    out->diffuse_map = src.data[12UL];
    out->bump_map = src.data[13UL];
    out->opacity_map = src.data[14UL];
    out->generic_specular_map = src.data[15UL];
    out->specular_rgb[0] = src.data[16UL];
    out->specular_rgb[1] = src.data[17UL];
    out->specular_rgb[2] = src.data[18UL];
    out->unknown_08 = src.data[19UL];
    out->unknown_09 = src.data[20UL];
    out->specular_scale = src.data[21UL];
    out->unknown_11 = src.data[22UL];
    out->custom_specular_map = src.data[23UL];
    return RE4_OK;
}

re4_err re4_bin_material_core_read_ps2_12(re4_span src, re4_bin_material_core *out)
{
    if (out == 0) return RE4_ERR_NULL;
    if (!re4_span_ok(src, 0UL, 12UL)) return RE4_ERR_RANGE;
    re4_bin_material_core_zero(out);
    out->variant = RE4_BIN_MATERIAL_CORE_PS2_12;
    out->material_flag = src.data[0UL];
    out->diffuse_map = src.data[1UL];
    out->bump_map = src.data[2UL];
    out->opacity_map = src.data[3UL];
    out->generic_specular_map = src.data[4UL];
    out->specular_rgb[0] = src.data[5UL];
    out->specular_rgb[1] = src.data[6UL];
    out->specular_rgb[2] = src.data[7UL];
    out->unknown_08 = src.data[8UL];
    out->unknown_09 = src.data[9UL];
    out->specular_scale = src.data[10UL];
    out->custom_specular_map = src.data[11UL];
    return RE4_OK;
}

re4_bool re4_bin_material_core_has(const re4_bin_material_core *m, re4_u8 flag)
{
    if (m == 0) return RE4_FALSE;
    return (m->material_flag & flag) == flag ? RE4_TRUE : RE4_FALSE;
}

const char *re4_bin_material_flag_label(re4_u8 flag)
{
    switch (flag) {
    case RE4_BIN_MATERIAL_F_BUMP_MAP: return "bump_map";
    case RE4_BIN_MATERIAL_F_GENERIC_SPECULAR: return "generic_specular_map";
    case RE4_BIN_MATERIAL_F_OPACITY_MAP: return "opacity_map";
    case RE4_BIN_MATERIAL_F_CUSTOM_SPECULAR: return "custom_specular_map";
    case RE4_BIN_MATERIAL_F_NORMALS_EFFECT: return "normals_effect";
    default: return "unknown";
    }
}

static re4_err re4_emit_cstr(re4_emit_fn emit, void *user, const char *text)
{
    re4_size n;
    if (emit == 0 || text == 0) return RE4_ERR_NULL;
    n = 0UL;
    while (text[n] != 0) ++n;
    return emit(user, text, n);
}

re4_err re4_bin_material_core_emit_idxmaterial(const re4_bin_material_core *m, re4_u32 index,
                                               re4_emit_fn emit, void *emit_user)
{
    char buf[512];
    if (m == 0 || emit == 0) return RE4_ERR_NULL;
    sprintf(buf,
            "UseMaterial:UHD_MATERIAL_%03u\nmaterial_flag:%02X\ndiffuse_map:%u\nbump_map:%u\nopacity_map:%u\ngeneric_specular_map:%u\nintensity_specular_r:%u\nintensity_specular_g:%u\nintensity_specular_b:%u\nunk_08:%u\nunk_09:%u\nspecular_scale:%02X\nunk_11:%u\ncustom_specular_map:%u\n\n",
            (unsigned)index,
            (unsigned)m->material_flag,
            (unsigned)m->diffuse_map,
            (unsigned)m->bump_map,
            (unsigned)m->opacity_map,
            (unsigned)m->generic_specular_map,
            (unsigned)m->specular_rgb[0],
            (unsigned)m->specular_rgb[1],
            (unsigned)m->specular_rgb[2],
            (unsigned)m->unknown_08,
            (unsigned)m->unknown_09,
            (unsigned)m->specular_scale,
            (unsigned)m->unknown_11,
            (unsigned)m->custom_specular_map);
    return re4_emit_cstr(emit, emit_user, buf);
}
