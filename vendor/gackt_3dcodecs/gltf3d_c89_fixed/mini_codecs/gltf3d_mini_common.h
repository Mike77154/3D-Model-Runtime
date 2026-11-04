#ifndef GLTF3D_MINI_COMMON_H
#define GLTF3D_MINI_COMMON_H

#include "../include/gltf3d.h"

#define GLTF3D_MINI_OK 0
#define GLTF3D_MINI_ERR_BAD_ARG -101
#define GLTF3D_MINI_ERR_BAD_DATA -102
#define GLTF3D_MINI_ERR_UNSUPPORTED -103
#define GLTF3D_MINI_ERR_RANGE -104
#define GLTF3D_MINI_ERR_NEED_EXTERNAL -105

#define GLTF3D_MINI_RGBA8 1
#define GLTF3D_MINI_RGB8 2
#define GLTF3D_MINI_GRAY8 3

const char *gltf3d_mini_error_string(int code);

#endif
