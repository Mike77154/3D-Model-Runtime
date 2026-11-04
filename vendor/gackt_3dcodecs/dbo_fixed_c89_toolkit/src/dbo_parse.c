#include "dbo_int.h"

static dbo_bool dbo__read_code(dbo_reader* r, dbo_u32* code, dbo_u32* size)
{
    if (!dbo__read_u32(r, code)) return DBO_FALSE;
    if (!dbo__read_u32(r, size)) return DBO_FALSE;
    return DBO_TRUE;
}

static dbo_bool dbo__read_string_local(dbo_reader* r, char* buffer, dbo_u32 buffer_size)
{
    dbo_u32 len;
    if (!dbo__read_u32(r, &len))
    {
        return DBO_FALSE;
    }
    if (buffer != 0 && buffer_size > 0UL)
    {
        dbo_u32 copy_len = len;
        if (copy_len >= buffer_size)
        {
            copy_len = buffer_size - 1UL;
        }
        if (copy_len > 0UL)
        {
            memcpy(buffer, r->data + r->pos, copy_len);
        }
        buffer[copy_len] = '\0';
    }
    return dbo__skip(r, len);
}

static dbo_bool dbo__read_raw_blob(dbo_reader* r, dbo_u32 size, dbo_u32 align, dbo_blob_ref* out_ref)
{
    if (!dbo_scene_copy_bytes(r->scene, r->data + r->pos, size, align, out_ref))
    {
        return DBO_FALSE;
    }
    return dbo__skip(r, size);
}

static dbo_bool dbo__capture_unknown_chunk(dbo_reader* r, dbo_u32 scope_kind, dbo_u32 owner_index, dbo_u32 code, dbo_u32 size, dbo_u32 event_index)
{
    dbo_u32 unknown_index = r->scene->unknown_chunk_count;
    if (!dbo__scene_store_unknown_chunk(r->scene, scope_kind, owner_index, code, r->data + r->pos, size))
    {
        return DBO_FALSE;
    }
    if (event_index != DBO_INDEX_NONE && event_index < r->scene->chunk_event_count)
    {
        r->scene->chunk_events[event_index].aux_index = unknown_index;
    }
    return dbo__skip(r, size);
}

static dbo_bool dbo__parse_frame(dbo_reader* r, dbo_u32 parent_index, dbo_u32* out_index);
static dbo_bool dbo__parse_mesh(dbo_reader* r, dbo_u32* out_index);
static dbo_bool dbo__parse_animation(dbo_reader* r, dbo_u32* out_index);

static dbo_bool dbo__parse_bones(dbo_reader* r, dbo_mesh* mesh)
{
    dbo_u32 code;
    dbo_u32 code_size;
    dbo_u32 bi;
    dbo_u32 base_index;

    if (!dbo__read_code(r, &code, &code_size))
    {
        return DBO_FALSE;
    }

    if (code == 0UL)
    {
        mesh->first_bone = DBO_INDEX_NONE;
        return DBO_TRUE;
    }

    if (mesh->bone_count > 0UL)
    {
        mesh->first_bone = r->scene->bone_count;
    }
    else
    {
        mesh->first_bone = DBO_INDEX_NONE;
    }

    for (bi = 0UL; bi < mesh->bone_count; ++bi)
    {
        dbo_u32 bone_index;
        dbo_bone* bone;
        if (!dbo_scene_add_bone(r->scene, &bone_index))
        {
            return DBO_FALSE;
        }
        bone = &r->scene->bones[bone_index];
        base_index = bone_index;

        while (code > 0UL)
        {
            dbo_u32 event_index;
            if (!dbo__scene_store_chunk_event(r->scene, DBO_SCOPE_BONE, base_index, code, DBO_INDEX_NONE, &event_index))
            {
                return DBO_FALSE;
            }
            switch (code)
            {
            case DBOBLOCK_BONES_NAME:
                if (!dbo__read_string_ref(r, &bone->name)) return DBO_FALSE;
                break;
            case DBOBLOCK_BONES_NUMINFLUENCES:
                if (!dbo__read_u32(r, &bone->num_influences)) return DBO_FALSE;
                break;
            case DBOBLOCK_BONES_VERTICES:
                if (!dbo__read_raw_blob(r, bone->num_influences * 4UL, 4UL, &bone->vertices)) return DBO_FALSE;
                break;
            case DBOBLOCK_BONES_WEIGHTS:
            {
                dbo_blob_ref ref;
                dbo_u32 i;
                dbo_u32 bits;
                dbo_fx32* dst;
                if (!dbo_scene_copy_bytes(r->scene, 0, bone->num_influences * 4UL, 4UL, &ref)) return DBO_FALSE;
                dst = (dbo_fx32*)(r->scene->blob + ref.offset);
                for (i = 0UL; i < bone->num_influences; ++i)
                {
                    if (!dbo__read_u32(r, &bits)) return DBO_FALSE;
                    dst[i] = dbo_fx_from_float_bits(bits);
                }
                bone->weights = ref;
                break;
            }
            case DBOBLOCK_BONES_TRANSLATIONMATRIX:
                if (!dbo__read_mat4_fx(r, bone->translation)) return DBO_FALSE;
                break;
            default:
                if (!dbo__capture_unknown_chunk(r, DBO_SCOPE_BONE, base_index, code, code_size, event_index)) return DBO_FALSE;
                break;
            }

            if (!dbo__read_code(r, &code, &code_size))
            {
                return DBO_FALSE;
            }
        }

                if (bi + 1UL < mesh->bone_count)
        {
            if (!dbo__read_code(r, &code, &code_size))
            {
                return DBO_FALSE;
            }
        }
    }

    return DBO_TRUE;
}

static dbo_bool dbo__parse_textures(dbo_reader* r, dbo_mesh* mesh)
{
    dbo_u32 code;
    dbo_u32 code_size;
    dbo_u32 ti;

    if (!dbo__read_code(r, &code, &code_size))
    {
        return DBO_FALSE;
    }

    if (code == 0UL)
    {
        mesh->first_texture = DBO_INDEX_NONE;
        return DBO_TRUE;
    }

    if (mesh->texture_count > 0UL)
    {
        mesh->first_texture = r->scene->texture_count;
    }
    else
    {
        mesh->first_texture = DBO_INDEX_NONE;
    }

    for (ti = 0UL; ti < mesh->texture_count; ++ti)
    {
        dbo_u32 tex_index;
        dbo_texture* tex;
        if (!dbo_scene_add_texture(r->scene, &tex_index))
        {
            return DBO_FALSE;
        }
        tex = &r->scene->textures[tex_index];

        while (code > 0UL)
        {
            dbo_u32 event_index;
            if (!dbo__scene_store_chunk_event(r->scene, DBO_SCOPE_TEXTURE, tex_index, code, DBO_INDEX_NONE, &event_index))
            {
                return DBO_FALSE;
            }
            switch (code)
            {
            case DBOBLOCK_TEXTURE_NAME:
                if (!dbo__read_string_ref(r, &tex->name)) return DBO_FALSE;
                break;
            case DBOBLOCK_TEXTURE_STAGE:
                if (!dbo__read_u32(r, &tex->stage)) return DBO_FALSE;
                break;
            case DBOBLOCK_TEXTURE_BMODE:
                if (!dbo__read_u32(r, &tex->blend_mode)) return DBO_FALSE;
                break;
            case DBOBLOCK_TEXTURE_BARG1:
                if (!dbo__read_u32(r, &tex->blend_arg1)) return DBO_FALSE;
                break;
            case DBOBLOCK_TEXTURE_BARG2:
                if (!dbo__read_u32(r, &tex->blend_arg2)) return DBO_FALSE;
                break;
            case DBOBLOCK_TEXTURE_ADDRU:
                if (!dbo__read_u32(r, &tex->address_u)) return DBO_FALSE;
                break;
            case DBOBLOCK_TEXTURE_ADDRV:
                if (!dbo__read_u32(r, &tex->address_v)) return DBO_FALSE;
                break;
            case DBOBLOCK_TEXTURE_MAG:
                if (!dbo__read_u32(r, &tex->mag_state)) return DBO_FALSE;
                break;
            case DBOBLOCK_TEXTURE_MIN:
                if (!dbo__read_u32(r, &tex->min_state)) return DBO_FALSE;
                break;
            case DBOBLOCK_TEXTURE_MIP:
                if (!dbo__read_u32(r, &tex->mip_state)) return DBO_FALSE;
                break;
            case DBOBLOCK_TEXTURE_TCMODE:
                if (!dbo__read_u32(r, &tex->texcoord_mode)) return DBO_FALSE;
                break;
            case DBOBLOCK_TEXTURE_PRIMST:
                if (!dbo__read_u32(r, (dbo_u32*)&tex->start_index)) return DBO_FALSE;
                break;
            case DBOBLOCK_TEXTURE_PRIMCN:
                if (!dbo__read_u32(r, (dbo_u32*)&tex->primitive_count)) return DBO_FALSE;
                break;
            default:
                if (!dbo__capture_unknown_chunk(r, DBO_SCOPE_TEXTURE, tex_index, code, code_size, event_index)) return DBO_FALSE;
                break;
            }

            if (!dbo__read_code(r, &code, &code_size))
            {
                return DBO_FALSE;
            }
        }

        if (ti + 1UL < mesh->texture_count)
        {
            if (!dbo__read_code(r, &code, &code_size))
            {
                return DBO_FALSE;
            }
        }
    }

    return DBO_TRUE;
}

static dbo_bool dbo__parse_multimaterials(dbo_reader* r, dbo_mesh* mesh)
{
    dbo_u32 code;
    dbo_u32 code_size;
    dbo_u32 mi;

    if (!dbo__read_code(r, &code, &code_size))
    {
        return DBO_FALSE;
    }

    if (code == 0UL)
    {
        mesh->first_multimaterial = DBO_INDEX_NONE;
        return DBO_TRUE;
    }

    if (mesh->multimaterial_count > 0UL)
    {
        mesh->first_multimaterial = r->scene->multimaterial_count;
    }
    else
    {
        mesh->first_multimaterial = DBO_INDEX_NONE;
    }

    for (mi = 0UL; mi < mesh->multimaterial_count; ++mi)
    {
        dbo_u32 idx;
        dbo_multimaterial* mm;
        if (!dbo_scene_add_multimaterial(r->scene, &idx))
        {
            return DBO_FALSE;
        }
        mm = &r->scene->multimaterials[idx];

        while (code > 0UL)
        {
            dbo_u32 event_index;
            if (!dbo__scene_store_chunk_event(r->scene, DBO_SCOPE_MULTIMATERIAL, idx, code, DBO_INDEX_NONE, &event_index))
            {
                return DBO_FALSE;
            }
            switch (code)
            {
            case DBOBLOCK_MULTIMAT_NAME:
                if (!dbo__read_string_ref(r, &mm->name)) return DBO_FALSE;
                break;
            case DBOBLOCK_MULTIMAT_MATERIAL:
                if (!dbo__read_material_fx(r, &mm->material)) return DBO_FALSE;
                break;
            case DBOBLOCK_MULTIMAT_START:
                if (!dbo__read_u32(r, &mm->index_start)) return DBO_FALSE;
                break;
            case DBOBLOCK_MULTIMAT_COUNT:
                if (!dbo__read_u32(r, &mm->index_count)) return DBO_FALSE;
                break;
            case DBOBLOCK_MULTIMAT_POLY:
                if (!dbo__read_u32(r, &mm->poly_count)) return DBO_FALSE;
                break;
            default:
                if (!dbo__capture_unknown_chunk(r, DBO_SCOPE_MULTIMATERIAL, idx, code, code_size, event_index)) return DBO_FALSE;
                break;
            }

            if (!dbo__read_code(r, &code, &code_size))
            {
                return DBO_FALSE;
            }
        }

        if (mi + 1UL < mesh->multimaterial_count)
        {
            if (!dbo__read_code(r, &code, &code_size))
            {
                return DBO_FALSE;
            }
        }
    }

    return DBO_TRUE;
}

static dbo_bool dbo__parse_mesh(dbo_reader* r, dbo_u32* out_index)
{
    dbo_u32 code;
    dbo_u32 code_size;
    dbo_u32 mesh_index;
    dbo_mesh* mesh;

    if (!dbo__read_code(r, &code, &code_size))
    {
        return DBO_FALSE;
    }

    if (code == 0UL)
    {
        *out_index = DBO_INDEX_NONE;
        return DBO_TRUE;
    }

    if (!dbo_scene_add_mesh(r->scene, &mesh_index))
    {
        return DBO_FALSE;
    }
    mesh = &r->scene->meshes[mesh_index];
    *out_index = mesh_index;

    while (code > 0UL)
    {
        dbo_u32 event_index;
        if (!dbo__scene_store_chunk_event(r->scene, DBO_SCOPE_MESH, mesh_index, code, DBO_INDEX_NONE, &event_index))
        {
            return DBO_FALSE;
        }
        switch (code)
        {
        case DBOBLOCK_MESH_FVF:
            if (!dbo__read_u32(r, &mesh->fvf)) return DBO_FALSE;
            break;
        case DBOBLOCK_MESH_FVFSIZE:
            if (!dbo__read_u32(r, &mesh->fvf_size)) return DBO_FALSE;
            break;
        case DBOBLOCK_MESH_VERTEXCOUNT:
            if (!dbo__read_u32(r, &mesh->vertex_count)) return DBO_FALSE;
            break;
        case DBOBLOCK_MESH_INDEXCOUNT:
            if (!dbo__read_u32(r, &mesh->index_count)) return DBO_FALSE;
            break;
        case DBOBLOCK_MESH_VERTEXDEC:
        {
            dbo_u32 read_size = code_size;
            if (read_size == 4UL)
            {
                read_size = DBO_VERTEX_DECL_BYTES;
            }
            if (read_size > DBO_VERTEX_DECL_BYTES)
            {
                read_size = DBO_VERTEX_DECL_BYTES;
            }
            if (!dbo__read_raw_blob(r, read_size, 4UL, &mesh->vertex_decl)) return DBO_FALSE;
            mesh->vertex_decl_len = read_size;
            if (code_size > read_size)
            {
                if (!dbo__skip(r, code_size - read_size)) return DBO_FALSE;
            }
            break;
        }
        case DBOBLOCK_MESH_SUBFRAMES:
        {
            dbo_u32 i;
            dbo_blob_ref ref;
            dbo_u32* dst = 0;
            if (!dbo__read_u32(r, &mesh->subframe_count)) return DBO_FALSE;
            if (mesh->subframe_count > 0UL)
            {
                if (!dbo_scene_copy_bytes(r->scene, 0, mesh->subframe_count * 4UL, 4UL, &ref)) return DBO_FALSE;
                dst = (dbo_u32*)(r->scene->blob + ref.offset);
                for (i = 0UL; i < mesh->subframe_count; ++i)
                {
                    if (!dbo__parse_frame(r, DBO_INDEX_NONE, &dst[i])) return DBO_FALSE;
                }
                mesh->subframe_indices = ref;
            }
            break;
        }
        case DBOBLOCK_MESH_VERTEXDATA:
        {
            dbo_u32 size = mesh->fvf_size * mesh->vertex_count;
            if (size == 0UL)
            {
                size = code_size;
            }
            if (!dbo__read_raw_blob(r, size, 4UL, &mesh->vertex_data)) return DBO_FALSE;
            if (code_size > size)
            {
                if (!dbo__skip(r, code_size - size)) return DBO_FALSE;
            }
            break;
        }
        case DBOBLOCK_MESH_INDEXDATA:
        {
            dbo_u32 size = mesh->index_count * 2UL;
            if (size == 0UL)
            {
                size = code_size;
            }
            if (!dbo__read_raw_blob(r, size, 2UL, &mesh->index_data)) return DBO_FALSE;
            if (code_size > size)
            {
                if (!dbo__skip(r, code_size - size)) return DBO_FALSE;
            }
            break;
        }
        case DBOBLOCK_MESH_PRIMTYPE:
            if (!dbo__read_u32(r, &mesh->primitive_type)) return DBO_FALSE;
            break;
        case DBOBLOCK_MESH_DRAWVERTCOUNT:
            if (!dbo__read_u32(r, &mesh->draw_vertex_count)) return DBO_FALSE;
            break;
        case DBOBLOCK_MESH_DRAWPRIMCOUNT:
            if (!dbo__read_u32(r, &mesh->draw_primitive_count)) return DBO_FALSE;
            break;
        case DBOBLOCK_MESH_BONECOUNT:
            if (!dbo__read_u32(r, &mesh->bone_count)) return DBO_FALSE;
            break;
        case DBOBLOCK_MESH_BONESDATA:
            if (!dbo__parse_bones(r, mesh)) return DBO_FALSE;
            break;
        case DBOBLOCK_MESH_USEMATERIAL:
        {
            dbo_u8 b;
            if (!dbo__read_u8(r, &b)) return DBO_FALSE;
            mesh->uses_material = (b != 0U) ? DBO_TRUE : DBO_FALSE;
            break;
        }
        case DBOBLOCK_MESH_MATERIAL:
            if (!dbo__read_material_fx(r, &mesh->material)) return DBO_FALSE;
            break;
        case DBOBLOCK_MESH_TEXTURECOUNT:
            if (!dbo__read_u32(r, &mesh->texture_count)) return DBO_FALSE;
            break;
        case DBOBLOCK_MESH_TEXTURES:
            if (!dbo__parse_textures(r, mesh)) return DBO_FALSE;
            break;
        case DBOBLOCK_MESH_USEMULTIMAT:
        {
            dbo_u8 b2;
            if (!dbo__read_u8(r, &b2)) return DBO_FALSE;
            mesh->use_multimaterial = (b2 != 0U) ? DBO_TRUE : DBO_FALSE;
            break;
        }
        case DBOBLOCK_MESH_MULTIMATCOUNT:
            if (!dbo__read_u32(r, &mesh->multimaterial_count)) return DBO_FALSE;
            break;
        case DBOBLOCK_MESH_MULTIMAT:
            if (!dbo__parse_multimaterials(r, mesh)) return DBO_FALSE;
            break;
        case DBOBLOCK_MESH_WIREFRAME:
        {
            dbo_u8 b3;
            if (!dbo__read_u8(r, &b3)) return DBO_FALSE;
            mesh->wireframe = (b3 != 0U) ? DBO_TRUE : DBO_FALSE;
            break;
        }
        case DBOBLOCK_MESH_LIGHT:
        {
            dbo_u8 b4;
            if (!dbo__read_u8(r, &b4)) return DBO_FALSE;
            mesh->light = (b4 != 0U) ? DBO_TRUE : DBO_FALSE;
            break;
        }
        case DBOBLOCK_MESH_CULL:
        {
            dbo_u8 b5;
            if (!dbo__read_u8(r, &b5)) return DBO_FALSE;
            mesh->cull = (b5 != 0U) ? DBO_TRUE : DBO_FALSE;
            break;
        }
        case DBOBLOCK_MESH_FOG:
        {
            dbo_u8 b6;
            if (!dbo__read_u8(r, &b6)) return DBO_FALSE;
            mesh->fog = (b6 != 0U) ? DBO_TRUE : DBO_FALSE;
            break;
        }
        case DBOBLOCK_MESH_AMBIENT:
        {
            dbo_u8 b7;
            if (!dbo__read_u8(r, &b7)) return DBO_FALSE;
            mesh->ambient = (b7 != 0U) ? DBO_TRUE : DBO_FALSE;
            break;
        }
        case DBOBLOCK_MESH_TRANSPARENCY:
        {
            dbo_u8 b8;
            if (!dbo__read_u8(r, &b8)) return DBO_FALSE;
            mesh->transparency = (b8 != 0U) ? DBO_TRUE : DBO_FALSE;
            break;
        }
        case DBOBLOCK_MESH_GHOST:
        {
            dbo_u8 b9;
            if (!dbo__read_u8(r, &b9)) return DBO_FALSE;
            mesh->ghost = (b9 != 0U) ? DBO_TRUE : DBO_FALSE;
            break;
        }
        case DBOBLOCK_MESH_GHOSTMODE:
            if (!dbo__read_u32(r, &mesh->ghost_mode)) return DBO_FALSE;
            break;
        case DBOBLOCK_MESH_VISIBLE:
        {
            dbo_u8 b10;
            if (!dbo__read_u8(r, &b10)) return DBO_FALSE;
            mesh->visible = (b10 != 0U) ? DBO_TRUE : DBO_FALSE;
            break;
        }
        case DBOBLOCK_MESH_LINKED:
        {
            dbo_u8 b11;
            if (!dbo__read_u8(r, &b11)) return DBO_FALSE;
            mesh->linked = (b11 != 0U) ? DBO_TRUE : DBO_FALSE;
            break;
        }
        case DBOBLOCK_MESH_FXEFFECTNAME:
            if (!dbo__read_string_ref(r, &mesh->effect_name)) return DBO_FALSE;
            mesh->use_vertex_shader = DBO_TRUE;
            break;
        case DBOBLOCK_MESH_ARBITARYVALUE:
            if (!dbo__read_u32(r, &mesh->arbitary_value)) return DBO_FALSE;
            break;
        case DBOBLOCK_MESH_ZBIASFLAG:
        {
            dbo_u8 bz;
            if (!dbo__read_u8(r, &bz)) return DBO_FALSE;
            mesh->z_bias_active = (bz != 0U) ? DBO_TRUE : DBO_FALSE;
            break;
        }
        case DBOBLOCK_MESH_ZBIASSLOPE:
        {
            dbo_u32 bits;
            if (!dbo__read_u32(r, &bits)) return DBO_FALSE;
            mesh->z_bias_slope = dbo_fx_from_float_bits(bits);
            break;
        }
        case DBOBLOCK_MESH_ZBIASDEPTH:
        {
            dbo_u32 bits2;
            if (!dbo__read_u32(r, &bits2)) return DBO_FALSE;
            mesh->z_bias_depth = dbo_fx_from_float_bits(bits2);
            break;
        }
        case DBOBLOCK_MESH_ZREAD:
        {
            dbo_u8 bz2;
            if (!dbo__read_u8(r, &bz2)) return DBO_FALSE;
            mesh->z_read = (bz2 != 0U) ? DBO_TRUE : DBO_FALSE;
            break;
        }
        case DBOBLOCK_MESH_ZWRITE:
        {
            dbo_u8 bz3;
            if (!dbo__read_u8(r, &bz3)) return DBO_FALSE;
            mesh->z_write = (bz3 != 0U) ? DBO_TRUE : DBO_FALSE;
            break;
        }
        case DBOBLOCK_MESH_ALPHATESTVALUE:
            if (!dbo__read_u32(r, &mesh->alpha_test_value)) return DBO_FALSE;
            break;
        default:
            if (!dbo__capture_unknown_chunk(r, DBO_SCOPE_MESH, mesh_index, code, code_size, event_index)) return DBO_FALSE;
            break;
        }

        if (!dbo__read_code(r, &code, &code_size))
        {
            return DBO_FALSE;
        }
    }

    return DBO_TRUE;
}

static dbo_bool dbo__parse_frame(dbo_reader* r, dbo_u32 parent_index, dbo_u32* out_index)
{
    dbo_u32 code;
    dbo_u32 code_size;
    dbo_u32 frame_index;
    dbo_frame* frame;

    if (!dbo__read_code(r, &code, &code_size))
    {
        return DBO_FALSE;
    }

    if (code == 0UL)
    {
        *out_index = DBO_INDEX_NONE;
        return DBO_TRUE;
    }

    if (!dbo_scene_add_frame(r->scene, &frame_index))
    {
        return DBO_FALSE;
    }
    frame = &r->scene->frames[frame_index];
    frame->parent = parent_index;
    *out_index = frame_index;

    while (code > 0UL)
    {
        dbo_u32 event_index;
        if (!dbo__scene_store_chunk_event(r->scene, DBO_SCOPE_FRAME, frame_index, code, DBO_INDEX_NONE, &event_index))
        {
            return DBO_FALSE;
        }
        switch (code)
        {
        case DBOBLOCK_FRAME_NAME:
            if (!dbo__read_string_ref(r, &frame->name)) return DBO_FALSE;
            break;
        case DBOBLOCK_FRAME_MATRIX:
            if (!dbo__read_mat4_fx(r, frame->original)) return DBO_FALSE;
            break;
        case DBOBLOCK_FRAME_MESH:
            if (!dbo__parse_mesh(r, &frame->mesh)) return DBO_FALSE;
            break;
        case DBOBLOCK_FRAME_CHILD:
            if (!dbo__parse_frame(r, frame_index, &frame->child)) return DBO_FALSE;
            if (frame->child != DBO_INDEX_NONE)
            {
                r->scene->frames[frame->child].parent = frame_index;
            }
            break;
        case DBOBLOCK_FRAME_SIBLING:
            if (!dbo__parse_frame(r, parent_index, &frame->sibling)) return DBO_FALSE;
            break;
        case DBOBLOCK_FRAME_OFFSET:
            if (!dbo__read_vec3_fx(r, frame->offset)) return DBO_FALSE;
            break;
        case DBOBLOCK_FRAME_ROTATION:
            if (!dbo__read_vec3_fx(r, frame->rotation)) return DBO_FALSE;
            break;
        case DBOBLOCK_FRAME_SCALE:
            if (!dbo__read_vec3_fx(r, frame->scale)) return DBO_FALSE;
            break;
        default:
            if (!dbo__capture_unknown_chunk(r, DBO_SCOPE_FRAME, frame_index, code, code_size, event_index)) return DBO_FALSE;
            break;
        }

        if (!dbo__read_code(r, &code, &code_size))
        {
            return DBO_FALSE;
        }
    }

    return DBO_TRUE;
}

static dbo_bool dbo__parse_position_keys(dbo_reader* r, dbo_animation* anim)
{
    dbo_blob_ref ref;
    dbo_position_key* dst;
    dbo_u32 i;
    if (anim->num_position_keys == 0UL)
    {
        return DBO_TRUE;
    }
    if (!dbo_scene_copy_bytes(r->scene, 0, anim->num_position_keys * (dbo_u32)sizeof(dbo_position_key), 4UL, &ref))
    {
        return DBO_FALSE;
    }
    dst = (dbo_position_key*)(r->scene->blob + ref.offset);
    for (i = 0UL; i < anim->num_position_keys; ++i)
    {
        if (!dbo__read_u32(r, &dst[i].time)) return DBO_FALSE;
        if (!dbo__read_vec3_fx(r, dst[i].pos)) return DBO_FALSE;
        if (!dbo__read_vec3_fx(r, dst[i].interp)) return DBO_FALSE;
    }
    anim->position_keys = ref;
    return DBO_TRUE;
}

static dbo_bool dbo__parse_rotate_keys(dbo_reader* r, dbo_animation* anim)
{
    dbo_blob_ref ref;
    dbo_rotate_key* dst;
    dbo_u32 i;
    dbo_u32 j;
    dbo_u32 bits;
    if (anim->num_rotate_keys == 0UL)
    {
        return DBO_TRUE;
    }
    if (!dbo_scene_copy_bytes(r->scene, 0, anim->num_rotate_keys * (dbo_u32)sizeof(dbo_rotate_key), 4UL, &ref))
    {
        return DBO_FALSE;
    }
    dst = (dbo_rotate_key*)(r->scene->blob + ref.offset);
    for (i = 0UL; i < anim->num_rotate_keys; ++i)
    {
        if (!dbo__read_u32(r, &dst[i].time)) return DBO_FALSE;
        for (j = 0UL; j < 4UL; ++j)
        {
            if (!dbo__read_u32(r, &bits)) return DBO_FALSE;
            dst[i].quat[j] = dbo_fx_from_float_bits(bits);
        }
    }
    anim->rotate_keys = ref;
    return DBO_TRUE;
}

static dbo_bool dbo__parse_scale_keys(dbo_reader* r, dbo_animation* anim)
{
    dbo_blob_ref ref;
    dbo_scale_key* dst;
    dbo_u32 i;
    if (anim->num_scale_keys == 0UL)
    {
        return DBO_TRUE;
    }
    if (!dbo_scene_copy_bytes(r->scene, 0, anim->num_scale_keys * (dbo_u32)sizeof(dbo_scale_key), 4UL, &ref))
    {
        return DBO_FALSE;
    }
    dst = (dbo_scale_key*)(r->scene->blob + ref.offset);
    for (i = 0UL; i < anim->num_scale_keys; ++i)
    {
        if (!dbo__read_u32(r, &dst[i].time)) return DBO_FALSE;
        if (!dbo__read_vec3_fx(r, dst[i].scale)) return DBO_FALSE;
        if (!dbo__read_vec3_fx(r, dst[i].interp)) return DBO_FALSE;
    }
    anim->scale_keys = ref;
    return DBO_TRUE;
}

static dbo_bool dbo__parse_matrix_keys(dbo_reader* r, dbo_animation* anim)
{
    dbo_blob_ref ref;
    dbo_matrix_key* dst;
    dbo_u32 i;
    if (anim->num_matrix_keys == 0UL)
    {
        return DBO_TRUE;
    }
    if (!dbo_scene_copy_bytes(r->scene, 0, anim->num_matrix_keys * (dbo_u32)sizeof(dbo_matrix_key), 4UL, &ref))
    {
        return DBO_FALSE;
    }
    dst = (dbo_matrix_key*)(r->scene->blob + ref.offset);
    for (i = 0UL; i < anim->num_matrix_keys; ++i)
    {
        if (!dbo__read_u32(r, &dst[i].time)) return DBO_FALSE;
        if (!dbo__read_mat4_fx(r, dst[i].matrix)) return DBO_FALSE;
        if (!dbo__read_mat4_fx(r, dst[i].interp)) return DBO_FALSE;
    }
    anim->matrix_keys = ref;
    return DBO_TRUE;
}

static dbo_bool dbo__parse_bone_matrices(dbo_reader* r, dbo_animation* anim)
{
    dbo_u32 count_a;
    dbo_u32 count_b;
    dbo_u32 total;
    dbo_blob_ref ref;
    dbo_fx32* dst;
    dbo_u32 i;
    dbo_u32 bits;
    if (anim->bone_frame_a < 0 || anim->bone_frame_b < 0)
    {
        dbo__set_error(r->scene, "negative bone matrix dimensions");
        return DBO_FALSE;
    }
    count_a = (dbo_u32)anim->bone_frame_a;
    count_b = (dbo_u32)anim->bone_frame_b;
    total = count_a * count_b * 16UL;
    if (!dbo_scene_copy_bytes(r->scene, 0, total * 4UL, 4UL, &ref))
    {
        return DBO_FALSE;
    }
    dst = (dbo_fx32*)(r->scene->blob + ref.offset);
    for (i = 0UL; i < total; ++i)
    {
        if (!dbo__read_u32(r, &bits)) return DBO_FALSE;
        dst[i] = dbo_fx_from_float_bits(bits);
    }
    anim->bone_matrices = ref;
    return DBO_TRUE;
}

static dbo_bool dbo__parse_animation(dbo_reader* r, dbo_u32* out_index)
{
    dbo_u32 code;
    dbo_u32 code_size;
    dbo_u32 anim_index;
    dbo_animation* anim;

    if (!dbo__read_code(r, &code, &code_size))
    {
        return DBO_FALSE;
    }

    if (code == 0UL)
    {
        *out_index = DBO_INDEX_NONE;
        return DBO_TRUE;
    }

    if (!dbo_scene_add_animation(r->scene, &anim_index))
    {
        return DBO_FALSE;
    }
    anim = &r->scene->animations[anim_index];
    *out_index = anim_index;

    while (code > 0UL)
    {
        dbo_u32 event_index;
        if (!dbo__scene_store_chunk_event(r->scene, DBO_SCOPE_ANIMATION, anim_index, code, DBO_INDEX_NONE, &event_index))
        {
            return DBO_FALSE;
        }
        switch (code)
        {
        case DBOBLOCK_ANIM_NAME:
            if (!dbo__read_string_ref(r, &anim->name)) return DBO_FALSE;
            break;
        case DBOBLOCK_ANIM_NUMPOSKEYS:
            if (!dbo__read_u32(r, &anim->num_position_keys)) return DBO_FALSE;
            break;
        case DBOBLOCK_ANIM_POSDATA:
            if (!dbo__parse_position_keys(r, anim)) return DBO_FALSE;
            break;
        case DBOBLOCK_ANIM_NUMROTKEYS:
            if (!dbo__read_u32(r, &anim->num_rotate_keys)) return DBO_FALSE;
            break;
        case DBOBLOCK_ANIM_ROTDATA:
            if (!dbo__parse_rotate_keys(r, anim)) return DBO_FALSE;
            break;
        case DBOBLOCK_ANIM_NUMSCALEKEYS:
            if (!dbo__read_u32(r, &anim->num_scale_keys)) return DBO_FALSE;
            break;
        case DBOBLOCK_ANIM_SCALEDATA:
            if (!dbo__parse_scale_keys(r, anim)) return DBO_FALSE;
            break;
        case DBOBLOCK_ANIM_NUMMATRIXKEYS:
            if (!dbo__read_u32(r, &anim->num_matrix_keys)) return DBO_FALSE;
            break;
        case DBOBLOCK_ANIM_MATRIXDATA:
            if (!dbo__parse_matrix_keys(r, anim)) return DBO_FALSE;
            break;
        case DBOBLOCK_FRAME_BONETYPE:
            if (!dbo__read_u32(r, &anim->bone_type)) return DBO_FALSE;
            break;
        case DBOBLOCK_FRAME_BONEOFFSETLISTSIZE:
            if (!dbo__read_u32(r, &anim->bone_offset_count)) return DBO_FALSE;
            break;
        case DBOBLOCK_FRAME_BONEOFFSETLIST:
            if (!dbo__read_raw_blob(r, anim->bone_offset_count * 4UL, 4UL, &anim->bone_offsets)) return DBO_FALSE;
            break;
        case DBOBLOCK_FRAME_BONEMATRIXSIZEA:
            if (!dbo__read_u32(r, (dbo_u32*)&anim->bone_frame_a)) return DBO_FALSE;
            break;
        case DBOBLOCK_FRAME_BONEMATRIXSIZEB:
            if (!dbo__read_u32(r, (dbo_u32*)&anim->bone_frame_b)) return DBO_FALSE;
            break;
        case DBOBLOCK_FRAME_BONEMATRIXLIST:
            if (!dbo__parse_bone_matrices(r, anim)) return DBO_FALSE;
            break;
        case DBOBLOCK_ANIM_NEXT:
            if (!dbo__parse_animation(r, &anim->next_animation)) return DBO_FALSE;
            break;
        default:
            if (!dbo__capture_unknown_chunk(r, DBO_SCOPE_ANIMATION, anim_index, code, code_size, event_index)) return DBO_FALSE;
            break;
        }

        if (!dbo__read_code(r, &code, &code_size))
        {
            return DBO_FALSE;
        }
    }

    return DBO_TRUE;
}

static dbo_bool dbo__parse_animation_set(dbo_reader* r, dbo_u32* out_index)
{
    dbo_u32 code;
    dbo_u32 code_size;
    dbo_u32 set_index;
    dbo_animation_set* set;

    if (!dbo__read_code(r, &code, &code_size))
    {
        return DBO_FALSE;
    }

    if (code == 0UL)
    {
        *out_index = DBO_INDEX_NONE;
        return DBO_TRUE;
    }

    if (!dbo_scene_add_animation_set(r->scene, &set_index))
    {
        return DBO_FALSE;
    }
    set = &r->scene->animation_sets[set_index];
    *out_index = set_index;

    while (code > 0UL)
    {
        dbo_u32 event_index;
        if (!dbo__scene_store_chunk_event(r->scene, DBO_SCOPE_ANIMATION_SET, set_index, code, DBO_INDEX_NONE, &event_index))
        {
            return DBO_FALSE;
        }
        switch (code)
        {
        case DBOBLOCK_ANIMSET_NAME:
            if (!dbo__read_string_ref(r, &set->name)) return DBO_FALSE;
            break;
        case DBOBLOCK_ANIMSET_LENGTH:
            if (!dbo__read_u32(r, &set->length)) return DBO_FALSE;
            break;
        case DBOBLOCK_ANIMSET_DATA:
            if (!dbo__parse_animation(r, &set->first_animation)) return DBO_FALSE;
            break;
        default:
            if (!dbo__capture_unknown_chunk(r, DBO_SCOPE_ANIMATION_SET, set_index, code, code_size, event_index)) return DBO_FALSE;
            break;
        }

        if (!dbo__read_code(r, &code, &code_size))
        {
            return DBO_FALSE;
        }
    }

    return DBO_TRUE;
}

static dbo_bool dbo__parse_custom(dbo_reader* r, dbo_u32 version)
{
    dbo_u32 code;
    dbo_u32 code_size;
    dbo_u32 remain;
    if (!dbo__peek_remaining(r, &remain))
    {
        return DBO_FALSE;
    }
    if (remain < 8UL)
    {
        return DBO_TRUE;
    }
    if (!dbo__read_code(r, &code, &code_size))
    {
        return DBO_FALSE;
    }
    if (code == 0UL)
    {
        return DBO_TRUE;
    }

    if (version <= 1UL)
    {
        if (!dbo__read_raw_blob(r, code_size, 4UL, &r->scene->custom_data))
        {
            return DBO_FALSE;
        }
        return DBO_TRUE;
    }

    while (code > 0UL)
    {
        dbo_u32 event_index;
        if (!dbo__scene_store_chunk_event(r->scene, DBO_SCOPE_SCENE, 0UL, code, DBO_INDEX_NONE, &event_index))
        {
            return DBO_FALSE;
        }
        if (code == DBOBLOCK_OBJECT_CUSTOMDATA)
        {
            dbo_u32 custom_size;
            if (!dbo__read_u32(r, &custom_size))
            {
                return DBO_FALSE;
            }
            if (!dbo__read_raw_blob(r, custom_size, 4UL, &r->scene->custom_data))
            {
                return DBO_FALSE;
            }
        }
        else
        {
            if (!dbo__capture_unknown_chunk(r, DBO_SCOPE_SCENE, 0UL, code, code_size, event_index))
            {
                return DBO_FALSE;
            }
        }

        if (!dbo__peek_remaining(r, &remain))
        {
            return DBO_FALSE;
        }
        if (remain < 8UL)
        {
            break;
        }
        if (!dbo__read_code(r, &code, &code_size))
        {
            return DBO_FALSE;
        }
    }

    return DBO_TRUE;
}

dbo_bool dbo_parse_memory(dbo_scene* scene, const void* data, dbo_u32 size)
{
    dbo_reader r;
    char magic[32];
    dbo_u32 root_code;
    dbo_u32 root_size;

    if (scene == 0 || data == 0)
    {
        return DBO_FALSE;
    }

    dbo_scene_reset(scene);

    r.scene = scene;
    r.data = (const dbo_u8*)data;
    r.size = size;
    r.pos = 0UL;

    if (!dbo__read_string_local(&r, magic, (dbo_u32)sizeof(magic)))
    {
        return DBO_FALSE;
    }
    if (strcmp(magic, DBO_MAGIC_STRING) != 0)
    {
        dbo__set_error(scene, "not a DBO file");
        return DBO_FALSE;
    }

    if (!dbo__read_u32(&r, &scene->version)) return DBO_FALSE;
    if (!dbo__read_u32(&r, &scene->reserved1)) return DBO_FALSE;
    if (!dbo__read_u32(&r, &scene->reserved2)) return DBO_FALSE;

    if (!dbo__read_code(&r, &root_code, &root_size))
    {
        return DBO_FALSE;
    }
    (void)root_code;
    (void)root_size;
    if (!dbo__scene_store_chunk_event(scene, DBO_SCOPE_SCENE, 0UL, root_code, DBO_INDEX_NONE, 0))
    {
        return DBO_FALSE;
    }
    if (!dbo__parse_frame(&r, DBO_INDEX_NONE, &scene->root_frame))
    {
        return DBO_FALSE;
    }

    if (r.pos + 8UL <= r.size)
    {
        if (!dbo__read_code(&r, &root_code, &root_size))
        {
            return DBO_FALSE;
        }
        (void)root_code;
        (void)root_size;
        if (!dbo__scene_store_chunk_event(scene, DBO_SCOPE_SCENE, 0UL, root_code, DBO_INDEX_NONE, 0))
        {
            return DBO_FALSE;
        }
        if (!dbo__parse_animation_set(&r, &scene->root_animation_set))
        {
            return DBO_FALSE;
        }
    }

    if (r.pos < r.size)
    {
        if (!dbo__parse_custom(&r, scene->version))
        {
            return DBO_FALSE;
        }
    }

    if (!dbo_scene_resolve_animation_frames(scene))
    {
        return DBO_FALSE;
    }

    return DBO_TRUE;
}
