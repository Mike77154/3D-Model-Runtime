#include "CSM_chiquisumother3d.h"
#include "csm_util.h"

static void csm3d_bounds_add(CSM3D_Bounds *bounds, CSM3D_Vec3 p)
{
    if (p.x < bounds->min_point.x) bounds->min_point.x = p.x;
    if (p.y < bounds->min_point.y) bounds->min_point.y = p.y;
    if (p.z < bounds->min_point.z) bounds->min_point.z = p.z;
    if (p.x > bounds->max_point.x) bounds->max_point.x = p.x;
    if (p.y > bounds->max_point.y) bounds->max_point.y = p.y;
    if (p.z > bounds->max_point.z) bounds->max_point.z = p.z;
}

int csm3d_compute_frame_bounds(const CSM3D_Doc *doc, int frame_index, CSM3D_Bounds *bounds)
{
    int m;

    if (doc == 0 || bounds == 0) {
        return CSM3D_ERR_NULL;
    }
    if (frame_index < 0 || frame_index >= doc->frame_count || doc->marker_count <= 0) {
        return CSM3D_ERR_INDEX;
    }

    bounds->min_point = doc->frame[frame_index].point[0];
    bounds->max_point = doc->frame[frame_index].point[0];
    m = 1;
    while (m < doc->marker_count) {
        csm3d_bounds_add(bounds, doc->frame[frame_index].point[m]);
        m++;
    }
    return CSM3D_OK;
}

int csm3d_compute_velocity(const CSM3D_Doc *doc, int frame_index, int marker_index, CSM3D_Vec3 *out_units_per_second)
{
    CSM3D_Vec3 p0;
    CSM3D_Vec3 p1;
    int frame_delta;
    int rate;

    if (doc == 0 || out_units_per_second == 0) {
        return CSM3D_ERR_NULL;
    }
    if (frame_index < 0 || frame_index + 1 >= doc->frame_count || marker_index < 0 || marker_index >= doc->marker_count) {
        return CSM3D_ERR_INDEX;
    }
    frame_delta = doc->frame[frame_index + 1].frame_number - doc->frame[frame_index].frame_number;
    if (frame_delta == 0) {
        return CSM3D_ERR_DIV_ZERO;
    }
    if (frame_delta < 0) {
        frame_delta = -frame_delta;
    }
    rate = doc->rate;
    if (rate <= 0) {
        rate = 60;
    }

    p0 = doc->frame[frame_index].point[marker_index];
    p1 = doc->frame[frame_index + 1].point[marker_index];

    out_units_per_second->x = ((p1.x - p0.x) * (csm3d_i32)rate) / (csm3d_i32)frame_delta;
    out_units_per_second->y = ((p1.y - p0.y) * (csm3d_i32)rate) / (csm3d_i32)frame_delta;
    out_units_per_second->z = ((p1.z - p0.z) * (csm3d_i32)rate) / (csm3d_i32)frame_delta;
    return CSM3D_OK;
}

int csm3d_translate(CSM3D_Doc *doc, CSM3D_Vec3 delta)
{
    int f;
    int m;

    if (doc == 0) {
        return CSM3D_ERR_NULL;
    }
    f = 0;
    while (f < doc->frame_count) {
        m = 0;
        while (m < doc->marker_count) {
            doc->frame[f].point[m].x += delta.x;
            doc->frame[f].point[m].y += delta.y;
            doc->frame[f].point[m].z += delta.z;
            m++;
        }
        f++;
    }
    return CSM3D_OK;
}

int csm3d_scale_uniform(CSM3D_Doc *doc, csm3d_i32 scale_fx)
{
    int f;
    int m;

    if (doc == 0) {
        return CSM3D_ERR_NULL;
    }
    f = 0;
    while (f < doc->frame_count) {
        m = 0;
        while (m < doc->marker_count) {
            doc->frame[f].point[m].x = csm3d_fx_mul(doc->frame[f].point[m].x, scale_fx);
            doc->frame[f].point[m].y = csm3d_fx_mul(doc->frame[f].point[m].y, scale_fx);
            doc->frame[f].point[m].z = csm3d_fx_mul(doc->frame[f].point[m].z, scale_fx);
            m++;
        }
        f++;
    }
    return CSM3D_OK;
}

int csm3d_recenter_to_origin(CSM3D_Doc *doc)
{
    CSM3D_Bounds bounds;
    CSM3D_Vec3 delta;
    int r;

    if (doc == 0) {
        return CSM3D_ERR_NULL;
    }
    r = csm3d_compute_bounds(doc, &bounds);
    if (r != CSM3D_OK) {
        return r;
    }

    delta.x = -((bounds.min_point.x + bounds.max_point.x) / 2L);
    delta.y = -((bounds.min_point.y + bounds.max_point.y) / 2L);
    delta.z = -((bounds.min_point.z + bounds.max_point.z) / 2L);
    return csm3d_translate(doc, delta);
}

static int csm3d_axis_ok(int axis)
{
    return axis == CSM3D_AXIS_X || axis == CSM3D_AXIS_Y || axis == CSM3D_AXIS_Z;
}

static csm3d_i32 csm3d_vec_get_axis(CSM3D_Vec3 p, int axis)
{
    if (axis == CSM3D_AXIS_X) return p.x;
    if (axis == CSM3D_AXIS_Y) return p.y;
    return p.z;
}

static void csm3d_vec_set_axis(CSM3D_Vec3 *p, int axis, csm3d_i32 v)
{
    if (axis == CSM3D_AXIS_X) p->x = v;
    else if (axis == CSM3D_AXIS_Y) p->y = v;
    else p->z = v;
}

int csm3d_find_frame_index(const CSM3D_Doc *doc, int frame_number)
{
    int i;

    if (doc == 0) {
        return -1;
    }
    i = 0;
    while (i < doc->frame_count) {
        if (doc->frame[i].frame_number == frame_number) {
            return i;
        }
        i++;
    }
    return -1;
}

int csm3d_interpolate_frame(const CSM3D_Doc *doc, int frame_number, CSM3D_Frame *out_frame)
{
    int i;
    int lo;
    int hi;
    int den;
    int num;
    int m;
    CSM3D_Vec3 a;
    CSM3D_Vec3 b;

    if (doc == 0 || out_frame == 0) {
        return CSM3D_ERR_NULL;
    }
    if (doc->frame_count <= 0 || doc->marker_count <= 0) {
        return CSM3D_ERR_INVALID_DOC;
    }

    i = csm3d_find_frame_index(doc, frame_number);
    if (i >= 0) {
        *out_frame = doc->frame[i];
        return CSM3D_OK;
    }

    lo = -1;
    hi = -1;
    i = 0;
    while (i < doc->frame_count) {
        if (doc->frame[i].frame_number < frame_number) {
            if (lo < 0 || doc->frame[i].frame_number > doc->frame[lo].frame_number) {
                lo = i;
            }
        } else if (doc->frame[i].frame_number > frame_number) {
            if (hi < 0 || doc->frame[i].frame_number < doc->frame[hi].frame_number) {
                hi = i;
            }
        }
        i++;
    }
    if (lo < 0 || hi < 0) {
        return CSM3D_ERR_INDEX;
    }

    den = doc->frame[hi].frame_number - doc->frame[lo].frame_number;
    if (den == 0) {
        return CSM3D_ERR_DIV_ZERO;
    }
    num = frame_number - doc->frame[lo].frame_number;

    out_frame->frame_number = frame_number;
    m = 0;
    while (m < doc->marker_count) {
        a = doc->frame[lo].point[m];
        b = doc->frame[hi].point[m];
        out_frame->point[m].x = a.x + ((b.x - a.x) * (csm3d_i32)num) / (csm3d_i32)den;
        out_frame->point[m].y = a.y + ((b.y - a.y) * (csm3d_i32)num) / (csm3d_i32)den;
        out_frame->point[m].z = a.z + ((b.z - a.z) * (csm3d_i32)num) / (csm3d_i32)den;
        m++;
    }
    return CSM3D_OK;
}

int csm3d_keep_every_nth_frame(CSM3D_Doc *doc, int step, int keep_last)
{
    int src;
    int dst;
    int last_kept_src;

    if (doc == 0) {
        return CSM3D_ERR_NULL;
    }
    if (step <= 0) {
        return CSM3D_ERR_BAD_NUMBER;
    }
    if (doc->frame_count <= 0) {
        return CSM3D_ERR_NO_POINTS;
    }

    src = 0;
    dst = 0;
    last_kept_src = -1;
    while (src < doc->frame_count) {
        if ((src % step) == 0) {
            if (dst != src) {
                doc->frame[dst] = doc->frame[src];
            }
            last_kept_src = src;
            dst++;
        }
        src++;
    }

    if (keep_last && last_kept_src != doc->frame_count - 1) {
        doc->frame[dst] = doc->frame[doc->frame_count - 1];
        dst++;
    }

    doc->frame_count = dst;
    if (doc->frame_count > 0) {
        doc->first_frame = doc->frame[0].frame_number;
        doc->last_frame = doc->frame[doc->frame_count - 1].frame_number;
    }
    return CSM3D_OK;
}

int csm3d_detect_spike(const CSM3D_Doc *doc, csm3d_i32 max_units_per_second, CSM3D_Spike *out_spike)
{
    int f;
    int m;
    int r;
    CSM3D_Vec3 vel;
    csm3d_i32 speed;

    if (doc == 0 || out_spike == 0) {
        return CSM3D_ERR_NULL;
    }
    out_spike->present = 0;
    out_spike->frame_index = -1;
    out_spike->marker_index = -1;
    out_spike->approximate_speed = 0;
    if (doc->frame_count < 2 || doc->marker_count <= 0) {
        return CSM3D_OK;
    }

    f = 0;
    while (f + 1 < doc->frame_count) {
        m = 0;
        while (m < doc->marker_count) {
            r = csm3d_compute_velocity(doc, f, m, &vel);
            if (r != CSM3D_OK) {
                return r;
            }
            speed = csm3d_vec3_approx_length(vel);
            if (speed > max_units_per_second) {
                out_spike->present = 1;
                out_spike->frame_index = f;
                out_spike->marker_index = m;
                out_spike->approximate_speed = speed;
                return CSM3D_OK;
            }
            m++;
        }
        f++;
    }
    return CSM3D_OK;
}

static csm3d_i32 csm3d_vec3_speed_between(CSM3D_Vec3 a, CSM3D_Vec3 b, int rate, int frame_delta)
{
    CSM3D_Vec3 d;
    csm3d_i32 dist;

    if (frame_delta < 0) frame_delta = -frame_delta;
    if (frame_delta == 0) return 0;
    if (rate <= 0) rate = 60;
    d.x = b.x - a.x;
    d.y = b.y - a.y;
    d.z = b.z - a.z;
    dist = csm3d_vec3_approx_length(d);
    return (dist * (csm3d_i32)rate) / (csm3d_i32)frame_delta;
}

int csm3d_despike_linear(CSM3D_Doc *doc, csm3d_i32 max_units_per_second, int *out_replaced_count)
{
    int f;
    int m;
    int replaced;
    int den;
    int num;
    int d0;
    int d1;
    int dskip;
    int rate;
    CSM3D_Vec3 prev;
    CSM3D_Vec3 cur;
    CSM3D_Vec3 next;
    csm3d_i32 s0;
    csm3d_i32 s1;
    csm3d_i32 sskip;

    if (doc == 0) {
        return CSM3D_ERR_NULL;
    }
    if (max_units_per_second < 0) {
        return CSM3D_ERR_BAD_NUMBER;
    }
    if (out_replaced_count != 0) {
        *out_replaced_count = 0;
    }
    if (doc->frame_count < 3 || doc->marker_count <= 0) {
        return CSM3D_OK;
    }

    rate = doc->rate;
    if (rate <= 0) rate = 60;
    replaced = 0;
    f = 1;
    while (f + 1 < doc->frame_count) {
        d0 = doc->frame[f].frame_number - doc->frame[f - 1].frame_number;
        d1 = doc->frame[f + 1].frame_number - doc->frame[f].frame_number;
        dskip = doc->frame[f + 1].frame_number - doc->frame[f - 1].frame_number;
        if (d0 > 0 && d1 > 0 && dskip > 0) {
            m = 0;
            while (m < doc->marker_count) {
                prev = doc->frame[f - 1].point[m];
                cur = doc->frame[f].point[m];
                next = doc->frame[f + 1].point[m];
                s0 = csm3d_vec3_speed_between(prev, cur, rate, d0);
                s1 = csm3d_vec3_speed_between(cur, next, rate, d1);
                sskip = csm3d_vec3_speed_between(prev, next, rate, dskip);
                if (s0 > max_units_per_second && s1 > max_units_per_second && sskip <= max_units_per_second) {
                    den = dskip;
                    num = doc->frame[f].frame_number - doc->frame[f - 1].frame_number;
                    doc->frame[f].point[m].x = prev.x + ((next.x - prev.x) * (csm3d_i32)num) / (csm3d_i32)den;
                    doc->frame[f].point[m].y = prev.y + ((next.y - prev.y) * (csm3d_i32)num) / (csm3d_i32)den;
                    doc->frame[f].point[m].z = prev.z + ((next.z - prev.z) * (csm3d_i32)num) / (csm3d_i32)den;
                    replaced++;
                }
                m++;
            }
        }
        f++;
    }
    if (out_replaced_count != 0) {
        *out_replaced_count = replaced;
    }
    return CSM3D_OK;
}

int csm3d_flip_axis(CSM3D_Doc *doc, int axis)
{
    int f;
    int m;
    CSM3D_Vec3 *p;

    if (doc == 0) {
        return CSM3D_ERR_NULL;
    }
    if (!csm3d_axis_ok(axis)) {
        return CSM3D_ERR_BAD_AXIS;
    }
    f = 0;
    while (f < doc->frame_count) {
        m = 0;
        while (m < doc->marker_count) {
            p = &doc->frame[f].point[m];
            if (axis == CSM3D_AXIS_X) p->x = -p->x;
            else if (axis == CSM3D_AXIS_Y) p->y = -p->y;
            else p->z = -p->z;
            m++;
        }
        f++;
    }
    return CSM3D_OK;
}

int csm3d_convert_up_axis(CSM3D_Doc *doc, int from_axis, int to_axis)
{
    int f;
    int m;
    int src_axis;
    int dst_axis;
    int old_axes[3];
    int new_axes[3];
    CSM3D_Vec3 oldp;
    CSM3D_Vec3 newp;

    if (doc == 0) {
        return CSM3D_ERR_NULL;
    }
    if (!csm3d_axis_ok(from_axis) || !csm3d_axis_ok(to_axis)) {
        return CSM3D_ERR_BAD_AXIS;
    }
    if (from_axis == to_axis) {
        return CSM3D_OK;
    }

    new_axes[0] = -1;
    new_axes[1] = -1;
    new_axes[2] = -1;
    new_axes[to_axis] = from_axis;

    src_axis = 0;
    dst_axis = 0;
    while (dst_axis < 3) {
        if (new_axes[dst_axis] < 0) {
            while (src_axis < 3 && src_axis == from_axis) {
                src_axis++;
            }
            if (src_axis < 3) {
                new_axes[dst_axis] = src_axis;
                src_axis++;
            }
        }
        dst_axis++;
    }

    old_axes[0] = new_axes[0];
    old_axes[1] = new_axes[1];
    old_axes[2] = new_axes[2];

    f = 0;
    while (f < doc->frame_count) {
        m = 0;
        while (m < doc->marker_count) {
            oldp = doc->frame[f].point[m];
            newp.x = 0;
            newp.y = 0;
            newp.z = 0;
            csm3d_vec_set_axis(&newp, CSM3D_AXIS_X, csm3d_vec_get_axis(oldp, old_axes[0]));
            csm3d_vec_set_axis(&newp, CSM3D_AXIS_Y, csm3d_vec_get_axis(oldp, old_axes[1]));
            csm3d_vec_set_axis(&newp, CSM3D_AXIS_Z, csm3d_vec_get_axis(oldp, old_axes[2]));
            doc->frame[f].point[m] = newp;
            m++;
        }
        f++;
    }
    return CSM3D_OK;
}

int csm3d_sort_frames(CSM3D_Doc *doc)
{
    int i;
    int j;
    int min_i;
    CSM3D_Frame tmp;

    if (doc == 0) {
        return CSM3D_ERR_NULL;
    }
    i = 0;
    while (i < doc->frame_count) {
        min_i = i;
        j = i + 1;
        while (j < doc->frame_count) {
            if (doc->frame[j].frame_number < doc->frame[min_i].frame_number) {
                min_i = j;
            }
            j++;
        }
        if (min_i != i) {
            tmp = doc->frame[i];
            doc->frame[i] = doc->frame[min_i];
            doc->frame[min_i] = tmp;
        }
        i++;
    }
    if (doc->frame_count > 0) {
        doc->first_frame = doc->frame[0].frame_number;
        doc->last_frame = doc->frame[doc->frame_count - 1].frame_number;
    }
    return CSM3D_OK;
}

static void csm3d_lerp_between_frames(const CSM3D_Frame *a, const CSM3D_Frame *b, int frame_number, int marker_count, CSM3D_Frame *out_frame)
{
    int den;
    int num;
    int m;
    CSM3D_Vec3 pa;
    CSM3D_Vec3 pb;

    den = b->frame_number - a->frame_number;
    num = frame_number - a->frame_number;
    out_frame->frame_number = frame_number;
    m = 0;
    while (m < marker_count) {
        pa = a->point[m];
        pb = b->point[m];
        out_frame->point[m].x = pa.x + ((pb.x - pa.x) * (csm3d_i32)num) / (csm3d_i32)den;
        out_frame->point[m].y = pa.y + ((pb.y - pa.y) * (csm3d_i32)num) / (csm3d_i32)den;
        out_frame->point[m].z = pa.z + ((pb.z - pa.z) * (csm3d_i32)num) / (csm3d_i32)den;
        m++;
    }
}

int csm3d_fill_missing_frames(CSM3D_Doc *doc)
{
    int r;
    int i;
    int new_count;
    int delta;
    int old_i;
    int new_i;
    int step;
    CSM3D_Frame left;
    CSM3D_Frame right;
    int frame_no;

    if (doc == 0) {
        return CSM3D_ERR_NULL;
    }
    if (doc->frame_count <= 0 || doc->marker_count <= 0) {
        return CSM3D_ERR_INVALID_DOC;
    }

    r = csm3d_sort_frames(doc);
    if (r != CSM3D_OK) {
        return r;
    }

    new_count = 1;
    i = 1;
    while (i < doc->frame_count) {
        delta = doc->frame[i].frame_number - doc->frame[i - 1].frame_number;
        if (delta <= 0) {
            return CSM3D_ERR_INVALID_DOC;
        }
        new_count += delta;
        if (new_count > CSM3D_MAX_FRAMES) {
            return CSM3D_ERR_TOO_MANY_FRAMES;
        }
        i++;
    }

    if (new_count == doc->frame_count) {
        return CSM3D_OK;
    }

    old_i = doc->frame_count - 1;
    new_i = new_count - 1;
    doc->frame[new_i] = doc->frame[old_i];

    while (old_i > 0) {
        right = doc->frame[old_i];
        left = doc->frame[old_i - 1];
        delta = right.frame_number - left.frame_number;
        step = delta - 1;
        while (step >= 1) {
            frame_no = left.frame_number + step;
            new_i--;
            csm3d_lerp_between_frames(&left, &right, frame_no, doc->marker_count, &doc->frame[new_i]);
            step--;
        }
        new_i--;
        doc->frame[new_i] = left;
        old_i--;
    }

    doc->frame_count = new_count;
    doc->first_frame = doc->frame[0].frame_number;
    doc->last_frame = doc->frame[doc->frame_count - 1].frame_number;
    return CSM3D_OK;
}

int csm3d_compute_frame_center(const CSM3D_Doc *doc, int frame_index, CSM3D_Vec3 *out_center)
{
    int m;
    CSM3D_Vec3 sum;

    if (doc == 0 || out_center == 0) {
        return CSM3D_ERR_NULL;
    }
    if (frame_index < 0 || frame_index >= doc->frame_count || doc->marker_count <= 0) {
        return CSM3D_ERR_INDEX;
    }

    sum.x = 0;
    sum.y = 0;
    sum.z = 0;
    m = 0;
    while (m < doc->marker_count) {
        sum.x += doc->frame[frame_index].point[m].x;
        sum.y += doc->frame[frame_index].point[m].y;
        sum.z += doc->frame[frame_index].point[m].z;
        m++;
    }
    out_center->x = sum.x / (csm3d_i32)doc->marker_count;
    out_center->y = sum.y / (csm3d_i32)doc->marker_count;
    out_center->z = sum.z / (csm3d_i32)doc->marker_count;
    return CSM3D_OK;
}

int csm3d_compute_marker_stats(const CSM3D_Doc *doc, int marker_index, CSM3D_MarkerStats *out_stats)
{
    int f;
    int r;
    CSM3D_Vec3 p0;
    CSM3D_Vec3 p1;
    CSM3D_Vec3 d;
    CSM3D_Vec3 vel;
    csm3d_i32 speed;

    if (doc == 0 || out_stats == 0) {
        return CSM3D_ERR_NULL;
    }
    if (marker_index < 0 || marker_index >= doc->marker_count || doc->frame_count <= 0) {
        return CSM3D_ERR_INDEX;
    }

    out_stats->marker_index = marker_index;
    out_stats->first_point = doc->frame[0].point[marker_index];
    out_stats->last_point = doc->frame[doc->frame_count - 1].point[marker_index];
    out_stats->bounds.min_point = out_stats->first_point;
    out_stats->bounds.max_point = out_stats->first_point;
    out_stats->approximate_path_length = 0;
    out_stats->approximate_max_speed = 0;
    out_stats->max_speed_frame_index = -1;

    f = 0;
    while (f < doc->frame_count) {
        p1 = doc->frame[f].point[marker_index];
        csm3d_bounds_add(&out_stats->bounds, p1);
        if (f > 0) {
            p0 = doc->frame[f - 1].point[marker_index];
            d.x = p1.x - p0.x;
            d.y = p1.y - p0.y;
            d.z = p1.z - p0.z;
            out_stats->approximate_path_length += csm3d_vec3_approx_length(d);
        }
        f++;
    }

    f = 0;
    while (f + 1 < doc->frame_count) {
        r = csm3d_compute_velocity(doc, f, marker_index, &vel);
        if (r != CSM3D_OK) {
            return r;
        }
        speed = csm3d_vec3_approx_length(vel);
        if (f == 0 || speed > out_stats->approximate_max_speed) {
            out_stats->approximate_max_speed = speed;
            out_stats->max_speed_frame_index = f;
        }
        f++;
    }
    return CSM3D_OK;
}

int csm3d_update_declared_metadata(CSM3D_Doc *doc)
{
    if (doc == 0) {
        return CSM3D_ERR_NULL;
    }
    doc->declared_marker_count = doc->marker_count;
    if (doc->frame_count > 0) {
        doc->first_frame = doc->frame[0].frame_number;
        doc->last_frame = doc->frame[doc->frame_count - 1].frame_number;
    }
    if (doc->rate <= 0) {
        doc->rate = 60;
    }
    return CSM3D_OK;
}

static char csm3d_upper_char(char c)
{
    if (c >= 'a' && c <= 'z') {
        return (char)(c - 'a' + 'A');
    }
    return c;
}

int csm3d_normalize_marker_names_upper(CSM3D_Doc *doc)
{
    int i;
    int j;
    char c;

    if (doc == 0) {
        return CSM3D_ERR_NULL;
    }
    i = 0;
    while (i < doc->marker_count) {
        j = 0;
        while (j + 1 < CSM3D_MAX_NAME && doc->marker_name[i][j] != '\0') {
            c = doc->marker_name[i][j];
            if (c == ' ' || c == '\t' || c == ',' || c == ';') {
                doc->marker_name[i][j] = '_';
            } else {
                doc->marker_name[i][j] = csm3d_upper_char(c);
            }
            j++;
        }
        i++;
    }
    return CSM3D_OK;
}

int csm3d_remove_duplicate_frames(CSM3D_Doc *doc, int keep_last)
{
    int r;
    int src;
    int dst;

    if (doc == 0) {
        return CSM3D_ERR_NULL;
    }
    if (doc->frame_count <= 1) {
        return csm3d_update_declared_metadata(doc);
    }

    r = csm3d_sort_frames(doc);
    if (r != CSM3D_OK) {
        return r;
    }

    src = 0;
    dst = 0;
    while (src < doc->frame_count) {
        if (keep_last && src + 1 < doc->frame_count &&
            doc->frame[src].frame_number == doc->frame[src + 1].frame_number) {
            src++;
        } else {
            if (dst != src) {
                doc->frame[dst] = doc->frame[src];
            }
            dst++;
            src++;
        }
    }

    doc->frame_count = dst;
    return csm3d_update_declared_metadata(doc);
}

int csm3d_repair_basic(CSM3D_Doc *doc)
{
    int r;

    if (doc == 0) {
        return CSM3D_ERR_NULL;
    }
    r = csm3d_sort_frames(doc);
    if (r != CSM3D_OK) {
        return r;
    }
    r = csm3d_remove_duplicate_frames(doc, 1);
    if (r != CSM3D_OK) {
        return r;
    }
    return csm3d_update_declared_metadata(doc);
}

int csm3d_crop_frame_range(CSM3D_Doc *doc, int first_frame, int last_frame)
{
    int src;
    int dst;

    if (doc == 0) {
        return CSM3D_ERR_NULL;
    }
    if (last_frame < first_frame) {
        return CSM3D_ERR_BAD_RANGE;
    }
    src = 0;
    dst = 0;
    while (src < doc->frame_count) {
        if (doc->frame[src].frame_number >= first_frame && doc->frame[src].frame_number <= last_frame) {
            if (dst != src) {
                doc->frame[dst] = doc->frame[src];
            }
            dst++;
        }
        src++;
    }
    if (dst <= 0) {
        return CSM3D_ERR_BAD_RANGE;
    }
    doc->frame_count = dst;
    return csm3d_update_declared_metadata(doc);
}

int csm3d_offset_frame_numbers(CSM3D_Doc *doc, int delta)
{
    int f;

    if (doc == 0) {
        return CSM3D_ERR_NULL;
    }
    f = 0;
    while (f < doc->frame_count) {
        doc->frame[f].frame_number += delta;
        f++;
    }
    return csm3d_update_declared_metadata(doc);
}

int csm3d_renumber_frames(CSM3D_Doc *doc, int first_frame, int step)
{
    int f;
    int n;

    if (doc == 0) {
        return CSM3D_ERR_NULL;
    }
    if (step <= 0) {
        return CSM3D_ERR_BAD_NUMBER;
    }
    n = first_frame;
    f = 0;
    while (f < doc->frame_count) {
        doc->frame[f].frame_number = n;
        n += step;
        f++;
    }
    return csm3d_update_declared_metadata(doc);
}

static csm3d_i32 csm3d_cross_shrink(csm3d_i32 v)
{
    return v / 100L;
}

static CSM3D_Vec3 csm3d_vec_sub(CSM3D_Vec3 a, CSM3D_Vec3 b)
{
    CSM3D_Vec3 r;
    r.x = a.x - b.x;
    r.y = a.y - b.y;
    r.z = a.z - b.z;
    return r;
}

static CSM3D_Vec3 csm3d_cross_rough(CSM3D_Vec3 a, CSM3D_Vec3 b)
{
    CSM3D_Vec3 r;
    csm3d_i32 ax;
    csm3d_i32 ay;
    csm3d_i32 az;
    csm3d_i32 bx;
    csm3d_i32 by;
    csm3d_i32 bz;

    ax = csm3d_cross_shrink(a.x);
    ay = csm3d_cross_shrink(a.y);
    az = csm3d_cross_shrink(a.z);
    bx = csm3d_cross_shrink(b.x);
    by = csm3d_cross_shrink(b.y);
    bz = csm3d_cross_shrink(b.z);

    r.x = ay * bz - az * by;
    r.y = az * bx - ax * bz;
    r.z = ax * by - ay * bx;
    return r;
}

int csm3d_compute_prop_pose_at_frame(const CSM3D_Doc *doc, int prop_side, int frame_index, CSM3D_PropPose *out_pose)
{
    int r;
    int b;
    int m;
    int t;
    CSM3D_Vec3 bottom;
    CSM3D_Vec3 middle;
    CSM3D_Vec3 top;

    if (doc == 0 || out_pose == 0) {
        return CSM3D_ERR_NULL;
    }

    out_pose->present = 0;
    out_pose->prop_side = prop_side;
    out_pose->frame_index = frame_index;
    out_pose->origin.x = 0;
    out_pose->origin.y = 0;
    out_pose->origin.z = 0;
    out_pose->bottom_to_middle.x = 0;
    out_pose->bottom_to_middle.y = 0;
    out_pose->bottom_to_middle.z = 0;
    out_pose->bottom_to_top.x = 0;
    out_pose->bottom_to_top.y = 0;
    out_pose->bottom_to_top.z = 0;
    out_pose->rough_plane_normal.x = 0;
    out_pose->rough_plane_normal.y = 0;
    out_pose->rough_plane_normal.z = 0;
    out_pose->approximate_length = 0;

    if (frame_index < 0 || frame_index >= doc->frame_count) {
        return CSM3D_ERR_INDEX;
    }

    r = csm3d_find_prop_markers(doc, prop_side, &b, &m, &t);
    if (r != CSM3D_OK) {
        return r;
    }

    bottom = doc->frame[frame_index].point[b];
    middle = doc->frame[frame_index].point[m];
    top = doc->frame[frame_index].point[t];

    out_pose->present = 1;
    out_pose->origin = bottom;
    out_pose->bottom_to_middle = csm3d_vec_sub(middle, bottom);
    out_pose->bottom_to_top = csm3d_vec_sub(top, bottom);
    out_pose->rough_plane_normal = csm3d_cross_rough(out_pose->bottom_to_top, out_pose->bottom_to_middle);
    out_pose->approximate_length = csm3d_vec3_approx_length(out_pose->bottom_to_top);
    return CSM3D_OK;
}

int csm3d_compute_motion_quality(const CSM3D_Doc *doc, csm3d_i32 stationary_epsilon, csm3d_i32 high_speed_threshold, CSM3D_MotionQuality *out_quality)
{
    int i;
    int r;
    CSM3D_MarkerStats stats;

    if (doc == 0 || out_quality == 0) {
        return CSM3D_ERR_NULL;
    }
    if (stationary_epsilon < 0 || high_speed_threshold < 0) {
        return CSM3D_ERR_BAD_NUMBER;
    }
    if (doc->marker_count <= 0 || doc->frame_count <= 0) {
        return CSM3D_ERR_INVALID_DOC;
    }

    out_quality->marker_count = doc->marker_count;
    out_quality->stationary_marker_count = 0;
    out_quality->moving_marker_count = 0;
    out_quality->high_speed_marker_count = 0;
    out_quality->total_path_length = 0;
    out_quality->max_path_length = 0;
    out_quality->max_path_marker_index = -1;
    out_quality->max_marker_speed = 0;
    out_quality->max_speed_marker_index = -1;
    out_quality->max_speed_frame_index = -1;

    i = 0;
    while (i < doc->marker_count) {
        r = csm3d_compute_marker_stats(doc, i, &stats);
        if (r != CSM3D_OK) {
            return r;
        }
        out_quality->total_path_length += stats.approximate_path_length;
        if (i == 0 || stats.approximate_path_length > out_quality->max_path_length) {
            out_quality->max_path_length = stats.approximate_path_length;
            out_quality->max_path_marker_index = i;
        }
        if (i == 0 || stats.approximate_max_speed > out_quality->max_marker_speed) {
            out_quality->max_marker_speed = stats.approximate_max_speed;
            out_quality->max_speed_marker_index = i;
            out_quality->max_speed_frame_index = stats.max_speed_frame_index;
        }
        if (stats.approximate_path_length <= stationary_epsilon) {
            out_quality->stationary_marker_count++;
        } else {
            out_quality->moving_marker_count++;
        }
        if (high_speed_threshold > 0 && stats.approximate_max_speed > high_speed_threshold) {
            out_quality->high_speed_marker_count++;
        }
        i++;
    }
    return CSM3D_OK;
}

static int csm3d_order_contains(const int *order, int count, int idx)
{
    int i;
    i = 0;
    while (i < count) {
        if (order[i] == idx) {
            return 1;
        }
        i++;
    }
    return 0;
}

static void csm3d_order_add_if_present(const CSM3D_Doc *doc, const char *name, int *order, int *count)
{
    int idx;
    idx = csm3d_marker_index(doc, name);
    if (idx >= 0 && !csm3d_order_contains(order, *count, idx)) {
        order[*count] = idx;
        *count = *count + 1;
    }
}

int csm3d_reorder_markers_to_standard(CSM3D_Doc *doc)
{
    int order[CSM3D_MAX_MARKERS];
    int count;
    int i;
    int f;
    int m;
    char old_names[CSM3D_MAX_MARKERS][CSM3D_MAX_NAME];
    CSM3D_Vec3 old_points[CSM3D_MAX_MARKERS];

    if (doc == 0) {
        return CSM3D_ERR_NULL;
    }
    if (doc->marker_count <= 0) {
        return CSM3D_ERR_NO_ORDER;
    }

    count = 0;
    i = 0;
    while (i < csm3d_standard_marker_count()) {
        csm3d_order_add_if_present(doc, csm3d_standard_marker_name(i), order, &count);
        i++;
    }

    csm3d_order_add_if_present(doc, "LPRPB", order, &count);
    csm3d_order_add_if_present(doc, "LPRPM", order, &count);
    csm3d_order_add_if_present(doc, "LPRPT", order, &count);
    csm3d_order_add_if_present(doc, "MPRPB", order, &count);
    csm3d_order_add_if_present(doc, "MPRPM", order, &count);
    csm3d_order_add_if_present(doc, "MPRPT", order, &count);
    csm3d_order_add_if_present(doc, "RPRPB", order, &count);
    csm3d_order_add_if_present(doc, "RPRPM", order, &count);
    csm3d_order_add_if_present(doc, "RPRPT", order, &count);

    i = 0;
    while (i < doc->marker_count) {
        if (!csm3d_order_contains(order, count, i)) {
            order[count] = i;
            count++;
        }
        i++;
    }

    if (count != doc->marker_count) {
        return CSM3D_ERR_INVALID_DOC;
    }

    i = 0;
    while (i < doc->marker_count) {
        csm3d_copy(old_names[i], (csm3d_u32)CSM3D_MAX_NAME, doc->marker_name[i]);
        i++;
    }
    i = 0;
    while (i < doc->marker_count) {
        csm3d_copy(doc->marker_name[i], (csm3d_u32)CSM3D_MAX_NAME, old_names[order[i]]);
        i++;
    }

    f = 0;
    while (f < doc->frame_count) {
        m = 0;
        while (m < doc->marker_count) {
            old_points[m] = doc->frame[f].point[m];
            m++;
        }
        m = 0;
        while (m < doc->marker_count) {
            doc->frame[f].point[m] = old_points[order[m]];
            m++;
        }
        f++;
    }

    return csm3d_update_declared_metadata(doc);
}

