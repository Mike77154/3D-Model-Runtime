#include "x3d_codec.h"

static int xv_eq(const char *a, const char *b)
{
    int i;
    if (!a || !b) return 0;
    i = 0;
    while (a[i] && b[i]) {
        if (a[i] != b[i]) return 0;
        ++i;
    }
    return a[i] == b[i];
}

static void xv_msg(x3d_diag *diag, int code, int node, const char *msg)
{
    int i;
    if (!diag) return;
    diag->code = code;
    diag->node_index = node;
    diag->line = 0;
    diag->column = 0;
    i = 0;
    if (msg) {
        while (i + 1 < (int)sizeof(diag->message) && msg[i]) {
            diag->message[i] = msg[i];
            ++i;
        }
    }
    diag->message[i] = 0;
}

int x3d_validate_scene(const x3d_scene *scene, x3d_diag *diag)
{
    int warnings;
    int i;
    int j;
    int d;
    warnings = 0;
    if (!scene) {
        xv_msg(diag, X3D_STATUS_BAD_ARG, X3D_NONE, "bad validate argument");
        return X3D_STATUS_BAD_ARG;
    }
    if (diag) {
        diag->code = X3D_STATUS_OK;
        diag->line = 0;
        diag->column = 0;
        diag->node_index = X3D_NONE;
        diag->warnings = 0;
        diag->message[0] = 0;
    }
    for (i = 0; i < scene->node_count; ++i) {
        if (scene->nodes[i].def_name[0]) {
            for (j = i + 1; j < scene->node_count; ++j) {
                if (scene->nodes[j].def_name[0] && xv_eq(scene->nodes[i].def_name, scene->nodes[j].def_name)) {
                    warnings = warnings + 1;
                    if (diag && diag->message[0] == 0) xv_msg(diag, X3D_STATUS_WARNINGS, j, "duplicate DEF name");
                }
            }
        }
        if (scene->nodes[i].use_name[0]) {
            d = x3d_find_def(scene, scene->nodes[i].use_name);
            if (d == X3D_NONE) {
                warnings = warnings + 1;
                if (diag && diag->message[0] == 0) xv_msg(diag, X3D_STATUS_WARNINGS, i, "unresolved USE name");
            }
        }
    }
    for (i = 0; i < scene->route_count; ++i) {
        d = x3d_find_def(scene, scene->routes[i].from_node);
        if (d == X3D_NONE) {
            warnings = warnings + 1;
            if (diag && diag->message[0] == 0) xv_msg(diag, X3D_STATUS_WARNINGS, scene->routes[i].node_index, "ROUTE fromNode not found");
        }
        d = x3d_find_def(scene, scene->routes[i].to_node);
        if (d == X3D_NONE) {
            warnings = warnings + 1;
            if (diag && diag->message[0] == 0) xv_msg(diag, X3D_STATUS_WARNINGS, scene->routes[i].node_index, "ROUTE toNode not found");
        }
    }
    if (scene->unsupported_node_count > 0) warnings = warnings + scene->unsupported_node_count;
    if (scene->clipped_value_count > 0) warnings = warnings + scene->clipped_value_count;
    if (diag) {
        diag->warnings = warnings;
        if (warnings > 0 && diag->message[0] == 0) xv_msg(diag, X3D_STATUS_WARNINGS, X3D_NONE, "validation warnings present");
        if (warnings == 0) diag->code = X3D_STATUS_OK;
    }
    if (warnings > 0) return X3D_STATUS_WARNINGS;
    return X3D_STATUS_OK;
}
