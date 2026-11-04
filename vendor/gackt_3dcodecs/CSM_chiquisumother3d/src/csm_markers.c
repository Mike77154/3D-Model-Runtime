#include "CSM_chiquisumother3d.h"
#include "csm_util.h"

static const char * const csm3d_standard_markers[] = {
    "C7", "CLAV", "STRN", "T10",
    "LFHD", "LBHD", "RFHD", "RBHD",
    "LSHO", "LUPA", "LELB", "LFRM", "LWRA", "LWRB", "LFIN",
    "RSHO", "RUPA", "RELB", "RFRM", "RWRA", "RWRB", "RFIN",
    "LASI", "RASI", "LPSI", "RPSI",
    "LTHI", "LKNE", "LTIB", "LANK", "LHEE", "LTOE",
    "RTHI", "RKNE", "RTIB", "RANK", "RHEE", "RTOE",
    0
};

static const char *csm3d_prop_name(int prop_side, int role)
{
    if (prop_side == CSM3D_PROP_LEFT) {
        if (role == 0) return "LPRPB";
        if (role == 1) return "LPRPM";
        return "LPRPT";
    }
    if (prop_side == CSM3D_PROP_MIDDLE) {
        if (role == 0) return "MPRPB";
        if (role == 1) return "MPRPM";
        return "MPRPT";
    }
    if (prop_side == CSM3D_PROP_RIGHT) {
        if (role == 0) return "RPRPB";
        if (role == 1) return "RPRPM";
        return "RPRPT";
    }
    return "";
}

int csm3d_standard_marker_count(void)
{
    int i;

    i = 0;
    while (csm3d_standard_markers[i] != 0) {
        i++;
    }
    return i;
}

const char *csm3d_standard_marker_name(int index)
{
    int count;

    count = csm3d_standard_marker_count();
    if (index < 0 || index >= count) {
        return "";
    }
    return csm3d_standard_markers[index];
}

int csm3d_compute_marker_coverage(const CSM3D_Doc *doc, CSM3D_MarkerCoverage *out_coverage)
{
    int i;
    int kind;

    if (doc == 0 || out_coverage == 0) {
        return CSM3D_ERR_NULL;
    }

    out_coverage->standard_marker_total = csm3d_standard_marker_count();
    out_coverage->present_standard_count = 0;
    out_coverage->present_prop_count = 0;
    out_coverage->unknown_count = 0;
    out_coverage->missing_standard_count = 0;
    i = 0;
    while (i < CSM3D_MAX_COVERAGE_MISSING) {
        out_coverage->missing_standard[i][0] = '\0';
        i++;
    }

    i = 0;
    while (i < doc->marker_count) {
        kind = csm3d_marker_kind_from_name(doc->marker_name[i]);
        if (kind == CSM3D_MARKER_STANDARD) {
            out_coverage->present_standard_count++;
        } else if (kind == CSM3D_MARKER_PROP_LEFT || kind == CSM3D_MARKER_PROP_MIDDLE || kind == CSM3D_MARKER_PROP_RIGHT) {
            out_coverage->present_prop_count++;
        } else {
            out_coverage->unknown_count++;
        }
        i++;
    }

    i = 0;
    while (csm3d_standard_markers[i] != 0) {
        if (csm3d_marker_index(doc, csm3d_standard_markers[i]) < 0) {
            if (out_coverage->missing_standard_count < CSM3D_MAX_COVERAGE_MISSING) {
                csm3d_copy(out_coverage->missing_standard[out_coverage->missing_standard_count], (csm3d_u32)CSM3D_MAX_NAME, csm3d_standard_markers[i]);
            }
            out_coverage->missing_standard_count++;
        }
        i++;
    }
    return CSM3D_OK;
}

int csm3d_marker_is_known_standard_name(const char *marker_name)
{
    int i;

    if (marker_name == 0) {
        return 0;
    }
    i = 0;
    while (csm3d_standard_markers[i] != 0) {
        if (csm3d_streq_ci(marker_name, csm3d_standard_markers[i])) {
            return 1;
        }
        i++;
    }
    return 0;
}

int csm3d_marker_is_prop_name(const char *marker_name)
{
    int side;
    int role;

    if (marker_name == 0) {
        return 0;
    }
    side = CSM3D_PROP_LEFT;
    while (side <= CSM3D_PROP_RIGHT) {
        role = 0;
        while (role < 3) {
            if (csm3d_streq_ci(marker_name, csm3d_prop_name(side, role))) {
                return 1;
            }
            role++;
        }
        side++;
    }
    return 0;
}

int csm3d_marker_kind_from_name(const char *marker_name)
{
    if (marker_name == 0) {
        return CSM3D_MARKER_UNKNOWN;
    }
    if (csm3d_streq_ci(marker_name, "LPRPB") ||
        csm3d_streq_ci(marker_name, "LPRPM") ||
        csm3d_streq_ci(marker_name, "LPRPT")) {
        return CSM3D_MARKER_PROP_LEFT;
    }
    if (csm3d_streq_ci(marker_name, "MPRPB") ||
        csm3d_streq_ci(marker_name, "MPRPM") ||
        csm3d_streq_ci(marker_name, "MPRPT")) {
        return CSM3D_MARKER_PROP_MIDDLE;
    }
    if (csm3d_streq_ci(marker_name, "RPRPB") ||
        csm3d_streq_ci(marker_name, "RPRPM") ||
        csm3d_streq_ci(marker_name, "RPRPT")) {
        return CSM3D_MARKER_PROP_RIGHT;
    }
    if (csm3d_marker_is_known_standard_name(marker_name)) {
        return CSM3D_MARKER_STANDARD;
    }
    return CSM3D_MARKER_UNKNOWN;
}

const char *csm3d_marker_kind_string(int marker_kind)
{
    switch (marker_kind) {
    case CSM3D_MARKER_STANDARD: return "standard";
    case CSM3D_MARKER_PROP_LEFT: return "left-prop";
    case CSM3D_MARKER_PROP_MIDDLE: return "middle-prop";
    case CSM3D_MARKER_PROP_RIGHT: return "right-prop";
    default: return "unknown";
    }
}

const char *csm3d_prop_side_name(int prop_side)
{
    switch (prop_side) {
    case CSM3D_PROP_LEFT: return "left";
    case CSM3D_PROP_MIDDLE: return "middle";
    case CSM3D_PROP_RIGHT: return "right";
    default: return "unknown";
    }
}

int csm3d_find_prop_markers(const CSM3D_Doc *doc, int prop_side, int *bottom_index, int *middle_index, int *top_index)
{
    int b;
    int m;
    int t;

    if (doc == 0) {
        return CSM3D_ERR_NULL;
    }
    if (prop_side < CSM3D_PROP_LEFT || prop_side > CSM3D_PROP_RIGHT) {
        return CSM3D_ERR_BAD_PROP;
    }

    b = csm3d_marker_index(doc, csm3d_prop_name(prop_side, 0));
    m = csm3d_marker_index(doc, csm3d_prop_name(prop_side, 1));
    t = csm3d_marker_index(doc, csm3d_prop_name(prop_side, 2));
    if (b < 0 || m < 0 || t < 0) {
        return CSM3D_ERR_BAD_PROP;
    }

    if (bottom_index != 0) *bottom_index = b;
    if (middle_index != 0) *middle_index = m;
    if (top_index != 0) *top_index = t;
    return CSM3D_OK;
}

static void csm3d_bounds_init_from_point(CSM3D_Bounds *bounds, CSM3D_Vec3 p)
{
    bounds->min_point = p;
    bounds->max_point = p;
}

static void csm3d_bounds_add_point(CSM3D_Bounds *bounds, CSM3D_Vec3 p)
{
    if (p.x < bounds->min_point.x) bounds->min_point.x = p.x;
    if (p.y < bounds->min_point.y) bounds->min_point.y = p.y;
    if (p.z < bounds->min_point.z) bounds->min_point.z = p.z;
    if (p.x > bounds->max_point.x) bounds->max_point.x = p.x;
    if (p.y > bounds->max_point.y) bounds->max_point.y = p.y;
    if (p.z > bounds->max_point.z) bounds->max_point.z = p.z;
}

int csm3d_compute_prop_info(const CSM3D_Doc *doc, int prop_side, CSM3D_PropInfo *info)
{
    int r;
    int b;
    int m;
    int t;
    int f;
    CSM3D_Vec3 pb;
    CSM3D_Vec3 pm;
    CSM3D_Vec3 pt;
    CSM3D_Vec3 delta;
    csm3d_i32 len;
    csm3d_i32 sum;

    if (doc == 0 || info == 0) {
        return CSM3D_ERR_NULL;
    }

    info->present = 0;
    info->bottom_index = -1;
    info->middle_index = -1;
    info->top_index = -1;
    info->approximate_average_length = 0;
    info->approximate_min_length = 0;
    info->approximate_max_length = 0;
    info->bounds.min_point.x = 0;
    info->bounds.min_point.y = 0;
    info->bounds.min_point.z = 0;
    info->bounds.max_point.x = 0;
    info->bounds.max_point.y = 0;
    info->bounds.max_point.z = 0;

    r = csm3d_find_prop_markers(doc, prop_side, &b, &m, &t);
    if (r != CSM3D_OK) {
        return r;
    }
    if (doc->frame_count <= 0) {
        return CSM3D_ERR_NO_POINTS;
    }

    info->present = 1;
    info->bottom_index = b;
    info->middle_index = m;
    info->top_index = t;

    pb = doc->frame[0].point[b];
    csm3d_bounds_init_from_point(&info->bounds, pb);
    csm3d_bounds_add_point(&info->bounds, doc->frame[0].point[m]);
    csm3d_bounds_add_point(&info->bounds, doc->frame[0].point[t]);

    sum = 0;
    f = 0;
    while (f < doc->frame_count) {
        pb = doc->frame[f].point[b];
        pm = doc->frame[f].point[m];
        pt = doc->frame[f].point[t];
        csm3d_bounds_add_point(&info->bounds, pb);
        csm3d_bounds_add_point(&info->bounds, pm);
        csm3d_bounds_add_point(&info->bounds, pt);
        delta.x = pt.x - pb.x;
        delta.y = pt.y - pb.y;
        delta.z = pt.z - pb.z;
        len = csm3d_vec3_approx_length(delta);
        if (f == 0 || len < info->approximate_min_length) {
            info->approximate_min_length = len;
        }
        if (f == 0 || len > info->approximate_max_length) {
            info->approximate_max_length = len;
        }
        sum += len;
        f++;
    }
    info->approximate_average_length = sum / (csm3d_i32)doc->frame_count;
    return CSM3D_OK;
}
