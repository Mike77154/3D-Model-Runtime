#ifndef FRAG_FIXED_MATRIX_H
#define FRAG_FIXED_MATRIX_H
/* Signed 16.16 affine transforms, ISO C89, no heap/no float/no long long.
 * Saturation uses +/-2147483647 regardless of host long width.
 * DirectX .x matrices use row-major, row-vector convention. */
#define FX_ONE 65536L
long fx_add(long a,long b);
long fx_sub(long a,long b);
long fx_mul(long a,long b);
long fx_lerp(long a,long b,long t);
void fx_identity(long out[16]);
void fx_matrix_mul(const long a[16],const long b[16],long out[16]);
void fx_transform_point(const long m[16],const long v[3],long out[3]);
int fx_affine(const long m[16]);
/* R15: affine rigid inverse. Rejects scale, shear, reflections and singularity. */
int fx_inverse_rigid(const long m[16],long out[16]);
int fx_div_q16(long numerator,long denominator,long *out);
int fx_inverse_affine(const long matrix[16],long out[16]);
#endif
