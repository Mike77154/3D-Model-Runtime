#include "dbo_int.h"

static void dbo__clear_blob_ref(dbo_blob_ref* ref)
{
    ref->offset = DBO_INDEX_NONE;
    ref->size = 0UL;
}

void dbo__set_error(dbo_scene* scene, const char* text)
{
    if (scene == 0)
    {
        return;
    }
    if (text == 0)
    {
        scene->last_error[0] = '\0';
        return;
    }
    strncpy(scene->last_error, text, DBO_MAX_ERROR_TEXT - 1);
    scene->last_error[DBO_MAX_ERROR_TEXT - 1] = '\0';
}

const char* dbo_scene_last_error(const dbo_scene* scene)
{
    if (scene == 0)
    {
        return "null scene";
    }
    return scene->last_error;
}

void dbo_scene_reset(dbo_scene* scene)
{
    dbo_u32 i;
    if (scene == 0)
    {
        return;
    }

    memset(scene, 0, sizeof(*scene));
    scene->version = DBO_VERSION_NUMBER;
    scene->root_frame = DBO_INDEX_NONE;
    scene->root_animation_set = DBO_INDEX_NONE;
    dbo__clear_blob_ref(&scene->custom_data);

    for (i = 0UL; i < DBO_MAX_FRAMES; ++i)
    {
        scene->frames[i].parent = DBO_INDEX_NONE;
        scene->frames[i].child = DBO_INDEX_NONE;
        scene->frames[i].sibling = DBO_INDEX_NONE;
        scene->frames[i].mesh = DBO_INDEX_NONE;
        dbo__clear_blob_ref(&scene->frames[i].name);
        dbo_fx_identity4(scene->frames[i].original);
        scene->frames[i].scale[0] = DBO_FX_ONE;
        scene->frames[i].scale[1] = DBO_FX_ONE;
        scene->frames[i].scale[2] = DBO_FX_ONE;
    }

    for (i = 0UL; i < DBO_MAX_MESHES; ++i)
    {
        dbo_mesh* m = &scene->meshes[i];
        dbo__clear_blob_ref(&m->vertex_decl);
        dbo__clear_blob_ref(&m->vertex_data);
        dbo__clear_blob_ref(&m->index_data);
        dbo__clear_blob_ref(&m->subframe_indices);
        dbo__clear_blob_ref(&m->effect_name);
        m->first_bone = DBO_INDEX_NONE;
        m->first_texture = DBO_INDEX_NONE;
        m->first_multimaterial = DBO_INDEX_NONE;
        m->visible = DBO_TRUE;
        m->light = DBO_TRUE;
        m->cull = DBO_TRUE;
        m->fog = DBO_TRUE;
        m->ambient = DBO_TRUE;
        m->z_read = DBO_TRUE;
        m->z_write = DBO_TRUE;
        m->uses_material = DBO_TRUE;
    }

    for (i = 0UL; i < DBO_MAX_TEXTURES; ++i)
    {
        dbo__clear_blob_ref(&scene->textures[i].name);
    }

    for (i = 0UL; i < DBO_MAX_BONES; ++i)
    {
        dbo__clear_blob_ref(&scene->bones[i].name);
        dbo__clear_blob_ref(&scene->bones[i].vertices);
        dbo__clear_blob_ref(&scene->bones[i].weights);
        dbo_fx_identity4(scene->bones[i].translation);
    }

    for (i = 0UL; i < DBO_MAX_MULTIMATERIALS; ++i)
    {
        dbo__clear_blob_ref(&scene->multimaterials[i].name);
    }

    for (i = 0UL; i < DBO_MAX_ANIMATIONS; ++i)
    {
        dbo_animation* a = &scene->animations[i];
        dbo__clear_blob_ref(&a->name);
        dbo__clear_blob_ref(&a->position_keys);
        dbo__clear_blob_ref(&a->rotate_keys);
        dbo__clear_blob_ref(&a->scale_keys);
        dbo__clear_blob_ref(&a->matrix_keys);
        dbo__clear_blob_ref(&a->bone_offsets);
        dbo__clear_blob_ref(&a->bone_matrices);
        a->frame_index = DBO_INDEX_NONE;
        a->next_animation = DBO_INDEX_NONE;
    }

    for (i = 0UL; i < DBO_MAX_ANIMATION_SETS; ++i)
    {
        dbo__clear_blob_ref(&scene->animation_sets[i].name);
        scene->animation_sets[i].first_animation = DBO_INDEX_NONE;
    }

    scene->last_error[0] = '\0';
}

dbo_bool dbo__align_up(dbo_u32 value, dbo_u32 align, dbo_u32* out_value)
{
    dbo_u32 mask;
    if (out_value == 0)
    {
        return DBO_FALSE;
    }
    if (align <= 1UL)
    {
        *out_value = value;
        return DBO_TRUE;
    }
    mask = align - 1UL;
    *out_value = (value + mask) & ~mask;
    return DBO_TRUE;
}

static dbo_bool dbo__blob_alloc(dbo_scene* scene, dbo_u32 size, dbo_u32 align, dbo_blob_ref* out_ref, dbo_u32 include_nul)
{
    dbo_u32 start;
    dbo_u32 need;
    dbo_u32 total;
    if (scene == 0 || out_ref == 0)
    {
        return DBO_FALSE;
    }
    if (!dbo__align_up(scene->blob_used, align, &start))
    {
        dbo__set_error(scene, "align failed");
        return DBO_FALSE;
    }
    need = size + include_nul;
    if (need < size)
    {
        dbo__set_error(scene, "size overflow");
        return DBO_FALSE;
    }
    total = start + need;
    if (total < start || total > (dbo_u32)DBO_BLOB_CAPACITY)
    {
        dbo__set_error(scene, "blob capacity exceeded");
        return DBO_FALSE;
    }
    out_ref->offset = start;
    out_ref->size = size;
    scene->blob_used = total;
    return DBO_TRUE;
}

const void* dbo_blob_data(const dbo_scene* scene, dbo_blob_ref ref)
{
    if (scene == 0)
    {
        return 0;
    }
    if (ref.offset == DBO_INDEX_NONE)
    {
        return 0;
    }
    if (ref.offset > scene->blob_used)
    {
        return 0;
    }
    if (ref.size > scene->blob_used - ref.offset)
    {
        return 0;
    }
    return (const void*)(scene->blob + ref.offset);
}

const char* dbo_blob_cstr(const dbo_scene* scene, dbo_blob_ref ref)
{
    if (scene == 0)
    {
        return 0;
    }
    if (ref.offset == DBO_INDEX_NONE)
    {
        return 0;
    }
    if (ref.offset >= scene->blob_used)
    {
        return 0;
    }
    return (const char*)(scene->blob + ref.offset);
}

dbo_bool dbo_scene_copy_bytes(dbo_scene* scene, const void* src, dbo_u32 size, dbo_u32 align, dbo_blob_ref* out_ref)
{
    if (scene == 0 || out_ref == 0)
    {
        return DBO_FALSE;
    }
    if (!dbo__blob_alloc(scene, size, align, out_ref, 0UL))
    {
        return DBO_FALSE;
    }
    if (size > 0UL && src != 0)
    {
        memcpy(scene->blob + out_ref->offset, src, size);
    }
    else if (size > 0UL)
    {
        memset(scene->blob + out_ref->offset, 0, size);
    }
    return DBO_TRUE;
}

dbo_bool dbo_scene_copy_string(dbo_scene* scene, const char* text, dbo_blob_ref* out_ref)
{
    dbo_u32 len;
    if (scene == 0 || out_ref == 0)
    {
        return DBO_FALSE;
    }
    if (text == 0)
    {
        dbo__clear_blob_ref(out_ref);
        return DBO_TRUE;
    }
    len = (dbo_u32)strlen(text);
    if (!dbo__blob_alloc(scene, len, 1UL, out_ref, 1UL))
    {
        return DBO_FALSE;
    }
    memcpy(scene->blob + out_ref->offset, text, len);
    scene->blob[out_ref->offset + len] = 0;
    return DBO_TRUE;
}

static dbo_bool dbo__add_index(dbo_scene* scene, dbo_u32 count, dbo_u32 max_count, dbo_u32* out_index, const char* msg)
{
    if (scene == 0 || out_index == 0)
    {
        return DBO_FALSE;
    }
    if (count >= max_count)
    {
        dbo__set_error(scene, msg);
        return DBO_FALSE;
    }
    *out_index = count;
    return DBO_TRUE;
}

dbo_bool dbo_scene_add_frame(dbo_scene* scene, dbo_u32* out_index)
{
    dbo_u32 idx;
    if (!dbo__add_index(scene, scene->frame_count, DBO_MAX_FRAMES, &idx, "frame capacity exceeded"))
    {
        return DBO_FALSE;
    }
    scene->frame_count = idx + 1UL;
    scene->frames[idx].parent = DBO_INDEX_NONE;
    scene->frames[idx].child = DBO_INDEX_NONE;
    scene->frames[idx].sibling = DBO_INDEX_NONE;
    scene->frames[idx].mesh = DBO_INDEX_NONE;
    dbo__clear_blob_ref(&scene->frames[idx].name);
    dbo_fx_identity4(scene->frames[idx].original);
    scene->frames[idx].scale[0] = DBO_FX_ONE;
    scene->frames[idx].scale[1] = DBO_FX_ONE;
    scene->frames[idx].scale[2] = DBO_FX_ONE;
    *out_index = idx;
    return DBO_TRUE;
}

dbo_bool dbo_scene_add_mesh(dbo_scene* scene, dbo_u32* out_index)
{
    dbo_u32 idx;
    if (!dbo__add_index(scene, scene->mesh_count, DBO_MAX_MESHES, &idx, "mesh capacity exceeded"))
    {
        return DBO_FALSE;
    }
    scene->mesh_count = idx + 1UL;
    memset(&scene->meshes[idx], 0, sizeof(scene->meshes[idx]));
    scene->meshes[idx].first_bone = DBO_INDEX_NONE;
    scene->meshes[idx].first_texture = DBO_INDEX_NONE;
    scene->meshes[idx].first_multimaterial = DBO_INDEX_NONE;
    scene->meshes[idx].visible = DBO_TRUE;
    scene->meshes[idx].light = DBO_TRUE;
    scene->meshes[idx].cull = DBO_TRUE;
    scene->meshes[idx].fog = DBO_TRUE;
    scene->meshes[idx].ambient = DBO_TRUE;
    scene->meshes[idx].z_read = DBO_TRUE;
    scene->meshes[idx].z_write = DBO_TRUE;
    scene->meshes[idx].uses_material = DBO_TRUE;
    scene->meshes[idx].vertex_decl.offset = DBO_INDEX_NONE;
    scene->meshes[idx].vertex_data.offset = DBO_INDEX_NONE;
    scene->meshes[idx].index_data.offset = DBO_INDEX_NONE;
    scene->meshes[idx].subframe_indices.offset = DBO_INDEX_NONE;
    scene->meshes[idx].effect_name.offset = DBO_INDEX_NONE;
    *out_index = idx;
    return DBO_TRUE;
}

dbo_bool dbo_scene_add_texture(dbo_scene* scene, dbo_u32* out_index)
{
    dbo_u32 idx;
    if (!dbo__add_index(scene, scene->texture_count, DBO_MAX_TEXTURES, &idx, "texture capacity exceeded"))
    {
        return DBO_FALSE;
    }
    scene->texture_count = idx + 1UL;
    memset(&scene->textures[idx], 0, sizeof(scene->textures[idx]));
    scene->textures[idx].name.offset = DBO_INDEX_NONE;
    *out_index = idx;
    return DBO_TRUE;
}

dbo_bool dbo_scene_add_bone(dbo_scene* scene, dbo_u32* out_index)
{
    dbo_u32 idx;
    if (!dbo__add_index(scene, scene->bone_count, DBO_MAX_BONES, &idx, "bone capacity exceeded"))
    {
        return DBO_FALSE;
    }
    scene->bone_count = idx + 1UL;
    memset(&scene->bones[idx], 0, sizeof(scene->bones[idx]));
    scene->bones[idx].name.offset = DBO_INDEX_NONE;
    scene->bones[idx].vertices.offset = DBO_INDEX_NONE;
    scene->bones[idx].weights.offset = DBO_INDEX_NONE;
    dbo_fx_identity4(scene->bones[idx].translation);
    *out_index = idx;
    return DBO_TRUE;
}

dbo_bool dbo_scene_add_multimaterial(dbo_scene* scene, dbo_u32* out_index)
{
    dbo_u32 idx;
    if (!dbo__add_index(scene, scene->multimaterial_count, DBO_MAX_MULTIMATERIALS, &idx, "multimaterial capacity exceeded"))
    {
        return DBO_FALSE;
    }
    scene->multimaterial_count = idx + 1UL;
    memset(&scene->multimaterials[idx], 0, sizeof(scene->multimaterials[idx]));
    scene->multimaterials[idx].name.offset = DBO_INDEX_NONE;
    *out_index = idx;
    return DBO_TRUE;
}

dbo_bool dbo_scene_add_animation(dbo_scene* scene, dbo_u32* out_index)
{
    dbo_u32 idx;
    if (!dbo__add_index(scene, scene->animation_count, DBO_MAX_ANIMATIONS, &idx, "animation capacity exceeded"))
    {
        return DBO_FALSE;
    }
    scene->animation_count = idx + 1UL;
    memset(&scene->animations[idx], 0, sizeof(scene->animations[idx]));
    scene->animations[idx].name.offset = DBO_INDEX_NONE;
    scene->animations[idx].position_keys.offset = DBO_INDEX_NONE;
    scene->animations[idx].rotate_keys.offset = DBO_INDEX_NONE;
    scene->animations[idx].scale_keys.offset = DBO_INDEX_NONE;
    scene->animations[idx].matrix_keys.offset = DBO_INDEX_NONE;
    scene->animations[idx].bone_offsets.offset = DBO_INDEX_NONE;
    scene->animations[idx].bone_matrices.offset = DBO_INDEX_NONE;
    scene->animations[idx].frame_index = DBO_INDEX_NONE;
    scene->animations[idx].next_animation = DBO_INDEX_NONE;
    *out_index = idx;
    return DBO_TRUE;
}

dbo_bool dbo_scene_add_animation_set(dbo_scene* scene, dbo_u32* out_index)
{
    dbo_u32 idx;
    if (!dbo__add_index(scene, scene->animation_set_count, DBO_MAX_ANIMATION_SETS, &idx, "animation set capacity exceeded"))
    {
        return DBO_FALSE;
    }
    scene->animation_set_count = idx + 1UL;
    memset(&scene->animation_sets[idx], 0, sizeof(scene->animation_sets[idx]));
    scene->animation_sets[idx].name.offset = DBO_INDEX_NONE;
    scene->animation_sets[idx].first_animation = DBO_INDEX_NONE;
    *out_index = idx;
    return DBO_TRUE;
}

static dbo_bool dbo__frame_name_equals(const dbo_scene* scene, dbo_blob_ref a, dbo_blob_ref b)
{
    const char* sa;
    const char* sb;
    if (a.offset == DBO_INDEX_NONE || b.offset == DBO_INDEX_NONE)
    {
        return DBO_FALSE;
    }
    sa = dbo_blob_cstr(scene, a);
    sb = dbo_blob_cstr(scene, b);
    if (sa == 0 || sb == 0)
    {
        return DBO_FALSE;
    }
    return strcmp(sa, sb) == 0 ? DBO_TRUE : DBO_FALSE;
}

dbo_bool dbo_scene_resolve_animation_frames(dbo_scene* scene)
{
    dbo_u32 ai;
    dbo_u32 fi;
    if (scene == 0)
    {
        return DBO_FALSE;
    }
    for (ai = 0UL; ai < scene->animation_count; ++ai)
    {
        scene->animations[ai].frame_index = DBO_INDEX_NONE;
        for (fi = 0UL; fi < scene->frame_count; ++fi)
        {
            if (dbo__frame_name_equals(scene, scene->animations[ai].name, scene->frames[fi].name))
            {
                scene->animations[ai].frame_index = fi;
                break;
            }
        }
    }
    return DBO_TRUE;
}

dbo_bool dbo__read_u8(dbo_reader* r, dbo_u8* out_v)
{
    if (r == 0 || out_v == 0)
    {
        return DBO_FALSE;
    }
    if (r->pos + 1UL > r->size)
    {
        dbo__set_error(r->scene, "unexpected EOF");
        return DBO_FALSE;
    }
    *out_v = r->data[r->pos];
    r->pos += 1UL;
    return DBO_TRUE;
}

dbo_bool dbo__read_u32(dbo_reader* r, dbo_u32* out_v)
{
    if (r == 0 || out_v == 0)
    {
        return DBO_FALSE;
    }
    if (r->pos + 4UL > r->size)
    {
        dbo__set_error(r->scene, "unexpected EOF");
        return DBO_FALSE;
    }
    *out_v = (dbo_u32)r->data[r->pos]
           | ((dbo_u32)r->data[r->pos + 1UL] << 8)
           | ((dbo_u32)r->data[r->pos + 2UL] << 16)
           | ((dbo_u32)r->data[r->pos + 3UL] << 24);
    r->pos += 4UL;
    return DBO_TRUE;
}

dbo_bool dbo__read_bytes(dbo_reader* r, void* out, dbo_u32 size)
{
    if (r == 0)
    {
        return DBO_FALSE;
    }
    if (r->pos + size > r->size)
    {
        dbo__set_error(r->scene, "unexpected EOF");
        return DBO_FALSE;
    }
    if (size > 0UL && out != 0)
    {
        memcpy(out, r->data + r->pos, size);
    }
    r->pos += size;
    return DBO_TRUE;
}

dbo_bool dbo__skip(dbo_reader* r, dbo_u32 size)
{
    return dbo__read_bytes(r, 0, size);
}

dbo_bool dbo__peek_remaining(const dbo_reader* r, dbo_u32* out_size)
{
    if (r == 0 || out_size == 0)
    {
        return DBO_FALSE;
    }
    *out_size = r->size - r->pos;
    return DBO_TRUE;
}

static dbo_bool dbo__writer_ensure(dbo_writer* w, dbo_u32 add)
{
    if (w == 0)
    {
        return DBO_FALSE;
    }
    if (w->pos + add < w->pos)
    {
        if (w->error_text != 0 && w->error_text_size > 0UL)
        {
            strncpy(w->error_text, "size overflow", w->error_text_size - 1UL);
            w->error_text[w->error_text_size - 1UL] = '\0';
        }
        return DBO_FALSE;
    }
    if (w->data != 0 && w->pos + add > w->capacity)
    {
        if (w->error_text != 0 && w->error_text_size > 0UL)
        {
            strncpy(w->error_text, "output buffer too small", w->error_text_size - 1UL);
            w->error_text[w->error_text_size - 1UL] = '\0';
        }
        return DBO_FALSE;
    }
    return DBO_TRUE;
}

dbo_bool dbo__write_u8(dbo_writer* w, dbo_u8 v)
{
    if (!dbo__writer_ensure(w, 1UL))
    {
        return DBO_FALSE;
    }
    if (w->data != 0)
    {
        w->data[w->pos] = v;
    }
    w->pos += 1UL;
    return DBO_TRUE;
}

dbo_bool dbo__write_u32(dbo_writer* w, dbo_u32 v)
{
    if (!dbo__writer_ensure(w, 4UL))
    {
        return DBO_FALSE;
    }
    if (w->data != 0)
    {
        w->data[w->pos + 0UL] = (dbo_u8)(v & 0xFFUL);
        w->data[w->pos + 1UL] = (dbo_u8)((v >> 8) & 0xFFUL);
        w->data[w->pos + 2UL] = (dbo_u8)((v >> 16) & 0xFFUL);
        w->data[w->pos + 3UL] = (dbo_u8)((v >> 24) & 0xFFUL);
    }
    w->pos += 4UL;
    return DBO_TRUE;
}

dbo_bool dbo__write_bytes(dbo_writer* w, const void* data, dbo_u32 size)
{
    if (!dbo__writer_ensure(w, size))
    {
        return DBO_FALSE;
    }
    if (w->data != 0 && size > 0UL)
    {
        if (data != 0)
        {
            memcpy(w->data + w->pos, data, size);
        }
        else
        {
            memset(w->data + w->pos, 0, size);
        }
    }
    w->pos += size;
    return DBO_TRUE;
}

dbo_bool dbo__patch_u32(dbo_writer* w, dbo_u32 at, dbo_u32 v)
{
    if (w == 0)
    {
        return DBO_FALSE;
    }
    if (w->data == 0)
    {
        return DBO_TRUE;
    }
    if (at + 4UL > w->capacity)
    {
        return DBO_FALSE;
    }
    w->data[at + 0UL] = (dbo_u8)(v & 0xFFUL);
    w->data[at + 1UL] = (dbo_u8)((v >> 8) & 0xFFUL);
    w->data[at + 2UL] = (dbo_u8)((v >> 16) & 0xFFUL);
    w->data[at + 3UL] = (dbo_u8)((v >> 24) & 0xFFUL);
    return DBO_TRUE;
}

dbo_bool dbo__begin_chunk(dbo_writer* w, dbo_u32 code, dbo_u32* out_size_pos)
{
    if (out_size_pos == 0)
    {
        return DBO_FALSE;
    }
    if (!dbo__write_u32(w, code))
    {
        return DBO_FALSE;
    }
    *out_size_pos = w->pos;
    if (!dbo__write_u32(w, 0UL))
    {
        return DBO_FALSE;
    }
    return DBO_TRUE;
}

dbo_bool dbo__end_chunk(dbo_writer* w, dbo_u32 size_pos)
{
    dbo_u32 size;
    if (w == 0)
    {
        return DBO_FALSE;
    }
    if (w->pos < size_pos + 4UL)
    {
        return DBO_FALSE;
    }
    size = w->pos - (size_pos + 4UL);
    return dbo__patch_u32(w, size_pos, size);
}

dbo_bool dbo__write_terminator(dbo_writer* w)
{
    if (!dbo__write_u32(w, 0UL))
    {
        return DBO_FALSE;
    }
    if (!dbo__write_u32(w, 0UL))
    {
        return DBO_FALSE;
    }
    return DBO_TRUE;
}

dbo_bool dbo__read_string_ref(dbo_reader* r, dbo_blob_ref* out_ref)
{
    dbo_u32 len;
    dbo_blob_ref ref;
    if (!dbo__read_u32(r, &len))
    {
        return DBO_FALSE;
    }
    if (!dbo_scene_copy_bytes(r->scene, 0, len + 1UL, 1UL, &ref))
    {
        return DBO_FALSE;
    }
    memcpy(r->scene->blob + ref.offset, r->data + r->pos, len);
    r->scene->blob[ref.offset + len] = 0;
    ref.size = len;
    if (!dbo__skip(r, len))
    {
        return DBO_FALSE;
    }
    *out_ref = ref;
    return DBO_TRUE;
}

dbo_bool dbo__write_string_ref(dbo_writer* w, const dbo_scene* scene, dbo_blob_ref ref)
{
    const char* str;
    dbo_u32 len;
    if (ref.offset == DBO_INDEX_NONE)
    {
        len = 0UL;
        if (!dbo__write_u32(w, len))
        {
            return DBO_FALSE;
        }
        return DBO_TRUE;
    }
    str = dbo_blob_cstr(scene, ref);
    if (str == 0)
    {
        return DBO_FALSE;
    }
    len = ref.size;
    if (!dbo__write_u32(w, len))
    {
        return DBO_FALSE;
    }
    if (!dbo__write_bytes(w, str, len))
    {
        return DBO_FALSE;
    }
    return DBO_TRUE;
}

dbo_bool dbo__read_vec3_fx(dbo_reader* r, dbo_fx32 out3[3])
{
    dbo_u32 bits;
    dbo_u32 i;
    for (i = 0UL; i < 3UL; ++i)
    {
        if (!dbo__read_u32(r, &bits))
        {
            return DBO_FALSE;
        }
        out3[i] = dbo_fx_from_float_bits(bits);
    }
    return DBO_TRUE;
}

dbo_bool dbo__write_vec3_fx(dbo_writer* w, const dbo_fx32 v[3])
{
    dbo_u32 i;
    for (i = 0UL; i < 3UL; ++i)
    {
        if (!dbo__write_u32(w, dbo_fx_to_float_bits(v[i])))
        {
            return DBO_FALSE;
        }
    }
    return DBO_TRUE;
}

dbo_bool dbo__read_mat4_fx(dbo_reader* r, dbo_fx32 out16[16])
{
    dbo_u32 bits;
    dbo_u32 i;
    for (i = 0UL; i < 16UL; ++i)
    {
        if (!dbo__read_u32(r, &bits))
        {
            return DBO_FALSE;
        }
        out16[i] = dbo_fx_from_float_bits(bits);
    }
    return DBO_TRUE;
}

dbo_bool dbo__write_mat4_fx(dbo_writer* w, const dbo_fx32 m[16])
{
    dbo_u32 i;
    for (i = 0UL; i < 16UL; ++i)
    {
        if (!dbo__write_u32(w, dbo_fx_to_float_bits(m[i])))
        {
            return DBO_FALSE;
        }
    }
    return DBO_TRUE;
}

dbo_bool dbo__read_material_fx(dbo_reader* r, dbo_material* out_mat)
{
    dbo_u32 i;
    dbo_u32 bits;
    if (out_mat == 0)
    {
        return DBO_FALSE;
    }
    for (i = 0UL; i < 4UL; ++i)
    {
        if (!dbo__read_u32(r, &bits)) return DBO_FALSE;
        out_mat->diffuse[i] = dbo_fx_from_float_bits(bits);
    }
    for (i = 0UL; i < 4UL; ++i)
    {
        if (!dbo__read_u32(r, &bits)) return DBO_FALSE;
        out_mat->ambient[i] = dbo_fx_from_float_bits(bits);
    }
    for (i = 0UL; i < 4UL; ++i)
    {
        if (!dbo__read_u32(r, &bits)) return DBO_FALSE;
        out_mat->specular[i] = dbo_fx_from_float_bits(bits);
    }
    for (i = 0UL; i < 4UL; ++i)
    {
        if (!dbo__read_u32(r, &bits)) return DBO_FALSE;
        out_mat->emissive[i] = dbo_fx_from_float_bits(bits);
    }
    if (!dbo__read_u32(r, &bits)) return DBO_FALSE;
    out_mat->power = dbo_fx_from_float_bits(bits);
    return DBO_TRUE;
}

dbo_bool dbo__write_material_fx(dbo_writer* w, const dbo_material* mat)
{
    dbo_u32 i;
    if (mat == 0)
    {
        return DBO_FALSE;
    }
    for (i = 0UL; i < 4UL; ++i)
    {
        if (!dbo__write_u32(w, dbo_fx_to_float_bits(mat->diffuse[i]))) return DBO_FALSE;
    }
    for (i = 0UL; i < 4UL; ++i)
    {
        if (!dbo__write_u32(w, dbo_fx_to_float_bits(mat->ambient[i]))) return DBO_FALSE;
    }
    for (i = 0UL; i < 4UL; ++i)
    {
        if (!dbo__write_u32(w, dbo_fx_to_float_bits(mat->specular[i]))) return DBO_FALSE;
    }
    for (i = 0UL; i < 4UL; ++i)
    {
        if (!dbo__write_u32(w, dbo_fx_to_float_bits(mat->emissive[i]))) return DBO_FALSE;
    }
    if (!dbo__write_u32(w, dbo_fx_to_float_bits(mat->power))) return DBO_FALSE;
    return DBO_TRUE;
}
