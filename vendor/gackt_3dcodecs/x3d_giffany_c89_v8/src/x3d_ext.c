#include "x3d_codec.h"

int x3d_is_sensor_node(int type)
{
    return type == X3D_NODE_TIME_SENSOR || type == X3D_NODE_TOUCH_SENSOR ||
        type == X3D_NODE_PLANE_SENSOR || type == X3D_NODE_CYLINDER_SENSOR ||
        type == X3D_NODE_SPHERE_SENSOR || type == X3D_NODE_PROXIMITY_SENSOR ||
        type == X3D_NODE_VISIBILITY_SENSOR || type == X3D_NODE_LOAD_SENSOR ||
        type == X3D_NODE_KEY_SENSOR || type == X3D_NODE_STRING_SENSOR ||
        type == X3D_NODE_COLLISION_SENSOR || type == X3D_NODE_LINE_PICK_SENSOR ||
        type == X3D_NODE_POINT_PICK_SENSOR || type == X3D_NODE_PRIMITIVE_PICK_SENSOR ||
        type == X3D_NODE_VOLUME_PICK_SENSOR || type == X3D_NODE_GEO_PROXIMITY_SENSOR ||
        type == X3D_NODE_GEO_TOUCH_SENSOR || type == X3D_NODE_TRANSFORM_SENSOR;
}

int x3d_eval_time_sensor_fraction(const x3d_scene *scene, int node_index, x3d_fx now_time, x3d_fx *fraction_out, int *active_out)
{
    const x3d_time_sensor *ts;
    x3d_fx span;
    x3d_fx rem;
    x3d_fx cyc;
    long q;
    int active;
    if (!scene || node_index < 0 || node_index >= scene->node_count || !fraction_out || !active_out) {
        return X3D_STATUS_BAD_ARG;
    }
    if (scene->nodes[node_index].type != X3D_NODE_TIME_SENSOR) {
        return X3D_STATUS_BAD_ARG;
    }
    ts = &scene->time_sensors[node_index];
    cyc = ts->cycle_interval;
    if (cyc <= 0) cyc = X3D_ONE;
    active = X3D_FALSE;
    *fraction_out = 0;
    if (ts->enabled && now_time >= ts->start_time) {
        if (ts->stop_time <= ts->start_time || now_time < ts->stop_time || ts->loop) {
            active = X3D_TRUE;
        }
    }
    if (active) {
        span = now_time - ts->start_time;
        if (span < 0) span = 0;
        q = (long)(span / cyc);
        rem = span - (x3d_fx)(q * cyc);
        if (!ts->loop && ts->stop_time > ts->start_time && now_time >= ts->stop_time) {
            rem = cyc;
            active = X3D_FALSE;
        }
        if (rem < 0) rem = 0;
        if (rem > cyc) rem = cyc;
        *fraction_out = x3d_fx_div(rem, cyc);
    }
    *active_out = active;
    return X3D_STATUS_OK;
}

static int x3d_abs_fx_ge_zero(x3d_fx v)
{
    return v >= 0 ? X3D_TRUE : X3D_FALSE;
}

int x3d_eval_box_sensor_contains(const x3d_scene *scene, int node_index, x3d_vec3 point, int *inside_out)
{
    const x3d_sensor *sn;
    x3d_fx hx;
    x3d_fx hy;
    x3d_fx hz;
    x3d_fx dx;
    x3d_fx dy;
    x3d_fx dz;
    int t;
    if (!scene || node_index < 0 || node_index >= scene->node_count || !inside_out) {
        return X3D_STATUS_BAD_ARG;
    }
    t = scene->nodes[node_index].type;
    if (!x3d_is_sensor_node(t)) {
        return X3D_STATUS_BAD_ARG;
    }
    sn = &scene->sensors[node_index];
    *inside_out = X3D_FALSE;
    if (!sn->enabled) return X3D_STATUS_OK;
    if (sn->size.x == 0 && sn->size.y == 0 && sn->size.z == 0) return X3D_STATUS_OK;
    hx = sn->size.x / 2;
    hy = sn->size.y / 2;
    hz = sn->size.z / 2;
    if (!x3d_abs_fx_ge_zero(hx) || !x3d_abs_fx_ge_zero(hy) || !x3d_abs_fx_ge_zero(hz)) return X3D_STATUS_OK;
    dx = point.x - sn->center.x;
    dy = point.y - sn->center.y;
    dz = point.z - sn->center.z;
    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;
    if (dz < 0) dz = -dz;
    if (dx <= hx && dy <= hy && dz <= hz) *inside_out = X3D_TRUE;
    return X3D_STATUS_OK;
}

static int x3d_node_is_coord_like(int t)
{
    return t == X3D_NODE_COORDINATE || t == X3D_NODE_COORDINATE_DOUBLE || t == X3D_NODE_GEO_COORDINATE;
}

static int x3d_get_coord_ref(const x3d_scene *scene, int node, int *start, int *count)
{
    if (!scene || node < 0 || node >= scene->node_count || !start || !count) return X3D_STATUS_BAD_ARG;
    if (!x3d_node_is_coord_like(scene->nodes[node].type)) return X3D_STATUS_BAD_ARG;
    *start = scene->coord_refs[node].start;
    *count = scene->coord_refs[node].count;
    if (*start == X3D_NONE || *count <= 0) {
        if (scene->nodes[node].type == X3D_NODE_GEO_COORDINATE) {
            *start = scene->geo_nodes[node].point_start;
            *count = scene->geo_nodes[node].point_count;
        }
    }
    if (*start == X3D_NONE || *count <= 0) return X3D_STATUS_BAD_ARG;
    return X3D_STATUS_OK;
}

int x3d_eval_nurbs_curve_point(const x3d_scene *scene, int node_index, x3d_fx u, x3d_vec3 *out)
{
    const x3d_nurbs *nu;
    int cp_node;
    int cp_start;
    int cp_count;
    int order;
    int degree;
    int i;
    int j;
    int knot_start;
    int knot_count;
    x3d_fx basis[64];
    x3d_fx next[64];
    x3d_fx knots[96];
    x3d_fx denom;
    x3d_fx left;
    x3d_fx right;
    x3d_fx a;
    x3d_fx b;
    x3d_fx w;
    x3d_fx wb;
    x3d_fx sum_w;
    x3d_vec3 acc;
    if (!scene || node_index < 0 || node_index >= scene->node_count || !out) return X3D_STATUS_BAD_ARG;
    if (scene->nodes[node_index].type != X3D_NODE_NURBS_CURVE) return X3D_STATUS_BAD_ARG;
    nu = &scene->nurbs_nodes[node_index];
    cp_node = nu->control_point_node;
    if (cp_node == X3D_NONE) cp_node = x3d_find_first_child_type(scene, node_index, X3D_NODE_COORDINATE);
    if (cp_node == X3D_NONE) cp_node = x3d_find_first_child_type(scene, node_index, X3D_NODE_COORDINATE_DOUBLE);
    if (x3d_get_coord_ref(scene, cp_node, &cp_start, &cp_count) != X3D_STATUS_OK) return X3D_STATUS_BAD_ARG;
    if (cp_count <= 0 || cp_count > 63) return X3D_STATUS_CAPACITY;
    order = nu->order;
    if (order <= 0) order = 3;
    if (order > 8) return X3D_STATUS_CAPACITY;
    degree = order - 1;
    if (degree < 1) degree = 1;
    knot_start = nu->knot_start;
    knot_count = nu->knot_count;
    if (knot_start != X3D_NONE && knot_count >= cp_count + order && knot_count <= 96) {
        for (i = 0; i < knot_count; ++i) knots[i] = scene->fx_pool[knot_start + i];
    } else {
        knot_count = cp_count + order;
        for (i = 0; i < knot_count; ++i) {
            if (i < order) knots[i] = 0;
            else if (i >= cp_count) knots[i] = X3D_ONE;
            else knots[i] = x3d_fx_div(x3d_fx_from_int((long)(i - degree)), x3d_fx_from_int((long)(cp_count - degree)));
        }
    }
    if (u < knots[degree]) u = knots[degree];
    if (u > knots[cp_count]) u = knots[cp_count];
    for (i = 0; i < cp_count; ++i) {
        if ((u >= knots[i] && u < knots[i + 1]) || (u == knots[cp_count] && i == cp_count - 1)) basis[i] = X3D_ONE;
        else basis[i] = 0;
    }
    for (j = 1; j <= degree; ++j) {
        for (i = 0; i < cp_count; ++i) {
            a = 0;
            b = 0;
            denom = knots[i + j] - knots[i];
            if (denom != 0) {
                left = u - knots[i];
                a = x3d_fx_mul(x3d_fx_div(left, denom), basis[i]);
            }
            if (i + 1 < cp_count) {
                denom = knots[i + j + 1] - knots[i + 1];
                if (denom != 0) {
                    right = knots[i + j + 1] - u;
                    b = x3d_fx_mul(x3d_fx_div(right, denom), basis[i + 1]);
                }
            }
            next[i] = a + b;
        }
        for (i = 0; i < cp_count; ++i) basis[i] = next[i];
    }
    acc.x = 0;
    acc.y = 0;
    acc.z = 0;
    sum_w = 0;
    for (i = 0; i < cp_count; ++i) {
        w = X3D_ONE;
        if (nu->weight_start != X3D_NONE && i < nu->weight_count) w = scene->fx_pool[nu->weight_start + i];
        wb = x3d_fx_mul(basis[i], w);
        acc.x += x3d_fx_mul(scene->vec3_pool[cp_start + i].x, wb);
        acc.y += x3d_fx_mul(scene->vec3_pool[cp_start + i].y, wb);
        acc.z += x3d_fx_mul(scene->vec3_pool[cp_start + i].z, wb);
        sum_w += wb;
    }
    if (sum_w == 0) return X3D_STATUS_NUMBER;
    out->x = x3d_fx_div(acc.x, sum_w);
    out->y = x3d_fx_div(acc.y, sum_w);
    out->z = x3d_fx_div(acc.z, sum_w);
    return X3D_STATUS_OK;
}

int x3d_geo_point_to_local(const x3d_scene *scene, int geo_node, int point_index, x3d_vec3 *out)
{
    const x3d_geo *g;
    x3d_vec3 origin;
    int idx;
    if (!scene || geo_node < 0 || geo_node >= scene->node_count || !out) return X3D_STATUS_BAD_ARG;
    if (scene->nodes[geo_node].type != X3D_NODE_GEO_COORDINATE) return X3D_STATUS_BAD_ARG;
    g = &scene->geo_nodes[geo_node];
    if (g->point_start == X3D_NONE || g->point_count <= 0 || point_index < 0 || point_index >= g->point_count) return X3D_STATUS_BAD_ARG;
    origin = scene->vec3_pool[g->point_start];
    idx = g->point_start + point_index;
    out->x = scene->vec3_pool[idx].x - origin.x;
    out->y = scene->vec3_pool[idx].y - origin.y;
    out->z = scene->vec3_pool[idx].z - origin.z;
    return X3D_STATUS_OK;
}

void x3d_hanim_collect_summary(const x3d_scene *scene, x3d_hanim_summary *summary)
{
    int i;
    if (!summary) return;
    summary->humanoids = 0;
    summary->joints = 0;
    summary->segments = 0;
    summary->sites = 0;
    summary->displacers = 0;
    summary->motions = 0;
    summary->skinned_joints = 0;
    if (!scene) return;
    for (i = 0; i < scene->node_count; ++i) {
        if (scene->nodes[i].type == X3D_NODE_HANIM_HUMANOID) summary->humanoids = summary->humanoids + 1;
        if (scene->nodes[i].type == X3D_NODE_HANIM_JOINT) {
            summary->joints = summary->joints + 1;
            if (scene->hanim_nodes[i].skin_coord_index_count > 0 || scene->hanim_nodes[i].skin_coord_weight_count > 0) summary->skinned_joints = summary->skinned_joints + 1;
        }
        if (scene->nodes[i].type == X3D_NODE_HANIM_SEGMENT) summary->segments = summary->segments + 1;
        if (scene->nodes[i].type == X3D_NODE_HANIM_SITE) summary->sites = summary->sites + 1;
        if (scene->nodes[i].type == X3D_NODE_HANIM_DISPLACER) summary->displacers = summary->displacers + 1;
        if (scene->nodes[i].type == X3D_NODE_HANIM_MOTION) summary->motions = summary->motions + 1;
    }
}
