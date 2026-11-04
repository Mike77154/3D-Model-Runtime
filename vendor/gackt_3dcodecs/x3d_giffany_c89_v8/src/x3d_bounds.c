#include "x3d_codec.h"

static void xb_take_point(x3d_bounds *b, x3d_vec3 p)
{
    if (!b->valid) {
        b->min = p;
        b->max = p;
        b->valid = X3D_TRUE;
        return;
    }
    if (p.x < b->min.x) b->min.x = p.x;
    if (p.y < b->min.y) b->min.y = p.y;
    if (p.z < b->min.z) b->min.z = p.z;
    if (p.x > b->max.x) b->max.x = p.x;
    if (p.y > b->max.y) b->max.y = p.y;
    if (p.z > b->max.z) b->max.z = p.z;
}

int x3d_compute_coordinate_bounds(const x3d_scene *scene, x3d_bounds *bounds)
{
    int i;
    int j;
    int start;
    int count;
    if (!scene || !bounds) return X3D_STATUS_BAD_ARG;
    bounds->valid = X3D_FALSE;
    bounds->min.x = 0;
    bounds->min.y = 0;
    bounds->min.z = 0;
    bounds->max.x = 0;
    bounds->max.y = 0;
    bounds->max.z = 0;
    for (i = 0; i < scene->node_count; ++i) {
        if (scene->nodes[i].type == X3D_NODE_COORDINATE) {
            start = scene->coord_refs[i].start;
            count = scene->coord_refs[i].count;
            if (start >= 0 && count > 0) {
                for (j = 0; j < count; ++j) {
                    xb_take_point(bounds, scene->vec3_pool[start + j]);
                }
            }
        }
    }
    return bounds->valid ? X3D_STATUS_OK : X3D_STATUS_BAD_ARG;
}
