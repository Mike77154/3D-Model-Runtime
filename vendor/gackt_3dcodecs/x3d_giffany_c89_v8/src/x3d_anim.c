#include "x3d_codec.h"

static x3d_fx xa_lerp(x3d_fx a, x3d_fx b, x3d_fx t)
{
    return a + x3d_fx_mul(b - a, t);
}

static int xa_find_span(const x3d_scene *s, const x3d_interpolator *ip, x3d_fx f, int *lo, int *hi, x3d_fx *t)
{
    int i;
    x3d_fx k0;
    x3d_fx k1;
    if (!s || !ip || ip->key_start < 0 || ip->key_count <= 0 || !lo || !hi || !t) return X3D_STATUS_BAD_ARG;
    if (ip->key_count == 1 || f <= s->fx_pool[ip->key_start]) {
        *lo = 0;
        *hi = 0;
        *t = 0;
        return X3D_STATUS_OK;
    }
    if (f >= s->fx_pool[ip->key_start + ip->key_count - 1]) {
        *lo = ip->key_count - 1;
        *hi = *lo;
        *t = 0;
        return X3D_STATUS_OK;
    }
    for (i = 0; i + 1 < ip->key_count; ++i) {
        k0 = s->fx_pool[ip->key_start + i];
        k1 = s->fx_pool[ip->key_start + i + 1];
        if (f >= k0 && f <= k1) {
            *lo = i;
            *hi = i + 1;
            if (k1 == k0) *t = 0;
            else *t = x3d_fx_div(f - k0, k1 - k0);
            return X3D_STATUS_OK;
        }
    }
    *lo = 0;
    *hi = 0;
    *t = 0;
    return X3D_STATUS_OK;
}

int x3d_eval_position_interpolator(const x3d_scene *scene, int node_index, x3d_fx fraction, x3d_vec3 *out)
{
    const x3d_interpolator *ip;
    int lo;
    int hi;
    x3d_fx t;
    x3d_vec3 a;
    x3d_vec3 b;
    if (!scene || !out || node_index < 0 || node_index >= scene->node_count) return X3D_STATUS_BAD_ARG;
    if (scene->nodes[node_index].type != X3D_NODE_POSITION_INTERPOLATOR) return X3D_STATUS_BAD_ARG;
    ip = &scene->interpolators[node_index];
    if (ip->value_start < 0 || ip->value_count <= 0) return X3D_STATUS_BAD_ARG;
    if (xa_find_span(scene, ip, fraction, &lo, &hi, &t) != X3D_STATUS_OK) return X3D_STATUS_BAD_ARG;
    if (lo >= ip->value_count) lo = ip->value_count - 1;
    if (hi >= ip->value_count) hi = ip->value_count - 1;
    a = scene->vec3_pool[ip->value_start + lo];
    b = scene->vec3_pool[ip->value_start + hi];
    out->x = xa_lerp(a.x, b.x, t);
    out->y = xa_lerp(a.y, b.y, t);
    out->z = xa_lerp(a.z, b.z, t);
    return X3D_STATUS_OK;
}

int x3d_eval_scalar_interpolator(const x3d_scene *scene, int node_index, x3d_fx fraction, x3d_fx *out)
{
    const x3d_interpolator *ip;
    int lo;
    int hi;
    x3d_fx t;
    x3d_fx a;
    x3d_fx b;
    if (!scene || !out || node_index < 0 || node_index >= scene->node_count) return X3D_STATUS_BAD_ARG;
    if (scene->nodes[node_index].type != X3D_NODE_SCALAR_INTERPOLATOR) return X3D_STATUS_BAD_ARG;
    ip = &scene->interpolators[node_index];
    if (ip->value_start < 0 || ip->value_count <= 0) return X3D_STATUS_BAD_ARG;
    if (xa_find_span(scene, ip, fraction, &lo, &hi, &t) != X3D_STATUS_OK) return X3D_STATUS_BAD_ARG;
    if (lo >= ip->value_count) lo = ip->value_count - 1;
    if (hi >= ip->value_count) hi = ip->value_count - 1;
    a = scene->fx_pool[ip->value_start + lo];
    b = scene->fx_pool[ip->value_start + hi];
    *out = xa_lerp(a, b, t);
    return X3D_STATUS_OK;
}

int x3d_eval_orientation_interpolator_linear(const x3d_scene *scene, int node_index, x3d_fx fraction, x3d_rot *out)
{
    const x3d_interpolator *ip;
    int lo;
    int hi;
    int aidx;
    int bidx;
    x3d_fx t;
    if (!scene || !out || node_index < 0 || node_index >= scene->node_count) return X3D_STATUS_BAD_ARG;
    if (scene->nodes[node_index].type != X3D_NODE_ORIENTATION_INTERPOLATOR) return X3D_STATUS_BAD_ARG;
    ip = &scene->interpolators[node_index];
    if (ip->value_start < 0 || ip->value_count <= 0) return X3D_STATUS_BAD_ARG;
    if (xa_find_span(scene, ip, fraction, &lo, &hi, &t) != X3D_STATUS_OK) return X3D_STATUS_BAD_ARG;
    if (lo >= ip->value_count) lo = ip->value_count - 1;
    if (hi >= ip->value_count) hi = ip->value_count - 1;
    aidx = ip->value_start + lo * 4;
    bidx = ip->value_start + hi * 4;
    out->x = xa_lerp(scene->fx_pool[aidx], scene->fx_pool[bidx], t);
    out->y = xa_lerp(scene->fx_pool[aidx + 1], scene->fx_pool[bidx + 1], t);
    out->z = xa_lerp(scene->fx_pool[aidx + 2], scene->fx_pool[bidx + 2], t);
    out->angle = xa_lerp(scene->fx_pool[aidx + 3], scene->fx_pool[bidx + 3], t);
    return X3D_STATUS_OK;
}
