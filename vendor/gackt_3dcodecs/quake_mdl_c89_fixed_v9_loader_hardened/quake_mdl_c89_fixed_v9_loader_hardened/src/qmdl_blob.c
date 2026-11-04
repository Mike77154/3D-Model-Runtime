#include "qmdl_internal.h"

static int qmdl_align_size(size_t *value, size_t alignment)
{
    size_t remainder;
    size_t delta;

    if (value == 0 || alignment == 0u)
    {
        return QMDL_ERR_ARGS;
    }

    remainder = (*value) % alignment;
    if (remainder == 0u)
    {
        return QMDL_OK;
    }

    delta = alignment - remainder;
    return qmdl_size_add(value, delta);
}

static int qmdl_plan_segment(size_t *cursor,
                             size_t alignment,
                             size_t count,
                             size_t elem_size,
                             size_t *offset_out)
{
    int rc;

    if (cursor == 0 || offset_out == 0)
    {
        return QMDL_ERR_ARGS;
    }

    rc = qmdl_align_size(cursor, alignment);
    if (rc != QMDL_OK)
    {
        return rc;
    }

    *offset_out = *cursor;
    if (count == 0u || elem_size == 0u)
    {
        return QMDL_OK;
    }

    return qmdl_size_add_mul(cursor, count, elem_size);
}

int qmdl_plan_storage_blob(const qmdl_requirements *requirements,
                           qmdl_blob_plan *plan)
{
    size_t cursor;
    int rc;

    if (requirements == 0 || plan == 0)
    {
        return QMDL_ERR_ARGS;
    }

    qmdl_blob_plan_clear(plan);
    cursor = 0u;

    rc = qmdl_plan_segment(&cursor,
                           sizeof(qmdl_u32),
                           (size_t)requirements->skins,
                           sizeof(qmdl_skin),
                           &plan->skins_offset);
    if (rc != QMDL_OK) return rc;

    rc = qmdl_plan_segment(&cursor,
                           sizeof(qmdl_u32),
                           (size_t)requirements->skin_intervals,
                           sizeof(qmdl_fx16),
                           &plan->skin_intervals_offset);
    if (rc != QMDL_OK) return rc;

    rc = qmdl_plan_segment(&cursor,
                           sizeof(qmdl_u8),
                           (size_t)requirements->skin_pixels,
                           sizeof(qmdl_u8),
                           &plan->skin_pixels_offset);
    if (rc != QMDL_OK) return rc;

    rc = qmdl_plan_segment(&cursor,
                           sizeof(qmdl_u32),
                           (size_t)requirements->stverts,
                           sizeof(qmdl_stvert),
                           &plan->stverts_offset);
    if (rc != QMDL_OK) return rc;

    rc = qmdl_plan_segment(&cursor,
                           sizeof(qmdl_u32),
                           (size_t)requirements->triangles,
                           sizeof(qmdl_triangle),
                           &plan->triangles_offset);
    if (rc != QMDL_OK) return rc;

    rc = qmdl_plan_segment(&cursor,
                           sizeof(qmdl_u32),
                           (size_t)requirements->frames,
                           sizeof(qmdl_frame),
                           &plan->frames_offset);
    if (rc != QMDL_OK) return rc;

    rc = qmdl_plan_segment(&cursor,
                           sizeof(qmdl_u32),
                           (size_t)requirements->frame_intervals,
                           sizeof(qmdl_fx16),
                           &plan->frame_intervals_offset);
    if (rc != QMDL_OK) return rc;

    rc = qmdl_plan_segment(&cursor,
                           sizeof(qmdl_u32),
                           (size_t)requirements->subframes,
                           sizeof(qmdl_subframe),
                           &plan->subframes_offset);
    if (rc != QMDL_OK) return rc;

    rc = qmdl_plan_segment(&cursor,
                           sizeof(qmdl_u32),
                           (size_t)requirements->packed_verts,
                           sizeof(qmdl_trivertx),
                           &plan->packed_verts_offset);
    if (rc != QMDL_OK) return rc;

    plan->total_bytes = cursor;
    return QMDL_OK;
}

int qmdl_bind_storage_blob(void *buffer,
                           size_t buffer_size,
                           const qmdl_requirements *requirements,
                           qmdl_storage *storage,
                           qmdl_blob_plan *plan_out)
{
    qmdl_blob_plan plan;
    qmdl_u8 *base;
    int rc;

    if (buffer == 0 || requirements == 0 || storage == 0)
    {
        return QMDL_ERR_ARGS;
    }

    rc = qmdl_plan_storage_blob(requirements, &plan);
    if (rc != QMDL_OK)
    {
        return rc;
    }

    if (buffer_size < plan.total_bytes)
    {
        return QMDL_ERR_SIZE;
    }

    qmdl_storage_clear(storage);
    base = (qmdl_u8 *)buffer;

    storage->skins = (requirements->skins != 0u) ?
        (qmdl_skin *)(base + plan.skins_offset) : 0;
    storage->skins_cap = requirements->skins;

    storage->skin_intervals = (requirements->skin_intervals != 0u) ?
        (qmdl_fx16 *)(base + plan.skin_intervals_offset) : 0;
    storage->skin_intervals_cap = requirements->skin_intervals;

    storage->skin_pixels = (requirements->skin_pixels != 0u) ?
        (qmdl_u8 *)(base + plan.skin_pixels_offset) : 0;
    storage->skin_pixels_cap = requirements->skin_pixels;

    storage->stverts = (requirements->stverts != 0u) ?
        (qmdl_stvert *)(base + plan.stverts_offset) : 0;
    storage->stverts_cap = requirements->stverts;

    storage->triangles = (requirements->triangles != 0u) ?
        (qmdl_triangle *)(base + plan.triangles_offset) : 0;
    storage->triangles_cap = requirements->triangles;

    storage->frames = (requirements->frames != 0u) ?
        (qmdl_frame *)(base + plan.frames_offset) : 0;
    storage->frames_cap = requirements->frames;

    storage->frame_intervals = (requirements->frame_intervals != 0u) ?
        (qmdl_fx16 *)(base + plan.frame_intervals_offset) : 0;
    storage->frame_intervals_cap = requirements->frame_intervals;

    storage->subframes = (requirements->subframes != 0u) ?
        (qmdl_subframe *)(base + plan.subframes_offset) : 0;
    storage->subframes_cap = requirements->subframes;

    storage->packed_verts = (requirements->packed_verts != 0u) ?
        (qmdl_trivertx *)(base + plan.packed_verts_offset) : 0;
    storage->packed_verts_cap = requirements->packed_verts;

    if (plan_out != 0)
    {
        *plan_out = plan;
    }

    return QMDL_OK;
}
