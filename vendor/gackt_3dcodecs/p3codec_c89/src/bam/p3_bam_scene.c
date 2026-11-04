#include <string.h>
#include "p3_bam_scene.h"
#include "p3_bam_datagram.h"
#include "p3_bam_special.h"
#include "p3_bytes.h"
#include "p3_error.h"
#include "p3_ieee.h"

typedef struct p3_scene_cursor {
    p3_bam_cursor cur;
    int long_object_id;
} p3_scene_cursor;

static void p3_scene_copy_text(char *dst, p3_u16 cap, const char *src) {
    p3_u16 i;
    if (dst == 0 || cap == 0U) {
        return;
    }
    if (src == 0) {
        dst[0] = 0;
        return;
    }
    i = 0U;
    while (src[i] != 0 && (p3_u32)i + 1UL < (p3_u32)cap) {
        dst[i] = src[i];
        i = (p3_u16)(i + 1U);
    }
    dst[i] = 0;
}

static int p3_scene_get_type_name(const p3_bam_archive *archive, p3_u16 type_id, char *dst, p3_u16 cap) {
    int idx;
    if (dst != 0 && cap > 0U) {
        dst[0] = 0;
    }
    idx = p3_bam_find_type_index(archive, type_id);
    if (idx < 0) {
        return P3_ERR_RANGE;
    }
    if (p3_string_pool_copy_out(&archive->pool,
        archive->types[idx].name_off,
        archive->types[idx].name_len,
        dst,
        cap) != P3_OK) {
        return P3_ERR_CAPACITY;
    }
    return P3_OK;
}

static void p3_scene_cursor_init(p3_scene_cursor *pc, const p3_bam_archive *archive, const p3_bam_record *rec) {
    p3_bam_cursor_init(&pc->cur, archive->raw + rec->payload_off, rec->payload_len);
    pc->long_object_id = rec->payload_long_object_id ? 1 : 0;
}

static int p3_scene_get_s32(p3_scene_cursor *pc, p3_s32 *out_value) {
    p3_u32 tmp;
    int rc;
    rc = p3_bam_get_u32(&pc->cur, &tmp);
    if (rc != P3_OK) {
        return rc;
    }
    *out_value = (p3_s32)tmp;
    return P3_OK;
}

static int p3_scene_get_pointer(p3_scene_cursor *pc, p3_u32 *out_id) {
    int rc;
    p3_u16 tmp16;
    if (pc->long_object_id) {
        return p3_bam_get_u32(&pc->cur, out_id);
    }
    rc = p3_bam_get_u16(&pc->cur, &tmp16);
    if (rc != P3_OK) {
        return rc;
    }
    *out_id = (p3_u32)tmp16;
    if (*out_id == 0xffffUL) {
        pc->long_object_id = 1;
    }
    return P3_OK;
}

static int p3_scene_put_s32(p3_bam_builder *b, p3_s32 value) {
    return p3_bam_put_u32(b, (p3_u32)value);
}

static int p3_scene_put_pointer(p3_bam_builder *b, int *io_long_object_id, p3_u32 object_id) {
    int rc;
    if (*io_long_object_id) {
        return p3_bam_put_u32(b, object_id);
    }
    if (object_id > 0xffffUL) {
        return P3_ERR_RANGE;
    }
    rc = p3_bam_put_u16(b, (p3_u16)object_id);
    if (rc != P3_OK) {
        return rc;
    }
    if (object_id == 0xffffUL) {
        *io_long_object_id = 1;
    }
    return P3_OK;
}

static int p3_scene_get_fixed(const p3_bam_archive *archive, p3_scene_cursor *pc, p3_fixed *out_value) {
    p3_u32 bits;
    p3_u32 hi_bits;
    int rc;
    if (archive == 0 || pc == 0 || out_value == 0) {
        return P3_ERR_INVALID;
    }
    if (archive->stdfloat_double != 0U) {
        rc = p3_bam_get_u32(&pc->cur, &bits);
        if (rc == P3_OK) {
            rc = p3_bam_get_u32(&pc->cur, &hi_bits);
        }
        if (rc != P3_OK) {
            return rc;
        }
        *out_value = p3_fixed_from_f64_bits(bits, hi_bits);
        return P3_OK;
    }
    rc = p3_bam_get_u32(&pc->cur, &bits);
    if (rc != P3_OK) {
        return rc;
    }
    *out_value = p3_fixed_from_f32_bits(bits);
    return P3_OK;
}

static int p3_scene_put_fixed(p3_bam_builder *b, p3_fixed value) {
    return p3_bam_put_u32(b, p3_f32_bits_from_fixed(value));
}

static int p3_scene_get_color4(const p3_bam_archive *archive, p3_scene_cursor *pc, p3_fixed *dst4) {
    int rc;
    p3_u16 i;
    for (i = 0U; i < 4U; ++i) {
        rc = p3_scene_get_fixed(archive, pc, &dst4[i]);
        if (rc != P3_OK) {
            return rc;
        }
    }
    return P3_OK;
}

static int p3_scene_put_color4(p3_bam_builder *b, const p3_fixed *src4) {
    int rc;
    p3_u16 i;
    for (i = 0U; i < 4U; ++i) {
        rc = p3_scene_put_fixed(b, src4[i]);
        if (rc != P3_OK) {
            return rc;
        }
    }
    return P3_OK;
}

static void p3_scene_sampler_init(p3_bam_sampler_state *sampler) {
    p3_mem_set(sampler, 0, (p3_u32)sizeof(*sampler));
    sampler->max_lod = p3_fixed_from_int(1000);
}

static int p3_scene_get_sampler(const p3_bam_archive *archive, p3_scene_cursor *pc, p3_bam_sampler_state *sampler) {
    int rc;
    p3_scene_sampler_init(sampler);
    rc = p3_bam_get_u8(&pc->cur, &sampler->wrap_u);
    if (rc == P3_OK) {
        rc = p3_bam_get_u8(&pc->cur, &sampler->wrap_v);
    }
    if (rc == P3_OK) {
        rc = p3_bam_get_u8(&pc->cur, &sampler->wrap_w);
    }
    if (rc == P3_OK) {
        rc = p3_bam_get_u8(&pc->cur, &sampler->minfilter);
    }
    if (rc == P3_OK) {
        rc = p3_bam_get_u8(&pc->cur, &sampler->magfilter);
    }
    if (rc == P3_OK) {
        rc = p3_bam_get_u16(&pc->cur, &sampler->anisotropic_degree);
    }
    if (rc == P3_OK) {
        rc = p3_scene_get_color4(archive, pc, sampler->border_color);
    }
    if (rc == P3_OK && archive != 0 && archive->minor >= 36U) {
        rc = p3_scene_get_fixed(archive, pc, &sampler->min_lod);
        if (rc == P3_OK) {
            rc = p3_scene_get_fixed(archive, pc, &sampler->max_lod);
        }
        if (rc == P3_OK) {
            rc = p3_scene_get_fixed(archive, pc, &sampler->lod_bias);
        }
    } else if (rc == P3_OK) {
        sampler->min_lod = (p3_fixed)(-1000L * 65536L);
        sampler->max_lod = (p3_fixed)(1000L * 65536L);
        sampler->lod_bias = 0;
    }
    return rc;
}

static int p3_scene_put_sampler_compat(p3_bam_builder *b, const p3_bam_sampler_state *sampler, p3_u8 write_lod) {
    int rc;
    rc = p3_bam_put_u8(b, sampler->wrap_u);
    if (rc == P3_OK) {
        rc = p3_bam_put_u8(b, sampler->wrap_v);
    }
    if (rc == P3_OK) {
        rc = p3_bam_put_u8(b, sampler->wrap_w);
    }
    if (rc == P3_OK) {
        rc = p3_bam_put_u8(b, sampler->minfilter);
    }
    if (rc == P3_OK) {
        rc = p3_bam_put_u8(b, sampler->magfilter);
    }
    if (rc == P3_OK) {
        rc = p3_bam_put_u16(b, sampler->anisotropic_degree);
    }
    if (rc == P3_OK) {
        rc = p3_scene_put_color4(b, sampler->border_color);
    }
    if (rc == P3_OK && write_lod != 0U) {
        rc = p3_scene_put_fixed(b, sampler->min_lod);
        if (rc == P3_OK) {
            rc = p3_scene_put_fixed(b, sampler->max_lod);
        }
        if (rc == P3_OK) {
            rc = p3_scene_put_fixed(b, sampler->lod_bias);
        }
    }
    return rc;
}

static int p3_scene_put_sampler(p3_bam_builder *b, const p3_bam_sampler_state *sampler) {
    return p3_scene_put_sampler_compat(b, sampler, 1U);
}

static void p3_scene_material_zero(p3_bam_material *mat) {
    p3_mem_set(mat, 0, (p3_u32)sizeof(*mat));
    mat->refractive_index = P3_FIXED_ONE;
}

static void p3_scene_texture_stage_defaults(p3_bam_texture_stage *stage) {
    if (stage == 0) {
        return;
    }
    p3_mem_set(stage, 0, (p3_u32)sizeof(*stage));
    stage->mode = P3_TEXTURE_STAGE_M_MODULATE;
    stage->color[3] = P3_FIXED_ONE;
    stage->rgb_scale = 1U;
    stage->alpha_scale = 1U;
    stage->combine_rgb_mode = P3_TEXTURE_STAGE_CM_UNDEFINED;
    stage->combine_alpha_mode = P3_TEXTURE_STAGE_CM_UNDEFINED;
}

static p3_u8 p3_scene_texture_stage_uses_source(const p3_u8 *src, p3_u8 source) {
    if (src == 0) {
        return 0U;
    }
    return (src[0] == source || src[1] == source || src[2] == source) ? 1U : 0U;
}

static void p3_scene_texture_stage_update_flags(p3_bam_texture_stage *stage) {
    p3_u8 combine;
    if (stage == 0) {
        return;
    }
    combine = (stage->mode == P3_TEXTURE_STAGE_M_COMBINE) ? 1U : 0U;
    stage->involves_color_scale = 0U;
    stage->uses_color = 0U;
    stage->uses_primary_color = 0U;
    stage->uses_last_saved_result = 0U;
    if (stage->mode == P3_TEXTURE_STAGE_M_BLEND_COLOR_SCALE) {
        stage->involves_color_scale = 1U;
    }
    if (stage->mode == P3_TEXTURE_STAGE_M_BLEND || stage->mode == P3_TEXTURE_STAGE_M_BLEND_COLOR_SCALE) {
        stage->uses_color = 1U;
    }
    if (combine != 0U) {
        if (p3_scene_texture_stage_uses_source(stage->combine_rgb_source, P3_TEXTURE_STAGE_CS_CONSTANT_COLOR_SCALE) != 0U ||
            p3_scene_texture_stage_uses_source(stage->combine_alpha_source, P3_TEXTURE_STAGE_CS_CONSTANT_COLOR_SCALE) != 0U) {
            stage->involves_color_scale = 1U;
        }
        if (p3_scene_texture_stage_uses_source(stage->combine_rgb_source, P3_TEXTURE_STAGE_CS_CONSTANT) != 0U ||
            p3_scene_texture_stage_uses_source(stage->combine_alpha_source, P3_TEXTURE_STAGE_CS_CONSTANT) != 0U) {
            stage->uses_color = 1U;
        }
        if (p3_scene_texture_stage_uses_source(stage->combine_rgb_source, P3_TEXTURE_STAGE_CS_PRIMARY_COLOR) != 0U ||
            p3_scene_texture_stage_uses_source(stage->combine_alpha_source, P3_TEXTURE_STAGE_CS_PRIMARY_COLOR) != 0U) {
            stage->uses_primary_color = 1U;
        }
        if (p3_scene_texture_stage_uses_source(stage->combine_rgb_source, P3_TEXTURE_STAGE_CS_LAST_SAVED_RESULT) != 0U ||
            p3_scene_texture_stage_uses_source(stage->combine_alpha_source, P3_TEXTURE_STAGE_CS_LAST_SAVED_RESULT) != 0U) {
            stage->uses_last_saved_result = 1U;
        }
    }
}

static int p3_scene_parse_render_state(const p3_bam_archive *archive, p3_scene_cursor *pc, p3_bam_render_state *state) {
    int rc;
    p3_u16 total;
    p3_u16 i;
    (void)archive;
    p3_mem_set(state, 0, (p3_u32)sizeof(*state));
    rc = p3_bam_get_u16(&pc->cur, &total);
    if (rc != P3_OK) {
        return rc;
    }
    state->total_attrib_count = total;
    for (i = 0U; i < total; ++i) {
        p3_u32 ptr;
        p3_s32 ov;
        rc = p3_scene_get_pointer(pc, &ptr);
        if (rc == P3_OK) {
            rc = p3_scene_get_s32(pc, &ov);
        }
        if (rc != P3_OK) {
            return rc;
        }
        if (state->attrib_count < P3_BAM_SCENE_MAX_ATTRIBS) {
            state->attribs[state->attrib_count].attrib_object_id = ptr;
            state->attribs[state->attrib_count].override_value = ov;
            state->attrib_count = (p3_u16)(state->attrib_count + 1U);
        }
    }
    return P3_OK;
}

static int p3_scene_parse_transform_state(const p3_bam_archive *archive, p3_scene_cursor *pc, p3_bam_transform_state *tr) {
    int rc;
    p3_u16 i;
    p3_mem_set(tr, 0, (p3_u32)sizeof(*tr));
    rc = p3_bam_get_u32(&pc->cur, &tr->flags);
    if (rc != P3_OK) {
        return rc;
    }
    if ((tr->flags & P3_TRANSFORM_F_IS_IDENTITY) != 0UL || (tr->flags & P3_TRANSFORM_F_IS_INVALID) != 0UL) {
        tr->representation = 0U;
        return P3_OK;
    }
    if ((tr->flags & P3_TRANSFORM_F_COMPONENTS_GIVEN) != 0UL || (tr->flags & P3_TRANSFORM_F_HAS_COMPONENTS) != 0UL) {
        tr->representation = 1U;
        for (i = 0U; i < 3U; ++i) {
            rc = p3_scene_get_fixed(archive, pc, &tr->pos[i]);
            if (rc != P3_OK) {
                return rc;
            }
        }
        if ((tr->flags & P3_TRANSFORM_F_QUAT_GIVEN) != 0UL || (tr->flags & P3_TRANSFORM_F_QUAT_KNOWN) != 0UL) {
            for (i = 0U; i < 4U; ++i) {
                rc = p3_scene_get_fixed(archive, pc, &tr->quat[i]);
                if (rc != P3_OK) {
                    return rc;
                }
            }
        } else {
            for (i = 0U; i < 3U; ++i) {
                rc = p3_scene_get_fixed(archive, pc, &tr->hpr[i]);
                if (rc != P3_OK) {
                    return rc;
                }
            }
        }
        for (i = 0U; i < 3U; ++i) {
            rc = p3_scene_get_fixed(archive, pc, &tr->scale[i]);
            if (rc != P3_OK) {
                return rc;
            }
        }
        for (i = 0U; i < 3U; ++i) {
            rc = p3_scene_get_fixed(archive, pc, &tr->shear[i]);
            if (rc != P3_OK) {
                return rc;
            }
        }
        return P3_OK;
    }
    tr->representation = 2U;
    for (i = 0U; i < 16U; ++i) {
        rc = p3_scene_get_fixed(archive, pc, &tr->mat[i]);
        if (rc != P3_OK) {
            return rc;
        }
    }
    return P3_OK;
}

static int p3_scene_parse_texture_stage(const p3_bam_archive *archive, p3_scene_cursor *pc, p3_bam_texture_stage *stage) {
    int rc;
    p3_u8 default_flag;
    p3_u16 name_len;
    p3_u32 remaining;
    p3_scene_texture_stage_defaults(stage);
    rc = p3_bam_get_u8(&pc->cur, &default_flag);
    if (rc != P3_OK) {
        return rc;
    }
    stage->is_default_stage = default_flag;
    if (default_flag != 0U) {
        p3_scene_copy_text(stage->name, (p3_u16)sizeof(stage->name), "default");
        return P3_OK;
    }
    rc = p3_bam_get_string(&pc->cur, stage->name, (p3_u16)sizeof(stage->name), &name_len);
    if (rc == P3_OK) {
        rc = p3_scene_get_s32(pc, &stage->sort);
    }
    if (rc == P3_OK) {
        rc = p3_scene_get_s32(pc, &stage->priority);
    }
    if (rc == P3_OK) {
        rc = p3_scene_get_pointer(pc, &stage->texcoord_name_object_id);
    }
    if (rc == P3_OK && stage->texcoord_name_object_id != 0UL) {
        p3_bam_resolve_internal_name(archive, stage->texcoord_name_object_id, stage->texcoord_name, (p3_u16)sizeof(stage->texcoord_name));
    }
    if (rc == P3_OK) {
        rc = p3_bam_get_u8(&pc->cur, &stage->mode);
    }
    if (rc == P3_OK) {
        rc = p3_scene_get_color4(archive, pc, stage->color);
    }
    if (rc == P3_OK) {
        rc = p3_bam_get_u8(&pc->cur, &stage->rgb_scale);
    }
    if (rc == P3_OK) {
        rc = p3_bam_get_u8(&pc->cur, &stage->alpha_scale);
    }
    if (rc == P3_OK) {
        rc = p3_bam_get_u8(&pc->cur, &stage->saved_result);
    }
    if (rc == P3_OK && archive->minor >= 26U) {
        rc = p3_scene_get_s32(pc, &stage->tex_view_offset);
    }
    if (rc == P3_OK) {
        remaining = p3_bam_cursor_remaining(&pc->cur);
        if (remaining >= 16UL) {
            rc = p3_bam_get_u8(&pc->cur, &stage->combine_rgb_mode);
            if (rc == P3_OK) {
                rc = p3_bam_get_u8(&pc->cur, &stage->num_combine_rgb_operands);
            }
            if (rc == P3_OK) {
                rc = p3_bam_get_u8(&pc->cur, &stage->combine_rgb_source[0]);
            }
            if (rc == P3_OK) {
                rc = p3_bam_get_u8(&pc->cur, &stage->combine_rgb_operand[0]);
            }
            if (rc == P3_OK) {
                rc = p3_bam_get_u8(&pc->cur, &stage->combine_rgb_source[1]);
            }
            if (rc == P3_OK) {
                rc = p3_bam_get_u8(&pc->cur, &stage->combine_rgb_operand[1]);
            }
            if (rc == P3_OK) {
                rc = p3_bam_get_u8(&pc->cur, &stage->combine_rgb_source[2]);
            }
            if (rc == P3_OK) {
                rc = p3_bam_get_u8(&pc->cur, &stage->combine_rgb_operand[2]);
            }
            if (rc == P3_OK) {
                rc = p3_bam_get_u8(&pc->cur, &stage->combine_alpha_mode);
            }
            if (rc == P3_OK) {
                rc = p3_bam_get_u8(&pc->cur, &stage->num_combine_alpha_operands);
            }
            if (rc == P3_OK) {
                rc = p3_bam_get_u8(&pc->cur, &stage->combine_alpha_source[0]);
            }
            if (rc == P3_OK) {
                rc = p3_bam_get_u8(&pc->cur, &stage->combine_alpha_operand[0]);
            }
            if (rc == P3_OK) {
                rc = p3_bam_get_u8(&pc->cur, &stage->combine_alpha_source[1]);
            }
            if (rc == P3_OK) {
                rc = p3_bam_get_u8(&pc->cur, &stage->combine_alpha_operand[1]);
            }
            if (rc == P3_OK) {
                rc = p3_bam_get_u8(&pc->cur, &stage->combine_alpha_source[2]);
            }
            if (rc == P3_OK) {
                rc = p3_bam_get_u8(&pc->cur, &stage->combine_alpha_operand[2]);
            }
        } else {
            if (remaining >= 1UL) {
                rc = p3_bam_get_u8(&pc->cur, &stage->combine_rgb_mode);
            }
            if (rc == P3_OK && p3_bam_cursor_remaining(&pc->cur) >= 1UL) {
                rc = p3_bam_get_u8(&pc->cur, &stage->combine_alpha_mode);
            }
        }
    }
    if (rc == P3_OK) {
        p3_scene_texture_stage_update_flags(stage);
    }
    return rc;
}

static int p3_scene_parse_material(const p3_bam_archive *archive, p3_scene_cursor *pc, p3_bam_material *mat) {
    int rc;
    p3_u16 i;
    p3_u16 dummy_len;
    (void)archive;
    p3_scene_material_zero(mat);
    rc = p3_bam_get_string(&pc->cur, mat->name, (p3_u16)sizeof(mat->name), &dummy_len);
    if (rc != P3_OK) {
        return rc;
    }
    if (archive->minor >= 39U) {
        rc = p3_bam_get_u32(&pc->cur, &mat->flags);
        if (rc != P3_OK) {
            return rc;
        }
        if ((mat->flags & P3_MATERIAL_F_METALLIC) != 0UL) {
            rc = p3_scene_get_color4(archive, pc, mat->base_color);
            if (rc == P3_OK) {
                rc = p3_scene_get_fixed(archive, pc, &mat->metallic);
            }
        } else {
            rc = p3_scene_get_color4(archive, pc, mat->ambient);
            if (rc == P3_OK) {
                rc = p3_scene_get_color4(archive, pc, mat->diffuse);
            }
            if (rc == P3_OK) {
                rc = p3_scene_get_color4(archive, pc, mat->specular);
            }
        }
        if (rc == P3_OK) {
            rc = p3_scene_get_color4(archive, pc, mat->emission);
        }
        if (rc == P3_OK) {
            if ((mat->flags & P3_MATERIAL_F_ROUGHNESS) != 0UL) {
                rc = p3_scene_get_fixed(archive, pc, &mat->roughness);
            } else {
                rc = p3_scene_get_fixed(archive, pc, &mat->shininess);
            }
        }
        if (rc == P3_OK) {
            rc = p3_scene_get_fixed(archive, pc, &mat->refractive_index);
        }
        return rc;
    }
    for (i = 0U; i < 4U; ++i) {
        rc = p3_scene_get_fixed(archive, pc, &mat->ambient[i]);
        if (rc != P3_OK) {
            return rc;
        }
    }
    for (i = 0U; i < 4U; ++i) {
        rc = p3_scene_get_fixed(archive, pc, &mat->diffuse[i]);
        if (rc != P3_OK) {
            return rc;
        }
    }
    for (i = 0U; i < 4U; ++i) {
        rc = p3_scene_get_fixed(archive, pc, &mat->specular[i]);
        if (rc != P3_OK) {
            return rc;
        }
    }
    for (i = 0U; i < 4U; ++i) {
        rc = p3_scene_get_fixed(archive, pc, &mat->emission[i]);
        if (rc != P3_OK) {
            return rc;
        }
    }
    rc = p3_scene_get_fixed(archive, pc, &mat->shininess);
    if (rc == P3_OK) {
        rc = p3_bam_get_u32(&pc->cur, &mat->flags);
    }
    return rc;
}

static int p3_scene_parse_material_attrib(const p3_bam_archive *archive, p3_scene_cursor *pc, p3_bam_material_attrib *matattr) {
    (void)archive;
    return p3_scene_get_pointer(pc, &matattr->material_object_id);
}


static void p3_scene_color_scale_update(p3_bam_color_scale_attrib *attr) {
    if (attr == 0) {
        return;
    }
    attr->has_scale = 0U;
    attr->has_rgb_scale = 0U;
    attr->has_alpha_scale = 0U;
    if (attr->scale[0] != P3_FIXED_ONE || attr->scale[1] != P3_FIXED_ONE || attr->scale[2] != P3_FIXED_ONE || attr->scale[3] != P3_FIXED_ONE) {
        attr->has_scale = 1U;
    }
    if (attr->scale[0] != P3_FIXED_ONE || attr->scale[1] != P3_FIXED_ONE || attr->scale[2] != P3_FIXED_ONE) {
        attr->has_rgb_scale = 1U;
    }
    if (attr->scale[3] != P3_FIXED_ONE) {
        attr->has_alpha_scale = 1U;
    }
}

static int p3_scene_parse_color_attrib(const p3_bam_archive *archive, p3_scene_cursor *pc, p3_bam_color_attrib *attr) {
    int rc;
    p3_mem_set(attr, 0, (p3_u32)sizeof(*attr));
    attr->color_type = P3_COLOR_ATTRIB_T_OFF;
    attr->color[3] = P3_FIXED_ONE;
    rc = p3_bam_get_u8(&pc->cur, &attr->color_type);
    if (rc == P3_OK) {
        rc = p3_scene_get_color4(archive, pc, attr->color);
    }
    return rc;
}

static int p3_scene_parse_color_scale_attrib(const p3_bam_archive *archive, p3_scene_cursor *pc, p3_bam_color_scale_attrib *attr) {
    int rc;
    p3_mem_set(attr, 0, (p3_u32)sizeof(*attr));
    attr->scale[0] = P3_FIXED_ONE;
    attr->scale[1] = P3_FIXED_ONE;
    attr->scale[2] = P3_FIXED_ONE;
    attr->scale[3] = P3_FIXED_ONE;
    rc = p3_bam_get_u8(&pc->cur, &attr->off);
    if (rc == P3_OK) {
        rc = p3_scene_get_color4(archive, pc, attr->scale);
    }
    if (rc == P3_OK) {
        p3_scene_color_scale_update(attr);
    }
    return rc;
}

static int p3_scene_parse_tex_matrix_attrib(const p3_bam_archive *archive, p3_scene_cursor *pc, p3_bam_tex_matrix_attrib *attr) {
    int rc;
    p3_u16 total;
    p3_u16 i;
    p3_bam_tex_matrix_attrib_stage stage;
    p3_mem_set(attr, 0, (p3_u32)sizeof(*attr));
    rc = p3_bam_get_u16(&pc->cur, &total);
    if (rc != P3_OK) {
        return rc;
    }
    attr->total_stage_count = total;
    for (i = 0U; i < total; ++i) {
        p3_mem_set(&stage, 0, (p3_u32)sizeof(stage));
        rc = p3_scene_get_pointer(pc, &stage.stage_object_id);
        if (rc == P3_OK) {
            rc = p3_scene_get_pointer(pc, &stage.transform_object_id);
        }
        if (rc == P3_OK && archive->minor >= 24U) {
            rc = p3_scene_get_s32(pc, &stage.override_value);
        }
        if (rc != P3_OK) {
            return rc;
        }
        if (attr->stage_count < P3_BAM_SCENE_MAX_TEX_MATRIX_STAGES) {
            attr->stages[attr->stage_count] = stage;
            attr->stage_count = (p3_u16)(attr->stage_count + 1U);
        }
    }
    return P3_OK;
}

static int p3_scene_parse_tex_gen_attrib(const p3_bam_archive *archive, p3_scene_cursor *pc, p3_bam_tex_gen_attrib *attr) {
    int rc;
    p3_u16 total;
    p3_u16 i;
    p3_bam_tex_gen_attrib_stage stage;
    (void)archive;
    p3_mem_set(attr, 0, (p3_u32)sizeof(*attr));
    rc = p3_bam_get_u16(&pc->cur, &total);
    if (rc != P3_OK) {
        return rc;
    }
    attr->total_stage_count = total;
    for (i = 0U; i < total; ++i) {
        p3_mem_set(&stage, 0, (p3_u32)sizeof(stage));
        rc = p3_scene_get_pointer(pc, &stage.stage_object_id);
        if (rc == P3_OK) {
            rc = p3_bam_get_u8(&pc->cur, &stage.mode);
        }
        if (rc != P3_OK) {
            return rc;
        }
        if (attr->stage_count < P3_BAM_SCENE_MAX_TEX_GEN_STAGES) {
            attr->stages[attr->stage_count] = stage;
            attr->stage_count = (p3_u16)(attr->stage_count + 1U);
        }
    }
    return P3_OK;
}

static int p3_scene_parse_texture_attrib(const p3_bam_archive *archive, p3_scene_cursor *pc, p3_bam_texture_attrib *ta) {
    int rc;
    p3_u16 total;
    p3_u16 i;
    p3_mem_set(ta, 0, (p3_u32)sizeof(*ta));
    rc = p3_bam_get_u8(&pc->cur, &ta->off_all_stages);
    if (rc != P3_OK) {
        return rc;
    }
    rc = p3_bam_get_u16(&pc->cur, &total);
    if (rc != P3_OK) {
        return rc;
    }
    ta->total_off_stage_count = total;
    for (i = 0U; i < total; ++i) {
        p3_u32 stage_id;
        rc = p3_scene_get_pointer(pc, &stage_id);
        if (rc != P3_OK) {
            return rc;
        }
        if (ta->off_stage_count < P3_BAM_SCENE_MAX_OFF_STAGES) {
            ta->off_stage_object_ids[ta->off_stage_count] = stage_id;
            ta->off_stage_count = (p3_u16)(ta->off_stage_count + 1U);
        }
    }
    rc = p3_bam_get_u16(&pc->cur, &total);
    if (rc != P3_OK) {
        return rc;
    }
    ta->total_on_stage_count = total;
    for (i = 0U; i < total; ++i) {
        p3_bam_texture_attrib_stage stage;
        p3_mem_set(&stage, 0, (p3_u32)sizeof(stage));
        rc = p3_scene_get_pointer(pc, &stage.stage_object_id);
        if (rc == P3_OK) {
            rc = p3_scene_get_pointer(pc, &stage.texture_object_id);
        }
        if (rc == P3_OK) {
            if (archive->minor >= 15U) {
                p3_u16 implicit_sort16;
                rc = p3_bam_get_u16(&pc->cur, &implicit_sort16);
                stage.implicit_sort = (p3_s32)implicit_sort16;
            } else {
                stage.implicit_sort = (p3_s32)i;
            }
        }
        if (rc == P3_OK && archive->minor >= 23U) {
            rc = p3_scene_get_s32(pc, &stage.override_value);
        }
        if (rc == P3_OK && archive->minor >= 36U) {
            rc = p3_bam_get_u8(&pc->cur, &stage.has_sampler);
            if (rc == P3_OK && stage.has_sampler != 0U) {
                rc = p3_scene_get_sampler(archive, pc, &stage.sampler);
            }
        }
        if (rc != P3_OK) {
            return rc;
        }
        if (ta->on_stage_count < P3_BAM_SCENE_MAX_ON_STAGES) {
            ta->on_stages[ta->on_stage_count] = stage;
            ta->on_stage_count = (p3_u16)(ta->on_stage_count + 1U);
        }
    }
    return P3_OK;
}

static int p3_scene_parse_transform_table(const p3_bam_archive *archive, p3_scene_cursor *pc, p3_bam_transform_table *tt) {
    int rc;
    p3_u16 total;
    p3_u16 i;
    (void)archive;
    p3_mem_set(tt, 0, (p3_u32)sizeof(*tt));
    rc = p3_bam_get_u16(&pc->cur, &total);
    if (rc != P3_OK) {
        return rc;
    }
    tt->total_transform_count = total;
    for (i = 0U; i < total; ++i) {
        p3_u32 id;
        rc = p3_scene_get_pointer(pc, &id);
        if (rc != P3_OK) {
            return rc;
        }
        if (tt->transform_count < P3_BAM_SCENE_MAX_TRANSFORMS) {
            tt->transform_object_ids[tt->transform_count] = id;
            tt->transform_count = (p3_u16)(tt->transform_count + 1U);
        }
    }
    return P3_OK;
}

static int p3_scene_parse_transform_blend(const p3_bam_archive *archive, p3_scene_cursor *pc, p3_bam_transform_blend *tb) {
    int rc;
    p3_u16 total;
    p3_u16 i;
    (void)archive;
    p3_mem_set(tb, 0, (p3_u32)sizeof(*tb));
    rc = p3_bam_get_u16(&pc->cur, &total);
    if (rc != P3_OK) {
        return rc;
    }
    tb->total_entry_count = total;
    for (i = 0U; i < total; ++i) {
        p3_bam_transform_blend_entry entry;
        p3_mem_set(&entry, 0, (p3_u32)sizeof(entry));
        rc = p3_scene_get_pointer(pc, &entry.transform_object_id);
        if (rc == P3_OK) {
            rc = p3_scene_get_fixed(archive, pc, &entry.weight);
        }
        if (rc != P3_OK) {
            return rc;
        }
        if (tb->entry_count < P3_BAM_SCENE_MAX_BLEND_ENTRIES) {
            tb->entries[tb->entry_count] = entry;
            tb->entry_count = (p3_u16)(tb->entry_count + 1U);
        }
    }
    return P3_OK;
}

static int p3_scene_parse_transform_blend_table(const p3_bam_archive *archive, p3_scene_cursor *pc, p3_bam_transform_blend_table *tbl) {
    int rc;
    p3_u16 total;
    p3_u16 i;
    p3_mem_set(tbl, 0, (p3_u32)sizeof(*tbl));
    rc = p3_bam_get_u16(&pc->cur, &total);
    if (rc != P3_OK) {
        return rc;
    }
    tbl->total_blend_count = total;
    for (i = 0U; i < total; ++i) {
        p3_bam_transform_blend blend;
        rc = p3_scene_parse_transform_blend(archive, pc, &blend);
        if (rc != P3_OK) {
            return rc;
        }
        if (tbl->blend_count < P3_BAM_SCENE_MAX_BLENDS) {
            tbl->blends[tbl->blend_count] = blend;
            tbl->blend_count = (p3_u16)(tbl->blend_count + 1U);
        }
    }
    rc = p3_bam_get_u16(&pc->cur, &tbl->rows);
    return rc;
}

static int p3_scene_parse_joint_vertex_transform(const p3_bam_archive *archive, p3_scene_cursor *pc, p3_bam_joint_vertex_transform *jvt) {
    (void)archive;
    return p3_scene_get_pointer(pc, &jvt->joint_object_id);
}

static int p3_scene_parse_character_joint(const p3_bam_archive *archive, p3_scene_cursor *pc, p3_bam_character_joint *joint) {
    int rc;
    p3_u16 total;
    p3_u16 i;
    p3_u16 dummy_len;
    (void)archive;
    p3_mem_set(joint, 0, (p3_u32)sizeof(*joint));
    rc = p3_bam_get_string(&pc->cur, joint->name, (p3_u16)sizeof(joint->name), &dummy_len);
    if (rc != P3_OK) {
        return rc;
    }
    rc = p3_bam_get_u16(&pc->cur, &total);
    if (rc != P3_OK) {
        return rc;
    }
    joint->total_child_count = total;
    for (i = 0U; i < total; ++i) {
        p3_u32 id;
        rc = p3_scene_get_pointer(pc, &id);
        if (rc != P3_OK) {
            return rc;
        }
        if (joint->child_count < P3_BAM_SPECIAL_MAX_CHILDREN) {
            joint->child_object_ids[joint->child_count] = id;
            joint->child_count = (p3_u16)(joint->child_count + 1U);
        }
    }
    rc = p3_scene_get_pointer(pc, &joint->forced_channel_object_id);
    if (rc == P3_OK) {
        rc = p3_scene_get_pointer(pc, &joint->character_object_id);
    }
    if (rc == P3_OK) {
        rc = p3_bam_get_u16(&pc->cur, &total);
    }
    if (rc != P3_OK) {
        return rc;
    }
    joint->total_net_node_count = total;
    for (i = 0U; i < total; ++i) {
        p3_u32 id2;
        rc = p3_scene_get_pointer(pc, &id2);
        if (rc != P3_OK) {
            return rc;
        }
        if (joint->net_node_count < P3_BAM_SPECIAL_MAX_CHILDREN) {
            joint->net_node_object_ids[joint->net_node_count] = id2;
            joint->net_node_count = (p3_u16)(joint->net_node_count + 1U);
        }
    }
    rc = p3_bam_get_u16(&pc->cur, &total);
    if (rc != P3_OK) {
        return rc;
    }
    joint->total_local_node_count = total;
    for (i = 0U; i < total; ++i) {
        p3_u32 id3;
        rc = p3_scene_get_pointer(pc, &id3);
        if (rc != P3_OK) {
            return rc;
        }
        if (joint->local_node_count < P3_BAM_SPECIAL_MAX_CHILDREN) {
            joint->local_node_object_ids[joint->local_node_count] = id3;
            joint->local_node_count = (p3_u16)(joint->local_node_count + 1U);
        }
    }
    for (i = 0U; i < 16U; ++i) {
        rc = p3_scene_get_fixed(archive, pc, &joint->initial_net_inverse[i]);
        if (rc != P3_OK) {
            return rc;
        }
    }
    return P3_OK;
}

static int p3_scene_parse_texture(const p3_bam_archive *archive, p3_scene_cursor *pc, p3_bam_texture *tx) {
    int rc;
    p3_u16 len16;
    p3_u32 tmp32;
    p3_mem_set(tx, 0, (p3_u32)sizeof(*tx));
    p3_scene_sampler_init(&tx->sampler);
    rc = p3_bam_get_string(&pc->cur, tx->name, (p3_u16)sizeof(tx->name), &len16);
    if (rc == P3_OK) {
        rc = p3_bam_get_string(&pc->cur, tx->filename, (p3_u16)sizeof(tx->filename), &len16);
    }
    if (rc == P3_OK) {
        rc = p3_bam_get_string(&pc->cur, tx->alpha_filename, (p3_u16)sizeof(tx->alpha_filename), &len16);
    }
    if (rc == P3_OK) {
        rc = p3_bam_get_u8(&pc->cur, &tx->primary_file_num_channels);
    }
    if (rc == P3_OK) {
        rc = p3_bam_get_u8(&pc->cur, &tx->alpha_file_channel);
    }
    if (rc == P3_OK) {
        rc = p3_bam_get_u8(&pc->cur, &tx->has_rawdata);
    }
    if (rc == P3_OK) {
        rc = p3_bam_get_u8(&pc->cur, &tx->texture_type);
    }
    if (rc == P3_OK && archive->minor >= 32U) {
        rc = p3_bam_get_u8(&pc->cur, &tx->has_read_mipmaps);
    }
    if (rc == P3_OK) {
        rc = p3_scene_get_sampler(archive, pc, &tx->sampler);
    }
    if (rc == P3_OK && archive->minor >= 1U) {
        rc = p3_bam_get_u8(&pc->cur, &tx->compression);
    }
    if (rc == P3_OK && archive->minor >= 16U) {
        rc = p3_bam_get_u8(&pc->cur, &tx->quality_level);
    }
    if (rc == P3_OK) {
        rc = p3_bam_get_u8(&pc->cur, &tx->format);
    }
    if (rc == P3_OK) {
        rc = p3_bam_get_u8(&pc->cur, &tx->num_components);
    }
    if (rc == P3_OK && tx->texture_type == P3_TEXTURE_TT_BUFFER_TEXTURE) {
        rc = p3_bam_get_u8(&pc->cur, &tx->usage_hint);
    }
    if (rc == P3_OK && archive->minor >= 28U) {
        rc = p3_bam_get_u8(&pc->cur, &tx->auto_texture_scale);
    }
    if (rc == P3_OK && archive->minor >= 18U) {
        rc = p3_bam_get_u32(&pc->cur, &tmp32);
        tx->orig_file_x = (tmp32 > 0xffffUL) ? 0xffffU : (p3_u16)tmp32;
    }
    if (rc == P3_OK && archive->minor >= 18U) {
        rc = p3_bam_get_u32(&pc->cur, &tmp32);
        tx->orig_file_y = (tmp32 > 0xffffUL) ? 0xffffU : (p3_u16)tmp32;
    }
    if (rc == P3_OK && archive->minor >= 18U) {
        p3_u8 has_simple_ram_image;
        rc = p3_bam_get_u8(&pc->cur, &has_simple_ram_image);
        if (rc == P3_OK && has_simple_ram_image != 0U) {
            p3_u32 ignored_simple_x;
            p3_u32 ignored_simple_y;
            p3_s32 ignored_date;
            p3_u32 to_copy;
            rc = p3_bam_get_u32(&pc->cur, &ignored_simple_x);
            if (rc == P3_OK) {
                rc = p3_bam_get_u32(&pc->cur, &ignored_simple_y);
            }
            if (rc == P3_OK) {
                rc = p3_scene_get_s32(pc, &ignored_date);
            }
            if (rc == P3_OK) {
                rc = p3_bam_get_u32(&pc->cur, &tx->ram_image_size);
            }
            if (rc == P3_OK) {
                to_copy = tx->ram_image_size;
                if (to_copy > (p3_u32)P3_BAM_SPECIAL_DATA_PREVIEW) {
                    to_copy = (p3_u32)P3_BAM_SPECIAL_DATA_PREVIEW;
                }
                tx->ram_image_preview_size = (p3_u8)to_copy;
                if (to_copy > 0UL) {
                    rc = p3_bam_get_bytes(&pc->cur, tx->ram_image_preview, to_copy);
                }
                if (rc == P3_OK && tx->ram_image_size > to_copy) {
                    pc->cur.pos += tx->ram_image_size - to_copy;
                    if (pc->cur.pos > pc->cur.size) {
                        rc = P3_ERR_TRUNCATED;
                    }
                }
            }
            (void)ignored_simple_x;
            (void)ignored_simple_y;
            (void)ignored_date;
        }
    }
    if (rc == P3_OK && archive->minor >= 45U) {
        rc = p3_bam_get_u8(&pc->cur, &tx->has_clear_color);
        if (rc == P3_OK && tx->has_clear_color != 0U) {
            rc = p3_scene_get_color4(archive, pc, tx->clear_color);
        }
    }
    if (rc == P3_OK && tx->has_rawdata != 0U) {
        rc = p3_bam_get_u32(&pc->cur, &tmp32);
        tx->size_x = (tmp32 > 0xffffUL) ? 0xffffU : (p3_u16)tmp32;
    }
    if (rc == P3_OK && tx->has_rawdata != 0U) {
        rc = p3_bam_get_u32(&pc->cur, &tmp32);
        tx->size_y = (tmp32 > 0xffffUL) ? 0xffffU : (p3_u16)tmp32;
    }
    if (rc == P3_OK && tx->has_rawdata != 0U) {
        rc = p3_bam_get_u32(&pc->cur, &tmp32);
        tx->size_z = (tmp32 > 0xffffUL) ? 0xffffU : (p3_u16)tmp32;
    }
    if (rc == P3_OK && tx->has_rawdata != 0U && archive->minor >= 30U) {
        rc = p3_bam_get_u32(&pc->cur, &tmp32);
        tx->pad_xsize = (tmp32 > 0xffffUL) ? 0xffffU : (p3_u16)tmp32;
    }
    if (rc == P3_OK && tx->has_rawdata != 0U && archive->minor >= 30U) {
        rc = p3_bam_get_u32(&pc->cur, &tmp32);
        tx->pad_ysize = (tmp32 > 0xffffUL) ? 0xffffU : (p3_u16)tmp32;
    }
    if (rc == P3_OK && tx->has_rawdata != 0U && archive->minor >= 30U) {
        rc = p3_bam_get_u32(&pc->cur, &tmp32);
        tx->pad_zsize = (tmp32 > 0xffffUL) ? 0xffffU : (p3_u16)tmp32;
    }
    if (rc == P3_OK && tx->has_rawdata != 0U) {
        tx->num_views = 1U;
        tx->ram_image_count = 1U;
    }
    if (rc == P3_OK && tx->has_rawdata != 0U && archive->minor >= 26U) {
        rc = p3_bam_get_u32(&pc->cur, &tmp32);
        tx->num_views = (tmp32 > 0xffffUL) ? 0xffffU : (p3_u16)tmp32;
    }
    if (rc == P3_OK && tx->has_rawdata != 0U) {
        rc = p3_bam_get_u8(&pc->cur, &tx->component_type);
    }
    if (rc == P3_OK && tx->has_rawdata != 0U) {
        rc = p3_bam_get_u8(&pc->cur, &tx->component_width);
    }
    if (rc == P3_OK && tx->has_rawdata != 0U && archive->minor >= 1U) {
        rc = p3_bam_get_u8(&pc->cur, &tx->ram_image_compression);
    }
    if (rc == P3_OK && tx->has_rawdata != 0U && archive->minor >= 3U) {
        rc = p3_bam_get_u8(&pc->cur, &tx->ram_image_count);
    }
    if (rc == P3_OK && tx->has_rawdata != 0U && archive->minor >= 1U) {
        rc = p3_bam_get_u32(&pc->cur, &tx->ram_image_page_size);
    }
    if (rc == P3_OK && tx->has_rawdata != 0U) {
        rc = p3_bam_get_u32(&pc->cur, &tx->ram_image_size);
    }
    if (rc == P3_OK && tx->has_rawdata != 0U) {
        p3_u32 to_copy2;
        to_copy2 = tx->ram_image_size;
        if (to_copy2 > (p3_u32)P3_BAM_SPECIAL_DATA_PREVIEW) {
            to_copy2 = (p3_u32)P3_BAM_SPECIAL_DATA_PREVIEW;
        }
        tx->ram_image_preview_size = (p3_u8)to_copy2;
        if (to_copy2 > 0UL) {
            rc = p3_bam_get_bytes(&pc->cur, tx->ram_image_preview, to_copy2);
        }
        if (rc == P3_OK && tx->ram_image_size > to_copy2) {
            pc->cur.pos += tx->ram_image_size - to_copy2;
            if (pc->cur.pos > pc->cur.size) {
                rc = P3_ERR_TRUNCATED;
            }
        }
    }
    return rc;
}

void p3_bam_scene_object_init(p3_bam_scene_object *obj) {
    p3_mem_set(obj, 0, (p3_u32)sizeof(*obj));
}

const char *p3_bam_scene_class_name(p3_u8 class_kind) {
    switch (class_kind) {
    case P3_BAM_SCENE_RENDER_STATE:
        return "RenderState";
    case P3_BAM_SCENE_TRANSFORM_STATE:
        return "TransformState";
    case P3_BAM_SCENE_TEXTURE_STAGE:
        return "TextureStage";
    case P3_BAM_SCENE_TEXTURE:
        return "Texture";
    case P3_BAM_SCENE_MATERIAL:
        return "Material";
    case P3_BAM_SCENE_TEXTURE_ATTRIB:
        return "TextureAttrib";
    case P3_BAM_SCENE_MATERIAL_ATTRIB:
        return "MaterialAttrib";
    case P3_BAM_SCENE_COLOR_ATTRIB:
        return "ColorAttrib";
    case P3_BAM_SCENE_COLOR_SCALE_ATTRIB:
        return "ColorScaleAttrib";
    case P3_BAM_SCENE_TEX_MATRIX_ATTRIB:
        return "TexMatrixAttrib";
    case P3_BAM_SCENE_TEX_GEN_ATTRIB:
        return "TexGenAttrib";
    case P3_BAM_SCENE_TRANSFORM_TABLE:
        return "TransformTable";
    case P3_BAM_SCENE_TRANSFORM_BLEND:
        return "TransformBlend";
    case P3_BAM_SCENE_TRANSFORM_BLEND_TABLE:
        return "TransformBlendTable";
    case P3_BAM_SCENE_JOINT_VERTEX_TRANSFORM:
        return "JointVertexTransform";
    case P3_BAM_SCENE_CHARACTER_JOINT:
        return "CharacterJoint";
    default:
        return "unknown";
    }
}

int p3_bam_scene_parse_object(const p3_bam_archive *archive, p3_u16 record_index, p3_bam_scene_object *out_obj) {
    const p3_bam_record *rec;
    p3_scene_cursor pc;
    if (archive == 0 || out_obj == 0) {
        return P3_ERR_INVALID;
    }
    if (record_index >= archive->record_count) {
        return P3_ERR_RANGE;
    }
    rec = &archive->records[record_index];
    if (rec->kind != P3_BAM_BOC_PUSH && rec->kind != P3_BAM_BOC_ADJUNCT) {
        return P3_ERR_UNSUPPORTED;
    }
    p3_bam_scene_object_init(out_obj);
    out_obj->record_index = record_index;
    out_obj->record_kind = rec->kind;
    out_obj->type_id = rec->type_id;
    out_obj->object_id = rec->object_id;
    p3_scene_get_type_name(archive, rec->type_id, out_obj->type_name, (p3_u16)sizeof(out_obj->type_name));
    p3_scene_cursor_init(&pc, archive, rec);

    if (strcmp(out_obj->type_name, "RenderState") == 0) {
        out_obj->class_kind = P3_BAM_SCENE_RENDER_STATE;
        return p3_scene_parse_render_state(archive, &pc, &out_obj->u.render_state);
    }
    if (strcmp(out_obj->type_name, "TransformState") == 0) {
        out_obj->class_kind = P3_BAM_SCENE_TRANSFORM_STATE;
        return p3_scene_parse_transform_state(archive, &pc, &out_obj->u.transform_state);
    }
    if (strcmp(out_obj->type_name, "TextureStage") == 0) {
        out_obj->class_kind = P3_BAM_SCENE_TEXTURE_STAGE;
        return p3_scene_parse_texture_stage(archive, &pc, &out_obj->u.texture_stage);
    }
    if (strcmp(out_obj->type_name, "Texture") == 0) {
        out_obj->class_kind = P3_BAM_SCENE_TEXTURE;
        return p3_scene_parse_texture(archive, &pc, &out_obj->u.texture);
    }
    if (strcmp(out_obj->type_name, "Material") == 0) {
        out_obj->class_kind = P3_BAM_SCENE_MATERIAL;
        return p3_scene_parse_material(archive, &pc, &out_obj->u.material);
    }
    if (strcmp(out_obj->type_name, "TextureAttrib") == 0) {
        out_obj->class_kind = P3_BAM_SCENE_TEXTURE_ATTRIB;
        return p3_scene_parse_texture_attrib(archive, &pc, &out_obj->u.texture_attrib);
    }
    if (strcmp(out_obj->type_name, "MaterialAttrib") == 0) {
        out_obj->class_kind = P3_BAM_SCENE_MATERIAL_ATTRIB;
        return p3_scene_parse_material_attrib(archive, &pc, &out_obj->u.material_attrib);
    }
    if (strcmp(out_obj->type_name, "ColorAttrib") == 0) {
        out_obj->class_kind = P3_BAM_SCENE_COLOR_ATTRIB;
        return p3_scene_parse_color_attrib(archive, &pc, &out_obj->u.color_attrib);
    }
    if (strcmp(out_obj->type_name, "ColorScaleAttrib") == 0) {
        out_obj->class_kind = P3_BAM_SCENE_COLOR_SCALE_ATTRIB;
        return p3_scene_parse_color_scale_attrib(archive, &pc, &out_obj->u.color_scale_attrib);
    }
    if (strcmp(out_obj->type_name, "TexMatrixAttrib") == 0) {
        out_obj->class_kind = P3_BAM_SCENE_TEX_MATRIX_ATTRIB;
        return p3_scene_parse_tex_matrix_attrib(archive, &pc, &out_obj->u.tex_matrix_attrib);
    }
    if (strcmp(out_obj->type_name, "TexGenAttrib") == 0) {
        out_obj->class_kind = P3_BAM_SCENE_TEX_GEN_ATTRIB;
        return p3_scene_parse_tex_gen_attrib(archive, &pc, &out_obj->u.tex_gen_attrib);
    }
    if (strcmp(out_obj->type_name, "TransformTable") == 0) {
        out_obj->class_kind = P3_BAM_SCENE_TRANSFORM_TABLE;
        return p3_scene_parse_transform_table(archive, &pc, &out_obj->u.transform_table);
    }
    if (strcmp(out_obj->type_name, "TransformBlend") == 0) {
        out_obj->class_kind = P3_BAM_SCENE_TRANSFORM_BLEND;
        return p3_scene_parse_transform_blend(archive, &pc, &out_obj->u.transform_blend);
    }
    if (strcmp(out_obj->type_name, "TransformBlendTable") == 0) {
        out_obj->class_kind = P3_BAM_SCENE_TRANSFORM_BLEND_TABLE;
        return p3_scene_parse_transform_blend_table(archive, &pc, &out_obj->u.transform_blend_table);
    }
    if (strcmp(out_obj->type_name, "JointVertexTransform") == 0) {
        out_obj->class_kind = P3_BAM_SCENE_JOINT_VERTEX_TRANSFORM;
        return p3_scene_parse_joint_vertex_transform(archive, &pc, &out_obj->u.joint_vertex_transform);
    }
    if (strcmp(out_obj->type_name, "CharacterJoint") == 0) {
        out_obj->class_kind = P3_BAM_SCENE_CHARACTER_JOINT;
        return p3_scene_parse_character_joint(archive, &pc, &out_obj->u.character_joint);
    }
    return P3_ERR_UNSUPPORTED;
}

int p3_bam_build_sampler_state_payload(const p3_bam_sampler_state *src, p3_u8 *dst, p3_u32 cap, p3_u32 *out_len) {
    p3_bam_builder b;
    int rc;
    if (src == 0 || dst == 0 || out_len == 0) {
        return P3_ERR_INVALID;
    }
    p3_bam_builder_init(&b, dst, cap);
    rc = p3_scene_put_sampler(&b, src);
    if (rc != P3_OK) {
        return rc;
    }
    *out_len = b.size;
    return P3_OK;
}

int p3_bam_build_render_state_payload(const p3_bam_render_state *src, p3_u8 *dst, p3_u32 cap, p3_u32 *out_len) {
    p3_bam_builder b;
    int rc;
    int long_ids;
    p3_u16 i;
    if (src == 0 || dst == 0 || out_len == 0) {
        return P3_ERR_INVALID;
    }
    if (src->attrib_count > P3_BAM_SCENE_MAX_ATTRIBS) {
        return P3_ERR_RANGE;
    }
    p3_bam_builder_init(&b, dst, cap);
    rc = p3_bam_put_u16(&b, src->attrib_count);
    long_ids = 0;
    for (i = 0U; rc == P3_OK && i < src->attrib_count; ++i) {
        rc = p3_scene_put_pointer(&b, &long_ids, src->attribs[i].attrib_object_id);
        if (rc == P3_OK) {
            rc = p3_scene_put_s32(&b, src->attribs[i].override_value);
        }
    }
    if (rc != P3_OK) {
        return rc;
    }
    *out_len = b.size;
    return P3_OK;
}

int p3_bam_build_transform_state_payload(const p3_bam_transform_state *src, p3_u8 *dst, p3_u32 cap, p3_u32 *out_len) {
    p3_bam_builder b;
    int rc;
    p3_u16 i;
    if (src == 0 || dst == 0 || out_len == 0) {
        return P3_ERR_INVALID;
    }
    p3_bam_builder_init(&b, dst, cap);
    rc = p3_bam_put_u32(&b, src->flags);
    if (rc != P3_OK) {
        return rc;
    }
    if ((src->flags & P3_TRANSFORM_F_IS_IDENTITY) != 0UL || (src->flags & P3_TRANSFORM_F_IS_INVALID) != 0UL) {
        *out_len = b.size;
        return P3_OK;
    }
    if (src->representation == 1U) {
        for (i = 0U; rc == P3_OK && i < 3U; ++i) {
            rc = p3_scene_put_fixed(&b, src->pos[i]);
        }
        if ((src->flags & P3_TRANSFORM_F_QUAT_GIVEN) != 0UL || (src->flags & P3_TRANSFORM_F_QUAT_KNOWN) != 0UL) {
            for (i = 0U; rc == P3_OK && i < 4U; ++i) {
                rc = p3_scene_put_fixed(&b, src->quat[i]);
            }
        } else {
            for (i = 0U; rc == P3_OK && i < 3U; ++i) {
                rc = p3_scene_put_fixed(&b, src->hpr[i]);
            }
        }
        for (i = 0U; rc == P3_OK && i < 3U; ++i) {
            rc = p3_scene_put_fixed(&b, src->scale[i]);
        }
        for (i = 0U; rc == P3_OK && i < 3U; ++i) {
            rc = p3_scene_put_fixed(&b, src->shear[i]);
        }
    } else {
        for (i = 0U; rc == P3_OK && i < 16U; ++i) {
            rc = p3_scene_put_fixed(&b, src->mat[i]);
        }
    }
    if (rc != P3_OK) {
        return rc;
    }
    *out_len = b.size;
    return P3_OK;
}

int p3_bam_build_texture_stage_payload(const p3_bam_texture_stage *src, p3_u8 *dst, p3_u32 cap, p3_u32 *out_len, p3_u16 file_minor) {
    p3_bam_builder b;
    int rc;
    int long_ids;
    if (src == 0 || dst == 0 || out_len == 0) {
        return P3_ERR_INVALID;
    }
    p3_bam_builder_init(&b, dst, cap);
    rc = p3_bam_put_u8(&b, src->is_default_stage);
    if (rc != P3_OK || src->is_default_stage != 0U) {
        if (rc == P3_OK) {
            *out_len = b.size;
        }
        return rc;
    }
    rc = p3_bam_put_string(&b, src->name, (p3_u16)strlen(src->name));
    if (rc == P3_OK) {
        rc = p3_scene_put_s32(&b, src->sort);
    }
    if (rc == P3_OK) {
        rc = p3_scene_put_s32(&b, src->priority);
    }
    long_ids = 0;
    if (rc == P3_OK) {
        rc = p3_scene_put_pointer(&b, &long_ids, src->texcoord_name_object_id);
    }
    if (rc == P3_OK) {
        rc = p3_bam_put_u8(&b, src->mode);
    }
    if (rc == P3_OK) {
        rc = p3_scene_put_color4(&b, src->color);
    }
    if (rc == P3_OK) {
        rc = p3_bam_put_u8(&b, src->rgb_scale);
    }
    if (rc == P3_OK) {
        rc = p3_bam_put_u8(&b, src->alpha_scale);
    }
    if (rc == P3_OK) {
        rc = p3_bam_put_u8(&b, src->saved_result);
    }
    if (rc == P3_OK && file_minor >= 26U) {
        rc = p3_scene_put_s32(&b, src->tex_view_offset);
    }
    if (rc == P3_OK) {
        rc = p3_bam_put_u8(&b, src->combine_rgb_mode);
    }
    if (rc == P3_OK) {
        rc = p3_bam_put_u8(&b, src->num_combine_rgb_operands);
    }
    if (rc == P3_OK) {
        rc = p3_bam_put_u8(&b, src->combine_rgb_source[0]);
    }
    if (rc == P3_OK) {
        rc = p3_bam_put_u8(&b, src->combine_rgb_operand[0]);
    }
    if (rc == P3_OK) {
        rc = p3_bam_put_u8(&b, src->combine_rgb_source[1]);
    }
    if (rc == P3_OK) {
        rc = p3_bam_put_u8(&b, src->combine_rgb_operand[1]);
    }
    if (rc == P3_OK) {
        rc = p3_bam_put_u8(&b, src->combine_rgb_source[2]);
    }
    if (rc == P3_OK) {
        rc = p3_bam_put_u8(&b, src->combine_rgb_operand[2]);
    }
    if (rc == P3_OK) {
        rc = p3_bam_put_u8(&b, src->combine_alpha_mode);
    }
    if (rc == P3_OK) {
        rc = p3_bam_put_u8(&b, src->num_combine_alpha_operands);
    }
    if (rc == P3_OK) {
        rc = p3_bam_put_u8(&b, src->combine_alpha_source[0]);
    }
    if (rc == P3_OK) {
        rc = p3_bam_put_u8(&b, src->combine_alpha_operand[0]);
    }
    if (rc == P3_OK) {
        rc = p3_bam_put_u8(&b, src->combine_alpha_source[1]);
    }
    if (rc == P3_OK) {
        rc = p3_bam_put_u8(&b, src->combine_alpha_operand[1]);
    }
    if (rc == P3_OK) {
        rc = p3_bam_put_u8(&b, src->combine_alpha_source[2]);
    }
    if (rc == P3_OK) {
        rc = p3_bam_put_u8(&b, src->combine_alpha_operand[2]);
    }
    if (rc != P3_OK) {
        return rc;
    }
    *out_len = b.size;
    return P3_OK;
}

int p3_bam_build_texture_payload(const p3_bam_texture *src, p3_u8 *dst, p3_u32 cap, p3_u32 *out_len, p3_u16 file_minor) {
    p3_bam_builder b;
    int rc;
    p3_u32 preview_len;
    if (src == 0 || dst == 0 || out_len == 0) {
        return P3_ERR_INVALID;
    }
    p3_bam_builder_init(&b, dst, cap);
    rc = p3_bam_put_string(&b, src->name, (p3_u16)strlen(src->name));
    if (rc == P3_OK) {
        rc = p3_bam_put_string(&b, src->filename, (p3_u16)strlen(src->filename));
    }
    if (rc == P3_OK) {
        rc = p3_bam_put_string(&b, src->alpha_filename, (p3_u16)strlen(src->alpha_filename));
    }
    if (rc == P3_OK) {
        rc = p3_bam_put_u8(&b, src->primary_file_num_channels);
    }
    if (rc == P3_OK) {
        rc = p3_bam_put_u8(&b, src->alpha_file_channel);
    }
    if (rc == P3_OK) {
        rc = p3_bam_put_u8(&b, src->has_rawdata);
    }
    if (rc == P3_OK) {
        rc = p3_bam_put_u8(&b, src->texture_type);
    }
    if (rc == P3_OK && file_minor >= 32U) {
        rc = p3_bam_put_u8(&b, src->has_read_mipmaps);
    }
    if (rc == P3_OK) {
        rc = p3_scene_put_sampler_compat(&b, &src->sampler, file_minor >= 36U ? 1U : 0U);
    }
    if (rc == P3_OK && file_minor >= 1U) {
        rc = p3_bam_put_u8(&b, src->compression);
    }
    if (rc == P3_OK && file_minor >= 16U) {
        rc = p3_bam_put_u8(&b, src->quality_level);
    }
    if (rc == P3_OK) {
        rc = p3_bam_put_u8(&b, src->format);
    }
    if (rc == P3_OK) {
        rc = p3_bam_put_u8(&b, src->num_components);
    }
    if (rc == P3_OK && src->texture_type == P3_TEXTURE_TT_BUFFER_TEXTURE) {
        rc = p3_bam_put_u8(&b, src->usage_hint);
    }
    if (rc == P3_OK && file_minor >= 28U) {
        rc = p3_bam_put_u8(&b, src->auto_texture_scale);
    }
    preview_len = src->ram_image_preview_size;
    if (preview_len > (p3_u32)P3_BAM_SPECIAL_DATA_PREVIEW) {
        preview_len = (p3_u32)P3_BAM_SPECIAL_DATA_PREVIEW;
    }
    if (rc == P3_OK && file_minor >= 18U) {
        rc = p3_bam_put_u32(&b, src->orig_file_x);
    }
    if (rc == P3_OK && file_minor >= 18U) {
        rc = p3_bam_put_u32(&b, src->orig_file_y);
    }
    if (rc == P3_OK && file_minor >= 18U) {
        p3_u8 write_simple;
        write_simple = (src->has_rawdata == 0U && preview_len > 0UL) ? 1U : 0U;
        rc = p3_bam_put_u8(&b, write_simple);
        if (rc == P3_OK && write_simple != 0U) {
            rc = p3_bam_put_u32(&b, src->orig_file_x);
        }
        if (rc == P3_OK && write_simple != 0U) {
            rc = p3_bam_put_u32(&b, src->orig_file_y);
        }
        if (rc == P3_OK && write_simple != 0U) {
            rc = p3_scene_put_s32(&b, 0L);
        }
        if (rc == P3_OK && write_simple != 0U) {
            rc = p3_bam_put_u32(&b, preview_len);
        }
        if (rc == P3_OK && write_simple != 0U && preview_len > 0UL) {
            rc = p3_bam_put_bytes(&b, src->ram_image_preview, preview_len);
        }
    }
    if (rc == P3_OK && file_minor >= 45U) {
        rc = p3_bam_put_u8(&b, src->has_clear_color);
        if (rc == P3_OK && src->has_clear_color != 0U) {
            rc = p3_scene_put_color4(&b, src->clear_color);
        }
    }
    if (rc == P3_OK && src->has_rawdata != 0U) {
        rc = p3_bam_put_u32(&b, src->size_x);
    }
    if (rc == P3_OK && src->has_rawdata != 0U) {
        rc = p3_bam_put_u32(&b, src->size_y);
    }
    if (rc == P3_OK && src->has_rawdata != 0U) {
        rc = p3_bam_put_u32(&b, src->size_z);
    }
    if (rc == P3_OK && src->has_rawdata != 0U && file_minor >= 30U) {
        rc = p3_bam_put_u32(&b, src->pad_xsize);
    }
    if (rc == P3_OK && src->has_rawdata != 0U && file_minor >= 30U) {
        rc = p3_bam_put_u32(&b, src->pad_ysize);
    }
    if (rc == P3_OK && src->has_rawdata != 0U && file_minor >= 30U) {
        rc = p3_bam_put_u32(&b, src->pad_zsize);
    }
    if (rc == P3_OK && src->has_rawdata != 0U && file_minor >= 26U) {
        rc = p3_bam_put_u32(&b, src->num_views);
    }
    if (rc == P3_OK && src->has_rawdata != 0U) {
        rc = p3_bam_put_u8(&b, src->component_type);
    }
    if (rc == P3_OK && src->has_rawdata != 0U) {
        rc = p3_bam_put_u8(&b, src->component_width);
    }
    if (rc == P3_OK && src->has_rawdata != 0U && file_minor >= 1U) {
        rc = p3_bam_put_u8(&b, src->ram_image_compression);
    }
    if (rc == P3_OK && src->has_rawdata != 0U && file_minor >= 3U) {
        rc = p3_bam_put_u8(&b, src->ram_image_count == 0U ? 1U : src->ram_image_count);
    }
    if (rc == P3_OK && src->has_rawdata != 0U && file_minor >= 1U) {
        rc = p3_bam_put_u32(&b, src->ram_image_page_size);
    }
    if (rc == P3_OK && src->has_rawdata != 0U) {
        rc = p3_bam_put_u32(&b, preview_len);
    }
    if (rc == P3_OK && src->has_rawdata != 0U && preview_len > 0UL) {
        rc = p3_bam_put_bytes(&b, src->ram_image_preview, preview_len);
    }
    if (rc != P3_OK) {
        return rc;
    }
    *out_len = b.size;
    return P3_OK;
}

int p3_bam_build_material_payload(const p3_bam_material *src, p3_u8 *dst, p3_u32 cap, p3_u32 *out_len, p3_u16 file_minor) {
    p3_bam_builder b;
    int rc;
    if (src == 0 || dst == 0 || out_len == 0) {
        return P3_ERR_INVALID;
    }
    p3_bam_builder_init(&b, dst, cap);
    rc = p3_bam_put_string(&b, src->name, (p3_u16)strlen(src->name));
    if (file_minor >= 39U) {
        if (rc == P3_OK) {
            rc = p3_bam_put_u32(&b, src->flags);
        }
        if (rc == P3_OK) {
            if ((src->flags & P3_MATERIAL_F_METALLIC) != 0UL) {
                rc = p3_scene_put_color4(&b, src->base_color);
                if (rc == P3_OK) {
                    rc = p3_scene_put_fixed(&b, src->metallic);
                }
            } else {
                rc = p3_scene_put_color4(&b, src->ambient);
                if (rc == P3_OK) {
                    rc = p3_scene_put_color4(&b, src->diffuse);
                }
                if (rc == P3_OK) {
                    rc = p3_scene_put_color4(&b, src->specular);
                }
            }
        }
        if (rc == P3_OK) {
            rc = p3_scene_put_color4(&b, src->emission);
        }
        if (rc == P3_OK) {
            if ((src->flags & P3_MATERIAL_F_ROUGHNESS) != 0UL) {
                rc = p3_scene_put_fixed(&b, src->roughness);
            } else {
                rc = p3_scene_put_fixed(&b, src->shininess);
            }
        }
        if (rc == P3_OK) {
            rc = p3_scene_put_fixed(&b, src->refractive_index);
        }
    } else {
        if (rc == P3_OK) {
            rc = p3_scene_put_color4(&b, src->ambient);
        }
        if (rc == P3_OK) {
            rc = p3_scene_put_color4(&b, src->diffuse);
        }
        if (rc == P3_OK) {
            rc = p3_scene_put_color4(&b, src->specular);
        }
        if (rc == P3_OK) {
            rc = p3_scene_put_color4(&b, src->emission);
        }
        if (rc == P3_OK) {
            rc = p3_scene_put_fixed(&b, src->shininess);
        }
        if (rc == P3_OK) {
            rc = p3_bam_put_u32(&b, src->flags & 0x7fUL);
        }
    }
    if (rc != P3_OK) {
        return rc;
    }
    *out_len = b.size;
    return P3_OK;
}

int p3_bam_build_texture_attrib_payload(const p3_bam_texture_attrib *src, p3_u8 *dst, p3_u32 cap, p3_u32 *out_len, p3_u16 file_minor) {
    p3_bam_builder b;
    int rc;
    int long_ids;
    p3_u16 i;
    if (src == 0 || dst == 0 || out_len == 0) {
        return P3_ERR_INVALID;
    }
    if (src->off_stage_count > P3_BAM_SCENE_MAX_OFF_STAGES || src->on_stage_count > P3_BAM_SCENE_MAX_ON_STAGES) {
        return P3_ERR_RANGE;
    }
    p3_bam_builder_init(&b, dst, cap);
    rc = p3_bam_put_u8(&b, src->off_all_stages);
    if (rc == P3_OK) {
        rc = p3_bam_put_u16(&b, src->off_stage_count);
    }
    long_ids = 0;
    for (i = 0U; rc == P3_OK && i < src->off_stage_count; ++i) {
        rc = p3_scene_put_pointer(&b, &long_ids, src->off_stage_object_ids[i]);
    }
    if (rc == P3_OK) {
        rc = p3_bam_put_u16(&b, src->on_stage_count);
    }
    for (i = 0U; rc == P3_OK && i < src->on_stage_count; ++i) {
        rc = p3_scene_put_pointer(&b, &long_ids, src->on_stages[i].stage_object_id);
        if (rc == P3_OK) {
            rc = p3_scene_put_pointer(&b, &long_ids, src->on_stages[i].texture_object_id);
        }
        if (rc == P3_OK && file_minor >= 15U) {
            if (src->on_stages[i].implicit_sort < 0 || src->on_stages[i].implicit_sort > 65535L) {
                return P3_ERR_RANGE;
            }
            rc = p3_bam_put_u16(&b, (p3_u16)src->on_stages[i].implicit_sort);
        }
        if (rc == P3_OK && file_minor >= 23U) {
            rc = p3_scene_put_s32(&b, src->on_stages[i].override_value);
        }
        if (rc == P3_OK && file_minor >= 36U) {
            rc = p3_bam_put_u8(&b, src->on_stages[i].has_sampler);
            if (rc == P3_OK && src->on_stages[i].has_sampler != 0U) {
                rc = p3_scene_put_sampler(&b, &src->on_stages[i].sampler);
            }
        }
    }
    if (rc != P3_OK) {
        return rc;
    }
    *out_len = b.size;
    return P3_OK;
}

int p3_bam_build_material_attrib_payload(const p3_bam_material_attrib *src, p3_u8 *dst, p3_u32 cap, p3_u32 *out_len) {
    p3_bam_builder b;
    int rc;
    int long_ids;
    if (src == 0 || dst == 0 || out_len == 0) {
        return P3_ERR_INVALID;
    }
    p3_bam_builder_init(&b, dst, cap);
    long_ids = 0;
    rc = p3_scene_put_pointer(&b, &long_ids, src->material_object_id);
    if (rc != P3_OK) {
        return rc;
    }
    *out_len = b.size;
    return P3_OK;
}


int p3_bam_build_color_attrib_payload(const p3_bam_color_attrib *src, p3_u8 *dst, p3_u32 cap, p3_u32 *out_len) {
    p3_bam_builder b;
    int rc;
    if (src == 0 || dst == 0 || out_len == 0) {
        return P3_ERR_INVALID;
    }
    p3_bam_builder_init(&b, dst, cap);
    rc = p3_bam_put_u8(&b, src->color_type);
    if (rc == P3_OK) {
        rc = p3_scene_put_color4(&b, src->color);
    }
    if (rc != P3_OK) {
        return rc;
    }
    *out_len = b.size;
    return P3_OK;
}

int p3_bam_build_color_scale_attrib_payload(const p3_bam_color_scale_attrib *src, p3_u8 *dst, p3_u32 cap, p3_u32 *out_len) {
    p3_bam_builder b;
    int rc;
    if (src == 0 || dst == 0 || out_len == 0) {
        return P3_ERR_INVALID;
    }
    p3_bam_builder_init(&b, dst, cap);
    rc = p3_bam_put_u8(&b, src->off);
    if (rc == P3_OK) {
        rc = p3_scene_put_color4(&b, src->scale);
    }
    if (rc != P3_OK) {
        return rc;
    }
    *out_len = b.size;
    return P3_OK;
}

int p3_bam_build_tex_matrix_attrib_payload(const p3_bam_tex_matrix_attrib *src, p3_u8 *dst, p3_u32 cap, p3_u32 *out_len, p3_u16 file_minor) {
    p3_bam_builder b;
    int rc;
    int long_ids;
    p3_u16 i;
    if (src == 0 || dst == 0 || out_len == 0) {
        return P3_ERR_INVALID;
    }
    if (src->stage_count > P3_BAM_SCENE_MAX_TEX_MATRIX_STAGES) {
        return P3_ERR_RANGE;
    }
    p3_bam_builder_init(&b, dst, cap);
    rc = p3_bam_put_u16(&b, src->stage_count);
    long_ids = 0;
    for (i = 0U; rc == P3_OK && i < src->stage_count; ++i) {
        rc = p3_scene_put_pointer(&b, &long_ids, src->stages[i].stage_object_id);
        if (rc == P3_OK) {
            rc = p3_scene_put_pointer(&b, &long_ids, src->stages[i].transform_object_id);
        }
        if (rc == P3_OK && file_minor >= 24U) {
            rc = p3_scene_put_s32(&b, src->stages[i].override_value);
        }
    }
    if (rc != P3_OK) {
        return rc;
    }
    *out_len = b.size;
    return P3_OK;
}

int p3_bam_build_tex_gen_attrib_payload(const p3_bam_tex_gen_attrib *src, p3_u8 *dst, p3_u32 cap, p3_u32 *out_len) {
    p3_bam_builder b;
    int rc;
    int long_ids;
    p3_u16 i;
    if (src == 0 || dst == 0 || out_len == 0) {
        return P3_ERR_INVALID;
    }
    if (src->stage_count > P3_BAM_SCENE_MAX_TEX_GEN_STAGES) {
        return P3_ERR_RANGE;
    }
    p3_bam_builder_init(&b, dst, cap);
    rc = p3_bam_put_u16(&b, src->stage_count);
    long_ids = 0;
    for (i = 0U; rc == P3_OK && i < src->stage_count; ++i) {
        rc = p3_scene_put_pointer(&b, &long_ids, src->stages[i].stage_object_id);
        if (rc == P3_OK) {
            rc = p3_bam_put_u8(&b, src->stages[i].mode);
        }
    }
    if (rc != P3_OK) {
        return rc;
    }
    *out_len = b.size;
    return P3_OK;
}

int p3_bam_build_transform_table_payload(const p3_bam_transform_table *src, p3_u8 *dst, p3_u32 cap, p3_u32 *out_len) {
    p3_bam_builder b;
    int rc;
    int long_ids;
    p3_u16 i;
    if (src == 0 || dst == 0 || out_len == 0) {
        return P3_ERR_INVALID;
    }
    if (src->transform_count > P3_BAM_SCENE_MAX_TRANSFORMS) {
        return P3_ERR_RANGE;
    }
    p3_bam_builder_init(&b, dst, cap);
    rc = p3_bam_put_u16(&b, src->transform_count);
    long_ids = 0;
    for (i = 0U; rc == P3_OK && i < src->transform_count; ++i) {
        rc = p3_scene_put_pointer(&b, &long_ids, src->transform_object_ids[i]);
    }
    if (rc != P3_OK) {
        return rc;
    }
    *out_len = b.size;
    return P3_OK;
}

int p3_bam_build_transform_blend_payload(const p3_bam_transform_blend *src, p3_u8 *dst, p3_u32 cap, p3_u32 *out_len) {
    p3_bam_builder b;
    int rc;
    int long_ids;
    p3_u16 i;
    if (src == 0 || dst == 0 || out_len == 0) {
        return P3_ERR_INVALID;
    }
    if (src->entry_count > P3_BAM_SCENE_MAX_BLEND_ENTRIES) {
        return P3_ERR_RANGE;
    }
    p3_bam_builder_init(&b, dst, cap);
    rc = p3_bam_put_u16(&b, src->entry_count);
    long_ids = 0;
    for (i = 0U; rc == P3_OK && i < src->entry_count; ++i) {
        rc = p3_scene_put_pointer(&b, &long_ids, src->entries[i].transform_object_id);
        if (rc == P3_OK) {
            rc = p3_scene_put_fixed(&b, src->entries[i].weight);
        }
    }
    if (rc != P3_OK) {
        return rc;
    }
    *out_len = b.size;
    return P3_OK;
}

int p3_bam_build_transform_blend_table_payload(const p3_bam_transform_blend_table *src, p3_u8 *dst, p3_u32 cap, p3_u32 *out_len) {
    p3_bam_builder b;
    int rc;
    p3_u16 i;
    p3_u8 tmp[P3_TEXT_IO_CAP];
    p3_u32 tmp_len;
    if (src == 0 || dst == 0 || out_len == 0) {
        return P3_ERR_INVALID;
    }
    if (src->blend_count > P3_BAM_SCENE_MAX_BLENDS) {
        return P3_ERR_RANGE;
    }
    p3_bam_builder_init(&b, dst, cap);
    rc = p3_bam_put_u16(&b, src->blend_count);
    for (i = 0U; rc == P3_OK && i < src->blend_count; ++i) {
        rc = p3_bam_build_transform_blend_payload(&src->blends[i], tmp, (p3_u32)sizeof(tmp), &tmp_len);
        if (rc == P3_OK) {
            rc = p3_bam_put_bytes(&b, tmp, tmp_len);
        }
    }
    if (rc == P3_OK) {
        rc = p3_bam_put_u16(&b, src->rows);
    }
    if (rc != P3_OK) {
        return rc;
    }
    *out_len = b.size;
    return P3_OK;
}

int p3_bam_build_joint_vertex_transform_payload(const p3_bam_joint_vertex_transform *src, p3_u8 *dst, p3_u32 cap, p3_u32 *out_len) {
    p3_bam_builder b;
    int rc;
    int long_ids;
    if (src == 0 || dst == 0 || out_len == 0) {
        return P3_ERR_INVALID;
    }
    p3_bam_builder_init(&b, dst, cap);
    long_ids = 0;
    rc = p3_scene_put_pointer(&b, &long_ids, src->joint_object_id);
    if (rc != P3_OK) {
        return rc;
    }
    *out_len = b.size;
    return P3_OK;
}

int p3_bam_build_character_joint_payload(const p3_bam_character_joint *src, p3_u8 *dst, p3_u32 cap, p3_u32 *out_len) {
    p3_bam_builder b;
    int rc;
    int long_ids;
    p3_u16 i;
    if (src == 0 || dst == 0 || out_len == 0) {
        return P3_ERR_INVALID;
    }
    if (src->child_count > P3_BAM_SPECIAL_MAX_CHILDREN || src->net_node_count > P3_BAM_SPECIAL_MAX_CHILDREN || src->local_node_count > P3_BAM_SPECIAL_MAX_CHILDREN) {
        return P3_ERR_RANGE;
    }
    p3_bam_builder_init(&b, dst, cap);
    rc = p3_bam_put_string(&b, src->name, (p3_u16)strlen(src->name));
    if (rc == P3_OK) {
        rc = p3_bam_put_u16(&b, src->child_count);
    }
    long_ids = 0;
    for (i = 0U; rc == P3_OK && i < src->child_count; ++i) {
        rc = p3_scene_put_pointer(&b, &long_ids, src->child_object_ids[i]);
    }
    if (rc == P3_OK) {
        rc = p3_scene_put_pointer(&b, &long_ids, src->forced_channel_object_id);
    }
    if (rc == P3_OK) {
        rc = p3_scene_put_pointer(&b, &long_ids, src->character_object_id);
    }
    if (rc == P3_OK) {
        rc = p3_bam_put_u16(&b, src->net_node_count);
    }
    for (i = 0U; rc == P3_OK && i < src->net_node_count; ++i) {
        rc = p3_scene_put_pointer(&b, &long_ids, src->net_node_object_ids[i]);
    }
    if (rc == P3_OK) {
        rc = p3_bam_put_u16(&b, src->local_node_count);
    }
    for (i = 0U; rc == P3_OK && i < src->local_node_count; ++i) {
        rc = p3_scene_put_pointer(&b, &long_ids, src->local_node_object_ids[i]);
    }
    for (i = 0U; rc == P3_OK && i < 16U; ++i) {
        rc = p3_scene_put_fixed(&b, src->initial_net_inverse[i]);
    }
    if (rc != P3_OK) {
        return rc;
    }
    *out_len = b.size;
    return P3_OK;
}
