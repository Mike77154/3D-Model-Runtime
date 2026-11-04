#include "usd3d_voxel_sample.h"
#include <string.h>

void usd3d_voxel_sampler_init(usd3d_voxel_sampler *s)
{
    if (!s) return;
    memset(s, 0, sizeof(*s));
    s->background = 0;
}

int usd3d_voxel_sampler_add_tile(usd3d_voxel_sampler *s, unsigned long grid_index, long x, long y, long z, usd3d_fixed value)
{
    usd3d_voxel_sample_value *v;
    if (!s) return USD3D_ERR_BAD_ARG;
    if (s->value_count >= USD3D_VOXEL_SAMPLE_MAX_VALUES) return USD3D_ERR_LIMIT;
    v = &s->values[s->value_count];
    memset(v, 0, sizeof(*v));
    v->grid_index = grid_index;
    v->x = x; v->y = y; v->z = z;
    v->value = value;
    v->active = 1;
    s->value_count++;
    return USD3D_OK;
}

int usd3d_voxel_sampler_sample_nearest(const usd3d_voxel_sampler *s, unsigned long grid_index, long x, long y, long z, usd3d_fixed *out_value)
{
    unsigned long i;
    if (!s || !out_value) return USD3D_ERR_BAD_ARG;
    for (i = 0UL; i < s->value_count; i++) {
        if (s->values[i].active && s->values[i].grid_index == grid_index && s->values[i].x == x && s->values[i].y == y && s->values[i].z == z) {
            *out_value = s->values[i].value;
            return USD3D_OK;
        }
    }
    *out_value = s->background;
    return USD3D_ERR_PARSE;
}

int usd3d_voxel_sampler_import_tree_tiles(usd3d_voxel_sampler *s, const usd3d_voxel_tree *tree)
{
    unsigned long i;
    int r;
    if (!s || !tree) return USD3D_ERR_BAD_ARG;
    for (i = 0UL; i < tree->node_count; i++) {
        if (tree->nodes[i].active && (tree->nodes[i].kind == USD3D_VOXEL_NODE_TILE || tree->nodes[i].kind == USD3D_VOXEL_NODE_LEAF)) {
            r = usd3d_voxel_sampler_add_tile(s, tree->nodes[i].grid_index, tree->nodes[i].origin_x, tree->nodes[i].origin_y, tree->nodes[i].origin_z, tree->nodes[i].constant_value);
            if (r != USD3D_OK) return r;
        }
    }
    return USD3D_OK;
}

static long vs_i16le(const unsigned char *p)
{
    unsigned long u;
    u = (unsigned long)p[0] | ((unsigned long)p[1] << 8);
    if (u & 0x8000UL) return (long)u - 65536L;
    return (long)u;
}

int usd3d_voxel_sampler_decode_dense_i16_block(usd3d_voxel_sampler *s, unsigned long grid_index, const unsigned char *data, unsigned long size, long ox, long oy, long oz, unsigned long nx, unsigned long ny, unsigned long nz, usd3d_fixed scale)
{
    unsigned long x;
    unsigned long y;
    unsigned long z;
    unsigned long pos;
    long raw;
    usd3d_fixed v;
    int r;
    if (!s || !data) return USD3D_ERR_BAD_ARG;
    if (nx == 0UL || ny == 0UL || nz == 0UL) return USD3D_ERR_BAD_ARG;
    if (size < nx * ny * nz * 2UL) return USD3D_ERR_FORMAT;
    pos = 0UL;
    for (z = 0UL; z < nz; z++) {
        for (y = 0UL; y < ny; y++) {
            for (x = 0UL; x < nx; x++) {
                raw = vs_i16le(data + pos);
                v = usd3d_fixed_mul((usd3d_fixed)raw * USD3D_FIXED_ONE, scale);
                r = usd3d_voxel_sampler_add_tile(s, grid_index, ox + (long)x, oy + (long)y, oz + (long)z, v);
                if (r != USD3D_OK) return r;
                pos += 2UL;
            }
        }
    }
    return USD3D_OK;
}

int usd3d_voxel_sampler_emit(const usd3d_voxel_sampler *s, int (*value_cb)(void *user, const usd3d_voxel_sample_value *v), void *user)
{
    unsigned long i;
    int r;
    if (!s) return USD3D_ERR_BAD_ARG;
    for (i = 0UL; i < s->value_count; i++) {
        if (s->values[i].active && value_cb) {
            r = value_cb(user, &s->values[i]);
            if (r != 0) return r;
        }
    }
    return USD3D_OK;
}
