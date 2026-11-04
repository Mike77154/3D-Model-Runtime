#include <string.h>
#include "ask3d_eval.h"

void ask3d_eval_transform_clear(Ask3dEvalTransform *out)
{
    if (out == 0) {
        return;
    }
    memset(out, 0, sizeof(Ask3dEvalTransform));
    out->scale.x = ASK3D_FIXED_ONE;
    out->scale.y = ASK3D_FIXED_ONE;
    out->scale.z = ASK3D_FIXED_ONE;
}

static ask3d_fixed ask3d_lerp_fixed(ask3d_fixed a, ask3d_fixed b, long ta, long tb, long t)
{
    ask3d_fixed numer;
    ask3d_fixed denom;
    ask3d_fixed k;
    if (tb == ta) {
        return a;
    }
    if (t <= ta) {
        return a;
    }
    if (t >= tb) {
        return b;
    }
    numer = ask3d_fixed_from_int(t - ta);
    denom = ask3d_fixed_from_int(tb - ta);
    k = ask3d_fixed_div(numer, denom);
    return a + ask3d_fixed_mul(b - a, k);
}

static Ask3dVec3 ask3d_lerp_vec3(const Ask3dVec3 *a, const Ask3dVec3 *b, long ta, long tb, long t)
{
    Ask3dVec3 out;
    out.x = ask3d_lerp_fixed(a->x, b->x, ta, tb, t);
    out.y = ask3d_lerp_fixed(a->y, b->y, ta, tb, t);
    out.z = ask3d_lerp_fixed(a->z, b->z, ta, tb, t);
    return out;
}

static int ask3d_pick_anim_pair(const Ask3dScene *scene, int object_index, int track_type, long timevalue, const Ask3dAnimSample **left, const Ask3dAnimSample **right)
{
    unsigned int i;
    const Ask3dAnimSample *s;
    const Ask3dAnimSample *lo;
    const Ask3dAnimSample *hi;
    lo = 0;
    hi = 0;
    if (scene == 0 || left == 0 || right == 0) {
        return 0;
    }
    for (i = 0U; i < scene->anim_sample_count; i++) {
        s = &scene->anim_samples[i];
        if (s->used == 0 || s->object_index != object_index || s->track_type != track_type) {
            continue;
        }
        if (s->timevalue <= timevalue) {
            if (lo == 0 || s->timevalue > lo->timevalue) {
                lo = s;
            }
        }
        if (s->timevalue >= timevalue) {
            if (hi == 0 || s->timevalue < hi->timevalue) {
                hi = s;
            }
        }
    }
    if (lo == 0) lo = hi;
    if (hi == 0) hi = lo;
    if (lo == 0 || hi == 0) {
        return 0;
    }
    *left = lo;
    *right = hi;
    return 1;
}

int ask3d_eval_object_transform(const Ask3dScene *scene, int object_index, long timevalue, Ask3dEvalTransform *out)
{
    const Ask3dObject *obj;
    const Ask3dAnimSample *a;
    const Ask3dAnimSample *b;
    if (scene == 0 || out == 0 || object_index < 0 || (unsigned int)object_index >= scene->object_count) {
        return 0;
    }
    ask3d_eval_transform_clear(out);
    obj = &scene->objects[object_index];
    if (obj->has_transform != 0U) {
        out->position = obj->tm_row3;
        out->has_position = 1U;
    }
    if (obj->has_tm_rot != 0U) {
        out->rotation_axis = obj->tm_rot_axis;
        out->rotation_angle = obj->tm_rot_angle;
        out->has_rotation = 1U;
    }
    if (obj->has_tm_scale != 0U) {
        out->scale = obj->tm_scale;
        out->has_scale = 1U;
    }
    if (ask3d_pick_anim_pair(scene, object_index, ASK3D_ANIM_POS, timevalue, &a, &b)) {
        out->position = ask3d_lerp_vec3(&a->value, &b->value, a->timevalue, b->timevalue, timevalue);
        out->has_position = 1U;
    }
    if (ask3d_pick_anim_pair(scene, object_index, ASK3D_ANIM_SCALE, timevalue, &a, &b)) {
        out->scale = ask3d_lerp_vec3(&a->value, &b->value, a->timevalue, b->timevalue, timevalue);
        out->has_scale = 1U;
    }
    if (ask3d_pick_anim_pair(scene, object_index, ASK3D_ANIM_ROT, timevalue, &a, &b)) {
        out->rotation_axis = ask3d_lerp_vec3(&a->value, &b->value, a->timevalue, b->timevalue, timevalue);
        if (a->has_aux != 0U && b->has_aux != 0U) {
            out->rotation_angle = ask3d_lerp_fixed(a->aux, b->aux, a->timevalue, b->timevalue, timevalue);
        } else if (a->has_aux != 0U) {
            out->rotation_angle = a->aux;
        }
        out->has_rotation = 1U;
    }
    return 1;
}

static int ask3d_pick_setting_pair(const Ask3dScene *scene, int node_type, int node_index, int setting_type, long timevalue, const Ask3dSettingSample **left, const Ask3dSettingSample **right)
{
    unsigned int i;
    const Ask3dSettingSample *s;
    const Ask3dSettingSample *lo;
    const Ask3dSettingSample *hi;
    lo = 0;
    hi = 0;
    if (scene == 0 || left == 0 || right == 0) {
        return 0;
    }
    for (i = 0U; i < scene->setting_sample_count; i++) {
        s = &scene->setting_samples[i];
        if (s->used == 0 || s->node_type != node_type || s->node_index != node_index || s->setting_type != setting_type) {
            continue;
        }
        if (s->timevalue <= timevalue) {
            if (lo == 0 || s->timevalue > lo->timevalue) {
                lo = s;
            }
        }
        if (s->timevalue >= timevalue) {
            if (hi == 0 || s->timevalue < hi->timevalue) {
                hi = s;
            }
        }
    }
    if (lo == 0) lo = hi;
    if (hi == 0) hi = lo;
    if (lo == 0 || hi == 0) {
        return 0;
    }
    *left = lo;
    *right = hi;
    return 1;
}

int ask3d_eval_setting_scalar(const Ask3dScene *scene, int node_type, int node_index, int setting_type, long timevalue, ask3d_fixed *out)
{
    const Ask3dSettingSample *a;
    const Ask3dSettingSample *b;
    if (out == 0) {
        return 0;
    }
    if (!ask3d_pick_setting_pair(scene, node_type, node_index, setting_type, timevalue, &a, &b)) {
        return 0;
    }
    if (a->has_scalar == 0U || b->has_scalar == 0U) {
        return 0;
    }
    *out = ask3d_lerp_fixed(a->scalar, b->scalar, a->timevalue, b->timevalue, timevalue);
    return 1;
}

int ask3d_eval_setting_vec3(const Ask3dScene *scene, int node_type, int node_index, int setting_type, long timevalue, Ask3dVec3 *out)
{
    const Ask3dSettingSample *a;
    const Ask3dSettingSample *b;
    if (out == 0) {
        return 0;
    }
    if (!ask3d_pick_setting_pair(scene, node_type, node_index, setting_type, timevalue, &a, &b)) {
        return 0;
    }
    if (a->has_vec == 0U || b->has_vec == 0U) {
        return 0;
    }
    *out = ask3d_lerp_vec3(&a->vec, &b->vec, a->timevalue, b->timevalue, timevalue);
    return 1;
}
