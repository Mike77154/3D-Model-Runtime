#include <string.h>
#include "usd3d_voxel_bricks.h"

static unsigned long vb_index(unsigned long x, unsigned long y, unsigned long z)
{
    return z * USD3D_VOXEL_BRICK_EDGE * USD3D_VOXEL_BRICK_EDGE + y * USD3D_VOXEL_BRICK_EDGE + x;
}

void usd3d_voxel_brick_pool_init(usd3d_voxel_brick_pool *p)
{
    if (!p) return;
    memset(p, 0, sizeof(*p));
}

int usd3d_voxel_brick_add(usd3d_voxel_brick_pool *p, unsigned long grid_index, long ox, long oy, long oz, const usd3d_fixed *values, unsigned long value_count)
{
    usd3d_voxel_brick *b;
    unsigned long i;
    if (!p) return USD3D_ERR_BAD_ARG;
    if (p->brick_count >= USD3D_VOXEL_BRICK_MAX_BRICKS) return USD3D_ERR_LIMIT;
    b = &p->bricks[p->brick_count];
    memset(b, 0, sizeof(*b));
    b->grid_index = grid_index;
    b->ox = ox;
    b->oy = oy;
    b->oz = oz;
    for (i = 0UL; i < USD3D_VOXEL_BRICK_VALUES; ++i) {
        if (values && i < value_count) b->values[i] = values[i];
        else b->values[i] = p->background;
        if (i == 0UL || b->values[i] < b->min_value) b->min_value = b->values[i];
        if (i == 0UL || b->values[i] > b->max_value) b->max_value = b->values[i];
    }
    b->active = 1;
    p->brick_count++;
    return USD3D_OK;
}

int usd3d_voxel_brick_import_sampler(usd3d_voxel_brick_pool *p, const usd3d_voxel_sampler *s)
{
    unsigned long i;
    usd3d_fixed vals[USD3D_VOXEL_BRICK_VALUES];
    long bx;
    long by;
    long bz;
    unsigned long k;
    if (!p || !s) return USD3D_ERR_BAD_ARG;
    for (i = 0UL; i < s->value_count; ++i) {
        if (s->values[i].active) {
            for (k = 0UL; k < USD3D_VOXEL_BRICK_VALUES; ++k) vals[k] = p->background;
            bx = s->values[i].x - (s->values[i].x % (long)USD3D_VOXEL_BRICK_EDGE);
            by = s->values[i].y - (s->values[i].y % (long)USD3D_VOXEL_BRICK_EDGE);
            bz = s->values[i].z - (s->values[i].z % (long)USD3D_VOXEL_BRICK_EDGE);
            k = vb_index((unsigned long)(s->values[i].x - bx), (unsigned long)(s->values[i].y - by), (unsigned long)(s->values[i].z - bz));
            if (k < USD3D_VOXEL_BRICK_VALUES) vals[k] = s->values[i].value;
            if (usd3d_voxel_brick_add(p, s->values[i].grid_index, bx, by, bz, vals, USD3D_VOXEL_BRICK_VALUES) != USD3D_OK) return USD3D_ERR_LIMIT;
        }
    }
    return USD3D_OK;
}

int usd3d_voxel_brick_sample_nearest(const usd3d_voxel_brick_pool *p, unsigned long grid_index, long x, long y, long z, usd3d_fixed *out_value)
{
    unsigned long i;
    unsigned long lx;
    unsigned long ly;
    unsigned long lz;
    unsigned long idx;
    const usd3d_voxel_brick *b;
    if (!p || !out_value) return USD3D_ERR_BAD_ARG;
    for (i = 0UL; i < p->brick_count; ++i) {
        b = &p->bricks[i];
        if (b->active && b->grid_index == grid_index && x >= b->ox && y >= b->oy && z >= b->oz && x < b->ox + (long)USD3D_VOXEL_BRICK_EDGE && y < b->oy + (long)USD3D_VOXEL_BRICK_EDGE && z < b->oz + (long)USD3D_VOXEL_BRICK_EDGE) {
            lx = (unsigned long)(x - b->ox);
            ly = (unsigned long)(y - b->oy);
            lz = (unsigned long)(z - b->oz);
            idx = vb_index(lx, ly, lz);
            *out_value = b->values[idx];
            return USD3D_OK;
        }
    }
    *out_value = p->background;
    return USD3D_ERR_PARSE;
}

int usd3d_voxel_brick_build_mip_minmax(const usd3d_voxel_brick_pool *p, usd3d_fixed *out_min, usd3d_fixed *out_max)
{
    unsigned long i;
    int any;
    if (!p || !out_min || !out_max) return USD3D_ERR_BAD_ARG;
    any = 0;
    *out_min = 0;
    *out_max = 0;
    for (i = 0UL; i < p->brick_count; ++i) {
        if (p->bricks[i].active) {
            if (!any || p->bricks[i].min_value < *out_min) *out_min = p->bricks[i].min_value;
            if (!any || p->bricks[i].max_value > *out_max) *out_max = p->bricks[i].max_value;
            any = 1;
        }
    }
    return any ? USD3D_OK : USD3D_ERR_PARSE;
}

int usd3d_voxel_brick_emit(const usd3d_voxel_brick_pool *p, int (*brick_cb)(void *user, const usd3d_voxel_brick *b), void *user)
{
    unsigned long i;
    int r;
    if (!p) return USD3D_ERR_BAD_ARG;
    for (i = 0UL; i < p->brick_count; ++i) {
        if (p->bricks[i].active && brick_cb) {
            r = brick_cb(user, &p->bricks[i]);
            if (r != 0) return r;
        }
    }
    return USD3D_OK;
}
