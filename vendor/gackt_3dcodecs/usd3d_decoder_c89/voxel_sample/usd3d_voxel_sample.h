#ifndef USD3D_VOXEL_SAMPLE_H
#define USD3D_VOXEL_SAMPLE_H

#include "usd3d.h"
#include "usd3d_voxel_tree.h"

#ifndef USD3D_VOXEL_SAMPLE_MAX_VALUES
#define USD3D_VOXEL_SAMPLE_MAX_VALUES 2048
#endif

typedef struct usd3d_voxel_sample_value_s {
    long x;
    long y;
    long z;
    usd3d_fixed value;
    unsigned long grid_index;
    int active;
} usd3d_voxel_sample_value;

typedef struct usd3d_voxel_sampler_s {
    usd3d_voxel_sample_value values[USD3D_VOXEL_SAMPLE_MAX_VALUES];
    unsigned long value_count;
    usd3d_fixed background;
} usd3d_voxel_sampler;

void usd3d_voxel_sampler_init(usd3d_voxel_sampler *s);
int usd3d_voxel_sampler_add_tile(usd3d_voxel_sampler *s, unsigned long grid_index, long x, long y, long z, usd3d_fixed value);
int usd3d_voxel_sampler_sample_nearest(const usd3d_voxel_sampler *s, unsigned long grid_index, long x, long y, long z, usd3d_fixed *out_value);
int usd3d_voxel_sampler_import_tree_tiles(usd3d_voxel_sampler *s, const usd3d_voxel_tree *tree);
int usd3d_voxel_sampler_decode_dense_i16_block(usd3d_voxel_sampler *s, unsigned long grid_index, const unsigned char *data, unsigned long size, long ox, long oy, long oz, unsigned long nx, unsigned long ny, unsigned long nz, usd3d_fixed scale);
int usd3d_voxel_sampler_emit(const usd3d_voxel_sampler *s, int (*value_cb)(void *user, const usd3d_voxel_sample_value *v), void *user);

#endif
