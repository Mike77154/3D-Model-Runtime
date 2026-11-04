#ifndef MF3D_FINAL_BEAM_MESH_H
#define MF3D_FINAL_BEAM_MESH_H

#include "../../src/mf3d.h"
#include "../mf3d_ext_beam_lattice.h"
#include "../eval_heavy/mf3d_eval_mesh.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mf3d_beam_mesh_options_s {
    mf3d_u32 objectid;       /* 0 = all beam objects */
    mf3d_fix radius_override;/* 0 = use beam r1/r2 average, fallback default_radius */
    mf3d_fix default_radius; /* used when radii are zero */
} mf3d_beam_mesh_options;

int mf3d_eval_beam_lattice_to_mesh(const mf3d_beam_lattice *lat, mf3d_eval_mesh *out_mesh, const mf3d_beam_mesh_options *opt);

#ifdef __cplusplus
}
#endif
#endif
