#include "dts89.h"
#include <string.h>

static int put_u32_repeat(dts89_writer *w, const dts89_u32 *v, dts89_u32 n)
{
    dts89_u32 i;
    int rc;
    if (n && !v && w->data) return DTS89_ERR_BAD_ARG;
    for (i = 0u; i < n; ++i) {
        rc = dts89_write_u32le(w, v ? v[i] : 0u);
        if (rc) return rc;
    }
    return DTS89_OK;
}

static int put_s32_repeat(dts89_writer *w, const dts89_s32 *v, dts89_u32 n)
{
    dts89_u32 i;
    int rc;
    if (n && !v && w->data) return DTS89_ERR_BAD_ARG;
    for (i = 0u; i < n; ++i) {
        rc = dts89_write_u32le(w, v ? (dts89_u32)v[i] : 0u);
        if (rc) return rc;
    }
    return DTS89_OK;
}

static int put_quat16_from_u32_repeat(dts89_writer *w, const dts89_u32 *v, dts89_u32 count)
{
    dts89_u32 i;
    dts89_u32 n;
    int rc;
    if (!w || (!v && count && w->data)) return DTS89_ERR_BAD_ARG;
    if (count > 0xFFFFFFFFu / 4u) return DTS89_ERR_RANGE;
    n = count * 4u;
    for (i = 0u; i < n; ++i) {
        rc = dts89_write_u16le(w, (dts89_u16)(v ? v[i] : 0u));
        if (rc) return rc;
    }
    return DTS89_OK;
}

static int put_u16_repeat(dts89_writer *w, const dts89_u16 *v, dts89_u32 n)
{
    dts89_u32 i;
    int rc;
    if (n && !v && w->data) return DTS89_ERR_BAD_ARG;
    for (i = 0u; i < n; ++i) {
        rc = dts89_write_u16le(w, v ? v[i] : 0u);
        if (rc) return rc;
    }
    return DTS89_OK;
}


static int pad16_to_dword(dts89_writer *w)
{
    int rc;
    while (w->pos & 3u) {
        rc = dts89_write_u16le(w, 0u);
        if (rc) return rc;
    }
    return DTS89_OK;
}

static int pad8_to_dword(dts89_writer *w)
{
    int rc;
    while (w->pos & 3u) {
        rc = dts89_write_u8(w, 0u);
        if (rc) return rc;
    }
    return DTS89_OK;
}


typedef struct dts89_encode_guards {
    dts89_u32 g32;
    dts89_u32 g16;
    dts89_u32 g8;
} dts89_encode_guards;

static int write_guard(dts89_writer *mw32, dts89_writer *mw16, dts89_writer *mw8, dts89_encode_guards *g)
{
    int rc;
    if (!mw32 || !mw16 || !mw8 || !g) return DTS89_ERR_BAD_ARG;
    rc = dts89_write_u32le(mw32, g->g32); if (rc) return rc;
    rc = dts89_write_u16le(mw16, (dts89_u16)g->g16); if (rc) return rc;
    rc = dts89_write_u8(mw8, (dts89_u8)g->g8); if (rc) return rc;
    g->g32 += 1u;
    g->g16 += 1u;
    g->g8 += 1u;
    return DTS89_OK;
}

static int put_zero32_repeat(dts89_writer *w, dts89_u32 n)
{
    dts89_u32 i;
    int rc;
    for (i = 0u; i < n; ++i) {
        rc = dts89_write_u32le(w, 0u);
        if (rc) return rc;
    }
    return DTS89_OK;
}

static int write_sequence(dts89_writer *w, const dts89_sequence *q)
{
    int rc;
    rc = dts89_write_u32le(w, (dts89_u32)q->name_index); if (rc) return rc;
    rc = dts89_write_u32le(w, q->flags); if (rc) return rc;
    rc = dts89_write_u32le(w, (dts89_u32)q->num_keyframes); if (rc) return rc;
    rc = dts89_write_u32le(w, q->duration_bits); if (rc) return rc;
    rc = dts89_write_u32le(w, (dts89_u32)q->priority); if (rc) return rc;
    rc = dts89_write_u32le(w, (dts89_u32)q->first_ground_frame); if (rc) return rc;
    rc = dts89_write_u32le(w, (dts89_u32)q->num_ground_frames); if (rc) return rc;
    rc = dts89_write_u32le(w, (dts89_u32)q->base_rotation); if (rc) return rc;
    rc = dts89_write_u32le(w, (dts89_u32)q->base_translation); if (rc) return rc;
    rc = dts89_write_u32le(w, (dts89_u32)q->base_scale); if (rc) return rc;
    rc = dts89_write_u32le(w, (dts89_u32)q->base_object_state); if (rc) return rc;
    rc = dts89_write_u32le(w, (dts89_u32)q->base_decal_state); if (rc) return rc;
    rc = dts89_write_u32le(w, (dts89_u32)q->first_trigger); if (rc) return rc;
    rc = dts89_write_u32le(w, (dts89_u32)q->num_triggers); if (rc) return rc;
    rc = dts89_write_u32le(w, q->tool_begin_bits); if (rc) return rc;
    rc = dts89_bitset_write(w, &q->rotation_matters); if (rc) return rc;
    rc = dts89_bitset_write(w, &q->translation_matters); if (rc) return rc;
    rc = dts89_bitset_write(w, &q->scale_matters); if (rc) return rc;
    rc = dts89_bitset_write(w, &q->decal_matters); if (rc) return rc;
    rc = dts89_bitset_write(w, &q->ifl_matters); if (rc) return rc;
    rc = dts89_bitset_write(w, &q->vis_matters); if (rc) return rc;
    rc = dts89_bitset_write(w, &q->frame_matters); if (rc) return rc;
    rc = dts89_bitset_write(w, &q->mat_frame_matters); if (rc) return rc;
    return DTS89_OK;
}

static int write_torque_string(dts89_writer *w, const char *s, dts89_u32 len)
{
    int rc;
    if (!w || (!s && len && w->data)) return DTS89_ERR_BAD_ARG;
    if (len < 255u) {
        rc = dts89_write_u8(w, (dts89_u8)len);
        if (rc) return rc;
    } else {
        rc = dts89_write_u8(w, 255u);
        if (rc) return rc;
        if (len > 65535u) len = 65535u;
        rc = dts89_write_u16le(w, (dts89_u16)len);
        if (rc) return rc;
    }
    return dts89_write_bytes(w, s, len);
}

static int write_material_list(dts89_writer *w, const dts89_shape *shape, dts89_u32 version)
{
    dts89_u32 count;
    dts89_u32 i;
    int rc;
    if (!w || !shape) return DTS89_ERR_BAD_ARG;
    count = (shape->num_materials < 0) ? 0u : (dts89_u32)shape->num_materials;
    rc = dts89_write_u8(w, 1u); if (rc) return rc;
    rc = dts89_write_u32le(w, count); if (rc) return rc;
    for (i = 0u; i < count; ++i) {
        const dts89_material *m = &shape->materials[i];
        dts89_u32 len = m->name.ptr ? m->name.len : 0u;
        rc = write_torque_string(w, m->name.ptr, len); if (rc) return rc;
    }
    for (i = 0u; i < count; ++i) {
        const dts89_material *m = &shape->materials[i];
        dts89_u32 flags = m->flags ? m->flags : (DTS89_MATF_S_WRAP | DTS89_MATF_T_WRAP);
        rc = dts89_write_u32le(w, flags); if (rc) return rc;
    }
    for (i = 0u; i < count; ++i) { rc = dts89_write_u32le(w, shape->materials[i].reflectance_map); if (rc) return rc; }
    for (i = 0u; i < count; ++i) { rc = dts89_write_u32le(w, shape->materials[i].bump_map); if (rc) return rc; }
    for (i = 0u; i < count; ++i) { rc = dts89_write_u32le(w, shape->materials[i].detail_map); if (rc) return rc; }
    if (version == 25u) {
        for (i = 0u; i < count; ++i) { rc = dts89_write_u32le(w, 0xFFFFFFFFu); if (rc) return rc; }
    }
    for (i = 0u; i < count; ++i) {
        dts89_u32 bits = shape->materials[i].detail_scale_bits ? shape->materials[i].detail_scale_bits : DTS89_F32_BITS_ONE;
        rc = dts89_write_u32le(w, bits); if (rc) return rc;
    }
    for (i = 0u; i < count; ++i) {
        dts89_u32 bits = shape->materials[i].reflection_amount_bits ? shape->materials[i].reflection_amount_bits : DTS89_F32_BITS_ONE;
        rc = dts89_write_u32le(w, bits); if (rc) return rc;
    }
    return DTS89_OK;
}

int dts89_can_write_old_format(const dts89_shape *shape)
{
    dts89_s32 i;
    if (!shape) return DTS89_FALSE;
    for (i = 0; i < shape->num_details; ++i) {
        if (shape->details[i].subshape_num < 0) return DTS89_FALSE;
    }
    for (i = 0; i < shape->num_meshes; ++i) {
        const dts89_mesh_view *m = &shape->meshes[i];
        dts89_s32 j;
        if (m->mesh_type == DTS89_MESH_NULL) continue;
        if (m->num_tverts2 > 0 || m->num_colors > 0) return DTS89_FALSE;
        for (j = 0; j < m->num_primitives; ++j) {
            if (m->primitives[j].start < -32768 || m->primitives[j].start > 32767) return DTS89_FALSE;
            if (m->primitives[j].num_elements < -32768 || m->primitives[j].num_elements > 32767) return DTS89_FALSE;
            if ((m->primitives[j].start + m->primitives[j].num_elements) >= (1 << 15)) return DTS89_FALSE;
        }
        for (j = 0; j < m->num_indices; ++j) {
            if (m->indices[j] < 0 || m->indices[j] > 65535) return DTS89_FALSE;
        }
    }
    return DTS89_TRUE;
}


static int write_base_mesh_body(dts89_writer *mw32, dts89_writer *mw16, dts89_writer *mw8, const dts89_mesh_view *m, dts89_u32 version, dts89_encode_guards *g)
{
    dts89_s32 i;
    int rc;
    rc = write_guard(mw32, mw16, mw8, g); if (rc) return rc;
    rc = dts89_write_u32le(mw32, (dts89_u32)m->num_frames); if (rc) return rc;
    rc = dts89_write_u32le(mw32, (dts89_u32)m->num_mat_frames); if (rc) return rc;
    rc = dts89_write_u32le(mw32, (dts89_u32)m->parent_mesh); if (rc) return rc;
    rc = put_u32_repeat(mw32, m->bounds_bits, 6u); if (rc) return rc;
    rc = put_u32_repeat(mw32, m->center_bits, 3u); if (rc) return rc;
    rc = dts89_write_u32le(mw32, m->radius_bits); if (rc) return rc;
    if (version >= 27u) {
        rc = dts89_write_u32le(mw32, (dts89_u32)m->vert_offset); if (rc) return rc;
        rc = dts89_write_u32le(mw32, (dts89_u32)m->num_verts); if (rc) return rc;
        rc = dts89_write_u32le(mw32, (dts89_u32)m->vert_size); if (rc) return rc;
    }
    rc = dts89_write_u32le(mw32, (dts89_u32)m->num_verts_editable); if (rc) return rc;
    if (m->parent_mesh < 0) { rc = put_u32_repeat(mw32, m->verts_bits, (dts89_u32)m->num_verts_editable * 3u); if (rc) return rc; }
    rc = dts89_write_u32le(mw32, (dts89_u32)m->num_tverts); if (rc) return rc;
    if (m->parent_mesh < 0) { rc = put_u32_repeat(mw32, m->tverts_bits, (dts89_u32)m->num_tverts * 2u); if (rc) return rc; }
    if (version > 25u) {
        rc = dts89_write_u32le(mw32, (dts89_u32)m->num_tverts2); if (rc) return rc;
        if (m->parent_mesh < 0) { rc = put_u32_repeat(mw32, m->tverts2_bits, (dts89_u32)m->num_tverts2 * 2u); if (rc) return rc; }
        rc = dts89_write_u32le(mw32, (dts89_u32)m->num_colors); if (rc) return rc;
        if (m->parent_mesh < 0) { rc = put_u32_repeat(mw32, m->colors_bits, (dts89_u32)m->num_colors); if (rc) return rc; }
    }
    if (m->parent_mesh < 0) {
        rc = put_u32_repeat(mw32, m->norms_bits, (dts89_u32)m->num_verts_editable * 3u); if (rc) return rc;
        rc = dts89_write_bytes(mw8, m->encoded_norms, (dts89_u32)m->num_verts_editable); if (rc) return rc;
    }
    rc = dts89_write_u32le(mw32, (dts89_u32)m->num_primitives); if (rc) return rc;
    if (version > 25u) {
        for (i = 0; i < m->num_primitives; ++i) {
            rc = dts89_write_u32le(mw32, (dts89_u32)m->primitives[i].start); if (rc) return rc;
            rc = dts89_write_u32le(mw32, (dts89_u32)m->primitives[i].num_elements); if (rc) return rc;
            rc = dts89_write_u32le(mw32, (dts89_u32)m->primitives[i].mat_index); if (rc) return rc;
        }
        rc = dts89_write_u32le(mw32, (dts89_u32)m->num_indices); if (rc) return rc;
        rc = put_s32_repeat(mw32, m->indices, (dts89_u32)m->num_indices); if (rc) return rc;
    } else {
        for (i = 0; i < m->num_primitives; ++i) {
            rc = dts89_write_u16le(mw16, (dts89_u16)m->primitives[i].start); if (rc) return rc;
            rc = dts89_write_u16le(mw16, (dts89_u16)m->primitives[i].num_elements); if (rc) return rc;
        }
        for (i = 0; i < m->num_primitives; ++i) {
            rc = dts89_write_u32le(mw32, (dts89_u32)m->primitives[i].mat_index); if (rc) return rc;
        }
        rc = dts89_write_u32le(mw32, (dts89_u32)m->num_indices); if (rc) return rc;
        for (i = 0; i < m->num_indices; ++i) {
            rc = dts89_write_u16le(mw16, (dts89_u16)m->indices[i]); if (rc) return rc;
        }
    }
    rc = dts89_write_u32le(mw32, (dts89_u32)m->merge_indices_count); if (rc) return rc;
    rc = put_u16_repeat(mw16, m->merge_indices16, (dts89_u32)m->merge_indices_count); if (rc) return rc;
    rc = dts89_write_u32le(mw32, (dts89_u32)m->verts_per_frame); if (rc) return rc;
    rc = dts89_write_u32le(mw32, m->flags); if (rc) return rc;
    rc = write_guard(mw32, mw16, mw8, g); if (rc) return rc;
    return DTS89_OK;
}

static int write_skin_extra(dts89_writer *mw32, dts89_writer *mw8, const dts89_mesh_view *m, dts89_u32 version, dts89_encode_guards *g, dts89_writer *mw16)
{
    int rc;
    if (version >= 27u) {
        rc = dts89_write_u32le(mw32, (dts89_u32)m->skin_max_bones); if (rc) return rc;
    } else {
        rc = dts89_write_u32le(mw32, (dts89_u32)m->skin_initial_vert_count); if (rc) return rc;
        if (m->parent_mesh < 0) {
            rc = put_u32_repeat(mw32, m->skin_initial_verts_bits, (dts89_u32)m->skin_initial_vert_count * 3u); if (rc) return rc;
            rc = put_u32_repeat(mw32, m->skin_initial_norms_bits, (dts89_u32)m->skin_initial_vert_count * 3u); if (rc) return rc;
            rc = dts89_write_bytes(mw8, m->skin_initial_encoded_norms, (dts89_u32)m->skin_initial_vert_count); if (rc) return rc;
        }
    }
    rc = dts89_write_u32le(mw32, (dts89_u32)m->skin_initial_transform_count); if (rc) return rc;
    if (m->parent_mesh < 0) { rc = put_u32_repeat(mw32, m->skin_initial_transforms_bits, (dts89_u32)m->skin_initial_transform_count * 16u); if (rc) return rc; }
    rc = dts89_write_u32le(mw32, (dts89_u32)m->skin_vertex_index_count); if (rc) return rc;
    if (m->parent_mesh < 0) {
        rc = put_s32_repeat(mw32, m->skin_vertex_index, (dts89_u32)m->skin_vertex_index_count); if (rc) return rc;
        rc = put_s32_repeat(mw32, m->skin_bone_index, (dts89_u32)m->skin_vertex_index_count); if (rc) return rc;
        rc = put_u32_repeat(mw32, m->skin_weight_bits, (dts89_u32)m->skin_vertex_index_count); if (rc) return rc;
    }
    rc = dts89_write_u32le(mw32, (dts89_u32)m->skin_node_index_count); if (rc) return rc;
    if (m->parent_mesh < 0) { rc = put_s32_repeat(mw32, m->skin_node_index, (dts89_u32)m->skin_node_index_count); if (rc) return rc; }
    rc = write_guard(mw32, mw16, mw8, g); if (rc) return rc;
    return DTS89_OK;
}

static int write_sorted_extra(dts89_writer *mw32, dts89_writer *mw16, dts89_writer *mw8, const dts89_mesh_view *m, dts89_encode_guards *g)
{
    int rc;
    rc = dts89_write_u32le(mw32, (dts89_u32)m->sorted_num_clusters); if (rc) return rc;
    rc = put_u32_repeat(mw32, m->sorted_clusters_bits, (dts89_u32)m->sorted_num_clusters * 8u); if (rc) return rc;
    rc = dts89_write_u32le(mw32, (dts89_u32)m->sorted_start_cluster_count); if (rc) return rc;
    rc = put_s32_repeat(mw32, m->sorted_start_cluster, (dts89_u32)m->sorted_start_cluster_count); if (rc) return rc;
    rc = dts89_write_u32le(mw32, (dts89_u32)m->sorted_first_verts_count); if (rc) return rc;
    rc = put_s32_repeat(mw32, m->sorted_first_verts, (dts89_u32)m->sorted_first_verts_count); if (rc) return rc;
    rc = dts89_write_u32le(mw32, (dts89_u32)m->sorted_num_verts_count); if (rc) return rc;
    rc = put_s32_repeat(mw32, m->sorted_num_verts, (dts89_u32)m->sorted_num_verts_count); if (rc) return rc;
    rc = dts89_write_u32le(mw32, (dts89_u32)m->sorted_first_tverts_count); if (rc) return rc;
    rc = put_s32_repeat(mw32, m->sorted_first_tverts, (dts89_u32)m->sorted_first_tverts_count); if (rc) return rc;
    rc = dts89_write_u32le(mw32, (dts89_u32)m->sorted_always_write_depth); if (rc) return rc;
    rc = write_guard(mw32, mw16, mw8, g); if (rc) return rc;
    return DTS89_OK;
}

static int write_decal_mesh(dts89_writer *mw32, dts89_writer *mw16, dts89_writer *mw8, const dts89_mesh_view *m, dts89_u32 version, dts89_encode_guards *g)
{
    dts89_s32 i;
    int rc;
    (void)version;
    rc = dts89_write_u32le(mw32, (dts89_u32)m->num_primitives); if (rc) return rc;
    for (i = 0; i < m->num_primitives; ++i) {
        rc = dts89_write_u16le(mw16, (dts89_u16)m->primitives[i].start); if (rc) return rc;
        rc = dts89_write_u16le(mw16, (dts89_u16)m->primitives[i].num_elements); if (rc) return rc;
    }
    for (i = 0; i < m->num_primitives; ++i) {
        rc = dts89_write_u32le(mw32, (dts89_u32)m->primitives[i].mat_index); if (rc) return rc;
    }
    rc = dts89_write_u32le(mw32, (dts89_u32)m->num_indices); if (rc) return rc;
    for (i = 0; i < m->num_indices; ++i) {
        rc = dts89_write_u16le(mw16, (dts89_u16)m->indices[i]); if (rc) return rc;
    }
    rc = dts89_write_u32le(mw32, (dts89_u32)m->decal_start_primitive_count); if (rc) return rc;
    rc = put_s32_repeat(mw32, m->decal_start_primitives, (dts89_u32)m->decal_start_primitive_count); if (rc) return rc;
    rc = put_u32_repeat(mw32, m->decal_texgen_s_bits, (dts89_u32)m->decal_start_primitive_count * 4u); if (rc) return rc;
    rc = put_u32_repeat(mw32, m->decal_texgen_t_bits, (dts89_u32)m->decal_start_primitive_count * 4u); if (rc) return rc;
    rc = dts89_write_u32le(mw32, (dts89_u32)m->decal_material_index); if (rc) return rc;
    rc = write_guard(mw32, mw16, mw8, g); if (rc) return rc;
    return DTS89_OK;
}

static int write_one_mesh(dts89_writer *mw32, dts89_writer *mw16, dts89_writer *mw8, const dts89_shape *shape, const dts89_mesh_view *m, int save_old_format, dts89_encode_guards *g)
{
    dts89_u32 version;
    int rc;
    if (!m) return DTS89_ERR_BAD_ARG;
    version = save_old_format ? 24u : (shape->header.read_version ? shape->header.read_version : DTS89_VERSION_CURRENT);
    rc = dts89_write_u32le(mw32, m->mesh_type); if (rc) return rc;
    if (m->mesh_type == DTS89_MESH_NULL) return DTS89_OK;
    if (m->mesh_type == DTS89_MESH_DECAL) return write_decal_mesh(mw32, mw16, mw8, m, version, g);
    rc = write_base_mesh_body(mw32, mw16, mw8, m, version, g); if (rc) return rc;
    if (m->mesh_type == DTS89_MESH_SKIN) return write_skin_extra(mw32, mw8, m, version, g, mw16);
    if (m->mesh_type == DTS89_MESH_SORTED) return write_sorted_extra(mw32, mw16, mw8, m, g);
    return DTS89_OK;
}


static int write_shape_mem(dts89_writer *mw32, dts89_writer *mw16, dts89_writer *mw8, const dts89_shape *shape, int save_old_format)
{
    dts89_s32 i;
    int rc;
    dts89_u32 version;
    dts89_s32 arb;
    dts89_s32 ground;
    dts89_encode_guards g;
    if (!shape || !mw32 || !mw16 || !mw8) return DTS89_ERR_BAD_ARG;
    version = save_old_format ? 24u : (shape->header.read_version ? shape->header.read_version : DTS89_VERSION_CURRENT);
    if (version < 24u) return DTS89_ERR_UNSUPPORTED;
    if (shape->num_node_arbitrary_scale_rots != shape->num_node_arbitrary_scale_factors) return DTS89_ERR_FORMAT;
    if (shape->num_ground_rotations != shape->num_ground_translations) return DTS89_ERR_FORMAT;
    arb = shape->num_node_arbitrary_scale_factors;
    ground = shape->num_ground_translations;
    g.g32 = 0u;
    g.g16 = 0u;
    g.g8 = 0u;

    rc = dts89_write_u32le(mw32, (dts89_u32)shape->num_nodes); if (rc) return rc;
    rc = dts89_write_u32le(mw32, (dts89_u32)shape->num_objects); if (rc) return rc;
    rc = dts89_write_u32le(mw32, (dts89_u32)shape->num_decals_deprecated); if (rc) return rc;
    rc = dts89_write_u32le(mw32, (dts89_u32)shape->num_subshapes); if (rc) return rc;
    rc = dts89_write_u32le(mw32, (dts89_u32)shape->num_ifl_deprecated); if (rc) return rc;
    rc = dts89_write_u32le(mw32, (dts89_u32)shape->num_node_rotations); if (rc) return rc;
    rc = dts89_write_u32le(mw32, (dts89_u32)shape->num_node_translations); if (rc) return rc;
    rc = dts89_write_u32le(mw32, (dts89_u32)shape->num_node_uniform_scales); if (rc) return rc;
    rc = dts89_write_u32le(mw32, (dts89_u32)shape->num_node_aligned_scales); if (rc) return rc;
    rc = dts89_write_u32le(mw32, (dts89_u32)arb); if (rc) return rc;
    rc = dts89_write_u32le(mw32, (dts89_u32)ground); if (rc) return rc;
    rc = dts89_write_u32le(mw32, (dts89_u32)shape->num_object_states); if (rc) return rc;
    rc = dts89_write_u32le(mw32, (dts89_u32)shape->num_decal_states_deprecated); if (rc) return rc;
    rc = dts89_write_u32le(mw32, (dts89_u32)shape->num_triggers); if (rc) return rc;
    rc = dts89_write_u32le(mw32, (dts89_u32)shape->num_details); if (rc) return rc;
    rc = dts89_write_u32le(mw32, (dts89_u32)shape->num_meshes); if (rc) return rc;
    rc = dts89_write_u32le(mw32, (dts89_u32)shape->num_names); if (rc) return rc;
    rc = dts89_write_u32le(mw32, (dts89_u32)shape->smallest_visible_size); if (rc) return rc;
    rc = dts89_write_u32le(mw32, (dts89_u32)shape->smallest_visible_dl); if (rc) return rc;
    rc = write_guard(mw32, mw16, mw8, &g); if (rc) return rc;

    rc = dts89_write_u32le(mw32, shape->radius_bits); if (rc) return rc;
    rc = dts89_write_u32le(mw32, shape->tube_radius_bits); if (rc) return rc;
    rc = put_u32_repeat(mw32, shape->center_bits, 3u); if (rc) return rc;
    rc = put_u32_repeat(mw32, shape->bounds_bits, 6u); if (rc) return rc;
    rc = write_guard(mw32, mw16, mw8, &g); if (rc) return rc;

    for (i = 0; i < shape->num_nodes; ++i) {
        rc = dts89_write_u32le(mw32, (dts89_u32)shape->nodes[i].name_index); if (rc) return rc;
        rc = dts89_write_u32le(mw32, (dts89_u32)shape->nodes[i].parent_index); if (rc) return rc;
        rc = dts89_write_u32le(mw32, (dts89_u32)shape->nodes[i].first_object); if (rc) return rc;
        rc = dts89_write_u32le(mw32, (dts89_u32)shape->nodes[i].first_child); if (rc) return rc;
        rc = dts89_write_u32le(mw32, (dts89_u32)shape->nodes[i].next_sibling); if (rc) return rc;
    }
    rc = write_guard(mw32, mw16, mw8, &g); if (rc) return rc;

    for (i = 0; i < shape->num_objects; ++i) {
        rc = dts89_write_u32le(mw32, (dts89_u32)shape->objects[i].name_index); if (rc) return rc;
        rc = dts89_write_u32le(mw32, (dts89_u32)shape->objects[i].num_meshes); if (rc) return rc;
        rc = dts89_write_u32le(mw32, (dts89_u32)shape->objects[i].start_mesh_index); if (rc) return rc;
        rc = dts89_write_u32le(mw32, (dts89_u32)shape->objects[i].node_index); if (rc) return rc;
        rc = dts89_write_u32le(mw32, (dts89_u32)shape->objects[i].next_sibling); if (rc) return rc;
        rc = dts89_write_u32le(mw32, (dts89_u32)shape->objects[i].first_decal); if (rc) return rc;
    }
    rc = write_guard(mw32, mw16, mw8, &g); if (rc) return rc;

    rc = put_zero32_repeat(mw32, (dts89_u32)shape->num_decals_deprecated * 5u); if (rc) return rc;
    rc = write_guard(mw32, mw16, mw8, &g); if (rc) return rc;
    rc = put_zero32_repeat(mw32, (dts89_u32)shape->num_ifl_deprecated * 5u); if (rc) return rc;
    rc = write_guard(mw32, mw16, mw8, &g); if (rc) return rc;

    rc = put_s32_repeat(mw32, shape->subshape_first_node, (dts89_u32)shape->num_subshapes); if (rc) return rc;
    rc = put_s32_repeat(mw32, shape->subshape_first_object, (dts89_u32)shape->num_subshapes); if (rc) return rc;
    rc = put_s32_repeat(mw32, shape->subshape_first_decal, (dts89_u32)shape->num_subshapes); if (rc) return rc;
    rc = write_guard(mw32, mw16, mw8, &g); if (rc) return rc;
    rc = put_s32_repeat(mw32, shape->subshape_num_nodes, (dts89_u32)shape->num_subshapes); if (rc) return rc;
    rc = put_s32_repeat(mw32, shape->subshape_num_objects, (dts89_u32)shape->num_subshapes); if (rc) return rc;
    rc = put_s32_repeat(mw32, shape->subshape_num_decals, (dts89_u32)shape->num_subshapes); if (rc) return rc;
    rc = write_guard(mw32, mw16, mw8, &g); if (rc) return rc;

    rc = put_quat16_from_u32_repeat(mw16, shape->default_rotations_bits, (dts89_u32)shape->num_nodes); if (rc) return rc;
    rc = put_u32_repeat(mw32, shape->default_translations_bits, (dts89_u32)shape->num_nodes * 3u); if (rc) return rc;
    rc = put_u32_repeat(mw32, shape->node_translations_bits, (dts89_u32)shape->num_node_translations * 3u); if (rc) return rc;
    rc = put_quat16_from_u32_repeat(mw16, shape->node_rotations_bits, (dts89_u32)shape->num_node_rotations); if (rc) return rc;
    rc = write_guard(mw32, mw16, mw8, &g); if (rc) return rc;

    rc = put_u32_repeat(mw32, shape->node_uniform_scales_bits, (dts89_u32)shape->num_node_uniform_scales); if (rc) return rc;
    rc = put_u32_repeat(mw32, shape->node_aligned_scales_bits, (dts89_u32)shape->num_node_aligned_scales * 3u); if (rc) return rc;
    rc = put_u32_repeat(mw32, shape->node_arbitrary_scale_factors_bits, (dts89_u32)shape->num_node_arbitrary_scale_factors * 3u); if (rc) return rc;
    rc = put_quat16_from_u32_repeat(mw16, shape->node_arbitrary_scale_rots_bits, (dts89_u32)shape->num_node_arbitrary_scale_rots); if (rc) return rc;
    rc = write_guard(mw32, mw16, mw8, &g); if (rc) return rc;

    rc = put_u32_repeat(mw32, shape->ground_translations_bits, (dts89_u32)shape->num_ground_translations * 3u); if (rc) return rc;
    rc = put_quat16_from_u32_repeat(mw16, shape->ground_rotations_bits, (dts89_u32)shape->num_ground_rotations); if (rc) return rc;
    rc = write_guard(mw32, mw16, mw8, &g); if (rc) return rc;

    for (i = 0; i < shape->num_object_states; ++i) {
        rc = dts89_write_u32le(mw32, shape->object_states[i].vis_bits); if (rc) return rc;
        rc = dts89_write_u32le(mw32, (dts89_u32)shape->object_states[i].frame_index); if (rc) return rc;
        rc = dts89_write_u32le(mw32, (dts89_u32)shape->object_states[i].mat_frame_index); if (rc) return rc;
    }
    rc = write_guard(mw32, mw16, mw8, &g); if (rc) return rc;
    rc = put_zero32_repeat(mw32, (dts89_u32)shape->num_decal_states_deprecated); if (rc) return rc;
    rc = write_guard(mw32, mw16, mw8, &g); if (rc) return rc;
    for (i = 0; i < shape->num_triggers; ++i) {
        rc = dts89_write_u32le(mw32, shape->triggers[i].state); if (rc) return rc;
        rc = dts89_write_u32le(mw32, shape->triggers[i].pos_bits); if (rc) return rc;
    }
    rc = write_guard(mw32, mw16, mw8, &g); if (rc) return rc;

    if (!save_old_format) {
        for (i = 0; i < shape->num_details; ++i) {
            const dts89_detail *d;
            d = &shape->details[i];
            rc = dts89_write_u32le(mw32, (dts89_u32)d->name_index); if (rc) return rc;
            rc = dts89_write_u32le(mw32, (dts89_u32)d->subshape_num); if (rc) return rc;
            rc = dts89_write_u32le(mw32, (dts89_u32)d->object_detail_num); if (rc) return rc;
            rc = dts89_write_u32le(mw32, d->size_bits); if (rc) return rc;
            rc = dts89_write_u32le(mw32, d->average_error_bits); if (rc) return rc;
            rc = dts89_write_u32le(mw32, d->max_error_bits); if (rc) return rc;
            rc = dts89_write_u32le(mw32, (dts89_u32)d->poly_count); if (rc) return rc;
            rc = dts89_write_u32le(mw32, (dts89_u32)d->bb_dimension); if (rc) return rc;
            rc = dts89_write_u32le(mw32, (dts89_u32)d->bb_detail_level); if (rc) return rc;
            rc = dts89_write_u32le(mw32, d->bb_equator_steps); if (rc) return rc;
            rc = dts89_write_u32le(mw32, d->bb_polar_steps); if (rc) return rc;
            rc = dts89_write_u32le(mw32, d->bb_polar_angle_bits); if (rc) return rc;
            rc = dts89_write_u32le(mw32, d->bb_include_poles); if (rc) return rc;
        }
    } else {
        for (i = 0; i < shape->num_details; ++i) {
            const dts89_detail *d;
            d = &shape->details[i];
            rc = dts89_write_u32le(mw32, (dts89_u32)d->name_index); if (rc) return rc;
            rc = dts89_write_u32le(mw32, (dts89_u32)d->subshape_num); if (rc) return rc;
            rc = dts89_write_u32le(mw32, (dts89_u32)d->object_detail_num); if (rc) return rc;
            rc = dts89_write_u32le(mw32, d->size_bits); if (rc) return rc;
            rc = dts89_write_u32le(mw32, d->average_error_bits); if (rc) return rc;
            rc = dts89_write_u32le(mw32, d->max_error_bits); if (rc) return rc;
            rc = dts89_write_u32le(mw32, (dts89_u32)d->poly_count); if (rc) return rc;
        }
    }
    rc = write_guard(mw32, mw16, mw8, &g); if (rc) return rc;

    if (!save_old_format && version >= 27u) {
        dts89_u32 k;
        for (k = 0u; k < 5u; ++k) {
            rc = dts89_write_u16le(mw16, (dts89_u16)shape->vertex_format_words[k]); if (rc) return rc;
        }
        rc = dts89_write_u32le(mw32, shape->packed_vertex_size); if (rc) return rc;
        rc = dts89_write_bytes(mw8, shape->packed_vertex_data, shape->packed_vertex_size); if (rc) return rc;
    }

    for (i = 0; i < shape->num_meshes; ++i) {
        rc = write_one_mesh(mw32, mw16, mw8, shape, &shape->meshes[i], save_old_format, &g);
        if (rc) return rc;
    }
    rc = write_guard(mw32, mw16, mw8, &g); if (rc) return rc;

    for (i = 0; i < shape->num_names; ++i) {
        rc = dts89_write_bytes(mw8, shape->names[i].ptr, shape->names[i].len + 1u);
        if (rc) return rc;
    }
    rc = write_guard(mw32, mw16, mw8, &g); if (rc) return rc;

    rc = pad16_to_dword(mw16); if (rc) return rc;
    rc = pad8_to_dword(mw8); if (rc) return rc;
    return DTS89_OK;
}

static int count_tail_bytes(const dts89_shape *shape, dts89_u32 version, dts89_u32 *out_bytes)
{
    dts89_writer w;
    dts89_s32 i;
    int rc;
    if (!shape || !out_bytes) return DTS89_ERR_BAD_ARG;
    dts89_writer_init(&w, 0, 0xFFFFFFFFu);
    rc = dts89_write_u32le(&w, (dts89_u32)shape->num_sequences); if (rc) return rc;
    for (i = 0; i < shape->num_sequences; ++i) {
        rc = write_sequence(&w, &shape->sequences[i]);
        if (rc) return rc;
    }
    rc = write_material_list(&w, shape, version);
    if (rc) return rc;
    rc = dts89_write_bytes(&w, shape->sequence_tail_start, shape->sequence_tail_bytes);
    if (rc) return rc;
    *out_bytes = w.pos;
    return DTS89_OK;
}

int dts89_encode_shape(const dts89_shape *shape, dts89_writer *w, int save_old_format)
{
    dts89_writer count32, count16, count8;
    dts89_writer mw32, mw16, mw8;
    dts89_u32 start16_dwords;
    dts89_u32 start8_dwords;
    dts89_u32 total_dwords;
    dts89_u32 total_mem_bytes;
    dts89_u32 tail_bytes;
    dts89_u32 total_file_bytes;
    dts89_u32 version;
    dts89_u32 exporter;
    int rc;
    dts89_s32 i;
    if (!shape || !w) return DTS89_ERR_BAD_ARG;
    if (save_old_format && !dts89_can_write_old_format(shape)) return DTS89_ERR_UNSUPPORTED;

    dts89_writer_init(&count32, 0, 0xFFFFFFFFu);
    dts89_writer_init(&count16, 0, 0xFFFFFFFFu);
    dts89_writer_init(&count8, 0, 0xFFFFFFFFu);
    rc = write_shape_mem(&count32, &count16, &count8, shape, save_old_format);
    if (rc) return rc;

    start16_dwords = (count32.pos + 3u) / 4u;
    start8_dwords = start16_dwords + ((count16.pos + 3u) / 4u);
    total_dwords = start8_dwords + ((count8.pos + 3u) / 4u);
    total_mem_bytes = total_dwords * 4u;

    version = save_old_format ? 24u : (shape->header.read_version ? shape->header.read_version : DTS89_VERSION_CURRENT);
    rc = count_tail_bytes(shape, version, &tail_bytes);
    if (rc) return rc;

    total_file_bytes = 16u + total_mem_bytes + tail_bytes;
    if (w->size < total_file_bytes) return DTS89_ERR_NO_SPACE;

    dts89_writer_init(&mw32, w->data ? (w->data + 16u) : 0, start16_dwords * 4u);
    dts89_writer_init(&mw16, w->data ? (w->data + 16u + start16_dwords * 4u) : 0, (start8_dwords - start16_dwords) * 4u);
    dts89_writer_init(&mw8, w->data ? (w->data + 16u + start8_dwords * 4u) : 0, (total_dwords - start8_dwords) * 4u);
    rc = write_shape_mem(&mw32, &mw16, &mw8, shape, save_old_format);
    if (rc) return rc;

    version = save_old_format ? 24u : (shape->header.read_version ? shape->header.read_version : DTS89_VERSION_CURRENT);
    exporter = shape->header.exporter_version ? shape->header.exporter_version : DTS89_EXPORTER_CURRENT;

    w->pos = 0u;
    rc = dts89_write_u32le(w, version | (exporter << 16)); if (rc) return rc;
    rc = dts89_write_u32le(w, total_dwords); if (rc) return rc;
    rc = dts89_write_u32le(w, start16_dwords); if (rc) return rc;
    rc = dts89_write_u32le(w, start8_dwords); if (rc) return rc;

    w->pos = 16u + total_mem_bytes;
    rc = dts89_write_u32le(w, (dts89_u32)shape->num_sequences); if (rc) return rc;
    for (i = 0; i < shape->num_sequences; ++i) {
        rc = write_sequence(w, &shape->sequences[i]);
        if (rc) return rc;
    }
    rc = write_material_list(w, shape, version);
    if (rc) return rc;
    rc = dts89_write_bytes(w, shape->sequence_tail_start, shape->sequence_tail_bytes);
    if (rc) return rc;
    return DTS89_OK;
}
