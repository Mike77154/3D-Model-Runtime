#ifndef GIF_MS3D_QUAT_H
#define GIF_MS3D_QUAT_H
/* Integer quaternion interpolation: C89, Q16.16, 32-bit arithmetic only.
 * NLERP follows the quaternion shortest hemisphere, but is NOT SLERP.
 * Input Euler order matches MS3D: extrinsic X then Y then Z (Rz*Ry*Rx).
 */
#include "gif_ms3d_skin.h"
typedef struct ms3d_quat_s { ms3d_fx x,y,z,w; } ms3d_quat;
void ms3d_quat_from_euler(const ms3d_fx euler[3],ms3d_quat *out);
void ms3d_quat_nlerp(const ms3d_quat *a,const ms3d_quat *b,
                     ms3d_fx alpha,ms3d_quat *out);
/* Spherical linear interpolation, Q16.16 CORDIC trig + acos bisection.
 * No runtime float/double/64-bit; near-parallel inputs use NLERP.
 */
void ms3d_quat_slerp(const ms3d_quat *a,const ms3d_quat *b,
                     ms3d_fx alpha,ms3d_quat *out);
void ms3d_quat_to_affine(const ms3d_quat *q,ms3d_affine *out);
#endif
