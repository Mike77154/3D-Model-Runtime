#ifndef GLTF3D_MINI_MESHOPT_H
#define GLTF3D_MINI_MESHOPT_H

#include "gltf3d_mini_common.h"

#define GLTF3D_MESHOPT_MODE_ATTRIBUTES 1
#define GLTF3D_MESHOPT_MODE_TRIANGLES 2
#define GLTF3D_MESHOPT_MODE_INDICES 3
#define GLTF3D_MESHOPT_MODE_WEIGHTS 4

#define GLTF3D_MESHOPT_FILTER_NONE 0
#define GLTF3D_MESHOPT_FILTER_OCTAHEDRAL 1
#define GLTF3D_MESHOPT_FILTER_QUATERNION 2
#define GLTF3D_MESHOPT_FILTER_EXPONENTIAL 3

typedef struct gltf3d_mini_meshopt_probe_s {
    unsigned long byte_count;
    unsigned char tag0;
    unsigned char tag1;
    int maybe_meshopt;
    int version_guess;
} gltf3d_mini_meshopt_probe;

typedef struct gltf3d_mini_meshopt_job_s {
    const unsigned char *src;
    unsigned long src_size;
    unsigned char *dst;
    unsigned long dst_size;
    unsigned long count;
    unsigned long stride;
    int mode;
    int filter;
} gltf3d_mini_meshopt_job;

typedef struct gltf3d_mini_meshopt_callbacks_s {
    int (*decode)(void *user, const gltf3d_mini_meshopt_job *job);
    void *user;
} gltf3d_mini_meshopt_callbacks;

int gltf3d_mini_meshopt_probe_stream(const unsigned char *src, unsigned long src_size, gltf3d_mini_meshopt_probe *probe);
int gltf3d_mini_meshopt_validate_job(const gltf3d_mini_meshopt_job *job);
int gltf3d_mini_meshopt_decode(const gltf3d_mini_meshopt_job *job, const gltf3d_mini_meshopt_callbacks *cb);
int gltf3d_mini_meshopt_decode_raw_passthrough(const gltf3d_mini_meshopt_job *job);
int gltf3d_mini_meshopt_job_from_bufferview(const gltf3d_asset *asset, int buffer_view_index, unsigned char *dst, unsigned long dst_size, gltf3d_mini_meshopt_job *job);
int gltf3d_mini_meshopt_decode_best_effort(const gltf3d_mini_meshopt_job *job, const gltf3d_mini_meshopt_callbacks *cb);
const char *gltf3d_mini_meshopt_mode_name(int mode);
const char *gltf3d_mini_meshopt_filter_name(int filter);

#endif
