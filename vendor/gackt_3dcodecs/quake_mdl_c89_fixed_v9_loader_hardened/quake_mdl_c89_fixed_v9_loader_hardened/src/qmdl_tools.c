#include "qmdl_internal.h"

static int qmdl_fx_add_checked(qmdl_fx16 a, qmdl_fx16 b, qmdl_fx16 *out_value)
{
    if (out_value == 0)
    {
        return QMDL_ERR_ARGS;
    }

    if (b > 0 && a > QMDL_I32_MAX - b)
    {
        return QMDL_ERR_OVERFLOW;
    }
    if (b < 0 && a < QMDL_I32_MIN - b)
    {
        return QMDL_ERR_OVERFLOW;
    }

    *out_value = a + b;
    return QMDL_OK;
}

int qmdl_group_times_are_strictly_increasing(const qmdl_fx16 *times,
                                             qmdl_u32 count)
{
    qmdl_u32 i;
    qmdl_fx16 prev;

    if (count == 0u)
    {
        return QMDL_OK;
    }
    if (times == 0)
    {
        return QMDL_ERR_ARGS;
    }

    prev = 0;
    for (i = 0; i < count; ++i)
    {
        if (times[i] <= 0)
        {
            return QMDL_ERR_FORMAT;
        }
        if (i != 0u && times[i] <= prev)
        {
            return QMDL_ERR_FORMAT;
        }
        prev = times[i];
    }

    return QMDL_OK;
}

int qmdl_group_times_from_durations(const qmdl_fx16 *durations,
                                    qmdl_u32 count,
                                    qmdl_fx16 *times_out)
{
    qmdl_u32 i;
    qmdl_fx16 total;
    int rc;

    if ((count != 0u && durations == 0) || (count != 0u && times_out == 0))
    {
        return QMDL_ERR_ARGS;
    }

    total = 0;
    for (i = 0; i < count; ++i)
    {
        if (durations[i] <= 0)
        {
            return QMDL_ERR_RANGE;
        }
        rc = qmdl_fx_add_checked(total, durations[i], &total);
        if (rc != QMDL_OK)
        {
            return rc;
        }
        times_out[i] = total;
    }

    return QMDL_OK;
}

int qmdl_group_durations_from_times(const qmdl_fx16 *times,
                                    qmdl_u32 count,
                                    qmdl_fx16 *durations_out)
{
    qmdl_u32 i;
    qmdl_fx16 prev;

    if ((count != 0u && times == 0) || (count != 0u && durations_out == 0))
    {
        return QMDL_ERR_ARGS;
    }

    prev = 0;
    for (i = 0; i < count; ++i)
    {
        if (times[i] <= 0)
        {
            return QMDL_ERR_FORMAT;
        }
        if (i != 0u && times[i] <= prev)
        {
            return QMDL_ERR_FORMAT;
        }
        durations_out[i] = times[i] - prev;
        prev = times[i];
    }

    return QMDL_OK;
}

int qmdl_rebuild_subframe_bounds(qmdl_model *model)
{
    qmdl_u32 frame_index;
    qmdl_u32 sub_index;
    qmdl_u32 vert_index;
    qmdl_u32 base_vert;
    qmdl_subframe *subframe;
    qmdl_trivertx *packed;

    if (model == 0)
    {
        return QMDL_ERR_ARGS;
    }
    if (model->header.numverts != 0u && model->packed_verts == 0)
    {
        return QMDL_ERR_ARGS;
    }
    if (model->num_subframes != 0u && model->subframes == 0)
    {
        return QMDL_ERR_ARGS;
    }

    for (frame_index = 0u; frame_index < model->header.numframes; ++frame_index)
    {
        if (model->frames == 0)
        {
            return QMDL_ERR_ARGS;
        }
        if (model->frames[frame_index].first_subframe + model->frames[frame_index].num_subframes >
            model->num_subframes)
        {
            return QMDL_ERR_RANGE;
        }
    }

    for (sub_index = 0u; sub_index < model->num_subframes; ++sub_index)
    {
        subframe = &model->subframes[sub_index];
        base_vert = subframe->first_vert;
        if (base_vert > model->num_packed_verts)
        {
            return QMDL_ERR_RANGE;
        }
        if (model->header.numverts > model->num_packed_verts - base_vert)
        {
            return QMDL_ERR_RANGE;
        }
        if (model->header.numverts == 0u)
        {
            memset(&subframe->bboxmin, 0, sizeof(subframe->bboxmin));
            memset(&subframe->bboxmax, 0, sizeof(subframe->bboxmax));
            continue;
        }

        packed = &model->packed_verts[base_vert];
        subframe->bboxmin.v[0] = packed[0].v[0];
        subframe->bboxmin.v[1] = packed[0].v[1];
        subframe->bboxmin.v[2] = packed[0].v[2];
        subframe->bboxmin.lightnormalindex = 0u;
        subframe->bboxmax.v[0] = packed[0].v[0];
        subframe->bboxmax.v[1] = packed[0].v[1];
        subframe->bboxmax.v[2] = packed[0].v[2];
        subframe->bboxmax.lightnormalindex = 0u;

        for (vert_index = 1u; vert_index < model->header.numverts; ++vert_index)
        {
            packed = &model->packed_verts[base_vert + vert_index];
            if (packed->v[0] < subframe->bboxmin.v[0]) subframe->bboxmin.v[0] = packed->v[0];
            if (packed->v[1] < subframe->bboxmin.v[1]) subframe->bboxmin.v[1] = packed->v[1];
            if (packed->v[2] < subframe->bboxmin.v[2]) subframe->bboxmin.v[2] = packed->v[2];
            if (packed->v[0] > subframe->bboxmax.v[0]) subframe->bboxmax.v[0] = packed->v[0];
            if (packed->v[1] > subframe->bboxmax.v[1]) subframe->bboxmax.v[1] = packed->v[1];
            if (packed->v[2] > subframe->bboxmax.v[2]) subframe->bboxmax.v[2] = packed->v[2];
        }
    }

    return QMDL_OK;
}

int qmdl_rebuild_frame_bounds(qmdl_model *model)
{
    qmdl_u32 frame_index;
    qmdl_u32 sub_index;
    qmdl_frame *frame;
    qmdl_subframe *subframe;

    if (model == 0 || model->frames == 0)
    {
        return QMDL_ERR_ARGS;
    }
    if (model->header.numframes != 0u && model->subframes == 0)
    {
        return QMDL_ERR_ARGS;
    }

    for (frame_index = 0u; frame_index < model->header.numframes; ++frame_index)
    {
        frame = &model->frames[frame_index];
        if (frame->num_subframes == 0u)
        {
            return QMDL_ERR_FORMAT;
        }
        if (frame->first_subframe >= model->num_subframes)
        {
            return QMDL_ERR_RANGE;
        }
        if (frame->num_subframes > model->num_subframes - frame->first_subframe)
        {
            return QMDL_ERR_RANGE;
        }

        subframe = &model->subframes[frame->first_subframe];
        frame->bboxmin = subframe->bboxmin;
        frame->bboxmax = subframe->bboxmax;

        for (sub_index = 1u; sub_index < frame->num_subframes; ++sub_index)
        {
            subframe = &model->subframes[frame->first_subframe + sub_index];
            if (subframe->bboxmin.v[0] < frame->bboxmin.v[0]) frame->bboxmin.v[0] = subframe->bboxmin.v[0];
            if (subframe->bboxmin.v[1] < frame->bboxmin.v[1]) frame->bboxmin.v[1] = subframe->bboxmin.v[1];
            if (subframe->bboxmin.v[2] < frame->bboxmin.v[2]) frame->bboxmin.v[2] = subframe->bboxmin.v[2];
            if (subframe->bboxmax.v[0] > frame->bboxmax.v[0]) frame->bboxmax.v[0] = subframe->bboxmax.v[0];
            if (subframe->bboxmax.v[1] > frame->bboxmax.v[1]) frame->bboxmax.v[1] = subframe->bboxmax.v[1];
            if (subframe->bboxmax.v[2] > frame->bboxmax.v[2]) frame->bboxmax.v[2] = subframe->bboxmax.v[2];
        }
    }

    return QMDL_OK;
}

int qmdl_group_select_index(const qmdl_fx16 *times,
                            qmdl_u32 count,
                            qmdl_fx16 time_value,
                            qmdl_u32 *index_out)
{
    qmdl_fx16 total;
    qmdl_fx16 target;
    qmdl_u32 i;
    int rc;

    if (times == 0 || index_out == 0 || count == 0u)
    {
        return QMDL_ERR_ARGS;
    }

    rc = qmdl_group_times_are_strictly_increasing(times, count);
    if (rc != QMDL_OK)
    {
        return (rc == QMDL_ERR_FORMAT) ? QMDL_ERR_RANGE : rc;
    }

    total = times[count - 1u];
    if (total <= 0)
    {
        return QMDL_ERR_RANGE;
    }

    target = time_value % total;
    if (target < 0)
    {
        target += total;
    }

    for (i = 0u; i < count; ++i)
    {
        if (target < times[i])
        {
            *index_out = i;
            return QMDL_OK;
        }
    }

    *index_out = 0u;
    return QMDL_OK;
}

int qmdl_model_select_skin_image(const qmdl_model *model,
                                 qmdl_u32 skin_index,
                                 qmdl_fx16 time_value,
                                 qmdl_u32 *image_index_out)
{
    const qmdl_skin *skin;
    qmdl_u32 local_index;
    int rc;

    if (model == 0 || image_index_out == 0)
    {
        return QMDL_ERR_ARGS;
    }
    if (skin_index >= model->header.numskins || model->skins == 0)
    {
        return QMDL_ERR_RANGE;
    }

    skin = &model->skins[skin_index];
    if (skin->type == QMDL_SKIN_SINGLE)
    {
        if (skin->num_images != 1u)
        {
            return QMDL_ERR_FORMAT;
        }
        *image_index_out = 0u;
        return QMDL_OK;
    }

    if (skin->type != QMDL_SKIN_GROUP || skin->num_images == 0u)
    {
        return QMDL_ERR_FORMAT;
    }
    if (model->skin_intervals == 0 || skin->first_interval > model->num_skin_intervals ||
        skin->num_images > model->num_skin_intervals - skin->first_interval)
    {
        return QMDL_ERR_RANGE;
    }

    rc = qmdl_group_select_index(model->skin_intervals + skin->first_interval,
                                 skin->num_images,
                                 time_value,
                                 &local_index);
    if (rc != QMDL_OK)
    {
        return rc;
    }

    *image_index_out = local_index;
    return QMDL_OK;
}

int qmdl_model_select_subframe(const qmdl_model *model,
                               qmdl_u32 frame_index,
                               qmdl_fx16 time_value,
                               qmdl_u32 *subframe_index_out)
{
    const qmdl_frame *frame;
    qmdl_u32 local_index;
    int rc;

    if (model == 0 || subframe_index_out == 0)
    {
        return QMDL_ERR_ARGS;
    }
    if (frame_index >= model->header.numframes || model->frames == 0)
    {
        return QMDL_ERR_RANGE;
    }

    frame = &model->frames[frame_index];
    if (frame->type == QMDL_FRAME_SINGLE)
    {
        if (frame->num_subframes != 1u || frame->first_subframe >= model->num_subframes)
        {
            return QMDL_ERR_FORMAT;
        }
        *subframe_index_out = frame->first_subframe;
        return QMDL_OK;
    }

    if (frame->type != QMDL_FRAME_GROUP || frame->num_subframes == 0u)
    {
        return QMDL_ERR_FORMAT;
    }
    if (model->frame_intervals == 0 || frame->first_interval > model->num_frame_intervals ||
        frame->num_subframes > model->num_frame_intervals - frame->first_interval)
    {
        return QMDL_ERR_RANGE;
    }
    if (frame->first_subframe > model->num_subframes ||
        frame->num_subframes > model->num_subframes - frame->first_subframe)
    {
        return QMDL_ERR_RANGE;
    }

    rc = qmdl_group_select_index(model->frame_intervals + frame->first_interval,
                                 frame->num_subframes,
                                 time_value,
                                 &local_index);
    if (rc != QMDL_OK)
    {
        return rc;
    }

    *subframe_index_out = frame->first_subframe + local_index;
    return QMDL_OK;
}

static int qmdl_repair_interval_run(qmdl_fx16 *times,
                                    qmdl_u32 count,
                                    qmdl_u32 *changes)
{
    qmdl_u32 i;
    qmdl_fx16 step;
    qmdl_fx16 next;
    int bad;
    int rc;

    if (times == 0 || changes == 0)
    {
        return QMDL_ERR_ARGS;
    }
    if (count == 0u)
    {
        return QMDL_OK;
    }

    bad = 0;
    for (i = 0u; i < count; ++i)
    {
        if (times[i] <= 0)
        {
            bad = 1;
        }
        if (i != 0u && times[i] <= times[i - 1u])
        {
            bad = 1;
        }
    }
    if (!bad)
    {
        return QMDL_OK;
    }

    step = qmdl_fx_from_ratio(1, 10);
    next = 0;
    for (i = 0u; i < count; ++i)
    {
        rc = qmdl_fx_add_checked(next, step, &next);
        if (rc != QMDL_OK)
        {
            return rc;
        }
        times[i] = next;
    }
    *changes += 1u;
    return QMDL_OK;
}

static int qmdl_repair_group_times(qmdl_model *model, qmdl_u32 *changes)
{
    qmdl_u32 i;
    qmdl_skin *skin;
    qmdl_frame *frame;
    int rc;

    if (model == 0 || changes == 0)
    {
        return QMDL_ERR_ARGS;
    }

    if (model->header.numskins != 0u && model->skins == 0)
    {
        return QMDL_ERR_ARGS;
    }
    for (i = 0u; i < model->header.numskins; ++i)
    {
        skin = &model->skins[i];
        if (skin->type == QMDL_SKIN_GROUP)
        {
            if (skin->num_images == 0u || model->skin_intervals == 0 ||
                skin->first_interval > model->num_skin_intervals ||
                skin->num_images > model->num_skin_intervals - skin->first_interval)
            {
                return QMDL_ERR_RANGE;
            }
            rc = qmdl_repair_interval_run(model->skin_intervals + skin->first_interval,
                                          skin->num_images,
                                          changes);
            if (rc != QMDL_OK)
            {
                return rc;
            }
        }
    }

    if (model->header.numframes != 0u && model->frames == 0)
    {
        return QMDL_ERR_ARGS;
    }
    for (i = 0u; i < model->header.numframes; ++i)
    {
        frame = &model->frames[i];
        if (frame->type == QMDL_FRAME_GROUP)
        {
            if (frame->num_subframes == 0u || model->frame_intervals == 0 ||
                frame->first_interval > model->num_frame_intervals ||
                frame->num_subframes > model->num_frame_intervals - frame->first_interval)
            {
                return QMDL_ERR_RANGE;
            }
            rc = qmdl_repair_interval_run(model->frame_intervals + frame->first_interval,
                                          frame->num_subframes,
                                          changes);
            if (rc != QMDL_OK)
            {
                return rc;
            }
        }
    }

    return QMDL_OK;
}

int qmdl_model_repair_quake(qmdl_model *model,
                            qmdl_u32 repair_flags,
                            qmdl_u32 *changes_out)
{
    qmdl_u32 i;
    qmdl_u32 j;
    qmdl_u32 changes;
    int rc;

    if (model == 0)
    {
        return QMDL_ERR_ARGS;
    }

    changes = 0u;

    if ((repair_flags & QMDL_REPAIR_SYNCTYPE) != 0u)
    {
        if (model->header.synctype != QMDL_SYNC_SYNC &&
            model->header.synctype != QMDL_SYNC_RAND)
        {
            model->header.synctype = QMDL_SYNC_SYNC;
            changes += 1u;
        }
    }

    if ((repair_flags & QMDL_REPAIR_GROUP_TIMES) != 0u)
    {
        rc = qmdl_repair_group_times(model, &changes);
        if (rc != QMDL_OK)
        {
            return rc;
        }
    }

    if ((repair_flags & QMDL_REPAIR_ONSEAM) != 0u)
    {
        if (model->header.numverts != 0u && model->stverts == 0)
        {
            return QMDL_ERR_ARGS;
        }
        for (i = 0u; i < model->header.numverts; ++i)
        {
            if (model->stverts[i].onseam != 0 &&
                model->stverts[i].onseam != (qmdl_i32)QMDL_ONSEAM_FLAG)
            {
                model->stverts[i].onseam = (qmdl_i32)QMDL_ONSEAM_FLAG;
                changes += 1u;
            }
        }
    }

    if ((repair_flags & QMDL_REPAIR_FACESFRONT) != 0u)
    {
        if (model->header.numtris != 0u && model->triangles == 0)
        {
            return QMDL_ERR_ARGS;
        }
        for (i = 0u; i < model->header.numtris; ++i)
        {
            if (model->triangles[i].facesfront != 0 && model->triangles[i].facesfront != 1)
            {
                model->triangles[i].facesfront = 1;
                changes += 1u;
            }
        }
    }

    if ((repair_flags & QMDL_REPAIR_ST) != 0u)
    {
        if (model->header.numverts != 0u && model->stverts == 0)
        {
            return QMDL_ERR_ARGS;
        }
        for (i = 0u; i < model->header.numverts; ++i)
        {
            if (model->stverts[i].s < 0)
            {
                model->stverts[i].s = 0;
                changes += 1u;
            }
            else if (model->header.skinwidth != 0u &&
                     (qmdl_u32)model->stverts[i].s >= model->header.skinwidth)
            {
                model->stverts[i].s = (qmdl_i32)(model->header.skinwidth - 1u);
                changes += 1u;
            }

            if (model->stverts[i].t < 0)
            {
                model->stverts[i].t = 0;
                changes += 1u;
            }
            else if (model->header.skinheight != 0u &&
                     (qmdl_u32)model->stverts[i].t >= model->header.skinheight)
            {
                model->stverts[i].t = (qmdl_i32)(model->header.skinheight - 1u);
                changes += 1u;
            }
        }
    }

    if ((repair_flags & QMDL_REPAIR_NORMALS) != 0u)
    {
        if (model->num_packed_verts != 0u && model->packed_verts == 0)
        {
            return QMDL_ERR_ARGS;
        }
        for (i = 0u; i < model->num_packed_verts; ++i)
        {
            if (!qmdl_vertex_normal_is_valid(model->packed_verts[i].lightnormalindex))
            {
                model->packed_verts[i].lightnormalindex = 0u;
                changes += 1u;
            }
        }
    }

    if ((repair_flags & QMDL_REPAIR_BOUNDS) != 0u)
    {
        if (model->num_subframes != 0u && model->subframes == 0)
        {
            return QMDL_ERR_ARGS;
        }
        if (model->num_packed_verts != 0u && model->packed_verts == 0)
        {
            return QMDL_ERR_ARGS;
        }
        if (model->header.numframes != 0u && model->frames == 0)
        {
            return QMDL_ERR_ARGS;
        }
        for (i = 0u; i < model->header.numframes; ++i)
        {
            if (model->frames[i].first_subframe > model->num_subframes ||
                model->frames[i].num_subframes > model->num_subframes - model->frames[i].first_subframe)
            {
                return QMDL_ERR_RANGE;
            }
            for (j = 0u; j < model->frames[i].num_subframes; ++j)
            {
                qmdl_subframe *sub;
                sub = &model->subframes[model->frames[i].first_subframe + j];
                if (sub->first_vert > model->num_packed_verts ||
                    model->header.numverts > model->num_packed_verts - sub->first_vert)
                {
                    return QMDL_ERR_RANGE;
                }
            }
        }
        rc = qmdl_rebuild_subframe_bounds(model);
        if (rc != QMDL_OK)
        {
            return rc;
        }
        rc = qmdl_rebuild_frame_bounds(model);
        if (rc != QMDL_OK)
        {
            return rc;
        }
        changes += 1u;
    }

    if (changes_out != 0)
    {
        *changes_out = changes;
    }

    return QMDL_OK;
}

static int qmdl_engine_uses_gl_skin_slots(qmdl_u32 engine)
{
    return (engine & (QMDL_ENGINE_ID1_GLQUAKE |
                      QMDL_ENGINE_QUAKESPASM |
                      QMDL_ENGINE_IRONWAIL |
                      QMDL_ENGINE_EZQUAKE)) != 0u;
}

static int qmdl_engine_uses_uniform_frame_rate(qmdl_u32 engine)
{
    return (engine & (QMDL_ENGINE_ID1_GLQUAKE |
                      QMDL_ENGINE_QUAKESPASM |
                      QMDL_ENGINE_IRONWAIL |
                      QMDL_ENGINE_DARKPLACES |
                      QMDL_ENGINE_EZQUAKE)) != 0u;
}

int qmdl_group_select_index_uniform(const qmdl_fx16 *times,
                                    qmdl_u32 count,
                                    qmdl_fx16 time_value,
                                    qmdl_u32 *index_out)
{
    qmdl_fx16 interval;
    qmdl_fx16 period;
    qmdl_fx16 target;
    qmdl_i32 slot;

    if (times == 0 || index_out == 0 || count == 0u)
    {
        return QMDL_ERR_ARGS;
    }
    if (count > 2147483647u)
    {
        return QMDL_ERR_RANGE;
    }
    interval = times[0];
    if (interval <= 0)
    {
        return QMDL_ERR_RANGE;
    }
    period = qmdl_fx_mul_int(interval, (int)count);
    if (period <= 0)
    {
        return QMDL_ERR_RANGE;
    }
    target = time_value % period;
    if (target < 0)
    {
        target += period;
    }
    slot = target / interval;
    if (slot < 0)
    {
        slot = 0;
    }
    if ((qmdl_u32)slot >= count)
    {
        slot = (qmdl_i32)(count - 1u);
    }
    *index_out = (qmdl_u32)slot;
    return QMDL_OK;
}

int qmdl_group_select_index_gl_skin(qmdl_u32 image_count,
                                    qmdl_fx16 time_value,
                                    qmdl_u32 *image_index_out)
{
    qmdl_fx16 step;
    qmdl_fx16 period;
    qmdl_fx16 target;
    qmdl_u32 slot;
    qmdl_u32 image;

    if (image_index_out == 0 || image_count == 0u)
    {
        return QMDL_ERR_ARGS;
    }
    step = qmdl_fx_from_ratio(1, 10);
    period = qmdl_fx_mul_int(step, 4);
    if (period <= 0)
    {
        return QMDL_ERR_RANGE;
    }
    target = time_value % period;
    if (target < 0)
    {
        target += period;
    }
    slot = (qmdl_u32)(target / step);
    if (slot > 3u)
    {
        slot = 3u;
    }
    if (image_count < 4u)
    {
        *image_index_out = slot % image_count;
        return QMDL_OK;
    }
    image = slot;
    while (image + 4u < image_count)
    {
        image += 4u;
    }
    *image_index_out = image;
    return QMDL_OK;
}

int qmdl_model_select_skin_image_engine(const qmdl_model *model,
                                        qmdl_u32 engine,
                                        qmdl_u32 skin_index,
                                        qmdl_fx16 time_value,
                                        qmdl_u32 *image_index_out)
{
    const qmdl_skin *skin;

    if (model == 0 || image_index_out == 0)
    {
        return QMDL_ERR_ARGS;
    }
    if (skin_index >= model->header.numskins || model->skins == 0)
    {
        return QMDL_ERR_RANGE;
    }
    skin = &model->skins[skin_index];
    if (skin->type == QMDL_SKIN_SINGLE)
    {
        if (skin->num_images != 1u)
        {
            return QMDL_ERR_FORMAT;
        }
        *image_index_out = 0u;
        return QMDL_OK;
    }
    if (skin->type != QMDL_SKIN_GROUP || skin->num_images == 0u)
    {
        return QMDL_ERR_FORMAT;
    }
    if (model->skin_intervals == 0 || skin->first_interval > model->num_skin_intervals ||
        skin->num_images > model->num_skin_intervals - skin->first_interval)
    {
        return QMDL_ERR_RANGE;
    }
    if (qmdl_engine_uses_gl_skin_slots(engine))
    {
        return qmdl_group_select_index_gl_skin(skin->num_images,
                                               time_value,
                                               image_index_out);
    }
    if ((engine & QMDL_ENGINE_DARKPLACES) != 0u)
    {
        return qmdl_group_select_index_uniform(model->skin_intervals + skin->first_interval,
                                               skin->num_images,
                                               time_value,
                                               image_index_out);
    }
    return qmdl_group_select_index(model->skin_intervals + skin->first_interval,
                                   skin->num_images,
                                   time_value,
                                   image_index_out);
}

int qmdl_model_select_subframe_engine(const qmdl_model *model,
                                      qmdl_u32 engine,
                                      qmdl_u32 frame_index,
                                      qmdl_fx16 time_value,
                                      qmdl_u32 *subframe_index_out)
{
    const qmdl_frame *frame;
    qmdl_u32 local_index;
    int rc;

    if (model == 0 || subframe_index_out == 0)
    {
        return QMDL_ERR_ARGS;
    }
    if (frame_index >= model->header.numframes || model->frames == 0)
    {
        return QMDL_ERR_RANGE;
    }
    frame = &model->frames[frame_index];
    if (frame->type == QMDL_FRAME_SINGLE)
    {
        if (frame->num_subframes != 1u || frame->first_subframe >= model->num_subframes)
        {
            return QMDL_ERR_FORMAT;
        }
        *subframe_index_out = frame->first_subframe;
        return QMDL_OK;
    }
    if (frame->type != QMDL_FRAME_GROUP || frame->num_subframes == 0u)
    {
        return QMDL_ERR_FORMAT;
    }
    if (model->frame_intervals == 0 || frame->first_interval > model->num_frame_intervals ||
        frame->num_subframes > model->num_frame_intervals - frame->first_interval)
    {
        return QMDL_ERR_RANGE;
    }
    if (frame->first_subframe > model->num_subframes ||
        frame->num_subframes > model->num_subframes - frame->first_subframe)
    {
        return QMDL_ERR_RANGE;
    }
    if (qmdl_engine_uses_uniform_frame_rate(engine))
    {
        rc = qmdl_group_select_index_uniform(model->frame_intervals + frame->first_interval,
                                             frame->num_subframes,
                                             time_value,
                                             &local_index);
    }
    else
    {
        rc = qmdl_group_select_index(model->frame_intervals + frame->first_interval,
                                     frame->num_subframes,
                                     time_value,
                                     &local_index);
    }
    if (rc != QMDL_OK)
    {
        return rc;
    }
    *subframe_index_out = frame->first_subframe + local_index;
    return QMDL_OK;
}
