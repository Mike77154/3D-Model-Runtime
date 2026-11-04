#include "dts89.h"
#include <string.h>

static const dts89_u32 *copy_u32_array(dts89_arena *arena, const dts89_u32 *src, dts89_u32 count)
{
    dts89_u32 *dst;
    if (!src || count == 0u) return src;
    dst = (dts89_u32*)dts89_arena_alloc(arena, count * sizeof(dts89_u32), 4u);
    if (!dst) return 0;
    memcpy(dst, src, (size_t)(count * sizeof(dts89_u32)));
    return dst;
}

static const dts89_s32 *copy_s32_array(dts89_arena *arena, const dts89_s32 *src, dts89_u32 count)
{
    dts89_s32 *dst;
    if (!src || count == 0u) return src;
    dst = (dts89_s32*)dts89_arena_alloc(arena, count * sizeof(dts89_s32), 4u);
    if (!dst) return 0;
    memcpy(dst, src, (size_t)(count * sizeof(dts89_s32)));
    return dst;
}

static const dts89_u16 *copy_u16_array(dts89_arena *arena, const dts89_u16 *src, dts89_u32 count)
{
    dts89_u16 *dst;
    if (!src || count == 0u) return src;
    dst = (dts89_u16*)dts89_arena_alloc(arena, count * sizeof(dts89_u16), 2u);
    if (!dst) return 0;
    memcpy(dst, src, (size_t)(count * sizeof(dts89_u16)));
    return dst;
}

static const dts89_u8 *copy_u8_array(dts89_arena *arena, const dts89_u8 *src, dts89_u32 count)
{
    dts89_u8 *dst;
    if (!src || count == 0u) return src;
    dst = (dts89_u8*)dts89_arena_alloc(arena, count * sizeof(dts89_u8), 1u);
    if (!dst) return 0;
    memcpy(dst, src, (size_t)(count * sizeof(dts89_u8)));
    return dst;
}

static const dts89_draw_primitive *copy_primitives(dts89_arena *arena, const dts89_draw_primitive *src, dts89_u32 count)
{
    dts89_draw_primitive *dst;
    if (!src || count == 0u) return src;
    dst = (dts89_draw_primitive*)dts89_arena_alloc(arena, count * sizeof(dts89_draw_primitive), 4u);
    if (!dst) return 0;
    memcpy(dst, src, (size_t)(count * sizeof(dts89_draw_primitive)));
    return dst;
}

int dts89_detach_shape(dts89_shape *shape, dts89_arena *arena)
{
    dts89_s32 i;
    if (!shape || !arena) return DTS89_ERR_BAD_ARG;

    if (shape->packed_vertex_size > 0u) {
        const dts89_u8 *p = copy_u8_array(arena, shape->packed_vertex_data, shape->packed_vertex_size);
        if (!p) return DTS89_ERR_NO_SPACE;
        shape->packed_vertex_data = p;
    }
    if (shape->sequence_tail_bytes > 0u) {
        const dts89_u8 *p = copy_u8_array(arena, shape->sequence_tail_start, shape->sequence_tail_bytes);
        if (!p) return DTS89_ERR_NO_SPACE;
        shape->sequence_tail_start = p;
    }

    for (i = 0; i < shape->num_meshes; ++i) {
        dts89_mesh_view *m = &shape->meshes[i];
        if (m->mesh_type == DTS89_MESH_NULL) continue;
        m->verts_bits = copy_u32_array(arena, m->verts_bits, (dts89_u32)m->num_verts_editable * 3u); if (m->num_verts_editable && !m->verts_bits) return DTS89_ERR_NO_SPACE;
        m->tverts_bits = copy_u32_array(arena, m->tverts_bits, (dts89_u32)m->num_tverts * 2u); if (m->num_tverts && !m->tverts_bits) return DTS89_ERR_NO_SPACE;
        m->tverts2_bits = copy_u32_array(arena, m->tverts2_bits, (dts89_u32)m->num_tverts2 * 2u); if (m->num_tverts2 && !m->tverts2_bits) return DTS89_ERR_NO_SPACE;
        m->colors_bits = copy_u32_array(arena, m->colors_bits, (dts89_u32)m->num_colors); if (m->num_colors && !m->colors_bits) return DTS89_ERR_NO_SPACE;
        m->norms_bits = copy_u32_array(arena, m->norms_bits, (dts89_u32)m->num_verts_editable * 3u); if (m->num_verts_editable && !m->norms_bits) return DTS89_ERR_NO_SPACE;
        m->encoded_norms = copy_u8_array(arena, m->encoded_norms, (dts89_u32)m->num_verts_editable); if (m->num_verts_editable && !m->encoded_norms) return DTS89_ERR_NO_SPACE;
        m->primitives = copy_primitives(arena, m->primitives, (dts89_u32)m->num_primitives); if (m->num_primitives && !m->primitives) return DTS89_ERR_NO_SPACE;
        m->indices = copy_s32_array(arena, m->indices, (dts89_u32)m->num_indices); if (m->num_indices && !m->indices) return DTS89_ERR_NO_SPACE;
        m->merge_indices16 = copy_u16_array(arena, m->merge_indices16, (dts89_u32)m->merge_indices_count); if (m->merge_indices_count && !m->merge_indices16) return DTS89_ERR_NO_SPACE;

        if (m->mesh_type == DTS89_MESH_SKIN) {
            m->skin_initial_verts_bits = copy_u32_array(arena, m->skin_initial_verts_bits, (dts89_u32)m->skin_initial_vert_count * 3u); if (m->skin_initial_vert_count && !m->skin_initial_verts_bits) return DTS89_ERR_NO_SPACE;
            m->skin_initial_norms_bits = copy_u32_array(arena, m->skin_initial_norms_bits, (dts89_u32)m->skin_initial_vert_count * 3u); if (m->skin_initial_vert_count && !m->skin_initial_norms_bits) return DTS89_ERR_NO_SPACE;
            m->skin_initial_encoded_norms = copy_u8_array(arena, m->skin_initial_encoded_norms, (dts89_u32)m->skin_initial_vert_count); if (m->skin_initial_vert_count && !m->skin_initial_encoded_norms) return DTS89_ERR_NO_SPACE;
            m->skin_initial_transforms_bits = copy_u32_array(arena, m->skin_initial_transforms_bits, (dts89_u32)m->skin_initial_transform_count * 16u); if (m->skin_initial_transform_count && !m->skin_initial_transforms_bits) return DTS89_ERR_NO_SPACE;
            m->skin_vertex_index = copy_s32_array(arena, m->skin_vertex_index, (dts89_u32)m->skin_vertex_index_count); if (m->skin_vertex_index_count && !m->skin_vertex_index) return DTS89_ERR_NO_SPACE;
            m->skin_bone_index = copy_s32_array(arena, m->skin_bone_index, (dts89_u32)m->skin_vertex_index_count); if (m->skin_vertex_index_count && !m->skin_bone_index) return DTS89_ERR_NO_SPACE;
            m->skin_weight_bits = copy_u32_array(arena, m->skin_weight_bits, (dts89_u32)m->skin_vertex_index_count); if (m->skin_vertex_index_count && !m->skin_weight_bits) return DTS89_ERR_NO_SPACE;
            m->skin_node_index = copy_s32_array(arena, m->skin_node_index, (dts89_u32)m->skin_node_index_count); if (m->skin_node_index_count && !m->skin_node_index) return DTS89_ERR_NO_SPACE;
        }
        if (m->mesh_type == DTS89_MESH_SORTED) {
            m->sorted_clusters_bits = copy_u32_array(arena, m->sorted_clusters_bits, (dts89_u32)m->sorted_num_clusters * 8u); if (m->sorted_num_clusters && !m->sorted_clusters_bits) return DTS89_ERR_NO_SPACE;
            m->sorted_start_cluster = copy_s32_array(arena, m->sorted_start_cluster, (dts89_u32)m->sorted_start_cluster_count); if (m->sorted_start_cluster_count && !m->sorted_start_cluster) return DTS89_ERR_NO_SPACE;
            m->sorted_first_verts = copy_s32_array(arena, m->sorted_first_verts, (dts89_u32)m->sorted_first_verts_count); if (m->sorted_first_verts_count && !m->sorted_first_verts) return DTS89_ERR_NO_SPACE;
            m->sorted_num_verts = copy_s32_array(arena, m->sorted_num_verts, (dts89_u32)m->sorted_num_verts_count); if (m->sorted_num_verts_count && !m->sorted_num_verts) return DTS89_ERR_NO_SPACE;
            m->sorted_first_tverts = copy_s32_array(arena, m->sorted_first_tverts, (dts89_u32)m->sorted_first_tverts_count); if (m->sorted_first_tverts_count && !m->sorted_first_tverts) return DTS89_ERR_NO_SPACE;
        }
        if (m->mesh_type == DTS89_MESH_DECAL) {
            m->decal_start_primitives = copy_s32_array(arena, m->decal_start_primitives, (dts89_u32)m->decal_start_primitive_count); if (m->decal_start_primitive_count && !m->decal_start_primitives) return DTS89_ERR_NO_SPACE;
            m->decal_texgen_s_bits = copy_u32_array(arena, m->decal_texgen_s_bits, (dts89_u32)m->decal_start_primitive_count * 4u); if (m->decal_start_primitive_count && !m->decal_texgen_s_bits) return DTS89_ERR_NO_SPACE;
            m->decal_texgen_t_bits = copy_u32_array(arena, m->decal_texgen_t_bits, (dts89_u32)m->decal_start_primitive_count * 4u); if (m->decal_start_primitive_count && !m->decal_texgen_t_bits) return DTS89_ERR_NO_SPACE;
        }
    }

    memset(&shape->pools, 0, sizeof(shape->pools));
    return DTS89_OK;
}
