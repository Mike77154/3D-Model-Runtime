#include "xps_internal.h"

struct xps_count_state {
    xps_scan_result *scan;
};

static int xps_count_mesh_begin(void *user, const xps_mesh_info *mesh)
{
    struct xps_count_state *state;

    state = (struct xps_count_state *)user;
    if (state != NULL && state->scan != NULL) {
        if (mesh->uv_layers > state->scan->max_uv_layers) {
            state->scan->max_uv_layers = mesh->uv_layers;
        }
        state->scan->total_textures += mesh->texture_count;
    }
    return 0;
}

static int xps_count_vertex(void *user, xps_u32 mesh_index, const xps_vertex *vertex)
{
    struct xps_count_state *state;

    (void)mesh_index;
    state = (struct xps_count_state *)user;
    if (state != NULL && state->scan != NULL) {
        state->scan->total_vertices += 1U;
        if ((xps_u32)vertex->weight_count > state->scan->max_weights) {
            state->scan->max_weights = (xps_u32)vertex->weight_count;
        }
    }
    return 0;
}

static int xps_count_face(void *user, xps_u32 mesh_index, const xps_face *face)
{
    struct xps_count_state *state;

    (void)mesh_index;
    (void)face;
    state = (struct xps_count_state *)user;
    if (state != NULL && state->scan != NULL) {
        state->scan->total_faces += 1U;
    }
    return 0;
}

static const xps_sink XPS_COUNT_SINK = {
    0,
    0,
    xps_count_mesh_begin,
    0,
    xps_count_vertex,
    xps_count_face,
    0,
    0
};

static void xps_probe_to_model(const xps_probe_result *probe, xps_model_info *model)
{
    memset(model, 0, sizeof(*model));
    model->format = probe->format;
    model->has_header = probe->has_header;
    model->version_major = probe->version_major;
    model->version_minor = probe->version_minor;
}

static int xps_dispatch_parse(FILE *fp,
                              const xps_sink *sink,
                              void *user,
                              xps_model_info *model)
{
    if (model->format == XPS_FMT_MESH_ASCII) {
        return xps_internal_parse_ascii(fp, sink, user, model);
    }
    if (model->format == XPS_FMT_MESH_BIN || model->format == XPS_FMT_XPS) {
        return xps_internal_parse_bin(fp, sink, user, model);
    }
    return XPS_ERR_FORMAT;
}

int xps_probe_file(const char *path, xps_probe_result *out_probe)
{
    FILE *fp;
    int rc;

    if (path == NULL || out_probe == NULL) {
        return XPS_ERR_BAD_ARGUMENT;
    }

    fp = fopen(path, "rb");
    if (fp == NULL) {
        return XPS_ERR_OPEN;
    }
    rc = xps_internal_probe_fp(fp, path, out_probe);
    fclose(fp);
    return rc;
}

int xps_scan_file(const char *path, xps_scan_result *out_scan)
{
    FILE *fp;
    int rc;
    struct xps_count_state state;
    xps_probe_result probe;

    if (path == NULL || out_scan == NULL) {
        return XPS_ERR_BAD_ARGUMENT;
    }

    xps_internal_zero_scan(out_scan);
    rc = xps_probe_file(path, &probe);
    if (rc != XPS_OK) {
        return rc;
    }
    if (probe.format == XPS_FMT_POSE) {
        return XPS_ERR_FORMAT;
    }
    xps_probe_to_model(&probe, &out_scan->model);

    fp = fopen(path, "rb");
    if (fp == NULL) {
        return XPS_ERR_OPEN;
    }

    state.scan = out_scan;
    rc = xps_dispatch_parse(fp, &XPS_COUNT_SINK, &state, &out_scan->model);
    fclose(fp);
    return rc;
}

int xps_detect_file(const char *path, xps_model_info *out_model)
{
    xps_scan_result scan;
    int rc;

    if (out_model == NULL) {
        return XPS_ERR_BAD_ARGUMENT;
    }
    rc = xps_scan_file(path, &scan);
    if (rc != XPS_OK) {
        return rc;
    }
    *out_model = scan.model;
    return XPS_OK;
}

int xps_parse_file(const char *path, const xps_sink *sink, void *user)
{
    xps_scan_result scan;
    xps_model_info model;
    FILE *fp;
    int rc;

    if (path == NULL) {
        return XPS_ERR_BAD_ARGUMENT;
    }

    rc = xps_scan_file(path, &scan);
    if (rc != XPS_OK) {
        return rc;
    }

    model = scan.model;
    rc = xps_internal_invoke_model_begin(sink, user, &model);
    if (rc != XPS_OK) {
        return rc;
    }

    fp = fopen(path, "rb");
    if (fp == NULL) {
        return XPS_ERR_OPEN;
    }

    rc = xps_dispatch_parse(fp, sink, user, &model);
    fclose(fp);
    if (rc != XPS_OK) {
        return rc;
    }
    return xps_internal_invoke_model_end(sink, user, &model);
}
