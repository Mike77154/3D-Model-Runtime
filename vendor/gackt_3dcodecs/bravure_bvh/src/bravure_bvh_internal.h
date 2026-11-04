#ifndef BRAVURE_BVH_INTERNAL_H
#define BRAVURE_BVH_INTERNAL_H

#include "bravure_bvh.h"

#define BBVH_FALSE 0
#define BBVH_TRUE 1

void bbvh_mem_zero(void *ptr, long n);
void bbvh_str_copy(char *dst, int cap, const char *src);
int bbvh_str_eq(const char *a, const char *b);
int bbvh_str_ieq(const char *a, const char *b);
int bbvh_is_digit(char c);
int bbvh_is_space(char c);
long bbvh_str_to_long(const char *text, int *ok);
void bbvh_doc_error(bbvh_document *doc, int line, int col, const char *token);

void bbvh_mat_identity(bbvh_mat4 *m);
void bbvh_mat_mul(bbvh_mat4 *out_m, const bbvh_mat4 *a, const bbvh_mat4 *b);
void bbvh_mat_translation(bbvh_mat4 *m, bbvh_fixed x, bbvh_fixed y, bbvh_fixed z);
void bbvh_mat_rotation_x(bbvh_mat4 *m, bbvh_fixed degrees_q16);
void bbvh_mat_rotation_y(bbvh_mat4 *m, bbvh_fixed degrees_q16);
void bbvh_mat_rotation_z(bbvh_mat4 *m, bbvh_fixed degrees_q16);
void bbvh_mat_apply_point(const bbvh_mat4 *m, bbvh_fixed x, bbvh_fixed y,
                          bbvh_fixed z, bbvh_fixed out_xyz[3]);

#endif
