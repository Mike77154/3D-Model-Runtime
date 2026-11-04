#ifndef FRAG_FIXED_QUAT_H
#define FRAG_FIXED_QUAT_H
/* R14: ISO C89 Q16.16 quaternion NLERP for row-vector rigid transforms.
 * No heap, no long long, no float/double, no trigonometric library.
 * Rejects non-rotational 3x3 parts (shear, scale, reflection).
 */
/* Recalculate a flat normal from final deformed triangle positions. */
int fxq_face_normal(const long a[3],const long b[3],const long c[3],long out[3]);
int fxq_blend_rigid(const long a[16],const long b[16],long t,long out[16]);
int fxq_normalize_vec3(const long v[3],long out[3]);
#endif
