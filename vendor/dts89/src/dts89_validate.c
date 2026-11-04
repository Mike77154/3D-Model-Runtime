#include "dts89.h"
#include <string.h>

#define DTS89_VALIDATE_MAX_COUNT 1048576u

static int val_count_ok(dts89_s32 v)
{
    if (v < 0) return 0;
    if ((dts89_u32)v > DTS89_VALIDATE_MAX_COUNT) return 0;
    return 1;
}

static void val_init(dts89_validate_report *r)
{
    if (r) memset(r, 0, sizeof(*r));
}

static void val_error(dts89_validate_report *r, dts89_s32 *field, dts89_s32 code)
{
    if (!r) return;
    r->errors++;
    if (r->first_error_code == 0) r->first_error_code = code;
    if (field) (*field)++;
}

static void val_warn(dts89_validate_report *r, dts89_s32 *field, dts89_u32 flags)
{
    if (!r) return;
    r->warnings++;
    r->warning_flags |= flags;
    if (field) (*field)++;
}

static int val_index_or_minus_one(dts89_s32 idx, dts89_s32 count)
{
    if (idx < 0) return 1;
    if (idx >= count) return 0;
    return 1;
}

static int val_range_s32(dts89_s32 start, dts89_s32 count, dts89_s32 limit)
{
    dts89_u32 ustart;
    dts89_u32 ucount;
    dts89_u32 ulimit;
    if (start < 0 || count < 0 || limit < 0) return 0;
    ustart = (dts89_u32)start;
    ucount = (dts89_u32)count;
    ulimit = (dts89_u32)limit;
    if (ustart > ulimit) return 0;
    if (ucount > ulimit - ustart) return 0;
    return 1;
}

static int val_mul_s32(dts89_s32 a, dts89_s32 b, dts89_s32 *out_v)
{
    dts89_u32 ua;
    dts89_u32 ub;
    if (!out_v) return DTS89_ERR_BAD_ARG;
    if (a < 0 || b < 0) return DTS89_ERR_RANGE;
    ua = (dts89_u32)a;
    ub = (dts89_u32)b;
    if (ua != 0u && ub > 0x7FFFFFFFu / ua) return DTS89_ERR_RANGE;
    *out_v = (dts89_s32)(ua * ub);
    return DTS89_OK;
}

static void val_ptr_count(dts89_validate_report *r, const void *p, dts89_s32 count)
{
    if (count > 0 && !p) val_error(r, &r->bad_count, DTS89_ERR_BAD_ARG);
}

static void val_name_index(dts89_validate_report *r, dts89_s32 idx, dts89_s32 count)
{
    if (!val_index_or_minus_one(idx, count)) val_error(r, &r->bad_name_index, DTS89_ERR_RANGE);
}

static void val_mesh_material(const dts89_shape *shape, const dts89_mesh_view *m, dts89_validate_report *r)
{
    dts89_s32 i;
    if (!shape || !m || !r) return;
    if (m->mesh_type == DTS89_MESH_NULL) return;
    if (!val_count_ok(m->num_primitives) || !val_count_ok(m->num_indices)) {
        val_error(r, &r->bad_count, DTS89_ERR_RANGE);
        return;
    }
    val_ptr_count(r, m->primitives, m->num_primitives);
    if (m->num_indices > 0 && !m->indices) val_error(r, &r->bad_mesh_range, DTS89_ERR_BAD_ARG);
    for (i = 0; i < m->num_primitives && m->primitives; ++i) {
        dts89_u32 matword;
        dts89_s32 mat;
        if (!val_range_s32(m->primitives[i].start, m->primitives[i].num_elements, m->num_indices)) {
            val_error(r, &r->bad_primitive_range, DTS89_ERR_RANGE);
        }
        matword = (dts89_u32)m->primitives[i].mat_index;
        if ((matword & DTS89_PRIM_NOMAT) == 0u) {
            mat = (dts89_s32)(matword & DTS89_PRIM_MAT_MASK);
            if (mat < 0 || mat >= shape->num_materials) val_error(r, &r->bad_material_index, DTS89_ERR_RANGE);
        }
    }
}

static void val_mesh_payload(const dts89_shape *shape, const dts89_mesh_view *m, dts89_validate_report *r)
{
    if (!shape || !m || !r) return;
    if (m->mesh_type != DTS89_MESH_STANDARD && m->mesh_type != DTS89_MESH_SKIN && m->mesh_type != DTS89_MESH_DECAL && m->mesh_type != DTS89_MESH_SORTED && m->mesh_type != DTS89_MESH_NULL) {
        val_warn(r, &r->unsupported_meshes, DTS89_VALIDATE_WARN_UNSUPPORTED_MESH);
        return;
    }
    if (m->mesh_type == DTS89_MESH_NULL) return;
    if (!val_count_ok(m->num_frames) || !val_count_ok(m->num_mat_frames) || !val_count_ok(m->num_verts) || !val_count_ok(m->num_tverts) || !val_count_ok(m->num_primitives)) {
        val_error(r, &r->bad_count, DTS89_ERR_RANGE);
    }
    if (m->parent_mesh >= shape->num_meshes) val_error(r, &r->bad_mesh_range, DTS89_ERR_RANGE);
    if (m->parent_mesh >= 0 && m->parent_mesh == (dts89_s32)(m - shape->meshes)) val_error(r, &r->bad_mesh_range, DTS89_ERR_RANGE);
    val_ptr_count(r, m->verts_bits, m->num_verts > 0 ? m->num_verts * 3 : 0);
    val_ptr_count(r, m->tverts_bits, m->num_tverts > 0 ? m->num_tverts * 2 : 0);
    val_ptr_count(r, m->norms_bits, m->num_verts > 0 ? m->num_verts * 3 : 0);
    val_ptr_count(r, m->encoded_norms, m->num_verts > 0 ? m->num_verts : 0);
    val_mesh_material(shape, m, r);
    if (m->mesh_type == DTS89_MESH_SKIN) {
        if (!val_count_ok(m->skin_initial_vert_count) || !val_count_ok(m->skin_initial_transform_count) || !val_count_ok(m->skin_vertex_index_count) || !val_count_ok(m->skin_node_index_count)) {
            val_error(r, &r->bad_count, DTS89_ERR_RANGE);
        }
        val_ptr_count(r, m->skin_initial_verts_bits, m->skin_initial_vert_count > 0 ? m->skin_initial_vert_count * 3 : 0);
        val_ptr_count(r, m->skin_initial_norms_bits, m->skin_initial_vert_count > 0 ? m->skin_initial_vert_count * 3 : 0);
        val_ptr_count(r, m->skin_vertex_index, m->skin_vertex_index_count);
        val_ptr_count(r, m->skin_bone_index, m->skin_vertex_index_count);
        val_ptr_count(r, m->skin_weight_bits, m->skin_vertex_index_count);
        val_ptr_count(r, m->skin_node_index, m->skin_node_index_count);
    }
    if (m->mesh_type == DTS89_MESH_SORTED) {
        if (!val_count_ok(m->sorted_num_clusters)) val_error(r, &r->bad_count, DTS89_ERR_RANGE);
        val_ptr_count(r, m->sorted_clusters_bits, m->sorted_num_clusters > 0 ? m->sorted_num_clusters * 7 : 0);
    }
    if (m->mesh_type == DTS89_MESH_DECAL) {
        if (!val_count_ok(m->decal_start_primitive_count)) val_error(r, &r->bad_count, DTS89_ERR_RANGE);
        val_ptr_count(r, m->decal_start_primitives, m->decal_start_primitive_count);
    }
}

static void val_sequence_tracks(const dts89_shape *shape, const dts89_sequence *q, dts89_validate_report *r)
{
    dts89_s32 tracks;
    dts89_s32 need;
    dts89_s32 max_obj_tracks;
    int rc;
    if (!shape || !q || !r) return;
    val_name_index(r, q->name_index, shape->num_names);
    if (q->num_keyframes < 0) {
        val_error(r, &r->bad_sequence_range, DTS89_ERR_RANGE);
        return;
    }
    tracks = dts89_bitset_count(&q->rotation_matters);
    rc = val_mul_s32(tracks, q->num_keyframes, &need);
    if (rc || !val_range_s32(q->base_rotation, need, shape->num_node_rotations)) val_error(r, &r->bad_sequence_range, DTS89_ERR_RANGE);
    tracks = dts89_bitset_count(&q->translation_matters);
    rc = val_mul_s32(tracks, q->num_keyframes, &need);
    if (rc || !val_range_s32(q->base_translation, need, shape->num_node_translations)) val_error(r, &r->bad_sequence_range, DTS89_ERR_RANGE);
    tracks = dts89_bitset_count(&q->scale_matters);
    rc = val_mul_s32(tracks, q->num_keyframes, &need);
    if (rc) val_error(r, &r->bad_sequence_range, DTS89_ERR_RANGE);
    else if (tracks > 0) {
        if (q->flags & DTS89_SEQ_ARBITRARY_SCALE) {
            if (!val_range_s32(q->base_scale, need, shape->num_node_arbitrary_scale_rots)) val_error(r, &r->bad_sequence_range, DTS89_ERR_RANGE);
            if (!val_range_s32(q->base_scale, need, shape->num_node_arbitrary_scale_factors)) val_error(r, &r->bad_sequence_range, DTS89_ERR_RANGE);
        } else if (q->flags & DTS89_SEQ_ALIGNED_SCALE) {
            if (!val_range_s32(q->base_scale, need, shape->num_node_aligned_scales)) val_error(r, &r->bad_sequence_range, DTS89_ERR_RANGE);
        } else if (q->flags & DTS89_SEQ_UNIFORM_SCALE) {
            if (!val_range_s32(q->base_scale, need, shape->num_node_uniform_scales)) val_error(r, &r->bad_sequence_range, DTS89_ERR_RANGE);
        } else {
            val_warn(r, &r->bad_sequence_range, DTS89_VALIDATE_WARN_NAMED_RANGE);
        }
    }
    if (!val_range_s32(q->first_ground_frame, q->num_ground_frames, shape->num_ground_rotations)) val_error(r, &r->bad_sequence_range, DTS89_ERR_RANGE);
    if (!val_range_s32(q->first_ground_frame, q->num_ground_frames, shape->num_ground_translations)) val_error(r, &r->bad_sequence_range, DTS89_ERR_RANGE);
    if (!val_range_s32(q->first_trigger, q->num_triggers, shape->num_triggers)) val_error(r, &r->bad_trigger_range, DTS89_ERR_RANGE);
    max_obj_tracks = dts89_bitset_count(&q->vis_matters);
    tracks = dts89_bitset_count(&q->frame_matters);
    if (tracks > max_obj_tracks) max_obj_tracks = tracks;
    tracks = dts89_bitset_count(&q->mat_frame_matters);
    if (tracks > max_obj_tracks) max_obj_tracks = tracks;
    rc = val_mul_s32(max_obj_tracks, q->num_keyframes, &need);
    if (rc || !val_range_s32(q->base_object_state, need, shape->num_object_states)) val_error(r, &r->bad_sequence_range, DTS89_ERR_RANGE);
}

int dts89_validate_shape(const dts89_shape *shape, dts89_validate_report *out_report)
{
    dts89_validate_report local;
    dts89_validate_report *r;
    dts89_s32 i;
    if (!shape) return DTS89_ERR_BAD_ARG;
    r = out_report ? out_report : &local;
    val_init(r);
    if (!val_count_ok(shape->num_nodes) || !val_count_ok(shape->num_objects) || !val_count_ok(shape->num_subshapes) || !val_count_ok(shape->num_details) || !val_count_ok(shape->num_meshes) || !val_count_ok(shape->num_names) || !val_count_ok(shape->num_sequences) || !val_count_ok(shape->num_materials)) {
        val_error(r, &r->bad_count, DTS89_ERR_RANGE);
    }
    val_ptr_count(r, shape->nodes, shape->num_nodes);
    val_ptr_count(r, shape->objects, shape->num_objects);
    val_ptr_count(r, shape->meshes, shape->num_meshes);
    val_ptr_count(r, shape->names, shape->num_names);
    val_ptr_count(r, shape->details, shape->num_details);
    val_ptr_count(r, shape->sequences, shape->num_sequences);
    val_ptr_count(r, shape->materials, shape->num_materials);
    for (i = 0; i < shape->num_nodes && shape->nodes; ++i) {
        val_name_index(r, shape->nodes[i].name_index, shape->num_names);
        if (!val_index_or_minus_one(shape->nodes[i].parent_index, shape->num_nodes)) val_error(r, &r->bad_node_index, DTS89_ERR_RANGE);
        if (!val_index_or_minus_one(shape->nodes[i].first_object, shape->num_objects)) val_error(r, &r->bad_object_index, DTS89_ERR_RANGE);
        if (!val_index_or_minus_one(shape->nodes[i].first_child, shape->num_nodes)) val_error(r, &r->bad_node_index, DTS89_ERR_RANGE);
        if (!val_index_or_minus_one(shape->nodes[i].next_sibling, shape->num_nodes)) val_error(r, &r->bad_node_index, DTS89_ERR_RANGE);
    }
    for (i = 0; i < shape->num_objects && shape->objects; ++i) {
        const dts89_object *o;
        o = &shape->objects[i];
        val_name_index(r, o->name_index, shape->num_names);
        if (!val_index_or_minus_one(o->node_index, shape->num_nodes)) val_error(r, &r->bad_node_index, DTS89_ERR_RANGE);
        if (!val_index_or_minus_one(o->next_sibling, shape->num_objects)) val_error(r, &r->bad_object_index, DTS89_ERR_RANGE);
        if (!val_range_s32(o->start_mesh_index, o->num_meshes, shape->num_meshes)) val_error(r, &r->bad_mesh_range, DTS89_ERR_RANGE);
    }
    if (shape->num_subshapes > 0) {
        val_ptr_count(r, shape->subshape_first_node, shape->num_subshapes);
        val_ptr_count(r, shape->subshape_num_nodes, shape->num_subshapes);
        val_ptr_count(r, shape->subshape_first_object, shape->num_subshapes);
        val_ptr_count(r, shape->subshape_num_objects, shape->num_subshapes);
        for (i = 0; i < shape->num_subshapes && shape->subshape_first_node && shape->subshape_num_nodes && shape->subshape_first_object && shape->subshape_num_objects; ++i) {
            if (!val_range_s32(shape->subshape_first_node[i], shape->subshape_num_nodes[i], shape->num_nodes)) val_error(r, &r->bad_subshape_range, DTS89_ERR_RANGE);
            if (!val_range_s32(shape->subshape_first_object[i], shape->subshape_num_objects[i], shape->num_objects)) val_error(r, &r->bad_subshape_range, DTS89_ERR_RANGE);
        }
    }
    for (i = 0; i < shape->num_details && shape->details; ++i) {
        const dts89_detail *d;
        dts89_s32 ss;
        dts89_s32 start;
        dts89_s32 end;
        dts89_s32 oi;
        d = &shape->details[i];
        val_name_index(r, d->name_index, shape->num_names);
        ss = d->subshape_num;
        if (ss < 0 || ss >= shape->num_subshapes) {
            val_error(r, &r->bad_detail_range, DTS89_ERR_RANGE);
            continue;
        }
        if (d->object_detail_num < 0) val_warn(r, &r->bad_detail_range, DTS89_VALIDATE_WARN_EMPTY_DETAIL);
        start = shape->subshape_first_object ? shape->subshape_first_object[ss] : 0;
        end = start + (shape->subshape_num_objects ? shape->subshape_num_objects[ss] : 0);
        if (start < 0) start = 0;
        if (end > shape->num_objects) end = shape->num_objects;
        for (oi = start; oi < end && shape->objects; ++oi) {
            if (d->object_detail_num >= shape->objects[oi].num_meshes) val_warn(r, &r->bad_detail_range, DTS89_VALIDATE_WARN_EMPTY_DETAIL);
        }
    }
    for (i = 0; i < shape->num_meshes && shape->meshes; ++i) {
        val_mesh_payload(shape, &shape->meshes[i], r);
    }
    for (i = 0; i < shape->num_sequences && shape->sequences; ++i) {
        val_sequence_tracks(shape, &shape->sequences[i], r);
    }
    if (r->errors) return r->first_error_code ? r->first_error_code : DTS89_ERR_FORMAT;
    return DTS89_OK;
}

int dts89_validate_dsq(const dts89_dsq_file *dsq, dts89_validate_report *out_report)
{
    dts89_validate_report local;
    dts89_validate_report *r;
    dts89_s32 i;
    if (!dsq) return DTS89_ERR_BAD_ARG;
    r = out_report ? out_report : &local;
    val_init(r);
    if (dsq->version < 24 || dsq->version > (dts89_s16)DTS89_DSQ_VERSION_CURRENT) val_error(r, &r->bad_count, DTS89_ERR_UNSUPPORTED);
    if (!val_count_ok(dsq->num_nodes) || !val_count_ok(dsq->num_objects) || !val_count_ok(dsq->num_sequences) || !val_count_ok(dsq->num_triggers)) val_error(r, &r->bad_count, DTS89_ERR_RANGE);
    if (!val_count_ok(dsq->num_node_rotations) || !val_count_ok(dsq->num_node_translations) || !val_count_ok(dsq->num_node_uniform_scales) || !val_count_ok(dsq->num_node_aligned_scales) || !val_count_ok(dsq->num_node_arbitrary_scales) || !val_count_ok(dsq->num_ground_frames)) val_error(r, &r->bad_count, DTS89_ERR_RANGE);
    val_ptr_count(r, dsq->node_names, dsq->num_nodes);
    val_ptr_count(r, dsq->sequences, dsq->num_sequences);
    val_ptr_count(r, dsq->triggers, dsq->num_triggers);
    val_ptr_count(r, dsq->node_rotations, dsq->num_node_rotations);
    val_ptr_count(r, dsq->node_translations_bits, dsq->num_node_translations > 0 ? dsq->num_node_translations * 3 : 0);
    val_ptr_count(r, dsq->node_uniform_scales_bits, dsq->num_node_uniform_scales);
    val_ptr_count(r, dsq->node_aligned_scales_bits, dsq->num_node_aligned_scales > 0 ? dsq->num_node_aligned_scales * 3 : 0);
    val_ptr_count(r, dsq->node_arbitrary_scale_rots, dsq->num_node_arbitrary_scales);
    val_ptr_count(r, dsq->node_arbitrary_scale_factors_bits, dsq->num_node_arbitrary_scales > 0 ? dsq->num_node_arbitrary_scales * 3 : 0);
    val_ptr_count(r, dsq->ground_rotations, dsq->num_ground_frames);
    val_ptr_count(r, dsq->ground_translations_bits, dsq->num_ground_frames > 0 ? dsq->num_ground_frames * 3 : 0);
    for (i = 0; i < dsq->num_sequences && dsq->sequences; ++i) {
        const dts89_dsq_sequence *q;
        dts89_s32 need;
        dts89_s32 tracks;
        int rc;
        q = &dsq->sequences[i];
        if (q->num_keyframes < 0) val_error(r, &r->bad_sequence_range, DTS89_ERR_RANGE);
        tracks = dts89_bitset_count(&q->rotation_matters);
        rc = val_mul_s32(tracks, q->num_keyframes, &need);
        if (rc || !val_range_s32(q->base_rotation, need, dsq->num_node_rotations)) val_error(r, &r->bad_sequence_range, DTS89_ERR_RANGE);
        tracks = dts89_bitset_count(&q->translation_matters);
        rc = val_mul_s32(tracks, q->num_keyframes, &need);
        if (rc || !val_range_s32(q->base_translation, need, dsq->num_node_translations)) val_error(r, &r->bad_sequence_range, DTS89_ERR_RANGE);
        tracks = dts89_bitset_count(&q->scale_matters);
        rc = val_mul_s32(tracks, q->num_keyframes, &need);
        if (rc == DTS89_OK && tracks > 0) {
            if (q->flags & DTS89_SEQ_ARBITRARY_SCALE) {
                if (!val_range_s32(q->base_scale, need, dsq->num_node_arbitrary_scales)) val_error(r, &r->bad_sequence_range, DTS89_ERR_RANGE);
            } else if (q->flags & DTS89_SEQ_ALIGNED_SCALE) {
                if (!val_range_s32(q->base_scale, need, dsq->num_node_aligned_scales)) val_error(r, &r->bad_sequence_range, DTS89_ERR_RANGE);
            } else if (q->flags & DTS89_SEQ_UNIFORM_SCALE) {
                if (!val_range_s32(q->base_scale, need, dsq->num_node_uniform_scales)) val_error(r, &r->bad_sequence_range, DTS89_ERR_RANGE);
            }
        } else if (rc) val_error(r, &r->bad_sequence_range, DTS89_ERR_RANGE);
        if (!val_range_s32(q->first_ground_frame, q->num_ground_frames, dsq->num_ground_frames)) val_error(r, &r->bad_sequence_range, DTS89_ERR_RANGE);
        if (!val_range_s32(q->first_trigger, q->num_triggers, dsq->num_triggers)) val_error(r, &r->bad_trigger_range, DTS89_ERR_RANGE);
    }
    if (r->errors) return r->first_error_code ? r->first_error_code : DTS89_ERR_FORMAT;
    return DTS89_OK;
}
