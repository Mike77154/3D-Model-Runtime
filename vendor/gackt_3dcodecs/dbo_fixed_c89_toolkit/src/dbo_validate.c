
#include "dbo_int.h"

static void dbo__copy_text(char* dst, const char* src)
{
    if (dst == 0)
    {
        return;
    }
    if (src == 0)
    {
        dst[0] = '\0';
        return;
    }
    strncpy(dst, src, DBO_MAX_ERROR_TEXT - 1);
    dst[DBO_MAX_ERROR_TEXT - 1] = '\0';
}

void dbo_validation_report_reset(dbo_validation_report* report)
{
    if (report == 0)
    {
        return;
    }
    report->errors = 0UL;
    report->warnings = 0UL;
    report->first_error[0] = '\0';
    report->first_warning[0] = '\0';
}

static void dbo__report_error(dbo_validation_report* report, const char* text)
{
    if (report == 0)
    {
        return;
    }
    if (report->errors == 0UL)
    {
        dbo__copy_text(report->first_error, text);
    }
    report->errors += 1UL;
}

static void dbo__report_warning(dbo_validation_report* report, const char* text)
{
    if (report == 0)
    {
        return;
    }
    if (report->warnings == 0UL)
    {
        dbo__copy_text(report->first_warning, text);
    }
    report->warnings += 1UL;
}

static dbo_bool dbo__blob_ref_valid(const dbo_scene* scene, dbo_blob_ref ref)
{
    if (scene == 0)
    {
        return DBO_FALSE;
    }
    if (ref.offset == DBO_INDEX_NONE)
    {
        return ref.size == 0UL ? DBO_TRUE : DBO_FALSE;
    }
    if (ref.offset > scene->blob_used)
    {
        return DBO_FALSE;
    }
    if (ref.size > scene->blob_used - ref.offset)
    {
        return DBO_FALSE;
    }
    return DBO_TRUE;
}

static void dbo__validate_frame_links(const dbo_scene* scene, dbo_validation_report* report)
{
    dbo_u32 i;
    for (i = 0UL; i < scene->frame_count; ++i)
    {
        const dbo_frame* f = &scene->frames[i];
        if (!dbo__blob_ref_valid(scene, f->name))
        {
            dbo__report_error(report, "frame name blob is out of range");
        }
        if (f->parent != DBO_INDEX_NONE && f->parent >= scene->frame_count)
        {
            dbo__report_error(report, "frame parent index is out of range");
        }
        if (f->child != DBO_INDEX_NONE)
        {
            if (f->child >= scene->frame_count)
            {
                dbo__report_error(report, "frame child index is out of range");
            }
            else if (f->child == i)
            {
                dbo__report_error(report, "frame child points to itself");
            }
            else if (scene->frames[f->child].parent != i)
            {
                dbo__report_warning(report, "frame child parent link is inconsistent");
            }
        }
        if (f->sibling != DBO_INDEX_NONE)
        {
            if (f->sibling >= scene->frame_count)
            {
                dbo__report_error(report, "frame sibling index is out of range");
            }
            else if (f->sibling == i)
            {
                dbo__report_error(report, "frame sibling points to itself");
            }
            else if (scene->frames[f->sibling].parent != f->parent)
            {
                dbo__report_warning(report, "frame sibling parent link is inconsistent");
            }
        }
        if (f->mesh != DBO_INDEX_NONE && f->mesh >= scene->mesh_count)
        {
            dbo__report_error(report, "frame mesh index is out of range");
        }
    }
}

static void dbo__validate_meshes(const dbo_scene* scene, dbo_validation_report* report)
{
    dbo_u32 i;
    for (i = 0UL; i < scene->mesh_count; ++i)
    {
        const dbo_mesh* m = &scene->meshes[i];
        dbo_u32 expected_stride;
        if (!dbo__blob_ref_valid(scene, m->vertex_decl))
        {
            dbo__report_error(report, "mesh vertex declaration blob is out of range");
        }
        if (!dbo__blob_ref_valid(scene, m->vertex_data))
        {
            dbo__report_error(report, "mesh vertex blob is out of range");
        }
        if (!dbo__blob_ref_valid(scene, m->index_data))
        {
            dbo__report_error(report, "mesh index blob is out of range");
        }
        if (!dbo__blob_ref_valid(scene, m->subframe_indices))
        {
            dbo__report_error(report, "mesh subframe blob is out of range");
        }
        if (!dbo__blob_ref_valid(scene, m->effect_name))
        {
            dbo__report_error(report, "mesh effect name blob is out of range");
        }

        expected_stride = dbo_fvf_vertex_stride(m->fvf);
        if (m->fvf != 0UL && expected_stride == 0UL)
        {
            dbo__report_warning(report, "mesh FVF is set but stride could not be inferred");
        }
        if (m->fvf != 0UL && m->fvf_size != 0UL && expected_stride != 0UL && m->fvf_size != expected_stride)
        {
            dbo__report_warning(report, "mesh fvf_size does not match inferred FVF stride");
        }
        if (m->vertex_count > 0UL)
        {
            dbo_u32 stride = m->fvf_size != 0UL ? m->fvf_size : expected_stride;
            if (stride == 0UL)
            {
                dbo__report_error(report, "mesh has vertices but no usable stride");
            }
            else if (m->vertex_data.size < m->vertex_count * stride)
            {
                dbo__report_error(report, "mesh vertex blob is smaller than vertex_count * stride");
            }
        }
        if (m->index_count > 0UL && m->index_data.size < m->index_count * 2UL)
        {
            dbo__report_error(report, "mesh index blob is smaller than index_count * 2");
        }

        if (m->first_texture != DBO_INDEX_NONE)
        {
            if (m->first_texture >= scene->texture_count || m->texture_count > scene->texture_count - m->first_texture)
            {
                dbo__report_error(report, "mesh texture range is out of range");
            }
        }
        else if (m->texture_count != 0UL)
        {
            dbo__report_error(report, "mesh texture_count is non-zero but first_texture is NONE");
        }

        if (m->first_bone != DBO_INDEX_NONE)
        {
            if (m->first_bone >= scene->bone_count || m->bone_count > scene->bone_count - m->first_bone)
            {
                dbo__report_error(report, "mesh bone range is out of range");
            }
        }
        else if (m->bone_count != 0UL)
        {
            dbo__report_error(report, "mesh bone_count is non-zero but first_bone is NONE");
        }

        if (m->first_multimaterial != DBO_INDEX_NONE)
        {
            if (m->first_multimaterial >= scene->multimaterial_count || m->multimaterial_count > scene->multimaterial_count - m->first_multimaterial)
            {
                dbo__report_error(report, "mesh multimaterial range is out of range");
            }
        }
        else if (m->multimaterial_count != 0UL)
        {
            dbo__report_error(report, "mesh multimaterial_count is non-zero but first_multimaterial is NONE");
        }

        if (m->subframe_count > 0UL)
        {
            const dbo_u32* subframes;
            dbo_u32 k;
            if (m->subframe_indices.size < m->subframe_count * 4UL)
            {
                dbo__report_error(report, "mesh subframe blob is smaller than subframe_count * 4");
            }
            subframes = (const dbo_u32*)dbo_blob_data(scene, m->subframe_indices);
            if (subframes != 0)
            {
                for (k = 0UL; k < m->subframe_count; ++k)
                {
                    if (subframes[k] >= scene->frame_count)
                    {
                        dbo__report_error(report, "mesh subframe index is out of range");
                        break;
                    }
                }
            }
        }
    }
}

static void dbo__validate_textures(const dbo_scene* scene, dbo_validation_report* report)
{
    dbo_u32 i;
    for (i = 0UL; i < scene->texture_count; ++i)
    {
        if (!dbo__blob_ref_valid(scene, scene->textures[i].name))
        {
            dbo__report_error(report, "texture name blob is out of range");
        }
    }
}

static void dbo__validate_bones(const dbo_scene* scene, dbo_validation_report* report)
{
    dbo_u32 i;
    for (i = 0UL; i < scene->bone_count; ++i)
    {
        const dbo_bone* b = &scene->bones[i];
        if (!dbo__blob_ref_valid(scene, b->name))
        {
            dbo__report_error(report, "bone name blob is out of range");
        }
        if (!dbo__blob_ref_valid(scene, b->vertices))
        {
            dbo__report_error(report, "bone vertices blob is out of range");
        }
        if (!dbo__blob_ref_valid(scene, b->weights))
        {
            dbo__report_error(report, "bone weights blob is out of range");
        }
        if (b->num_influences > 0UL)
        {
            if (b->vertices.size < b->num_influences * 4UL)
            {
                dbo__report_error(report, "bone vertices blob is smaller than num_influences * 4");
            }
            if (b->weights.size < b->num_influences * 4UL)
            {
                dbo__report_error(report, "bone weights blob is smaller than num_influences * 4");
            }
        }
    }
}

static void dbo__validate_multimaterials(const dbo_scene* scene, dbo_validation_report* report)
{
    dbo_u32 i;
    for (i = 0UL; i < scene->multimaterial_count; ++i)
    {
        if (!dbo__blob_ref_valid(scene, scene->multimaterials[i].name))
        {
            dbo__report_error(report, "multimaterial name blob is out of range");
        }
    }
}

static void dbo__validate_animations(const dbo_scene* scene, dbo_validation_report* report)
{
    dbo_u32 i;
    for (i = 0UL; i < scene->animation_count; ++i)
    {
        const dbo_animation* a = &scene->animations[i];
        if (!dbo__blob_ref_valid(scene, a->name))
        {
            dbo__report_error(report, "animation name blob is out of range");
        }
        if (!dbo__blob_ref_valid(scene, a->position_keys))
        {
            dbo__report_error(report, "animation position key blob is out of range");
        }
        if (!dbo__blob_ref_valid(scene, a->rotate_keys))
        {
            dbo__report_error(report, "animation rotation key blob is out of range");
        }
        if (!dbo__blob_ref_valid(scene, a->scale_keys))
        {
            dbo__report_error(report, "animation scale key blob is out of range");
        }
        if (!dbo__blob_ref_valid(scene, a->matrix_keys))
        {
            dbo__report_error(report, "animation matrix key blob is out of range");
        }
        if (!dbo__blob_ref_valid(scene, a->bone_offsets))
        {
            dbo__report_error(report, "animation bone offset blob is out of range");
        }
        if (!dbo__blob_ref_valid(scene, a->bone_matrices))
        {
            dbo__report_error(report, "animation bone matrix blob is out of range");
        }

        if (a->frame_index != DBO_INDEX_NONE && a->frame_index >= scene->frame_count)
        {
            dbo__report_error(report, "animation frame index is out of range");
        }
        if (a->next_animation != DBO_INDEX_NONE && a->next_animation >= scene->animation_count)
        {
            dbo__report_error(report, "animation next link is out of range");
        }
        if (a->position_keys.size < a->num_position_keys * (dbo_u32)sizeof(dbo_position_key))
        {
            dbo__report_error(report, "animation position keys blob is too small");
        }
        if (a->rotate_keys.size < a->num_rotate_keys * (dbo_u32)sizeof(dbo_rotate_key))
        {
            dbo__report_error(report, "animation rotation keys blob is too small");
        }
        if (a->scale_keys.size < a->num_scale_keys * (dbo_u32)sizeof(dbo_scale_key))
        {
            dbo__report_error(report, "animation scale keys blob is too small");
        }
        if (a->matrix_keys.size < a->num_matrix_keys * (dbo_u32)sizeof(dbo_matrix_key))
        {
            dbo__report_error(report, "animation matrix keys blob is too small");
        }
        if (a->bone_offset_count > 0UL && a->bone_offsets.size < a->bone_offset_count * 16UL * 4UL)
        {
            dbo__report_error(report, "animation bone offset blob is too small");
        }
    }
}

static void dbo__validate_animation_sets(const dbo_scene* scene, dbo_validation_report* report)
{
    dbo_u32 i;
    for (i = 0UL; i < scene->animation_set_count; ++i)
    {
        const dbo_animation_set* s = &scene->animation_sets[i];
        if (!dbo__blob_ref_valid(scene, s->name))
        {
            dbo__report_error(report, "animation set name blob is out of range");
        }
        if (s->first_animation != DBO_INDEX_NONE && s->first_animation >= scene->animation_count)
        {
            dbo__report_error(report, "animation set first animation index is out of range");
        }
    }
}

dbo_bool dbo_validate_scene(const dbo_scene* scene, dbo_validation_report* report)
{
    if (report != 0)
    {
        dbo_validation_report_reset(report);
    }
    if (scene == 0)
    {
        if (report != 0)
        {
            dbo__report_error(report, "scene is null");
        }
        return DBO_FALSE;
    }

    if (!dbo__blob_ref_valid(scene, scene->custom_data))
    {
        dbo__report_error(report, "custom data blob is out of range");
    }
    if (scene->root_frame != DBO_INDEX_NONE && scene->root_frame >= scene->frame_count)
    {
        dbo__report_error(report, "root_frame is out of range");
    }
    if (scene->root_animation_set != DBO_INDEX_NONE && scene->root_animation_set >= scene->animation_set_count)
    {
        dbo__report_error(report, "root_animation_set is out of range");
    }
    if (scene->frame_count == 0UL)
    {
        dbo__report_warning(report, "scene has no frames");
    }
    if (scene->mesh_count == 0UL)
    {
        dbo__report_warning(report, "scene has no meshes");
    }
    if (scene->version != 0UL && scene->version != DBO_VERSION_NUMBER)
    {
        dbo__report_warning(report, "scene version differs from the common DBPro 1062 version");
    }

    dbo__validate_frame_links(scene, report);
    dbo__validate_meshes(scene, report);
    dbo__validate_textures(scene, report);
    dbo__validate_bones(scene, report);
    dbo__validate_multimaterials(scene, report);
    dbo__validate_animations(scene, report);
    dbo__validate_animation_sets(scene, report);

    return report == 0 || report->errors == 0UL ? DBO_TRUE : DBO_FALSE;
}
