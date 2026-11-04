#ifndef MF3D_EXT_SLICE_H
#define MF3D_EXT_SLICE_H

#include "../src/mf3d.h"

#ifndef MF3D_EXT_MAX_SLICE_STACKS
#define MF3D_EXT_MAX_SLICE_STACKS 64
#endif
#ifndef MF3D_EXT_MAX_SLICES
#define MF3D_EXT_MAX_SLICES 2048
#endif
#ifndef MF3D_EXT_MAX_SLICE_VERTS
#define MF3D_EXT_MAX_SLICE_VERTS 8192
#endif
#ifndef MF3D_EXT_MAX_SLICE_SEGS
#define MF3D_EXT_MAX_SLICE_SEGS 8192
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mf3d_slice_vertex_s { mf3d_fix x, y; } mf3d_slice_vertex;
typedef struct mf3d_slice_segment_s { mf3d_u32 v1, v2; } mf3d_slice_segment;

typedef struct mf3d_slice_s {
    mf3d_fix ztop;
    mf3d_u32 first_vertex, vertex_count;
    mf3d_u32 first_segment, segment_count;
} mf3d_slice;

typedef struct mf3d_slice_stack_s {
    mf3d_u32 id;
    mf3d_u32 objectid;
    mf3d_fix zbottom;
    mf3d_u32 first_slice, slice_count;
} mf3d_slice_stack;

typedef struct mf3d_slices_s {
    mf3d_slice_stack stacks[MF3D_EXT_MAX_SLICE_STACKS];
    mf3d_slice slices[MF3D_EXT_MAX_SLICES];
    mf3d_slice_vertex vertices[MF3D_EXT_MAX_SLICE_VERTS];
    mf3d_slice_segment segments[MF3D_EXT_MAX_SLICE_SEGS];
    mf3d_u32 stack_count, slice_count, vertex_count, segment_count;
} mf3d_slices;

void mf3d_slices_init(mf3d_slices *s);
int mf3d_slices_parse_xml(char *xml, mf3d_slices *out_slices);

#ifdef __cplusplus
}
#endif
#endif
