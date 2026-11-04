#ifndef USD3D_VOXEL_BRICKS_H
#define USD3D_VOXEL_BRICKS_H

#include "usd3d.h"
#include "usd3d_voxel_sample.h"

#ifndef USD3D_VOXEL_BRICK_MAX_BRICKS
#define USD3D_VOXEL_BRICK_MAX_BRICKS 128
#endif

#ifndef USD3D_VOXEL_BRICK_EDGE
#define USD3D_VOXEL_BRICK_EDGE 4
#endif

#define USD3D_VOXEL_BRICK_VALUES (USD3D_VOXEL_BRICK_EDGE * USD3D_VOXEL_BRICK_EDGE * USD3D_VOXEL_BRICK_EDGE)

typedef struct usd3d_voxel_brick_s {
    unsigned long grid_index;
    long ox;
    long oy;
    long oz;
    usd3d_fixed values[USD3D_VOXEL_BRICK_VALUES];
    usd3d_fixed min_value;
    usd3d_fixed max_value;
    int active;
} usd3d_voxel_brick;

typedef struct usd3d_voxel_brick_pool_s {
    usd3d_voxel_brick bricks[USD3D_VOXEL_BRICK_MAX_BRICKS];
    unsigned long brick_count;
    usd3d_fixed background;
} usd3d_voxel_brick_pool;

void usd3d_voxel_brick_pool_init(usd3d_voxel_brick_pool *p);
int usd3d_voxel_brick_add(usd3d_voxel_brick_pool *p, unsigned long grid_index, long ox, long oy, long oz, const usd3d_fixed *values, unsigned long value_count);
int usd3d_voxel_brick_import_sampler(usd3d_voxel_brick_pool *p, const usd3d_voxel_sampler *s);
int usd3d_voxel_brick_sample_nearest(const usd3d_voxel_brick_pool *p, unsigned long grid_index, long x, long y, long z, usd3d_fixed *out_value);
int usd3d_voxel_brick_build_mip_minmax(const usd3d_voxel_brick_pool *p, usd3d_fixed *out_min, usd3d_fixed *out_max);
int usd3d_voxel_brick_emit(const usd3d_voxel_brick_pool *p, int (*brick_cb)(void *user, const usd3d_voxel_brick *b), void *user);

#endif
