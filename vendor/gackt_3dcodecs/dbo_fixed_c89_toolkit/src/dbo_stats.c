
#include "dbo_int.h"

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

dbo_u32 dbo_scene_total_vertices(const dbo_scene* scene)
{
    dbo_u32 i;
    dbo_u32 total;
    if (scene == 0)
    {
        return 0UL;
    }
    total = 0UL;
    for (i = 0UL; i < scene->mesh_count; ++i)
    {
        if (0xFFFFFFFFUL - total < scene->meshes[i].vertex_count)
        {
            return 0xFFFFFFFFUL;
        }
        total += scene->meshes[i].vertex_count;
    }
    return total;
}

dbo_u32 dbo_scene_total_indices(const dbo_scene* scene)
{
    dbo_u32 i;
    dbo_u32 total;
    if (scene == 0)
    {
        return 0UL;
    }
    total = 0UL;
    for (i = 0UL; i < scene->mesh_count; ++i)
    {
        if (0xFFFFFFFFUL - total < scene->meshes[i].index_count)
        {
            return 0xFFFFFFFFUL;
        }
        total += scene->meshes[i].index_count;
    }
    return total;
}

dbo_s32 dbo_scene_find_frame_by_name(const dbo_scene* scene, const char* name)
{
    dbo_u32 i;
    const char* frame_name;
    if (scene == 0 || name == 0)
    {
        return -1;
    }
    for (i = 0UL; i < scene->frame_count; ++i)
    {
        frame_name = dbo_blob_cstr(scene, scene->frames[i].name);
        if (frame_name != 0 && strcmp(frame_name, name) == 0)
        {
            return (dbo_s32)i;
        }
    }
    return -1;
}

dbo_s32 dbo_scene_find_mesh_by_frame_name(const dbo_scene* scene, const char* name)
{
    dbo_s32 frame_index;
    if (scene == 0 || name == 0)
    {
        return -1;
    }
    frame_index = dbo_scene_find_frame_by_name(scene, name);
    if (frame_index < 0)
    {
        return -1;
    }
    if (scene->frames[(dbo_u32)frame_index].mesh == DBO_INDEX_NONE)
    {
        return -1;
    }
    return (dbo_s32)scene->frames[(dbo_u32)frame_index].mesh;
}

dbo_bool dbo_mesh_read_index_u16(const dbo_scene* scene, const dbo_mesh* mesh, dbo_u32 index_number, dbo_u16* out_index)
{
    const dbo_u16* indices;
    if (scene == 0 || mesh == 0 || out_index == 0)
    {
        return DBO_FALSE;
    }
    if (index_number >= mesh->index_count)
    {
        return DBO_FALSE;
    }
    if (!dbo__blob_ref_valid(scene, mesh->index_data))
    {
        return DBO_FALSE;
    }
    if (mesh->index_data.size < mesh->index_count * 2UL)
    {
        return DBO_FALSE;
    }
    indices = (const dbo_u16*)dbo_blob_data(scene, mesh->index_data);
    if (indices == 0)
    {
        return DBO_FALSE;
    }
    *out_index = indices[index_number];
    return DBO_TRUE;
}

dbo_bool dbo_mesh_compute_aabb(const dbo_scene* scene, const dbo_mesh* mesh, dbo_fx32 out_min[3], dbo_fx32 out_max[3])
{
    dbo_u32 i;
    dbo_fx32 p[3];

    if (scene == 0 || mesh == 0 || out_min == 0 || out_max == 0)
    {
        return DBO_FALSE;
    }
    if (mesh->vertex_count == 0UL)
    {
        return DBO_FALSE;
    }
    if (!dbo_fvf_decode_position(scene, mesh, 0UL, p))
    {
        return DBO_FALSE;
    }

    out_min[0] = p[0];
    out_min[1] = p[1];
    out_min[2] = p[2];
    out_max[0] = p[0];
    out_max[1] = p[1];
    out_max[2] = p[2];

    for (i = 1UL; i < mesh->vertex_count; ++i)
    {
        if (!dbo_fvf_decode_position(scene, mesh, i, p))
        {
            return DBO_FALSE;
        }
        if (p[0] < out_min[0]) out_min[0] = p[0];
        if (p[1] < out_min[1]) out_min[1] = p[1];
        if (p[2] < out_min[2]) out_min[2] = p[2];
        if (p[0] > out_max[0]) out_max[0] = p[0];
        if (p[1] > out_max[1]) out_max[1] = p[1];
        if (p[2] > out_max[2]) out_max[2] = p[2];
    }

    return DBO_TRUE;
}

dbo_bool dbo_scene_compute_aabb(const dbo_scene* scene, dbo_fx32 out_min[3], dbo_fx32 out_max[3])
{
    dbo_u32 i;
    dbo_bool got_any;
    dbo_fx32 mesh_min[3];
    dbo_fx32 mesh_max[3];

    if (scene == 0 || out_min == 0 || out_max == 0)
    {
        return DBO_FALSE;
    }

    got_any = DBO_FALSE;
    for (i = 0UL; i < scene->mesh_count; ++i)
    {
        if (!dbo_mesh_compute_aabb(scene, &scene->meshes[i], mesh_min, mesh_max))
        {
            continue;
        }

        if (!got_any)
        {
            out_min[0] = mesh_min[0];
            out_min[1] = mesh_min[1];
            out_min[2] = mesh_min[2];
            out_max[0] = mesh_max[0];
            out_max[1] = mesh_max[1];
            out_max[2] = mesh_max[2];
            got_any = DBO_TRUE;
        }
        else
        {
            if (mesh_min[0] < out_min[0]) out_min[0] = mesh_min[0];
            if (mesh_min[1] < out_min[1]) out_min[1] = mesh_min[1];
            if (mesh_min[2] < out_min[2]) out_min[2] = mesh_min[2];
            if (mesh_max[0] > out_max[0]) out_max[0] = mesh_max[0];
            if (mesh_max[1] > out_max[1]) out_max[1] = mesh_max[1];
            if (mesh_max[2] > out_max[2]) out_max[2] = mesh_max[2];
        }
    }

    return got_any;
}
