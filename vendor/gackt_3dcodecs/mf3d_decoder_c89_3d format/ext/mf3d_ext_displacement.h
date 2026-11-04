#ifndef MF3D_EXT_DISPLACEMENT_H
#define MF3D_EXT_DISPLACEMENT_H

#include "../src/mf3d.h"

#ifndef MF3D_EXT_MAX_DISP2D
#define MF3D_EXT_MAX_DISP2D 128
#endif
#ifndef MF3D_EXT_MAX_NORMGROUPS
#define MF3D_EXT_MAX_NORMGROUPS 128
#endif
#ifndef MF3D_EXT_MAX_NORMVECS
#define MF3D_EXT_MAX_NORMVECS 4096
#endif
#ifndef MF3D_EXT_MAX_DISPGROUPS
#define MF3D_EXT_MAX_DISPGROUPS 128
#endif
#ifndef MF3D_EXT_MAX_DISPCOORDS
#define MF3D_EXT_MAX_DISPCOORDS 8192
#endif
#ifndef MF3D_EXT_MAX_DISPMESHES
#define MF3D_EXT_MAX_DISPMESHES 128
#endif
#ifndef MF3D_EXT_MAX_DISPTRIS
#define MF3D_EXT_MAX_DISPTRIS 16384
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mf3d_disp2d_s {
    mf3d_u32 id;
    char path[160];
    char channel[8];
    char tilestyleu[24];
    char tilestylev[24];
    char filter[24];
} mf3d_disp2d;

typedef struct mf3d_normvec_s { mf3d_fix x, y, z; } mf3d_normvec;

typedef struct mf3d_normgroup_s {
    mf3d_u32 id;
    mf3d_u32 first;
    mf3d_u32 count;
} mf3d_normgroup;

typedef struct mf3d_dispcoord_s {
    mf3d_fix u, v;
    mf3d_u32 n;
    mf3d_fix f;
} mf3d_dispcoord;

typedef struct mf3d_dispgroup_s {
    mf3d_u32 id;
    mf3d_u32 dispid;
    mf3d_u32 nid;
    mf3d_fix height;
    mf3d_fix offset;
    mf3d_u32 first;
    mf3d_u32 count;
} mf3d_dispgroup;

typedef struct mf3d_disptri_s {
    mf3d_u32 owner_objectid;
    mf3d_u32 tri_index;
    mf3d_u32 did;
    mf3d_u32 d1, d2, d3;
    mf3d_u32 has_d;
} mf3d_disptri;

typedef struct mf3d_dispmesh_s {
    mf3d_u32 objectid;
    mf3d_u32 default_did;
    mf3d_u32 first_disptri;
    mf3d_u32 disptri_count;
} mf3d_dispmesh;

typedef struct mf3d_displacement_s {
    mf3d_disp2d disp2d[MF3D_EXT_MAX_DISP2D];
    mf3d_normgroup normgroups[MF3D_EXT_MAX_NORMGROUPS];
    mf3d_normvec normvecs[MF3D_EXT_MAX_NORMVECS];
    mf3d_dispgroup dispgroups[MF3D_EXT_MAX_DISPGROUPS];
    mf3d_dispcoord coords[MF3D_EXT_MAX_DISPCOORDS];
    mf3d_dispmesh meshes[MF3D_EXT_MAX_DISPMESHES];
    mf3d_disptri disptris[MF3D_EXT_MAX_DISPTRIS];
    mf3d_u32 disp2d_count, normgroup_count, normvec_count;
    mf3d_u32 dispgroup_count, coord_count, mesh_count, disptri_count;
} mf3d_displacement;

void mf3d_displacement_init(mf3d_displacement *d);
int mf3d_displacement_parse_xml(char *xml, mf3d_displacement *out_d);
const mf3d_disp2d *mf3d_displacement_find_disp2d(const mf3d_displacement *d, mf3d_u32 id);
const mf3d_dispgroup *mf3d_displacement_find_group(const mf3d_displacement *d, mf3d_u32 id);
int mf3d_displacement_validate_refs(const mf3d_displacement *d, mf3d_u32 *missing_refs);

#ifdef __cplusplus
}
#endif
#endif
