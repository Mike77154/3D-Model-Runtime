#ifndef USD3D_SKINNING_BAKE_H
#define USD3D_SKINNING_BAKE_H

#include "usd3d.h"
#include "usd3d_skinning_runtime.h"

#ifndef USD3D_SKIN_BAKE_MAX_VERTICES
#define USD3D_SKIN_BAKE_MAX_VERTICES 8192
#endif

#ifndef USD3D_SKIN_BAKE_MAX_JOINTS
#define USD3D_SKIN_BAKE_MAX_JOINTS 256
#endif

typedef struct usd3d_skin_bake_joint_s {
    char name[USD3D_PATH_MAX];
    long parent;
    usd3d_fixed local[16];
    usd3d_fixed world[16];
    usd3d_fixed inverse_bind[16];
    usd3d_fixed skin_matrix[16];
    int active;
} usd3d_skin_bake_joint;

typedef struct usd3d_skin_bake_mesh_s {
    usd3d_skinning_vertex_io vertices[USD3D_SKIN_BAKE_MAX_VERTICES];
    usd3d_skinning_result results[USD3D_SKIN_BAKE_MAX_VERTICES];
    unsigned long vertex_count;
} usd3d_skin_bake_mesh;

typedef struct usd3d_skin_bake_skeleton_s {
    usd3d_skin_bake_joint joints[USD3D_SKIN_BAKE_MAX_JOINTS];
    unsigned long joint_count;
    usd3d_fixed geom_bind_xform[16];
} usd3d_skin_bake_skeleton;

void usd3d_skin_bake_identity(usd3d_fixed *m16);
void usd3d_skin_bake_skeleton_init(usd3d_skin_bake_skeleton *skel);
void usd3d_skin_bake_mesh_init(usd3d_skin_bake_mesh *mesh);
int usd3d_skin_bake_add_joint(usd3d_skin_bake_skeleton *skel, const char *name, long parent, const usd3d_fixed *local, const usd3d_fixed *inverse_bind);
int usd3d_skin_bake_update_world(usd3d_skin_bake_skeleton *skel);
int usd3d_skin_bake_compute_skin_matrices(usd3d_skin_bake_skeleton *skel);
int usd3d_skin_bake_set_vertex(usd3d_skin_bake_mesh *mesh, unsigned long index, const usd3d_vec3 *point, const usd3d_vec3 *normal, const usd3d_vec3 *tangent);
int usd3d_skin_bake_set_influence(usd3d_skin_bake_mesh *mesh, unsigned long vertex, unsigned long slot, unsigned long joint, usd3d_fixed weight);
int usd3d_skin_bake_run(const usd3d_skin_bake_skeleton *skel, usd3d_skin_bake_mesh *mesh);
int usd3d_skin_bake_emit(const usd3d_skin_bake_mesh *mesh, int (*vertex_cb)(void *user, unsigned long index, const usd3d_skinning_result *r), void *user);

#endif
