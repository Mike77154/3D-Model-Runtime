#ifndef BC_IK2_H
#define BC_IK2_H
/* Independent analytic 2-link IK, Q16.16, 32-bit-only arithmetic, C89. */
#include "bc_rig_q16.h"
typedef struct BC_IK2Goal {
  int root[3],target[3],pole[3];
  int length[2];
} BC_IK2Goal;
typedef struct BC_IK2Result {
  int root[3],elbow[3],tip[3];
  BC_RigMat rest[2],pose[2],deform[2];
} BC_IK2Result;
/* Canonical rest pose: bone axes +Y, +Z as roll reference; child parent=bone0.
   The pole defines the elbow bending half-plane. Rejects unreachable targets,
   zero/colinear pole, degenerate lengths, and numeric overflows. */
int bc_ik2_solve(const BC_IK2Goal *goal,BC_IK2Result *dst);
/* Phase17: IK using serialized Blender rest matrices and head/tail data.
   Bone local +Y must align with its rest head-to-tail axis, but that axis can
   point in any armature-space direction and local roll is preserved by minimal
   swing rotation. Strictly rejects invalid rest frames, disconnected chains,
   non-rigid matrices and ambiguous 180-degree numerical inputs.
   Rest/pose are absolute armature-space matrices (column-vector convention). */
int bc_ik2_solve_rest(const BC_IK2Goal *g,const BC_RigMat rest[2],
                      const int *head,const int *tail,
                      BC_IK2Result *dst);
/* Compute a rest bone segment length from its serialized endpoints. */
int bc_ik2_segment_length(const int head[3],const int tail[3],int *len);
/* Standalone CLI interchange to existing --skin-tsv; does NOT read Blender IK. */
int bc_ik2_cli(const char *goal_path,const char *palette_path,
               const char *joints_path);
#endif
