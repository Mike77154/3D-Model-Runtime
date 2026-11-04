#ifndef BC_RIG_Q16_H
#define BC_RIG_Q16_H
/* Standalone Q16.16 skeletal kernel; C89, fixed storage, no heap/no floating point. */
#define BC_RIG_ONE 65536
#define BC_RIG_MAX_BONES 8192U
#define BC_RIG_MAX_WEIGHTS 8U
#define BC_RIG_OK 0
#define BC_RIG_BAD (-1)
#define BC_RIG_UNSUPPORTED (-2)
/* Row-major matrices, column-vector convention. */
typedef struct BC_RigMat { int m[16]; } BC_RigMat;
typedef struct BC_RigInfluence { unsigned int bone; int weight; } BC_RigInfluence;
/* One accepted, unparented Mesh Object with one Armature modifier.
   Mesh and armature indices match their individual ID-block enumerations. */
typedef struct BC_RigPair {
  unsigned int mesh_index,arm_index,arm_object_at,arm_object_index;
  BC_RigMat mesh_local,arm_local;
} BC_RigPair;
int bc_qadd_checked(int a,int b,int *dst);
int bc_qmul_checked(int a,int b,int *dst);
int bc_qdiv_checked(int a,int b,int *dst);
void bc_rig_mat_identity(BC_RigMat *dst);
int bc_rig_mat_mul(const BC_RigMat *a,const BC_RigMat *b,BC_RigMat *dst);
int bc_rig_mat_point(const BC_RigMat *mat,const int point[3],int dst[3]);
int bc_rig_euler_quat(const int euler[3],unsigned int order,int quat_wxyz[4]);
int bc_rig_mat_from_trs(const int loc[3],const int quat_wxyz[4],const int scale[3],BC_RigMat *dst);
int bc_rig_inverse_affine(const BC_RigMat *src,BC_RigMat *dst);
int bc_rig_inverse_rigid(const BC_RigMat *src,BC_RigMat *dst);
int bc_rig_deform_matrix(const BC_RigMat *pose,const BC_RigMat *rest,BC_RigMat *dst);
int bc_rig_world(const BC_RigMat *local,const int *parent,unsigned int count,BC_RigMat *world);
int bc_rig_skin_point(const BC_RigMat *deform,unsigned int nmat,const int source[3],
                      const BC_RigInfluence *weights,unsigned int count,int result[3]);
#endif
