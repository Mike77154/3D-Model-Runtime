#ifndef BC_IK_CHAIN_H
#define BC_IK_CHAIN_H
/* Phase 18: stand-alone, bounded 3D multi-link FABRIK in Q16.16.
   Does not replace Blender's IK solver or evaluate serialized chain IK. */
#include "bc_rig_q16.h"
#define BC_CHAIN_MAX_LINKS 8U
#define BC_CHAIN_MAX_ITERATIONS 96U
#define BC_CHAIN_TOLERANCE 512 /* 1/128 model unit */
typedef struct BC_IKChainGoal {
  unsigned int count;
  unsigned int iterations;
  int rest_joint[BC_CHAIN_MAX_LINKS+1U][3];
  BC_RigMat rest[BC_CHAIN_MAX_LINKS];
  int cosine_limit[BC_CHAIN_MAX_LINKS];
  int target[3];
} BC_IKChainGoal;
typedef struct BC_IKChainResult {
  unsigned int count;
  unsigned int iterations_used;
  int residual;
  int joint[BC_CHAIN_MAX_LINKS+1U][3];
  BC_RigMat pose[BC_CHAIN_MAX_LINKS];
  BC_RigMat deform[BC_CHAIN_MAX_LINKS];
} BC_IKChainResult;
/* Joint limit cosine: minimum dot between consecutive segment directions;
   -65536 disables that joint's cone. The limit is not a Blender axis/hinge
   constraint. Unsupported/unreachable/nonconvergent configurations fail. */
int bc_ik_chain_solve(const BC_IKChainGoal *goal,BC_IKChainResult *result);
/* CLI: fixed-size text input, rest matrices required; no dynamic allocation. */
int bc_ik_chain_cli(const char *goal_path,const char *palette_path,
                    const char *joints_path);
#endif
