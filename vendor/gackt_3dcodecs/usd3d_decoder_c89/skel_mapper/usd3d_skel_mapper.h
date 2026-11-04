#ifndef USD3D_SKEL_MAPPER_H
#define USD3D_SKEL_MAPPER_H

#include "usd3d.h"
#include "usd3d_skinning_runtime.h"

#ifndef USD3D_SKEL_MAPPER_MAX_JOINTS
#define USD3D_SKEL_MAPPER_MAX_JOINTS 256
#endif

#ifndef USD3D_SKEL_MAPPER_MAX_MESH_JOINTS
#define USD3D_SKEL_MAPPER_MAX_MESH_JOINTS 256
#endif

typedef struct usd3d_skel_joint_name_s {
    char name[USD3D_TOKEN_MAX];
    long parent;
    int active;
} usd3d_skel_joint_name;

typedef struct usd3d_skel_mapper_s {
    usd3d_skel_joint_name skeleton_joints[USD3D_SKEL_MAPPER_MAX_JOINTS];
    usd3d_skel_joint_name mesh_joints[USD3D_SKEL_MAPPER_MAX_MESH_JOINTS];
    long remap[USD3D_SKEL_MAPPER_MAX_MESH_JOINTS];
    unsigned long skeleton_count;
    unsigned long mesh_count;
    unsigned long missing_count;
} usd3d_skel_mapper;

void usd3d_skel_mapper_init(usd3d_skel_mapper *m);
int usd3d_skel_mapper_add_skeleton_joint(usd3d_skel_mapper *m, const char *name, long parent);
int usd3d_skel_mapper_add_mesh_joint(usd3d_skel_mapper *m, const char *name);
int usd3d_skel_mapper_build(usd3d_skel_mapper *m);
long usd3d_skel_mapper_find_skeleton_joint(const usd3d_skel_mapper *m, const char *name);
int usd3d_skel_mapper_remap_vertex(usd3d_skel_mapper *m, usd3d_skinning_vertex_io *v);
int usd3d_skel_mapper_normalize_and_remap_mesh(usd3d_skel_mapper *m, usd3d_skinning_vertex_io *vertices, unsigned long vertex_count);
int usd3d_skel_mapper_emit(const usd3d_skel_mapper *m, int (*joint_cb)(void *user, unsigned long mesh_joint, long skeleton_joint, const char *name), void *user);

#endif
