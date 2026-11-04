#include <stdio.h>
#include "p3_error.h"
#include "p3_fixed.h"
#include "p3_p3m_writer.h"

static void p3_p3m_put_fixed(FILE *fp, p3_fixed v) {
    char buf[32];
    p3_fixed_to_str(v, buf);
    fprintf(fp, "%s", buf);
}

static const char *p3_p3m_stage_mode_name(p3_u8 mode) {
    switch (mode) {
    case P3_TEXTURE_STAGE_M_MODULATE: return "modulate";
    case P3_TEXTURE_STAGE_M_DECAL: return "decal";
    case P3_TEXTURE_STAGE_M_BLEND: return "blend";
    case P3_TEXTURE_STAGE_M_REPLACE: return "replace";
    case P3_TEXTURE_STAGE_M_ADD: return "add";
    case P3_TEXTURE_STAGE_M_COMBINE: return "combine";
    case P3_TEXTURE_STAGE_M_BLEND_COLOR_SCALE: return "blend_color_scale";
    case P3_TEXTURE_STAGE_M_MODULATE_GLOW: return "modulate_glow";
    case P3_TEXTURE_STAGE_M_MODULATE_GLOSS: return "modulate_gloss";
    case P3_TEXTURE_STAGE_M_NORMAL: return "normal";
    case P3_TEXTURE_STAGE_M_NORMAL_HEIGHT: return "normal_height";
    case P3_TEXTURE_STAGE_M_GLOW: return "glow";
    case P3_TEXTURE_STAGE_M_GLOSS: return "gloss";
    case P3_TEXTURE_STAGE_M_HEIGHT: return "height";
    case P3_TEXTURE_STAGE_M_SELECTOR: return "selector";
    case P3_TEXTURE_STAGE_M_NORMAL_GLOSS: return "normal_gloss";
    case P3_TEXTURE_STAGE_M_EMISSION: return "emission";
    default: return "unknown";
    }
}

static const char *p3_p3m_combine_mode_name(p3_u8 mode) {
    switch (mode) {
    case P3_TEXTURE_STAGE_CM_UNDEFINED: return "undefined";
    case P3_TEXTURE_STAGE_CM_REPLACE: return "replace";
    case P3_TEXTURE_STAGE_CM_MODULATE: return "modulate";
    case P3_TEXTURE_STAGE_CM_ADD: return "add";
    case P3_TEXTURE_STAGE_CM_ADD_SIGNED: return "add_signed";
    case P3_TEXTURE_STAGE_CM_INTERPOLATE: return "interpolate";
    case P3_TEXTURE_STAGE_CM_SUBTRACT: return "subtract";
    case P3_TEXTURE_STAGE_CM_DOT3_RGB: return "dot3_rgb";
    case P3_TEXTURE_STAGE_CM_DOT3_RGBA: return "dot3_rgba";
    default: return "unknown";
    }
}

static const char *p3_p3m_combine_source_name(p3_u8 source) {
    switch (source) {
    case P3_TEXTURE_STAGE_CS_UNDEFINED: return "undefined";
    case P3_TEXTURE_STAGE_CS_TEXTURE: return "texture";
    case P3_TEXTURE_STAGE_CS_CONSTANT: return "constant";
    case P3_TEXTURE_STAGE_CS_PRIMARY_COLOR: return "primary_color";
    case P3_TEXTURE_STAGE_CS_PREVIOUS: return "previous";
    case P3_TEXTURE_STAGE_CS_CONSTANT_COLOR_SCALE: return "constant_color_scale";
    case P3_TEXTURE_STAGE_CS_LAST_SAVED_RESULT: return "last_saved_result";
    default: return "unknown";
    }
}

static const char *p3_p3m_combine_operand_name(p3_u8 operand) {
    switch (operand) {
    case P3_TEXTURE_STAGE_CO_UNDEFINED: return "undefined";
    case P3_TEXTURE_STAGE_CO_SRC_COLOR: return "src_color";
    case P3_TEXTURE_STAGE_CO_ONE_MINUS_SRC_COLOR: return "one_minus_src_color";
    case P3_TEXTURE_STAGE_CO_SRC_ALPHA: return "src_alpha";
    case P3_TEXTURE_STAGE_CO_ONE_MINUS_SRC_ALPHA: return "one_minus_src_alpha";
    default: return "unknown";
    }
}

static const char *p3_p3m_tex_gen_mode_name(p3_u8 mode) {
    switch (mode) {
    case P3_TEX_GEN_M_OFF: return "off";
    case P3_TEX_GEN_M_EYE_SPHERE_MAP: return "eye_sphere_map";
    case P3_TEX_GEN_M_WORLD_CUBE_MAP: return "world_cube_map";
    case P3_TEX_GEN_M_EYE_CUBE_MAP: return "eye_cube_map";
    case P3_TEX_GEN_M_WORLD_NORMAL: return "world_normal";
    case P3_TEX_GEN_M_EYE_NORMAL: return "eye_normal";
    case P3_TEX_GEN_M_WORLD_POSITION: return "world_position";
    case P3_TEX_GEN_M_EYE_POSITION: return "eye_position";
    case P3_TEX_GEN_M_POINT_SPRITE: return "point_sprite";
    case P3_TEX_GEN_M_CONSTANT: return "constant";
    default: return "unknown";
    }
}

static void p3_p3m_put_mat4(FILE *fp, const p3_fixed *m) {
    p3_u16 i;
    for (i = 0U; i < 16U; ++i) {
        if (i != 0U) {
            fprintf(fp, ",");
        }
        p3_p3m_put_fixed(fp, m[i]);
    }
}

static void p3_p3m_put_sampler(FILE *fp, const p3_bam_sampler_state *sampler) {
    fprintf(fp, " wrap=%u,%u,%u filter=%u,%u aniso=%u border=",
        (unsigned int)sampler->wrap_u,
        (unsigned int)sampler->wrap_v,
        (unsigned int)sampler->wrap_w,
        (unsigned int)sampler->minfilter,
        (unsigned int)sampler->magfilter,
        (unsigned int)sampler->anisotropic_degree);
    p3_p3m_put_fixed(fp, sampler->border_color[0]); fprintf(fp, ",");
    p3_p3m_put_fixed(fp, sampler->border_color[1]); fprintf(fp, ",");
    p3_p3m_put_fixed(fp, sampler->border_color[2]); fprintf(fp, ",");
    p3_p3m_put_fixed(fp, sampler->border_color[3]);
    fprintf(fp, " lod=");
    p3_p3m_put_fixed(fp, sampler->min_lod); fprintf(fp, ",");
    p3_p3m_put_fixed(fp, sampler->max_lod); fprintf(fp, ",");
    p3_p3m_put_fixed(fp, sampler->lod_bias);
}

int p3_p3m_write_file(const char *path, const p3_bam_ir *ir) {
    FILE *fp;
    p3_u16 i;
    p3_u16 j;
    if (path == 0 || ir == 0) {
        return P3_ERR_INVALID;
    }
    fp = fopen(path, "wb");
    if (fp == 0) {
        return P3_ERR_IO;
    }
    fprintf(fp, "p3m 1\n");
    fprintf(fp, "textures %u\n", (unsigned int)ir->texture_count);
    for (i = 0U; i < ir->texture_count; ++i) {
        fprintf(fp, "texture %u id=%lu name=\"%s\" file=\"%s\" alpha=\"%s\" size=%u %u %u views=%u type=%u format=%u components=%u component_type=%u component_width=%u compression=%u ram_compression=%u quality=%u usage=%u auto_scale=%u",
            (unsigned int)i,
            (unsigned long)ir->textures[i].object_id,
            ir->textures[i].name,
            ir->textures[i].filename,
            ir->textures[i].alpha_filename,
            (unsigned int)ir->textures[i].width,
            (unsigned int)ir->textures[i].height,
            (unsigned int)ir->textures[i].depth,
            (unsigned int)ir->textures[i].num_views,
            (unsigned int)ir->textures[i].texture_type,
            (unsigned int)ir->textures[i].format,
            (unsigned int)ir->textures[i].num_components,
            (unsigned int)ir->textures[i].component_type,
            (unsigned int)ir->textures[i].component_width,
            (unsigned int)ir->textures[i].compression,
            (unsigned int)ir->textures[i].ram_image_compression,
            (unsigned int)ir->textures[i].quality_level,
            (unsigned int)ir->textures[i].usage_hint,
            (unsigned int)ir->textures[i].auto_texture_scale);
        if (ir->textures[i].has_clear_color != 0U) {
            fprintf(fp, " clear_color=");
            p3_p3m_put_fixed(fp, ir->textures[i].clear_color[0]); fprintf(fp, ",");
            p3_p3m_put_fixed(fp, ir->textures[i].clear_color[1]); fprintf(fp, ",");
            p3_p3m_put_fixed(fp, ir->textures[i].clear_color[2]); fprintf(fp, ",");
            p3_p3m_put_fixed(fp, ir->textures[i].clear_color[3]);
        }
        fprintf(fp, "\n");
    }
    fprintf(fp, "materials %u\n", (unsigned int)ir->material_count);
    for (i = 0U; i < ir->material_count; ++i) {
        fprintf(fp, "material %u id=%lu name=\"%s\" flags=%lu base_color=", 
            (unsigned int)i,
            (unsigned long)ir->materials[i].object_id,
            ir->materials[i].name,
            (unsigned long)ir->materials[i].flags);
        p3_p3m_put_fixed(fp, ir->materials[i].base_color[0]); fprintf(fp, ",");
        p3_p3m_put_fixed(fp, ir->materials[i].base_color[1]); fprintf(fp, ",");
        p3_p3m_put_fixed(fp, ir->materials[i].base_color[2]); fprintf(fp, ",");
        p3_p3m_put_fixed(fp, ir->materials[i].base_color[3]);
        fprintf(fp, " rough_or_shiny=");
        p3_p3m_put_fixed(fp, ir->materials[i].roughness_or_shininess);
        fprintf(fp, " metallic=");
        p3_p3m_put_fixed(fp, ir->materials[i].metallic);
        fprintf(fp, "\n");
    }
    fprintf(fp, "joints %u\n", (unsigned int)ir->joint_count);
    for (i = 0U; i < ir->joint_count; ++i) {
        fprintf(fp, "joint %u id=%lu parent=%d name=\"%s\" bind_inverse=",
            (unsigned int)i,
            (unsigned long)ir->joints[i].object_id,
            (int)ir->joints[i].parent_index,
            ir->joints[i].name);
        for (j = 0U; j < 16U; ++j) {
            if (j != 0U) {
                fprintf(fp, ",");
            }
            p3_p3m_put_fixed(fp, ir->joints[i].bind_inverse[j]);
        }
        fprintf(fp, "\n");
    }
    fprintf(fp, "submeshes %u\n", (unsigned int)ir->submesh_count);
    for (i = 0U; i < ir->submesh_count; ++i) {
        fprintf(fp, "submesh %u name=\"%s\" geom=%lu node=%lu material=%d texture=%d texture_stage=%lu texcoord=\"%s\" texture_count=%u first_vertex=%u vertex_count=%u first_index=%u index_count=%u\n",
            (unsigned int)i,
            ir->submeshes[i].name,
            (unsigned long)ir->submeshes[i].geom_object_id,
            (unsigned long)ir->submeshes[i].node_object_id,
            (int)ir->submeshes[i].material_index,
            (int)ir->submeshes[i].texture_index,
            (unsigned long)ir->submeshes[i].texture_stage_object_id,
            ir->submeshes[i].texcoord_name,
            (unsigned int)ir->submeshes[i].texture_count,
            (unsigned int)ir->submeshes[i].first_vertex,
            (unsigned int)ir->submeshes[i].vertex_count,
            (unsigned int)ir->submeshes[i].first_index,
            (unsigned int)ir->submeshes[i].index_count);
        if (ir->submeshes[i].has_color_attrib != 0U || ir->submeshes[i].has_color_scale != 0U || ir->submeshes[i].color_scale_off != 0U) {
            fprintf(fp, "submesh_state submesh=%u color_attrib=%u color_type=%u color=",
                (unsigned int)i,
                (unsigned int)ir->submeshes[i].has_color_attrib,
                (unsigned int)ir->submeshes[i].color_type);
            p3_p3m_put_fixed(fp, ir->submeshes[i].flat_color[0]); fprintf(fp, ",");
            p3_p3m_put_fixed(fp, ir->submeshes[i].flat_color[1]); fprintf(fp, ",");
            p3_p3m_put_fixed(fp, ir->submeshes[i].flat_color[2]); fprintf(fp, ",");
            p3_p3m_put_fixed(fp, ir->submeshes[i].flat_color[3]);
            fprintf(fp, " color_scale=%u off=%u scale=",
                (unsigned int)ir->submeshes[i].has_color_scale,
                (unsigned int)ir->submeshes[i].color_scale_off);
            p3_p3m_put_fixed(fp, ir->submeshes[i].color_scale[0]); fprintf(fp, ",");
            p3_p3m_put_fixed(fp, ir->submeshes[i].color_scale[1]); fprintf(fp, ",");
            p3_p3m_put_fixed(fp, ir->submeshes[i].color_scale[2]); fprintf(fp, ",");
            p3_p3m_put_fixed(fp, ir->submeshes[i].color_scale[3]);
            fprintf(fp, " scale_flags=%u,%u,%u\n",
                (unsigned int)ir->submeshes[i].color_scale_has_scale,
                (unsigned int)ir->submeshes[i].color_scale_has_rgb_scale,
                (unsigned int)ir->submeshes[i].color_scale_has_alpha_scale);
        }
        for (j = 0U; j < ir->submeshes[i].texture_count && j < P3_BAM_IR_MAX_SUBMESH_TEXTURES; ++j) {
            fprintf(fp, "submesh_texture submesh=%u slot=%u render_order=%u texture=%d stage=%lu texcoord=\"%s\" priority=%ld sort=%ld implicit=%ld override=%ld sampler=%u fixed_function=%u ff_texcoord_index=%d\n",
                (unsigned int)i,
                (unsigned int)j,
                (unsigned int)ir->submeshes[i].textures[j].render_order,
                (int)ir->submeshes[i].textures[j].texture_index,
                (unsigned long)ir->submeshes[i].textures[j].texture_stage_object_id,
                ir->submeshes[i].textures[j].texcoord_name,
                (long)ir->submeshes[i].textures[j].stage_priority,
                (long)ir->submeshes[i].textures[j].stage_sort,
                (long)ir->submeshes[i].textures[j].implicit_sort,
                (long)ir->submeshes[i].textures[j].override_value,
                (unsigned int)ir->submeshes[i].textures[j].has_sampler,
                (unsigned int)ir->submeshes[i].textures[j].is_fixed_function,
                (int)ir->submeshes[i].textures[j].fixed_function_texcoord_index);
            fprintf(fp, "submesh_texture_stage submesh=%u slot=%u name=\"%s\" mode=%u mode_name=\"%s\" fixed_function=%u ff_texcoord_index=%d color=",
                (unsigned int)i,
                (unsigned int)j,
                ir->submeshes[i].textures[j].stage_name,
                (unsigned int)ir->submeshes[i].textures[j].stage_mode,
                p3_p3m_stage_mode_name(ir->submeshes[i].textures[j].stage_mode),
                (unsigned int)ir->submeshes[i].textures[j].is_fixed_function,
                (int)ir->submeshes[i].textures[j].fixed_function_texcoord_index);
            p3_p3m_put_fixed(fp, ir->submeshes[i].textures[j].stage_color[0]); fprintf(fp, ",");
            p3_p3m_put_fixed(fp, ir->submeshes[i].textures[j].stage_color[1]); fprintf(fp, ",");
            p3_p3m_put_fixed(fp, ir->submeshes[i].textures[j].stage_color[2]); fprintf(fp, ",");
            p3_p3m_put_fixed(fp, ir->submeshes[i].textures[j].stage_color[3]);
            fprintf(fp, " rgb_scale=%u alpha_scale=%u saved_result=%u tex_view_offset=%ld flags=color_scale:%u,uses_color:%u,primary:%u,last_saved:%u\n",
                (unsigned int)ir->submeshes[i].textures[j].rgb_scale,
                (unsigned int)ir->submeshes[i].textures[j].alpha_scale,
                (unsigned int)ir->submeshes[i].textures[j].saved_result,
                (long)ir->submeshes[i].textures[j].tex_view_offset,
                (unsigned int)ir->submeshes[i].textures[j].involves_color_scale,
                (unsigned int)ir->submeshes[i].textures[j].uses_color,
                (unsigned int)ir->submeshes[i].textures[j].uses_primary_color,
                (unsigned int)ir->submeshes[i].textures[j].uses_last_saved_result);
            fprintf(fp, "submesh_texture_combine submesh=%u slot=%u rgb_mode=%u rgb_mode_name=\"%s\" rgb_operands=%u rgb_pack=%u rgb0=%u:%s/%u:%s rgb1=%u:%s/%u:%s rgb2=%u:%s/%u:%s alpha_mode=%u alpha_mode_name=\"%s\" alpha_operands=%u alpha_pack=%u alpha0=%u:%s/%u:%s alpha1=%u:%s/%u:%s alpha2=%u:%s/%u:%s\n",
                (unsigned int)i,
                (unsigned int)j,
                (unsigned int)ir->submeshes[i].textures[j].combine_rgb_mode,
                p3_p3m_combine_mode_name(ir->submeshes[i].textures[j].combine_rgb_mode),
                (unsigned int)ir->submeshes[i].textures[j].num_combine_rgb_operands,
                (unsigned int)ir->submeshes[i].textures[j].combine_rgb_pack,
                (unsigned int)ir->submeshes[i].textures[j].combine_rgb_source[0],
                p3_p3m_combine_source_name(ir->submeshes[i].textures[j].combine_rgb_source[0]),
                (unsigned int)ir->submeshes[i].textures[j].combine_rgb_operand[0],
                p3_p3m_combine_operand_name(ir->submeshes[i].textures[j].combine_rgb_operand[0]),
                (unsigned int)ir->submeshes[i].textures[j].combine_rgb_source[1],
                p3_p3m_combine_source_name(ir->submeshes[i].textures[j].combine_rgb_source[1]),
                (unsigned int)ir->submeshes[i].textures[j].combine_rgb_operand[1],
                p3_p3m_combine_operand_name(ir->submeshes[i].textures[j].combine_rgb_operand[1]),
                (unsigned int)ir->submeshes[i].textures[j].combine_rgb_source[2],
                p3_p3m_combine_source_name(ir->submeshes[i].textures[j].combine_rgb_source[2]),
                (unsigned int)ir->submeshes[i].textures[j].combine_rgb_operand[2],
                p3_p3m_combine_operand_name(ir->submeshes[i].textures[j].combine_rgb_operand[2]),
                (unsigned int)ir->submeshes[i].textures[j].combine_alpha_mode,
                p3_p3m_combine_mode_name(ir->submeshes[i].textures[j].combine_alpha_mode),
                (unsigned int)ir->submeshes[i].textures[j].num_combine_alpha_operands,
                (unsigned int)ir->submeshes[i].textures[j].combine_alpha_pack,
                (unsigned int)ir->submeshes[i].textures[j].combine_alpha_source[0],
                p3_p3m_combine_source_name(ir->submeshes[i].textures[j].combine_alpha_source[0]),
                (unsigned int)ir->submeshes[i].textures[j].combine_alpha_operand[0],
                p3_p3m_combine_operand_name(ir->submeshes[i].textures[j].combine_alpha_operand[0]),
                (unsigned int)ir->submeshes[i].textures[j].combine_alpha_source[1],
                p3_p3m_combine_source_name(ir->submeshes[i].textures[j].combine_alpha_source[1]),
                (unsigned int)ir->submeshes[i].textures[j].combine_alpha_operand[1],
                p3_p3m_combine_operand_name(ir->submeshes[i].textures[j].combine_alpha_operand[1]),
                (unsigned int)ir->submeshes[i].textures[j].combine_alpha_source[2],
                p3_p3m_combine_source_name(ir->submeshes[i].textures[j].combine_alpha_source[2]),
                (unsigned int)ir->submeshes[i].textures[j].combine_alpha_operand[2],
                p3_p3m_combine_operand_name(ir->submeshes[i].textures[j].combine_alpha_operand[2]));
            fprintf(fp, "submesh_texture_sampler submesh=%u slot=%u source=%u override=%u",
                (unsigned int)i,
                (unsigned int)j,
                (unsigned int)ir->submeshes[i].textures[j].sampler_source,
                (unsigned int)ir->submeshes[i].textures[j].has_sampler);
            p3_p3m_put_sampler(fp, &ir->submeshes[i].textures[j].sampler);
            fprintf(fp, "\n");
            if (ir->submeshes[i].textures[j].has_tex_matrix != 0U) {
                fprintf(fp, "submesh_texture_matrix submesh=%u slot=%u override=%ld mat=",
                    (unsigned int)i,
                    (unsigned int)j,
                    (long)ir->submeshes[i].textures[j].tex_matrix_override);
                p3_p3m_put_mat4(fp, ir->submeshes[i].textures[j].tex_matrix);
                fprintf(fp, "\n");
            }
            if (ir->submeshes[i].textures[j].has_tex_gen != 0U) {
                fprintf(fp, "submesh_texture_texgen submesh=%u slot=%u mode=%u mode_name=\"%s\"\n",
                    (unsigned int)i,
                    (unsigned int)j,
                    (unsigned int)ir->submeshes[i].textures[j].tex_gen_mode,
                    p3_p3m_tex_gen_mode_name(ir->submeshes[i].textures[j].tex_gen_mode));
            }
        }
    }
    fprintf(fp, "vertices %u\n", (unsigned int)ir->vertex_count);
    for (i = 0U; i < ir->vertex_count; ++i) {
        fprintf(fp, "v %u p=", (unsigned int)i);
        p3_p3m_put_fixed(fp, ir->vertices[i].position[0]); fprintf(fp, ",");
        p3_p3m_put_fixed(fp, ir->vertices[i].position[1]); fprintf(fp, ",");
        p3_p3m_put_fixed(fp, ir->vertices[i].position[2]);
        fprintf(fp, " n=");
        p3_p3m_put_fixed(fp, ir->vertices[i].normal[0]); fprintf(fp, ",");
        p3_p3m_put_fixed(fp, ir->vertices[i].normal[1]); fprintf(fp, ",");
        p3_p3m_put_fixed(fp, ir->vertices[i].normal[2]);
        fprintf(fp, " uv=");
        p3_p3m_put_fixed(fp, ir->vertices[i].uv[0]); fprintf(fp, ",");
        p3_p3m_put_fixed(fp, ir->vertices[i].uv[1]);
        if (ir->vertices[i].has_color != 0U) {
            fprintf(fp, " color=");
            p3_p3m_put_fixed(fp, ir->vertices[i].color[0]); fprintf(fp, ",");
            p3_p3m_put_fixed(fp, ir->vertices[i].color[1]); fprintf(fp, ",");
            p3_p3m_put_fixed(fp, ir->vertices[i].color[2]); fprintf(fp, ",");
            p3_p3m_put_fixed(fp, ir->vertices[i].color[3]);
        }
        if (ir->vertices[i].has_tangent != 0U) {
            fprintf(fp, " tangent=");
            p3_p3m_put_fixed(fp, ir->vertices[i].tangent[0]); fprintf(fp, ",");
            p3_p3m_put_fixed(fp, ir->vertices[i].tangent[1]); fprintf(fp, ",");
            p3_p3m_put_fixed(fp, ir->vertices[i].tangent[2]); fprintf(fp, ",");
            p3_p3m_put_fixed(fp, ir->vertices[i].tangent[3]);
        }
        if (ir->vertices[i].has_binormal != 0U) {
            fprintf(fp, " binormal=");
            p3_p3m_put_fixed(fp, ir->vertices[i].binormal[0]); fprintf(fp, ",");
            p3_p3m_put_fixed(fp, ir->vertices[i].binormal[1]); fprintf(fp, ",");
            p3_p3m_put_fixed(fp, ir->vertices[i].binormal[2]);
        }
        fprintf(fp, " joints=%u", (unsigned int)ir->vertices[i].joint_count);
        for (j = 0U; j < ir->vertices[i].joint_count && j < 4U; ++j) {
            fprintf(fp, " [%u:", (unsigned int)ir->vertices[i].joint_index[j]);
            p3_p3m_put_fixed(fp, ir->vertices[i].joint_weight[j]);
            fprintf(fp, "]");
        }
        fprintf(fp, "\n");
    }
    fprintf(fp, "indices %u\n", (unsigned int)ir->index_count);
    for (i = 0U; i < ir->index_count; i += 3U) {
        fprintf(fp, "tri %u", (unsigned int)(i / 3U));
        for (j = 0U; j < 3U && (p3_u32)i + (p3_u32)j < (p3_u32)ir->index_count; ++j) {
            fprintf(fp, " %u", (unsigned int)ir->indices[(p3_u16)(i + j)]);
        }
        fprintf(fp, "\n");
    }
    fclose(fp);
    return P3_OK;
}
