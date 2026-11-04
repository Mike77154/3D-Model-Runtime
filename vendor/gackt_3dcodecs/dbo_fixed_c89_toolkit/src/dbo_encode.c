#include "dbo_int.h"

static const dbo_u32 g_scene_codes[] = {
    DBOBLOCK_ROOT_FRAME,
    DBOBLOCK_ROOT_ANIMATIONSET,
    DBOBLOCK_OBJECT_CUSTOMDATA
};

static const dbo_u32 g_frame_codes[] = {
    DBOBLOCK_FRAME_NAME,
    DBOBLOCK_FRAME_MATRIX,
    DBOBLOCK_FRAME_MESH,
    DBOBLOCK_FRAME_CHILD,
    DBOBLOCK_FRAME_SIBLING,
    DBOBLOCK_FRAME_OFFSET,
    DBOBLOCK_FRAME_ROTATION,
    DBOBLOCK_FRAME_SCALE
};

static const dbo_u32 g_mesh_codes[] = {
    DBOBLOCK_MESH_FVF,
    DBOBLOCK_MESH_FVFSIZE,
    DBOBLOCK_MESH_VERTEXCOUNT,
    DBOBLOCK_MESH_INDEXCOUNT,
    DBOBLOCK_MESH_VERTEXDEC,
    DBOBLOCK_MESH_SUBFRAMES,
    DBOBLOCK_MESH_VERTEXDATA,
    DBOBLOCK_MESH_INDEXDATA,
    DBOBLOCK_MESH_PRIMTYPE,
    DBOBLOCK_MESH_DRAWVERTCOUNT,
    DBOBLOCK_MESH_DRAWPRIMCOUNT,
    DBOBLOCK_MESH_BONECOUNT,
    DBOBLOCK_MESH_BONESDATA,
    DBOBLOCK_MESH_USEMATERIAL,
    DBOBLOCK_MESH_MATERIAL,
    DBOBLOCK_MESH_TEXTURECOUNT,
    DBOBLOCK_MESH_TEXTURES,
    DBOBLOCK_MESH_USEMULTIMAT,
    DBOBLOCK_MESH_MULTIMATCOUNT,
    DBOBLOCK_MESH_MULTIMAT,
    DBOBLOCK_MESH_WIREFRAME,
    DBOBLOCK_MESH_LIGHT,
    DBOBLOCK_MESH_CULL,
    DBOBLOCK_MESH_FOG,
    DBOBLOCK_MESH_AMBIENT,
    DBOBLOCK_MESH_TRANSPARENCY,
    DBOBLOCK_MESH_GHOST,
    DBOBLOCK_MESH_GHOSTMODE,
    DBOBLOCK_MESH_VISIBLE,
    DBOBLOCK_MESH_LINKED,
    DBOBLOCK_MESH_FXEFFECTNAME,
    DBOBLOCK_MESH_ARBITARYVALUE,
    DBOBLOCK_MESH_ZBIASFLAG,
    DBOBLOCK_MESH_ZBIASSLOPE,
    DBOBLOCK_MESH_ZBIASDEPTH,
    DBOBLOCK_MESH_ZREAD,
    DBOBLOCK_MESH_ZWRITE,
    DBOBLOCK_MESH_ALPHATESTVALUE
};

static const dbo_u32 g_texture_codes[] = {
    DBOBLOCK_TEXTURE_NAME,
    DBOBLOCK_TEXTURE_STAGE,
    DBOBLOCK_TEXTURE_BMODE,
    DBOBLOCK_TEXTURE_BARG1,
    DBOBLOCK_TEXTURE_BARG2,
    DBOBLOCK_TEXTURE_ADDRU,
    DBOBLOCK_TEXTURE_ADDRV,
    DBOBLOCK_TEXTURE_MAG,
    DBOBLOCK_TEXTURE_MIN,
    DBOBLOCK_TEXTURE_MIP,
    DBOBLOCK_TEXTURE_TCMODE,
    DBOBLOCK_TEXTURE_PRIMST,
    DBOBLOCK_TEXTURE_PRIMCN
};

static const dbo_u32 g_bone_codes[] = {
    DBOBLOCK_BONES_NAME,
    DBOBLOCK_BONES_NUMINFLUENCES,
    DBOBLOCK_BONES_VERTICES,
    DBOBLOCK_BONES_WEIGHTS,
    DBOBLOCK_BONES_TRANSLATIONMATRIX
};

static const dbo_u32 g_multimat_codes[] = {
    DBOBLOCK_MULTIMAT_NAME,
    DBOBLOCK_MULTIMAT_MATERIAL,
    DBOBLOCK_MULTIMAT_START,
    DBOBLOCK_MULTIMAT_COUNT,
    DBOBLOCK_MULTIMAT_POLY
};

static const dbo_u32 g_animation_codes[] = {
    DBOBLOCK_ANIM_NAME,
    DBOBLOCK_ANIM_NUMPOSKEYS,
    DBOBLOCK_ANIM_POSDATA,
    DBOBLOCK_ANIM_NUMROTKEYS,
    DBOBLOCK_ANIM_ROTDATA,
    DBOBLOCK_ANIM_NUMSCALEKEYS,
    DBOBLOCK_ANIM_SCALEDATA,
    DBOBLOCK_ANIM_NUMMATRIXKEYS,
    DBOBLOCK_ANIM_MATRIXDATA,
    DBOBLOCK_FRAME_BONETYPE,
    DBOBLOCK_FRAME_BONEOFFSETLISTSIZE,
    DBOBLOCK_FRAME_BONEOFFSETLIST,
    DBOBLOCK_FRAME_BONEMATRIXSIZEA,
    DBOBLOCK_FRAME_BONEMATRIXSIZEB,
    DBOBLOCK_FRAME_BONEMATRIXLIST,
    DBOBLOCK_ANIM_NEXT
};

static const dbo_u32 g_animation_set_codes[] = {
    DBOBLOCK_ANIMSET_NAME,
    DBOBLOCK_ANIMSET_LENGTH,
    DBOBLOCK_ANIMSET_DATA
};

static dbo_bool dbo__write_chunk_header(dbo_writer* w, dbo_u32 code, dbo_u32 size)
{
    if (!dbo__write_u32(w, code)) return DBO_FALSE;
    if (!dbo__write_u32(w, size)) return DBO_FALSE;
    return DBO_TRUE;
}

static dbo_bool dbo__write_string_chunk(dbo_writer* w, dbo_u32 code, dbo_blob_ref ref)
{
    dbo_u32 size_pos;
    if (!dbo__begin_chunk(w, code, &size_pos)) return DBO_FALSE;
    if (!dbo__write_string_ref(w, w->scene, ref)) return DBO_FALSE;
    if (!dbo__end_chunk(w, size_pos)) return DBO_FALSE;
    return DBO_TRUE;
}

static dbo_bool dbo__write_vec_chunk(dbo_writer* w, dbo_u32 code, const dbo_fx32 v[3])
{
    dbo_u32 size_pos;
    if (!dbo__begin_chunk(w, code, &size_pos)) return DBO_FALSE;
    if (!dbo__write_vec3_fx(w, v)) return DBO_FALSE;
    if (!dbo__end_chunk(w, size_pos)) return DBO_FALSE;
    return DBO_TRUE;
}

static dbo_bool dbo__write_mat_chunk(dbo_writer* w, dbo_u32 code, const dbo_fx32 m[16])
{
    dbo_u32 size_pos;
    if (!dbo__begin_chunk(w, code, &size_pos)) return DBO_FALSE;
    if (!dbo__write_mat4_fx(w, m)) return DBO_FALSE;
    if (!dbo__end_chunk(w, size_pos)) return DBO_FALSE;
    return DBO_TRUE;
}

static dbo_bool dbo__write_material_chunk(dbo_writer* w, dbo_u32 code, const dbo_material* mat)
{
    if (!dbo__write_chunk_header(w, code, 68UL)) return DBO_FALSE;
    if (!dbo__write_material_fx(w, mat)) return DBO_FALSE;
    return DBO_TRUE;
}

static dbo_bool dbo__write_bool_chunk(dbo_writer* w, dbo_u32 code, dbo_bool value)
{
    if (!dbo__write_chunk_header(w, code, 1UL)) return DBO_FALSE;
    if (!dbo__write_u8(w, value ? 1U : 0U)) return DBO_FALSE;
    return DBO_TRUE;
}

static dbo_bool dbo__write_dword_chunk(dbo_writer* w, dbo_u32 code, dbo_u32 value)
{
    if (!dbo__write_chunk_header(w, code, 4UL)) return DBO_FALSE;
    if (!dbo__write_u32(w, value)) return DBO_FALSE;
    return DBO_TRUE;
}

static dbo_bool dbo__scope_has_events(const dbo_scene* scene, dbo_u32 scope_kind, dbo_u32 owner_index)
{
    dbo_u32 i;
    if (scene == 0)
    {
        return DBO_FALSE;
    }
    for (i = 0UL; i < scene->chunk_event_count; ++i)
    {
        const dbo_chunk_event* event = &scene->chunk_events[i];
        if (event->scope_kind == scope_kind && event->owner_index == owner_index)
        {
            return DBO_TRUE;
        }
    }
    return DBO_FALSE;
}

static dbo_bool dbo__scope_has_event_code(const dbo_scene* scene, dbo_u32 scope_kind, dbo_u32 owner_index, dbo_u32 code)
{
    dbo_u32 i;
    if (scene == 0)
    {
        return DBO_FALSE;
    }
    for (i = 0UL; i < scene->chunk_event_count; ++i)
    {
        const dbo_chunk_event* event = &scene->chunk_events[i];
        if (event->scope_kind == scope_kind && event->owner_index == owner_index && event->code == code)
        {
            return DBO_TRUE;
        }
    }
    return DBO_FALSE;
}

static dbo_bool dbo__write_unknown_event(dbo_writer* w, const dbo_chunk_event* event)
{
    const dbo_unknown_chunk* chunk;
    const void* data;

    if (w == 0 || w->scene == 0 || event == 0)
    {
        return DBO_FALSE;
    }
    if (event->aux_index == DBO_INDEX_NONE || event->aux_index >= w->scene->unknown_chunk_count)
    {
        dbo__set_error((dbo_scene*)w->scene, "dangling unknown chunk event");
        return DBO_FALSE;
    }

    chunk = &w->scene->unknown_chunks[event->aux_index];
    if (chunk->scope_kind != event->scope_kind || chunk->owner_index != event->owner_index || chunk->code != event->code)
    {
        dbo__set_error((dbo_scene*)w->scene, "unknown chunk event mismatch");
        return DBO_FALSE;
    }

    data = dbo_blob_data(w->scene, chunk->payload);
    if (!dbo__write_u32(w, chunk->code)) return DBO_FALSE;
    if (!dbo__write_u32(w, chunk->payload.size)) return DBO_FALSE;
    if (!dbo__write_bytes(w, data, chunk->payload.size)) return DBO_FALSE;
    return DBO_TRUE;
}

static dbo_bool dbo__encode_frame(dbo_writer* w, dbo_u32 frame_index);
static dbo_bool dbo__encode_animation(dbo_writer* w, dbo_u32 anim_index);
static dbo_bool dbo__encode_animation_set(dbo_writer* w, dbo_u32 set_index);
static dbo_bool dbo__encode_bone(dbo_writer* w, dbo_u32 bone_index);
static dbo_bool dbo__encode_texture(dbo_writer* w, dbo_u32 tex_index);
static dbo_bool dbo__encode_multimaterial(dbo_writer* w, dbo_u32 mm_index);

static dbo_bool dbo__encode_position_keys(dbo_writer* w, const dbo_animation* anim, dbo_bool force)
{
    const dbo_position_key* keys;
    dbo_u32 i;
    dbo_u32 j;
    dbo_u32 size;

    if (anim->num_position_keys == 0UL && !force)
    {
        return DBO_TRUE;
    }

    size = anim->num_position_keys * 28UL;
    keys = (const dbo_position_key*)dbo_blob_data(w->scene, anim->position_keys);
    if (!dbo__write_chunk_header(w, DBOBLOCK_ANIM_POSDATA, size)) return DBO_FALSE;
    for (i = 0UL; i < anim->num_position_keys; ++i)
    {
        if (!dbo__write_u32(w, keys[i].time)) return DBO_FALSE;
        for (j = 0UL; j < 3UL; ++j)
        {
            if (!dbo__write_u32(w, dbo_fx_to_float_bits(keys[i].pos[j]))) return DBO_FALSE;
        }
        for (j = 0UL; j < 3UL; ++j)
        {
            if (!dbo__write_u32(w, dbo_fx_to_float_bits(keys[i].interp[j]))) return DBO_FALSE;
        }
    }
    return DBO_TRUE;
}

static dbo_bool dbo__encode_rotate_keys(dbo_writer* w, const dbo_animation* anim, dbo_bool force)
{
    const dbo_rotate_key* keys;
    dbo_u32 i;
    dbo_u32 j;
    dbo_u32 size;

    if (anim->num_rotate_keys == 0UL && !force)
    {
        return DBO_TRUE;
    }

    size = anim->num_rotate_keys * 20UL;
    keys = (const dbo_rotate_key*)dbo_blob_data(w->scene, anim->rotate_keys);
    if (!dbo__write_chunk_header(w, DBOBLOCK_ANIM_ROTDATA, size)) return DBO_FALSE;
    for (i = 0UL; i < anim->num_rotate_keys; ++i)
    {
        if (!dbo__write_u32(w, keys[i].time)) return DBO_FALSE;
        for (j = 0UL; j < 4UL; ++j)
        {
            if (!dbo__write_u32(w, dbo_fx_to_float_bits(keys[i].quat[j]))) return DBO_FALSE;
        }
    }
    return DBO_TRUE;
}

static dbo_bool dbo__encode_scale_keys(dbo_writer* w, const dbo_animation* anim, dbo_bool force)
{
    const dbo_scale_key* keys;
    dbo_u32 i;
    dbo_u32 j;
    dbo_u32 size;

    if (anim->num_scale_keys == 0UL && !force)
    {
        return DBO_TRUE;
    }

    size = anim->num_scale_keys * 28UL;
    keys = (const dbo_scale_key*)dbo_blob_data(w->scene, anim->scale_keys);
    if (!dbo__write_chunk_header(w, DBOBLOCK_ANIM_SCALEDATA, size)) return DBO_FALSE;
    for (i = 0UL; i < anim->num_scale_keys; ++i)
    {
        if (!dbo__write_u32(w, keys[i].time)) return DBO_FALSE;
        for (j = 0UL; j < 3UL; ++j)
        {
            if (!dbo__write_u32(w, dbo_fx_to_float_bits(keys[i].scale[j]))) return DBO_FALSE;
        }
        for (j = 0UL; j < 3UL; ++j)
        {
            if (!dbo__write_u32(w, dbo_fx_to_float_bits(keys[i].interp[j]))) return DBO_FALSE;
        }
    }
    return DBO_TRUE;
}

static dbo_bool dbo__encode_matrix_keys(dbo_writer* w, const dbo_animation* anim, dbo_bool force)
{
    const dbo_matrix_key* keys;
    dbo_u32 i;
    dbo_u32 j;
    dbo_u32 size;

    if (anim->num_matrix_keys == 0UL && !force)
    {
        return DBO_TRUE;
    }

    size = anim->num_matrix_keys * 132UL;
    keys = (const dbo_matrix_key*)dbo_blob_data(w->scene, anim->matrix_keys);
    if (!dbo__write_chunk_header(w, DBOBLOCK_ANIM_MATRIXDATA, size)) return DBO_FALSE;
    for (i = 0UL; i < anim->num_matrix_keys; ++i)
    {
        if (!dbo__write_u32(w, keys[i].time)) return DBO_FALSE;
        for (j = 0UL; j < 16UL; ++j)
        {
            if (!dbo__write_u32(w, dbo_fx_to_float_bits(keys[i].matrix[j]))) return DBO_FALSE;
        }
        for (j = 0UL; j < 16UL; ++j)
        {
            if (!dbo__write_u32(w, dbo_fx_to_float_bits(keys[i].interp[j]))) return DBO_FALSE;
        }
    }
    return DBO_TRUE;
}

static dbo_bool dbo__encode_bone_matrices(dbo_writer* w, const dbo_animation* anim)
{
    const dbo_fx32* mats;
    dbo_u32 total;
    dbo_u32 i;

    if (anim->bone_frame_a < 0 || anim->bone_frame_b < 0)
    {
        return DBO_FALSE;
    }

    mats = (const dbo_fx32*)dbo_blob_data(w->scene, anim->bone_matrices);
    total = (dbo_u32)anim->bone_frame_a * (dbo_u32)anim->bone_frame_b * 16UL;
    for (i = 0UL; i < total; ++i)
    {
        if (!dbo__write_u32(w, dbo_fx_to_float_bits(mats[i]))) return DBO_FALSE;
    }
    return DBO_TRUE;
}

static dbo_bool dbo__emit_bone_chunk(dbo_writer* w, dbo_u32 bone_index, dbo_u32 code, dbo_bool force)
{
    const dbo_bone* bone;
    const dbo_u32* vertices;
    const dbo_fx32* weights;
    dbo_u32 j;

    (void)force;
    bone = &w->scene->bones[bone_index];
    vertices = (const dbo_u32*)dbo_blob_data(w->scene, bone->vertices);
    weights = (const dbo_fx32*)dbo_blob_data(w->scene, bone->weights);

    switch (code)
    {
    case DBOBLOCK_BONES_NAME:
        return dbo__write_string_chunk(w, DBOBLOCK_BONES_NAME, bone->name);
    case DBOBLOCK_BONES_NUMINFLUENCES:
        return dbo__write_dword_chunk(w, DBOBLOCK_BONES_NUMINFLUENCES, bone->num_influences);
    case DBOBLOCK_BONES_VERTICES:
        if (!dbo__write_chunk_header(w, DBOBLOCK_BONES_VERTICES, bone->num_influences * 4UL)) return DBO_FALSE;
        return dbo__write_bytes(w, vertices, bone->num_influences * 4UL);
    case DBOBLOCK_BONES_WEIGHTS:
        if (!dbo__write_chunk_header(w, DBOBLOCK_BONES_WEIGHTS, bone->num_influences * 4UL)) return DBO_FALSE;
        for (j = 0UL; j < bone->num_influences; ++j)
        {
            if (!dbo__write_u32(w, dbo_fx_to_float_bits(weights[j]))) return DBO_FALSE;
        }
        return DBO_TRUE;
    case DBOBLOCK_BONES_TRANSLATIONMATRIX:
        return dbo__write_mat_chunk(w, DBOBLOCK_BONES_TRANSLATIONMATRIX, bone->translation);
    default:
        dbo__set_error((dbo_scene*)w->scene, "unknown bone chunk code");
        return DBO_FALSE;
    }
}

static dbo_bool dbo__encode_bone_canonical(dbo_writer* w, dbo_u32 bone_index)
{
    dbo_u32 i;
    for (i = 0UL; i < (dbo_u32)(sizeof(g_bone_codes) / sizeof(g_bone_codes[0])); ++i)
    {
        if (!dbo__emit_bone_chunk(w, bone_index, g_bone_codes[i], DBO_FALSE)) return DBO_FALSE;
    }
    if (!dbo__write_unknown_chunks_for_scope(w, DBO_SCOPE_BONE, bone_index)) return DBO_FALSE;
    if (!dbo__write_terminator(w)) return DBO_FALSE;
    return DBO_TRUE;
}

static dbo_bool dbo__encode_bone_ordered(dbo_writer* w, dbo_u32 bone_index)
{
    const dbo_scene* scene;
    dbo_u32 i;

    scene = w->scene;
    for (i = 0UL; i < scene->chunk_event_count; ++i)
    {
        const dbo_chunk_event* event = &scene->chunk_events[i];
        if (event->scope_kind != DBO_SCOPE_BONE || event->owner_index != bone_index)
        {
            continue;
        }
        if (event->aux_index != DBO_INDEX_NONE)
        {
            if (!dbo__write_unknown_event(w, event)) return DBO_FALSE;
        }
        else
        {
            if (!dbo__emit_bone_chunk(w, bone_index, event->code, DBO_TRUE)) return DBO_FALSE;
        }
    }

    for (i = 0UL; i < (dbo_u32)(sizeof(g_bone_codes) / sizeof(g_bone_codes[0])); ++i)
    {
        if (!dbo__scope_has_event_code(scene, DBO_SCOPE_BONE, bone_index, g_bone_codes[i]))
        {
            if (!dbo__emit_bone_chunk(w, bone_index, g_bone_codes[i], DBO_FALSE)) return DBO_FALSE;
        }
    }

    if (!dbo__write_terminator(w)) return DBO_FALSE;
    return DBO_TRUE;
}

static dbo_bool dbo__encode_bone(dbo_writer* w, dbo_u32 bone_index)
{
    if (dbo__scope_has_events(w->scene, DBO_SCOPE_BONE, bone_index))
    {
        return dbo__encode_bone_ordered(w, bone_index);
    }
    return dbo__encode_bone_canonical(w, bone_index);
}

static dbo_bool dbo__emit_texture_chunk(dbo_writer* w, dbo_u32 tex_index, dbo_u32 code, dbo_bool force)
{
    const dbo_texture* tex;
    (void)force;
    tex = &w->scene->textures[tex_index];

    switch (code)
    {
    case DBOBLOCK_TEXTURE_NAME:
        return dbo__write_string_chunk(w, DBOBLOCK_TEXTURE_NAME, tex->name);
    case DBOBLOCK_TEXTURE_STAGE:
        return dbo__write_dword_chunk(w, DBOBLOCK_TEXTURE_STAGE, tex->stage);
    case DBOBLOCK_TEXTURE_BMODE:
        return dbo__write_dword_chunk(w, DBOBLOCK_TEXTURE_BMODE, tex->blend_mode);
    case DBOBLOCK_TEXTURE_BARG1:
        return dbo__write_dword_chunk(w, DBOBLOCK_TEXTURE_BARG1, tex->blend_arg1);
    case DBOBLOCK_TEXTURE_BARG2:
        return dbo__write_dword_chunk(w, DBOBLOCK_TEXTURE_BARG2, tex->blend_arg2);
    case DBOBLOCK_TEXTURE_ADDRU:
        return dbo__write_dword_chunk(w, DBOBLOCK_TEXTURE_ADDRU, tex->address_u);
    case DBOBLOCK_TEXTURE_ADDRV:
        return dbo__write_dword_chunk(w, DBOBLOCK_TEXTURE_ADDRV, tex->address_v);
    case DBOBLOCK_TEXTURE_MAG:
        return dbo__write_dword_chunk(w, DBOBLOCK_TEXTURE_MAG, tex->mag_state);
    case DBOBLOCK_TEXTURE_MIN:
        return dbo__write_dword_chunk(w, DBOBLOCK_TEXTURE_MIN, tex->min_state);
    case DBOBLOCK_TEXTURE_MIP:
        return dbo__write_dword_chunk(w, DBOBLOCK_TEXTURE_MIP, tex->mip_state);
    case DBOBLOCK_TEXTURE_TCMODE:
        return dbo__write_dword_chunk(w, DBOBLOCK_TEXTURE_TCMODE, tex->texcoord_mode);
    case DBOBLOCK_TEXTURE_PRIMST:
        return dbo__write_dword_chunk(w, DBOBLOCK_TEXTURE_PRIMST, (dbo_u32)tex->start_index);
    case DBOBLOCK_TEXTURE_PRIMCN:
        return dbo__write_dword_chunk(w, DBOBLOCK_TEXTURE_PRIMCN, (dbo_u32)tex->primitive_count);
    default:
        dbo__set_error((dbo_scene*)w->scene, "unknown texture chunk code");
        return DBO_FALSE;
    }
}

static dbo_bool dbo__encode_texture_canonical(dbo_writer* w, dbo_u32 tex_index)
{
    dbo_u32 i;
    for (i = 0UL; i < (dbo_u32)(sizeof(g_texture_codes) / sizeof(g_texture_codes[0])); ++i)
    {
        if (!dbo__emit_texture_chunk(w, tex_index, g_texture_codes[i], DBO_FALSE)) return DBO_FALSE;
    }
    if (!dbo__write_unknown_chunks_for_scope(w, DBO_SCOPE_TEXTURE, tex_index)) return DBO_FALSE;
    if (!dbo__write_terminator(w)) return DBO_FALSE;
    return DBO_TRUE;
}

static dbo_bool dbo__encode_texture_ordered(dbo_writer* w, dbo_u32 tex_index)
{
    const dbo_scene* scene;
    dbo_u32 i;

    scene = w->scene;
    for (i = 0UL; i < scene->chunk_event_count; ++i)
    {
        const dbo_chunk_event* event = &scene->chunk_events[i];
        if (event->scope_kind != DBO_SCOPE_TEXTURE || event->owner_index != tex_index)
        {
            continue;
        }
        if (event->aux_index != DBO_INDEX_NONE)
        {
            if (!dbo__write_unknown_event(w, event)) return DBO_FALSE;
        }
        else
        {
            if (!dbo__emit_texture_chunk(w, tex_index, event->code, DBO_TRUE)) return DBO_FALSE;
        }
    }

    for (i = 0UL; i < (dbo_u32)(sizeof(g_texture_codes) / sizeof(g_texture_codes[0])); ++i)
    {
        if (!dbo__scope_has_event_code(scene, DBO_SCOPE_TEXTURE, tex_index, g_texture_codes[i]))
        {
            if (!dbo__emit_texture_chunk(w, tex_index, g_texture_codes[i], DBO_FALSE)) return DBO_FALSE;
        }
    }

    if (!dbo__write_terminator(w)) return DBO_FALSE;
    return DBO_TRUE;
}

static dbo_bool dbo__encode_texture(dbo_writer* w, dbo_u32 tex_index)
{
    if (dbo__scope_has_events(w->scene, DBO_SCOPE_TEXTURE, tex_index))
    {
        return dbo__encode_texture_ordered(w, tex_index);
    }
    return dbo__encode_texture_canonical(w, tex_index);
}

static dbo_bool dbo__emit_multimaterial_chunk(dbo_writer* w, dbo_u32 mm_index, dbo_u32 code, dbo_bool force)
{
    const dbo_multimaterial* mm;
    (void)force;
    mm = &w->scene->multimaterials[mm_index];

    switch (code)
    {
    case DBOBLOCK_MULTIMAT_NAME:
        return dbo__write_string_chunk(w, DBOBLOCK_MULTIMAT_NAME, mm->name);
    case DBOBLOCK_MULTIMAT_MATERIAL:
        return dbo__write_material_chunk(w, DBOBLOCK_MULTIMAT_MATERIAL, &mm->material);
    case DBOBLOCK_MULTIMAT_START:
        return dbo__write_dword_chunk(w, DBOBLOCK_MULTIMAT_START, mm->index_start);
    case DBOBLOCK_MULTIMAT_COUNT:
        return dbo__write_dword_chunk(w, DBOBLOCK_MULTIMAT_COUNT, mm->index_count);
    case DBOBLOCK_MULTIMAT_POLY:
        return dbo__write_dword_chunk(w, DBOBLOCK_MULTIMAT_POLY, mm->poly_count);
    default:
        dbo__set_error((dbo_scene*)w->scene, "unknown multimaterial chunk code");
        return DBO_FALSE;
    }
}

static dbo_bool dbo__encode_multimaterial_canonical(dbo_writer* w, dbo_u32 mm_index)
{
    dbo_u32 i;
    for (i = 0UL; i < (dbo_u32)(sizeof(g_multimat_codes) / sizeof(g_multimat_codes[0])); ++i)
    {
        if (!dbo__emit_multimaterial_chunk(w, mm_index, g_multimat_codes[i], DBO_FALSE)) return DBO_FALSE;
    }
    if (!dbo__write_unknown_chunks_for_scope(w, DBO_SCOPE_MULTIMATERIAL, mm_index)) return DBO_FALSE;
    if (!dbo__write_terminator(w)) return DBO_FALSE;
    return DBO_TRUE;
}

static dbo_bool dbo__encode_multimaterial_ordered(dbo_writer* w, dbo_u32 mm_index)
{
    const dbo_scene* scene;
    dbo_u32 i;

    scene = w->scene;
    for (i = 0UL; i < scene->chunk_event_count; ++i)
    {
        const dbo_chunk_event* event = &scene->chunk_events[i];
        if (event->scope_kind != DBO_SCOPE_MULTIMATERIAL || event->owner_index != mm_index)
        {
            continue;
        }
        if (event->aux_index != DBO_INDEX_NONE)
        {
            if (!dbo__write_unknown_event(w, event)) return DBO_FALSE;
        }
        else
        {
            if (!dbo__emit_multimaterial_chunk(w, mm_index, event->code, DBO_TRUE)) return DBO_FALSE;
        }
    }

    for (i = 0UL; i < (dbo_u32)(sizeof(g_multimat_codes) / sizeof(g_multimat_codes[0])); ++i)
    {
        if (!dbo__scope_has_event_code(scene, DBO_SCOPE_MULTIMATERIAL, mm_index, g_multimat_codes[i]))
        {
            if (!dbo__emit_multimaterial_chunk(w, mm_index, g_multimat_codes[i], DBO_FALSE)) return DBO_FALSE;
        }
    }

    if (!dbo__write_terminator(w)) return DBO_FALSE;
    return DBO_TRUE;
}

static dbo_bool dbo__encode_multimaterial(dbo_writer* w, dbo_u32 mm_index)
{
    if (dbo__scope_has_events(w->scene, DBO_SCOPE_MULTIMATERIAL, mm_index))
    {
        return dbo__encode_multimaterial_ordered(w, mm_index);
    }
    return dbo__encode_multimaterial_canonical(w, mm_index);
}

static dbo_bool dbo__encode_bones(dbo_writer* w, const dbo_mesh* mesh)
{
    dbo_u32 i;
    for (i = 0UL; i < mesh->bone_count; ++i)
    {
        if (!dbo__encode_bone(w, mesh->first_bone + i)) return DBO_FALSE;
    }
    if (!dbo__write_terminator(w)) return DBO_FALSE;
    return DBO_TRUE;
}

static dbo_bool dbo__encode_textures(dbo_writer* w, const dbo_mesh* mesh)
{
    dbo_u32 i;
    for (i = 0UL; i < mesh->texture_count; ++i)
    {
        if (!dbo__encode_texture(w, mesh->first_texture + i)) return DBO_FALSE;
    }
    if (!dbo__write_terminator(w)) return DBO_FALSE;
    return DBO_TRUE;
}

static dbo_bool dbo__encode_multimaterials(dbo_writer* w, const dbo_mesh* mesh)
{
    dbo_u32 i;
    for (i = 0UL; i < mesh->multimaterial_count; ++i)
    {
        if (!dbo__encode_multimaterial(w, mesh->first_multimaterial + i)) return DBO_FALSE;
    }
    if (!dbo__write_terminator(w)) return DBO_FALSE;
    return DBO_TRUE;
}

static dbo_bool dbo__emit_mesh_chunk(dbo_writer* w, dbo_u32 mesh_index, dbo_u32 code, dbo_bool force)
{
    const dbo_mesh* mesh;
    const void* ptr;
    const dbo_u32* subframes;
    dbo_u32 size_pos;
    dbo_u32 i;
    dbo_u32 size;

    mesh = &w->scene->meshes[mesh_index];
    ptr = 0;
    subframes = 0;
    size_pos = 0UL;
    i = 0UL;
    size = 0UL;

    switch (code)
    {
    case DBOBLOCK_MESH_FVF:
        return dbo__write_dword_chunk(w, DBOBLOCK_MESH_FVF, mesh->fvf);
    case DBOBLOCK_MESH_FVFSIZE:
        return dbo__write_dword_chunk(w, DBOBLOCK_MESH_FVFSIZE, mesh->fvf_size);
    case DBOBLOCK_MESH_VERTEXCOUNT:
        return dbo__write_dword_chunk(w, DBOBLOCK_MESH_VERTEXCOUNT, mesh->vertex_count);
    case DBOBLOCK_MESH_INDEXCOUNT:
        return dbo__write_dword_chunk(w, DBOBLOCK_MESH_INDEXCOUNT, mesh->index_count);
    case DBOBLOCK_MESH_VERTEXDEC:
        if (!force && mesh->fvf != 0UL)
        {
            return DBO_TRUE;
        }
        if (!dbo__write_chunk_header(w, DBOBLOCK_MESH_VERTEXDEC, 4UL)) return DBO_FALSE;
        ptr = dbo_blob_data(w->scene, mesh->vertex_decl);
        return dbo__write_bytes(w, ptr, DBO_VERTEX_DECL_BYTES);
    case DBOBLOCK_MESH_SUBFRAMES:
        if (!dbo__write_chunk_header(w, DBOBLOCK_MESH_SUBFRAMES, 4UL)) return DBO_FALSE;
        if (!dbo__write_u32(w, mesh->subframe_count)) return DBO_FALSE;
        subframes = (const dbo_u32*)dbo_blob_data(w->scene, mesh->subframe_indices);
        for (i = 0UL; i < mesh->subframe_count; ++i)
        {
            if (!dbo__encode_frame(w, subframes[i])) return DBO_FALSE;
        }
        return DBO_TRUE;
    case DBOBLOCK_MESH_VERTEXDATA:
        size = mesh->fvf_size * mesh->vertex_count;
        if (size == 0UL)
        {
            size = mesh->vertex_data.size;
        }
        if (!dbo__write_chunk_header(w, DBOBLOCK_MESH_VERTEXDATA, size)) return DBO_FALSE;
        ptr = dbo_blob_data(w->scene, mesh->vertex_data);
        return dbo__write_bytes(w, ptr, size);
    case DBOBLOCK_MESH_INDEXDATA:
        size = mesh->index_count * 2UL;
        if (size == 0UL && !force && mesh->index_data.size == 0UL)
        {
            return DBO_TRUE;
        }
        if (size == 0UL)
        {
            size = mesh->index_data.size;
        }
        if (!dbo__write_chunk_header(w, DBOBLOCK_MESH_INDEXDATA, size)) return DBO_FALSE;
        ptr = dbo_blob_data(w->scene, mesh->index_data);
        return dbo__write_bytes(w, ptr, size);
    case DBOBLOCK_MESH_PRIMTYPE:
        return dbo__write_dword_chunk(w, DBOBLOCK_MESH_PRIMTYPE, mesh->primitive_type);
    case DBOBLOCK_MESH_DRAWVERTCOUNT:
        return dbo__write_dword_chunk(w, DBOBLOCK_MESH_DRAWVERTCOUNT, mesh->draw_vertex_count);
    case DBOBLOCK_MESH_DRAWPRIMCOUNT:
        return dbo__write_dword_chunk(w, DBOBLOCK_MESH_DRAWPRIMCOUNT, mesh->draw_primitive_count);
    case DBOBLOCK_MESH_BONECOUNT:
        return dbo__write_dword_chunk(w, DBOBLOCK_MESH_BONECOUNT, mesh->bone_count);
    case DBOBLOCK_MESH_BONESDATA:
        if (!dbo__begin_chunk(w, DBOBLOCK_MESH_BONESDATA, &size_pos)) return DBO_FALSE;
        if (!dbo__encode_bones(w, mesh)) return DBO_FALSE;
        return dbo__end_chunk(w, size_pos);
    case DBOBLOCK_MESH_USEMATERIAL:
        return dbo__write_bool_chunk(w, DBOBLOCK_MESH_USEMATERIAL, mesh->uses_material);
    case DBOBLOCK_MESH_MATERIAL:
        return dbo__write_material_chunk(w, DBOBLOCK_MESH_MATERIAL, &mesh->material);
    case DBOBLOCK_MESH_TEXTURECOUNT:
        return dbo__write_dword_chunk(w, DBOBLOCK_MESH_TEXTURECOUNT, mesh->texture_count);
    case DBOBLOCK_MESH_TEXTURES:
        if (!dbo__begin_chunk(w, DBOBLOCK_MESH_TEXTURES, &size_pos)) return DBO_FALSE;
        if (!dbo__encode_textures(w, mesh)) return DBO_FALSE;
        return dbo__end_chunk(w, size_pos);
    case DBOBLOCK_MESH_USEMULTIMAT:
        return dbo__write_bool_chunk(w, DBOBLOCK_MESH_USEMULTIMAT, mesh->use_multimaterial);
    case DBOBLOCK_MESH_MULTIMATCOUNT:
        return dbo__write_dword_chunk(w, DBOBLOCK_MESH_MULTIMATCOUNT, mesh->multimaterial_count);
    case DBOBLOCK_MESH_MULTIMAT:
        if (!dbo__begin_chunk(w, DBOBLOCK_MESH_MULTIMAT, &size_pos)) return DBO_FALSE;
        if (!dbo__encode_multimaterials(w, mesh)) return DBO_FALSE;
        return dbo__end_chunk(w, size_pos);
    case DBOBLOCK_MESH_WIREFRAME:
        return dbo__write_bool_chunk(w, DBOBLOCK_MESH_WIREFRAME, mesh->wireframe);
    case DBOBLOCK_MESH_LIGHT:
        return dbo__write_bool_chunk(w, DBOBLOCK_MESH_LIGHT, mesh->light);
    case DBOBLOCK_MESH_CULL:
        return dbo__write_bool_chunk(w, DBOBLOCK_MESH_CULL, mesh->cull);
    case DBOBLOCK_MESH_FOG:
        return dbo__write_bool_chunk(w, DBOBLOCK_MESH_FOG, mesh->fog);
    case DBOBLOCK_MESH_AMBIENT:
        return dbo__write_bool_chunk(w, DBOBLOCK_MESH_AMBIENT, mesh->ambient);
    case DBOBLOCK_MESH_TRANSPARENCY:
        return dbo__write_bool_chunk(w, DBOBLOCK_MESH_TRANSPARENCY, mesh->transparency);
    case DBOBLOCK_MESH_GHOST:
        return dbo__write_bool_chunk(w, DBOBLOCK_MESH_GHOST, mesh->ghost);
    case DBOBLOCK_MESH_GHOSTMODE:
        return dbo__write_dword_chunk(w, DBOBLOCK_MESH_GHOSTMODE, mesh->ghost_mode);
    case DBOBLOCK_MESH_VISIBLE:
        return dbo__write_bool_chunk(w, DBOBLOCK_MESH_VISIBLE, mesh->visible);
    case DBOBLOCK_MESH_LINKED:
        return dbo__write_bool_chunk(w, DBOBLOCK_MESH_LINKED, mesh->linked);
    case DBOBLOCK_MESH_FXEFFECTNAME:
        if (!force && !mesh->use_vertex_shader)
        {
            return DBO_TRUE;
        }
        return dbo__write_string_chunk(w, DBOBLOCK_MESH_FXEFFECTNAME, mesh->effect_name);
    case DBOBLOCK_MESH_ARBITARYVALUE:
        return dbo__write_dword_chunk(w, DBOBLOCK_MESH_ARBITARYVALUE, mesh->arbitary_value);
    case DBOBLOCK_MESH_ZBIASFLAG:
        return dbo__write_bool_chunk(w, DBOBLOCK_MESH_ZBIASFLAG, mesh->z_bias_active);
    case DBOBLOCK_MESH_ZBIASSLOPE:
        return dbo__write_dword_chunk(w, DBOBLOCK_MESH_ZBIASSLOPE, dbo_fx_to_float_bits(mesh->z_bias_slope));
    case DBOBLOCK_MESH_ZBIASDEPTH:
        return dbo__write_dword_chunk(w, DBOBLOCK_MESH_ZBIASDEPTH, dbo_fx_to_float_bits(mesh->z_bias_depth));
    case DBOBLOCK_MESH_ZREAD:
        return dbo__write_bool_chunk(w, DBOBLOCK_MESH_ZREAD, mesh->z_read);
    case DBOBLOCK_MESH_ZWRITE:
        return dbo__write_bool_chunk(w, DBOBLOCK_MESH_ZWRITE, mesh->z_write);
    case DBOBLOCK_MESH_ALPHATESTVALUE:
        return dbo__write_dword_chunk(w, DBOBLOCK_MESH_ALPHATESTVALUE, mesh->alpha_test_value);
    default:
        dbo__set_error((dbo_scene*)w->scene, "unknown mesh chunk code");
        return DBO_FALSE;
    }
}

static dbo_bool dbo__encode_mesh_canonical(dbo_writer* w, dbo_u32 mesh_index)
{
    dbo_u32 i;
    for (i = 0UL; i < (dbo_u32)(sizeof(g_mesh_codes) / sizeof(g_mesh_codes[0])); ++i)
    {
        if (!dbo__emit_mesh_chunk(w, mesh_index, g_mesh_codes[i], DBO_FALSE)) return DBO_FALSE;
    }
    if (!dbo__write_unknown_chunks_for_scope(w, DBO_SCOPE_MESH, mesh_index)) return DBO_FALSE;
    if (!dbo__write_terminator(w)) return DBO_FALSE;
    return DBO_TRUE;
}

static dbo_bool dbo__encode_mesh_ordered(dbo_writer* w, dbo_u32 mesh_index)
{
    const dbo_scene* scene;
    dbo_u32 i;

    scene = w->scene;
    for (i = 0UL; i < scene->chunk_event_count; ++i)
    {
        const dbo_chunk_event* event = &scene->chunk_events[i];
        if (event->scope_kind != DBO_SCOPE_MESH || event->owner_index != mesh_index)
        {
            continue;
        }
        if (event->aux_index != DBO_INDEX_NONE)
        {
            if (!dbo__write_unknown_event(w, event)) return DBO_FALSE;
        }
        else
        {
            if (!dbo__emit_mesh_chunk(w, mesh_index, event->code, DBO_TRUE)) return DBO_FALSE;
        }
    }

    for (i = 0UL; i < (dbo_u32)(sizeof(g_mesh_codes) / sizeof(g_mesh_codes[0])); ++i)
    {
        if (!dbo__scope_has_event_code(scene, DBO_SCOPE_MESH, mesh_index, g_mesh_codes[i]))
        {
            if (!dbo__emit_mesh_chunk(w, mesh_index, g_mesh_codes[i], DBO_FALSE)) return DBO_FALSE;
        }
    }

    if (!dbo__write_terminator(w)) return DBO_FALSE;
    return DBO_TRUE;
}

static dbo_bool dbo__encode_mesh(dbo_writer* w, dbo_u32 mesh_index)
{
    if (mesh_index == DBO_INDEX_NONE)
    {
        return dbo__write_terminator(w);
    }

    if (dbo__scope_has_events(w->scene, DBO_SCOPE_MESH, mesh_index))
    {
        return dbo__encode_mesh_ordered(w, mesh_index);
    }
    return dbo__encode_mesh_canonical(w, mesh_index);
}

static dbo_bool dbo__emit_frame_chunk(dbo_writer* w, dbo_u32 frame_index, dbo_u32 code, dbo_bool force)
{
    const dbo_frame* frame;
    dbo_u32 size_pos;
    (void)force;
    frame = &w->scene->frames[frame_index];
    size_pos = 0UL;

    switch (code)
    {
    case DBOBLOCK_FRAME_NAME:
        return dbo__write_string_chunk(w, DBOBLOCK_FRAME_NAME, frame->name);
    case DBOBLOCK_FRAME_MATRIX:
        return dbo__write_mat_chunk(w, DBOBLOCK_FRAME_MATRIX, frame->original);
    case DBOBLOCK_FRAME_MESH:
        if (!dbo__begin_chunk(w, DBOBLOCK_FRAME_MESH, &size_pos)) return DBO_FALSE;
        if (!dbo__encode_mesh(w, frame->mesh)) return DBO_FALSE;
        return dbo__end_chunk(w, size_pos);
    case DBOBLOCK_FRAME_CHILD:
        if (!dbo__begin_chunk(w, DBOBLOCK_FRAME_CHILD, &size_pos)) return DBO_FALSE;
        if (!dbo__encode_frame(w, frame->child)) return DBO_FALSE;
        return dbo__end_chunk(w, size_pos);
    case DBOBLOCK_FRAME_SIBLING:
        if (!dbo__begin_chunk(w, DBOBLOCK_FRAME_SIBLING, &size_pos)) return DBO_FALSE;
        if (!dbo__encode_frame(w, frame->sibling)) return DBO_FALSE;
        return dbo__end_chunk(w, size_pos);
    case DBOBLOCK_FRAME_OFFSET:
        return dbo__write_vec_chunk(w, DBOBLOCK_FRAME_OFFSET, frame->offset);
    case DBOBLOCK_FRAME_ROTATION:
        return dbo__write_vec_chunk(w, DBOBLOCK_FRAME_ROTATION, frame->rotation);
    case DBOBLOCK_FRAME_SCALE:
        return dbo__write_vec_chunk(w, DBOBLOCK_FRAME_SCALE, frame->scale);
    default:
        dbo__set_error((dbo_scene*)w->scene, "unknown frame chunk code");
        return DBO_FALSE;
    }
}

static dbo_bool dbo__encode_frame_canonical(dbo_writer* w, dbo_u32 frame_index)
{
    dbo_u32 i;
    for (i = 0UL; i < (dbo_u32)(sizeof(g_frame_codes) / sizeof(g_frame_codes[0])); ++i)
    {
        if (!dbo__emit_frame_chunk(w, frame_index, g_frame_codes[i], DBO_FALSE)) return DBO_FALSE;
    }
    if (!dbo__write_unknown_chunks_for_scope(w, DBO_SCOPE_FRAME, frame_index)) return DBO_FALSE;
    if (!dbo__write_terminator(w)) return DBO_FALSE;
    return DBO_TRUE;
}

static dbo_bool dbo__encode_frame_ordered(dbo_writer* w, dbo_u32 frame_index)
{
    const dbo_scene* scene;
    dbo_u32 i;

    scene = w->scene;
    for (i = 0UL; i < scene->chunk_event_count; ++i)
    {
        const dbo_chunk_event* event = &scene->chunk_events[i];
        if (event->scope_kind != DBO_SCOPE_FRAME || event->owner_index != frame_index)
        {
            continue;
        }
        if (event->aux_index != DBO_INDEX_NONE)
        {
            if (!dbo__write_unknown_event(w, event)) return DBO_FALSE;
        }
        else
        {
            if (!dbo__emit_frame_chunk(w, frame_index, event->code, DBO_TRUE)) return DBO_FALSE;
        }
    }

    for (i = 0UL; i < (dbo_u32)(sizeof(g_frame_codes) / sizeof(g_frame_codes[0])); ++i)
    {
        if (!dbo__scope_has_event_code(scene, DBO_SCOPE_FRAME, frame_index, g_frame_codes[i]))
        {
            if (!dbo__emit_frame_chunk(w, frame_index, g_frame_codes[i], DBO_FALSE)) return DBO_FALSE;
        }
    }

    if (!dbo__write_terminator(w)) return DBO_FALSE;
    return DBO_TRUE;
}

static dbo_bool dbo__encode_frame(dbo_writer* w, dbo_u32 frame_index)
{
    if (frame_index == DBO_INDEX_NONE)
    {
        return dbo__write_terminator(w);
    }

    if (dbo__scope_has_events(w->scene, DBO_SCOPE_FRAME, frame_index))
    {
        return dbo__encode_frame_ordered(w, frame_index);
    }
    return dbo__encode_frame_canonical(w, frame_index);
}

static dbo_bool dbo__emit_animation_chunk(dbo_writer* w, dbo_u32 anim_index, dbo_u32 code, dbo_bool force)
{
    const dbo_animation* anim;
    const void* offsets;
    dbo_u32 size_pos;

    anim = &w->scene->animations[anim_index];
    offsets = 0;
    size_pos = 0UL;

    switch (code)
    {
    case DBOBLOCK_ANIM_NAME:
        return dbo__write_string_chunk(w, DBOBLOCK_ANIM_NAME, anim->name);
    case DBOBLOCK_ANIM_NUMPOSKEYS:
        return dbo__write_dword_chunk(w, DBOBLOCK_ANIM_NUMPOSKEYS, anim->num_position_keys);
    case DBOBLOCK_ANIM_POSDATA:
        return dbo__encode_position_keys(w, anim, force);
    case DBOBLOCK_ANIM_NUMROTKEYS:
        return dbo__write_dword_chunk(w, DBOBLOCK_ANIM_NUMROTKEYS, anim->num_rotate_keys);
    case DBOBLOCK_ANIM_ROTDATA:
        return dbo__encode_rotate_keys(w, anim, force);
    case DBOBLOCK_ANIM_NUMSCALEKEYS:
        return dbo__write_dword_chunk(w, DBOBLOCK_ANIM_NUMSCALEKEYS, anim->num_scale_keys);
    case DBOBLOCK_ANIM_SCALEDATA:
        return dbo__encode_scale_keys(w, anim, force);
    case DBOBLOCK_ANIM_NUMMATRIXKEYS:
        return dbo__write_dword_chunk(w, DBOBLOCK_ANIM_NUMMATRIXKEYS, anim->num_matrix_keys);
    case DBOBLOCK_ANIM_MATRIXDATA:
        return dbo__encode_matrix_keys(w, anim, force);
    case DBOBLOCK_FRAME_BONETYPE:
        if (!force && anim->bone_type != 0UL)
        {
            return DBO_TRUE;
        }
        return dbo__write_dword_chunk(w, DBOBLOCK_FRAME_BONETYPE, anim->bone_type);
    case DBOBLOCK_FRAME_BONEOFFSETLISTSIZE:
        if (!force && anim->bone_type != 0UL)
        {
            return DBO_TRUE;
        }
        return dbo__write_dword_chunk(w, DBOBLOCK_FRAME_BONEOFFSETLISTSIZE, anim->bone_offset_count);
    case DBOBLOCK_FRAME_BONEOFFSETLIST:
        if (!force && anim->bone_type != 0UL)
        {
            return DBO_TRUE;
        }
        if (!dbo__write_chunk_header(w, DBOBLOCK_FRAME_BONEOFFSETLIST, anim->bone_offset_count * 4UL)) return DBO_FALSE;
        offsets = dbo_blob_data(w->scene, anim->bone_offsets);
        return dbo__write_bytes(w, offsets, anim->bone_offset_count * 4UL);
    case DBOBLOCK_FRAME_BONEMATRIXSIZEA:
        if (!force && anim->bone_type != 0UL)
        {
            return DBO_TRUE;
        }
        return dbo__write_dword_chunk(w, DBOBLOCK_FRAME_BONEMATRIXSIZEA, (dbo_u32)anim->bone_frame_a);
    case DBOBLOCK_FRAME_BONEMATRIXSIZEB:
        if (!force && anim->bone_type != 0UL)
        {
            return DBO_TRUE;
        }
        return dbo__write_dword_chunk(w, DBOBLOCK_FRAME_BONEMATRIXSIZEB, (dbo_u32)anim->bone_frame_b);
    case DBOBLOCK_FRAME_BONEMATRIXLIST:
        if (!force && anim->bone_type != 0UL)
        {
            return DBO_TRUE;
        }
        if (!dbo__begin_chunk(w, DBOBLOCK_FRAME_BONEMATRIXLIST, &size_pos)) return DBO_FALSE;
        if (!dbo__encode_bone_matrices(w, anim)) return DBO_FALSE;
        return dbo__end_chunk(w, size_pos);
    case DBOBLOCK_ANIM_NEXT:
        if (!force && anim->next_animation == DBO_INDEX_NONE)
        {
            return DBO_TRUE;
        }
        if (!dbo__begin_chunk(w, DBOBLOCK_ANIM_NEXT, &size_pos)) return DBO_FALSE;
        if (!dbo__encode_animation(w, anim->next_animation)) return DBO_FALSE;
        return dbo__end_chunk(w, size_pos);
    default:
        dbo__set_error((dbo_scene*)w->scene, "unknown animation chunk code");
        return DBO_FALSE;
    }
}

static dbo_bool dbo__encode_animation_canonical(dbo_writer* w, dbo_u32 anim_index)
{
    dbo_u32 i;
    for (i = 0UL; i < (dbo_u32)(sizeof(g_animation_codes) / sizeof(g_animation_codes[0])); ++i)
    {
        if (!dbo__emit_animation_chunk(w, anim_index, g_animation_codes[i], DBO_FALSE)) return DBO_FALSE;
    }
    if (!dbo__write_unknown_chunks_for_scope(w, DBO_SCOPE_ANIMATION, anim_index)) return DBO_FALSE;
    if (!dbo__write_terminator(w)) return DBO_FALSE;
    return DBO_TRUE;
}

static dbo_bool dbo__encode_animation_ordered(dbo_writer* w, dbo_u32 anim_index)
{
    const dbo_scene* scene;
    dbo_u32 i;

    scene = w->scene;
    for (i = 0UL; i < scene->chunk_event_count; ++i)
    {
        const dbo_chunk_event* event = &scene->chunk_events[i];
        if (event->scope_kind != DBO_SCOPE_ANIMATION || event->owner_index != anim_index)
        {
            continue;
        }
        if (event->aux_index != DBO_INDEX_NONE)
        {
            if (!dbo__write_unknown_event(w, event)) return DBO_FALSE;
        }
        else
        {
            if (!dbo__emit_animation_chunk(w, anim_index, event->code, DBO_TRUE)) return DBO_FALSE;
        }
    }

    for (i = 0UL; i < (dbo_u32)(sizeof(g_animation_codes) / sizeof(g_animation_codes[0])); ++i)
    {
        if (!dbo__scope_has_event_code(scene, DBO_SCOPE_ANIMATION, anim_index, g_animation_codes[i]))
        {
            if (!dbo__emit_animation_chunk(w, anim_index, g_animation_codes[i], DBO_FALSE)) return DBO_FALSE;
        }
    }

    if (!dbo__write_terminator(w)) return DBO_FALSE;
    return DBO_TRUE;
}

static dbo_bool dbo__encode_animation(dbo_writer* w, dbo_u32 anim_index)
{
    if (anim_index == DBO_INDEX_NONE)
    {
        return dbo__write_terminator(w);
    }

    if (dbo__scope_has_events(w->scene, DBO_SCOPE_ANIMATION, anim_index))
    {
        return dbo__encode_animation_ordered(w, anim_index);
    }
    return dbo__encode_animation_canonical(w, anim_index);
}

static dbo_bool dbo__emit_animation_set_chunk(dbo_writer* w, dbo_u32 set_index, dbo_u32 code, dbo_bool force)
{
    const dbo_animation_set* set;
    dbo_u32 size_pos;

    set = &w->scene->animation_sets[set_index];
    size_pos = 0UL;

    switch (code)
    {
    case DBOBLOCK_ANIMSET_NAME:
        return dbo__write_string_chunk(w, DBOBLOCK_ANIMSET_NAME, set->name);
    case DBOBLOCK_ANIMSET_LENGTH:
        return dbo__write_dword_chunk(w, DBOBLOCK_ANIMSET_LENGTH, set->length);
    case DBOBLOCK_ANIMSET_DATA:
        if (!force && set->first_animation == DBO_INDEX_NONE)
        {
            return DBO_TRUE;
        }
        if (!dbo__begin_chunk(w, DBOBLOCK_ANIMSET_DATA, &size_pos)) return DBO_FALSE;
        if (!dbo__encode_animation(w, set->first_animation)) return DBO_FALSE;
        return dbo__end_chunk(w, size_pos);
    default:
        dbo__set_error((dbo_scene*)w->scene, "unknown animation set chunk code");
        return DBO_FALSE;
    }
}

static dbo_bool dbo__encode_animation_set_canonical(dbo_writer* w, dbo_u32 set_index)
{
    dbo_u32 i;
    for (i = 0UL; i < (dbo_u32)(sizeof(g_animation_set_codes) / sizeof(g_animation_set_codes[0])); ++i)
    {
        if (!dbo__emit_animation_set_chunk(w, set_index, g_animation_set_codes[i], DBO_FALSE)) return DBO_FALSE;
    }
    if (!dbo__write_unknown_chunks_for_scope(w, DBO_SCOPE_ANIMATION_SET, set_index)) return DBO_FALSE;
    if (!dbo__write_terminator(w)) return DBO_FALSE;
    return DBO_TRUE;
}

static dbo_bool dbo__encode_animation_set_ordered(dbo_writer* w, dbo_u32 set_index)
{
    const dbo_scene* scene;
    dbo_u32 i;

    scene = w->scene;
    for (i = 0UL; i < scene->chunk_event_count; ++i)
    {
        const dbo_chunk_event* event = &scene->chunk_events[i];
        if (event->scope_kind != DBO_SCOPE_ANIMATION_SET || event->owner_index != set_index)
        {
            continue;
        }
        if (event->aux_index != DBO_INDEX_NONE)
        {
            if (!dbo__write_unknown_event(w, event)) return DBO_FALSE;
        }
        else
        {
            if (!dbo__emit_animation_set_chunk(w, set_index, event->code, DBO_TRUE)) return DBO_FALSE;
        }
    }

    for (i = 0UL; i < (dbo_u32)(sizeof(g_animation_set_codes) / sizeof(g_animation_set_codes[0])); ++i)
    {
        if (!dbo__scope_has_event_code(scene, DBO_SCOPE_ANIMATION_SET, set_index, g_animation_set_codes[i]))
        {
            if (!dbo__emit_animation_set_chunk(w, set_index, g_animation_set_codes[i], DBO_FALSE)) return DBO_FALSE;
        }
    }

    if (!dbo__write_terminator(w)) return DBO_FALSE;
    return DBO_TRUE;
}

static dbo_bool dbo__encode_animation_set(dbo_writer* w, dbo_u32 set_index)
{
    if (set_index == DBO_INDEX_NONE)
    {
        return dbo__write_terminator(w);
    }

    if (dbo__scope_has_events(w->scene, DBO_SCOPE_ANIMATION_SET, set_index))
    {
        return dbo__encode_animation_set_ordered(w, set_index);
    }
    return dbo__encode_animation_set_canonical(w, set_index);
}

static dbo_bool dbo__emit_scene_chunk(dbo_writer* w, dbo_u32 code, dbo_bool force)
{
    dbo_u32 size_pos;
    (void)force;
    size_pos = 0UL;

    switch (code)
    {
    case DBOBLOCK_ROOT_FRAME:
        if (!dbo__begin_chunk(w, DBOBLOCK_ROOT_FRAME, &size_pos)) return DBO_FALSE;
        if (!dbo__encode_frame(w, w->scene->root_frame)) return DBO_FALSE;
        return dbo__end_chunk(w, size_pos);
    case DBOBLOCK_ROOT_ANIMATIONSET:
        if (!dbo__begin_chunk(w, DBOBLOCK_ROOT_ANIMATIONSET, &size_pos)) return DBO_FALSE;
        if (!dbo__encode_animation_set(w, w->scene->root_animation_set)) return DBO_FALSE;
        return dbo__end_chunk(w, size_pos);
    case DBOBLOCK_OBJECT_CUSTOMDATA:
        if (!dbo__begin_chunk(w, DBOBLOCK_OBJECT_CUSTOMDATA, &size_pos)) return DBO_FALSE;
        if (!dbo__write_u32(w, w->scene->custom_data.size)) return DBO_FALSE;
        if (!dbo__write_bytes(w, dbo_blob_data(w->scene, w->scene->custom_data), w->scene->custom_data.size)) return DBO_FALSE;
        return dbo__end_chunk(w, size_pos);
    default:
        dbo__set_error((dbo_scene*)w->scene, "unknown scene chunk code");
        return DBO_FALSE;
    }
}

static dbo_bool dbo__encode_scene_chunks_canonical(dbo_writer* w)
{
    dbo_u32 i;
    for (i = 0UL; i < (dbo_u32)(sizeof(g_scene_codes) / sizeof(g_scene_codes[0])); ++i)
    {
        if (!dbo__emit_scene_chunk(w, g_scene_codes[i], DBO_FALSE)) return DBO_FALSE;
    }
    if (!dbo__write_unknown_chunks_for_scope(w, DBO_SCOPE_SCENE, 0UL)) return DBO_FALSE;
    if (!dbo__write_terminator(w)) return DBO_FALSE;
    return DBO_TRUE;
}

static dbo_bool dbo__encode_scene_chunks_ordered(dbo_writer* w)
{
    const dbo_scene* scene;
    dbo_u32 i;

    scene = w->scene;
    for (i = 0UL; i < scene->chunk_event_count; ++i)
    {
        const dbo_chunk_event* event = &scene->chunk_events[i];
        if (event->scope_kind != DBO_SCOPE_SCENE || event->owner_index != 0UL)
        {
            continue;
        }
        if (event->aux_index != DBO_INDEX_NONE)
        {
            if (!dbo__write_unknown_event(w, event)) return DBO_FALSE;
        }
        else
        {
            if (!dbo__emit_scene_chunk(w, event->code, DBO_TRUE)) return DBO_FALSE;
        }
    }

    for (i = 0UL; i < (dbo_u32)(sizeof(g_scene_codes) / sizeof(g_scene_codes[0])); ++i)
    {
        if (!dbo__scope_has_event_code(scene, DBO_SCOPE_SCENE, 0UL, g_scene_codes[i]))
        {
            if (!dbo__emit_scene_chunk(w, g_scene_codes[i], DBO_FALSE)) return DBO_FALSE;
        }
    }

    if (!dbo__write_terminator(w)) return DBO_FALSE;
    return DBO_TRUE;
}

static dbo_bool dbo__encode_scene_to_writer(dbo_writer* w)
{
    if (!dbo__write_u32(w, 8UL)) return DBO_FALSE;
    if (!dbo__write_bytes(w, DBO_MAGIC_STRING, 8UL)) return DBO_FALSE;
    if (!dbo__write_u32(w, w->scene->version != 0UL ? w->scene->version : DBO_VERSION_NUMBER)) return DBO_FALSE;
    if (!dbo__write_u32(w, w->scene->reserved1)) return DBO_FALSE;
    if (!dbo__write_u32(w, w->scene->reserved2)) return DBO_FALSE;

    if (dbo__scope_has_events(w->scene, DBO_SCOPE_SCENE, 0UL))
    {
        return dbo__encode_scene_chunks_ordered(w);
    }
    return dbo__encode_scene_chunks_canonical(w);
}

dbo_bool dbo_measure_encoded_size(const dbo_scene* scene, dbo_u32* out_size)
{
    dbo_writer w;
    char err[DBO_MAX_ERROR_TEXT];
    if (scene == 0 || out_size == 0)
    {
        return DBO_FALSE;
    }
    memset(&w, 0, sizeof(w));
    w.scene = scene;
    w.data = 0;
    w.capacity = 0UL;
    w.pos = 0UL;
    w.error_text = err;
    w.error_text_size = (dbo_u32)sizeof(err);
    if (!dbo__encode_scene_to_writer(&w))
    {
        return DBO_FALSE;
    }
    *out_size = w.pos;
    return DBO_TRUE;
}

dbo_bool dbo_encode_memory(const dbo_scene* scene, void* out_buffer, dbo_u32 capacity, dbo_u32* out_size)
{
    dbo_writer w;
    if (scene == 0 || out_buffer == 0 || out_size == 0)
    {
        return DBO_FALSE;
    }
    memset(&w, 0, sizeof(w));
    w.scene = scene;
    w.data = (dbo_u8*)out_buffer;
    w.capacity = capacity;
    w.pos = 0UL;
    w.error_text = 0;
    w.error_text_size = 0UL;
    if (!dbo__encode_scene_to_writer(&w))
    {
        return DBO_FALSE;
    }
    *out_size = w.pos;
    return DBO_TRUE;
}
