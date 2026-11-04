#include "CSM_chiquisumother3d.h"
#include "csm_util.h"


static int csm3d_text_has_directive(const char *text, csm3d_u32 len, const char *needle)
{
    csm3d_u32 i;
    csm3d_u32 j;
    csm3d_u32 n;

    if (text == 0 || needle == 0) {
        return 0;
    }
    n = csm3d_strlen(needle);
    if (n == 0U || len < n) {
        return 0;
    }
    i = 0U;
    while (i + n <= len) {
        j = 0U;
        while (j < n && csm3d_lower(text[i + j]) == csm3d_lower(needle[j])) {
            j++;
        }
        if (j == n) {
            return 1;
        }
        i++;
    }
    return 0;
}

int csm3d_is_probably_csm(const char *text, csm3d_u32 text_len, int *out_score)
{
    int score;

    if (text == 0) {
        if (out_score != 0) *out_score = 0;
        return 0;
    }

    score = 0;
    if (csm3d_text_has_directive(text, text_len, "$Order")) score += 4;
    if (csm3d_text_has_directive(text, text_len, "$Points")) score += 4;
    if (csm3d_text_has_directive(text, text_len, "$FirstFrame")) score += 2;
    if (csm3d_text_has_directive(text, text_len, "$LastFrame")) score += 2;
    if (csm3d_text_has_directive(text, text_len, "$Rate")) score += 1;
    if (csm3d_text_has_directive(text, text_len, "$NumMarkers")) score += 1;
    if (csm3d_text_has_directive(text, text_len, "$SpineLinks")) score += 1;

    if (out_score != 0) {
        *out_score = score;
    }
    return score >= 8;
}

void csm3d_doc_init(CSM3D_Doc *doc)
{
    int i;
    int j;

    if (doc == 0) {
        return;
    }
    doc->date[0] = '\0';
    doc->time[0] = '\0';
    doc->filename[0] = '\0';
    doc->actor[0] = '\0';
    doc->comments[0] = '\0';
    doc->first_frame = 0;
    doc->last_frame = 0;
    doc->spine_links = 0;
    doc->rate = 0;
    doc->declared_marker_count = 0;
    doc->marker_count = 0;
    doc->frame_count = 0;

    i = 0;
    while (i < CSM3D_MAX_MARKERS) {
        doc->marker_name[i][0] = '\0';
        i++;
    }
    i = 0;
    while (i < CSM3D_MAX_FRAMES) {
        doc->frame[i].frame_number = 0;
        j = 0;
        while (j < CSM3D_MAX_MARKERS) {
            doc->frame[i].point[j].x = 0;
            doc->frame[i].point[j].y = 0;
            doc->frame[i].point[j].z = 0;
            j++;
        }
        i++;
    }
}

void csm3d_diag_init(CSM3D_Diagnostic *diag)
{
    if (diag == 0) {
        return;
    }
    diag->code = CSM3D_OK;
    diag->line = 0;
    diag->marker_index = -1;
    diag->frame_index = -1;
    diag->text[0] = '\0';
}


void csm3d_analysis_init(CSM3D_Analysis *analysis)
{
    if (analysis == 0) {
        return;
    }
    analysis->flags = 0UL;
    analysis->duplicate_marker_a = -1;
    analysis->duplicate_marker_b = -1;
    analysis->frame_order_break_index = -1;
    analysis->duplicate_frame_a = -1;
    analysis->duplicate_frame_b = -1;
    analysis->unknown_marker_count = 0;
    analysis->incomplete_prop_marker_count = 0;
    analysis->first_data_frame = 0;
    analysis->last_data_frame = 0;
    analysis->declared_frame_span = 0;
}

int csm3d_analyze(const CSM3D_Doc *doc, CSM3D_Analysis *analysis)
{
    int i;
    int j;
    int b;
    int m;
    int t;
    int prop_seen;

    if (doc == 0 || analysis == 0) {
        return CSM3D_ERR_NULL;
    }

    csm3d_analysis_init(analysis);

    if (doc->frame_count > 0) {
        analysis->first_data_frame = doc->frame[0].frame_number;
        analysis->last_data_frame = doc->frame[doc->frame_count - 1].frame_number;
    }
    if (doc->first_frame != 0 || doc->last_frame != 0) {
        analysis->declared_frame_span = doc->last_frame - doc->first_frame + 1;
        if (doc->frame_count > 0) {
            if (doc->first_frame != analysis->first_data_frame || doc->last_frame != analysis->last_data_frame) {
                analysis->flags |= CSM3D_ANALYSIS_DECLARED_RANGE_MISMATCH;
            }
        }
    }
    if (doc->declared_marker_count > 0 && doc->declared_marker_count != doc->marker_count) {
        analysis->flags |= CSM3D_ANALYSIS_DECLARED_MARKER_MISMATCH;
    }

    i = 0;
    while (i < doc->marker_count) {
        j = i + 1;
        while (j < doc->marker_count) {
            if (csm3d_streq_ci(doc->marker_name[i], doc->marker_name[j])) {
                analysis->flags |= CSM3D_ANALYSIS_DUPLICATE_MARKERS;
                if (analysis->duplicate_marker_a < 0) {
                    analysis->duplicate_marker_a = i;
                    analysis->duplicate_marker_b = j;
                }
            }
            j++;
        }
        i++;
    }

    i = 0;
    while (i < doc->marker_count) {
        if (csm3d_marker_kind_from_name(doc->marker_name[i]) == CSM3D_MARKER_UNKNOWN) {
            analysis->flags |= CSM3D_ANALYSIS_UNKNOWN_MARKERS;
            analysis->unknown_marker_count++;
        }
        i++;
    }

    i = 1;
    while (i < doc->frame_count) {
        if (doc->frame[i].frame_number <= doc->frame[i - 1].frame_number) {
            analysis->flags |= CSM3D_ANALYSIS_NON_MONOTONIC_FRAMES;
            if (analysis->frame_order_break_index < 0) {
                analysis->frame_order_break_index = i;
            }
        }
        i++;
    }

    i = 0;
    while (i < doc->frame_count) {
        j = i + 1;
        while (j < doc->frame_count) {
            if (doc->frame[i].frame_number == doc->frame[j].frame_number) {
                analysis->flags |= CSM3D_ANALYSIS_DUPLICATE_FRAMES;
                if (analysis->duplicate_frame_a < 0) {
                    analysis->duplicate_frame_a = i;
                    analysis->duplicate_frame_b = j;
                }
            }
            j++;
        }
        i++;
    }

    if (doc->first_frame != 0 || doc->last_frame != 0) {
        if (doc->last_frame < doc->first_frame) {
            analysis->flags |= CSM3D_ANALYSIS_EMPTY_DECLARED_RANGE;
        }
    }

    prop_seen = 0;
    if (csm3d_find_prop_markers(doc, CSM3D_PROP_LEFT, &b, &m, &t) == CSM3D_OK) {
        analysis->flags |= CSM3D_ANALYSIS_LEFT_PROP_PRESENT;
    } else {
        prop_seen = 0;
        if (csm3d_marker_index(doc, "LPRPB") >= 0) prop_seen++;
        if (csm3d_marker_index(doc, "LPRPM") >= 0) prop_seen++;
        if (csm3d_marker_index(doc, "LPRPT") >= 0) prop_seen++;
        if (prop_seen > 0) {
            analysis->flags |= CSM3D_ANALYSIS_INCOMPLETE_PROP_MARKERS;
            analysis->incomplete_prop_marker_count += prop_seen;
        }
    }
    if (csm3d_find_prop_markers(doc, CSM3D_PROP_MIDDLE, &b, &m, &t) == CSM3D_OK) {
        analysis->flags |= CSM3D_ANALYSIS_MIDDLE_PROP_PRESENT;
    } else {
        prop_seen = 0;
        if (csm3d_marker_index(doc, "MPRPB") >= 0) prop_seen++;
        if (csm3d_marker_index(doc, "MPRPM") >= 0) prop_seen++;
        if (csm3d_marker_index(doc, "MPRPT") >= 0) prop_seen++;
        if (prop_seen > 0) {
            analysis->flags |= CSM3D_ANALYSIS_INCOMPLETE_PROP_MARKERS;
            analysis->incomplete_prop_marker_count += prop_seen;
        }
    }
    if (csm3d_find_prop_markers(doc, CSM3D_PROP_RIGHT, &b, &m, &t) == CSM3D_OK) {
        analysis->flags |= CSM3D_ANALYSIS_RIGHT_PROP_PRESENT;
    } else {
        prop_seen = 0;
        if (csm3d_marker_index(doc, "RPRPB") >= 0) prop_seen++;
        if (csm3d_marker_index(doc, "RPRPM") >= 0) prop_seen++;
        if (csm3d_marker_index(doc, "RPRPT") >= 0) prop_seen++;
        if (prop_seen > 0) {
            analysis->flags |= CSM3D_ANALYSIS_INCOMPLETE_PROP_MARKERS;
            analysis->incomplete_prop_marker_count += prop_seen;
        }
    }

    return CSM3D_OK;
}

const char *csm3d_error_string(int code)
{
    switch (code) {
    case CSM3D_OK: return "ok";
    case CSM3D_ERR_NULL: return "null argument";
    case CSM3D_ERR_BAD_TOKEN: return "bad token";
    case CSM3D_ERR_BAD_NUMBER: return "bad number";
    case CSM3D_ERR_TOO_MANY_MARKERS: return "too many markers";
    case CSM3D_ERR_TOO_MANY_FRAMES: return "too many frames";
    case CSM3D_ERR_NO_ORDER: return "missing $Order";
    case CSM3D_ERR_NO_POINTS: return "missing $Points";
    case CSM3D_ERR_TRUNCATED_POINTS: return "truncated point stream";
    case CSM3D_ERR_OUTPUT_FULL: return "output buffer full";
    case CSM3D_ERR_INDEX: return "index out of range";
    case CSM3D_ERR_INVALID_DOC: return "invalid document";
    case CSM3D_ERR_DUPLICATE_MARKER: return "duplicate marker";
    case CSM3D_ERR_BAD_PROP: return "bad prop marker set";
    case CSM3D_ERR_DIV_ZERO: return "division by zero";
    case CSM3D_ERR_TOO_MANY_MAP_ENTRIES: return "too many marker map entries";
    case CSM3D_ERR_BAD_MNM: return "bad MNM marker map";
    case CSM3D_ERR_BAD_AXIS: return "bad axis";
    case CSM3D_ERR_BAD_RANGE: return "bad frame range";
    default: return "unknown error";
    }
}

int csm3d_validate(const CSM3D_Doc *doc, CSM3D_Diagnostic *diag)
{
    if (doc == 0) {
        csm3d_set_diag(diag, CSM3D_ERR_NULL, 0, -1, -1, "null document");
        return CSM3D_ERR_NULL;
    }
    if (doc->marker_count <= 0) {
        csm3d_set_diag(diag, CSM3D_ERR_NO_ORDER, 0, -1, -1, "document has no marker order");
        return CSM3D_ERR_NO_ORDER;
    }
    if (doc->frame_count <= 0) {
        csm3d_set_diag(diag, CSM3D_ERR_NO_POINTS, 0, -1, -1, "document has no point frames");
        return CSM3D_ERR_NO_POINTS;
    }
    if (doc->declared_marker_count > 0 && doc->declared_marker_count != doc->marker_count) {
        csm3d_set_diag(diag, CSM3D_ERR_INVALID_DOC, 0, -1, -1, "declared marker count does not match $Order");
        return CSM3D_ERR_INVALID_DOC;
    }
    {
        int i;
        int j;
        i = 0;
        while (i < doc->marker_count) {
            j = i + 1;
            while (j < doc->marker_count) {
                if (csm3d_streq_ci(doc->marker_name[i], doc->marker_name[j])) {
                    csm3d_set_diag(diag, CSM3D_ERR_DUPLICATE_MARKER, 0, -1, j, "duplicate marker name in $Order");
                    return CSM3D_ERR_DUPLICATE_MARKER;
                }
                j++;
            }
            i++;
        }
    }
    if (doc->rate < 0) {
        csm3d_set_diag(diag, CSM3D_ERR_INVALID_DOC, 0, -1, -1, "negative frame rate");
        return CSM3D_ERR_INVALID_DOC;
    }
    csm3d_diag_init(diag);
    return CSM3D_OK;
}

int csm3d_marker_index(const CSM3D_Doc *doc, const char *marker_name)
{
    int i;

    if (doc == 0 || marker_name == 0) {
        return -1;
    }
    i = 0;
    while (i < doc->marker_count) {
        if (csm3d_streq_ci(doc->marker_name[i], marker_name)) {
            return i;
        }
        i++;
    }
    return -1;
}

int csm3d_get_point(const CSM3D_Doc *doc, int frame_index, int marker_index, CSM3D_Vec3 *out_point)
{
    if (doc == 0 || out_point == 0) {
        return CSM3D_ERR_NULL;
    }
    if (frame_index < 0 || frame_index >= doc->frame_count || marker_index < 0 || marker_index >= doc->marker_count) {
        return CSM3D_ERR_INDEX;
    }
    *out_point = doc->frame[frame_index].point[marker_index];
    return CSM3D_OK;
}

int csm3d_set_point(CSM3D_Doc *doc, int frame_index, int marker_index, CSM3D_Vec3 point)
{
    if (doc == 0) {
        return CSM3D_ERR_NULL;
    }
    if (frame_index < 0 || frame_index >= CSM3D_MAX_FRAMES || marker_index < 0 || marker_index >= CSM3D_MAX_MARKERS) {
        return CSM3D_ERR_INDEX;
    }
    doc->frame[frame_index].point[marker_index] = point;
    if (frame_index >= doc->frame_count) {
        doc->frame_count = frame_index + 1;
    }
    if (marker_index >= doc->marker_count) {
        doc->marker_count = marker_index + 1;
    }
    return CSM3D_OK;
}

int csm3d_compute_bounds(const CSM3D_Doc *doc, CSM3D_Bounds *bounds)
{
    int f;
    int m;
    CSM3D_Vec3 p;

    if (doc == 0 || bounds == 0) {
        return CSM3D_ERR_NULL;
    }
    if (doc->frame_count <= 0 || doc->marker_count <= 0) {
        return CSM3D_ERR_INVALID_DOC;
    }

    bounds->min_point = doc->frame[0].point[0];
    bounds->max_point = doc->frame[0].point[0];

    f = 0;
    while (f < doc->frame_count) {
        m = 0;
        while (m < doc->marker_count) {
            p = doc->frame[f].point[m];
            if (p.x < bounds->min_point.x) bounds->min_point.x = p.x;
            if (p.y < bounds->min_point.y) bounds->min_point.y = p.y;
            if (p.z < bounds->min_point.z) bounds->min_point.z = p.z;
            if (p.x > bounds->max_point.x) bounds->max_point.x = p.x;
            if (p.y > bounds->max_point.y) bounds->max_point.y = p.y;
            if (p.z > bounds->max_point.z) bounds->max_point.z = p.z;
            m++;
        }
        f++;
    }
    return CSM3D_OK;
}
