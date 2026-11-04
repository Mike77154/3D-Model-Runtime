#ifndef GS3D_REGISTRY_H
#define GS3D_REGISTRY_H
#include "gs3d.h"
typedef struct GS3D_FormatInfo {
    int signature;
    const char *name;
    unsigned int flags;
} GS3D_FormatInfo;
#define GS3D_FEATURE_VERTEX_FRAMES 1U
#define GS3D_FEATURE_SKINS 2U
#define GS3D_FEATURE_BONES 4U
#define GS3D_FEATURE_UV2 8U
unsigned int gs3d_format_count(void);
const GS3D_FormatInfo *gs3d_format_info(unsigned int index);
const GS3D_FormatInfo *gs3d_format_by_signature(int signature);
#endif
