#ifndef CSM3D_UTIL_H
#define CSM3D_UTIL_H

#include "CSM_chiquisumother3d.h"

int csm3d_is_space(char c);
int csm3d_is_digit(char c);
int csm3d_is_alpha(char c);
char csm3d_lower(char c);
int csm3d_streq(const char *a, const char *b);
int csm3d_streq_ci(const char *a, const char *b);
csm3d_u32 csm3d_strlen(const char *s);
void csm3d_copy(char *dst, csm3d_u32 cap, const char *src);
void csm3d_append_word(char *dst, csm3d_u32 cap, const char *word);
int csm3d_int_parse(const char *token, int *out_value);
void csm3d_set_diag(CSM3D_Diagnostic *diag, int code, int line, int frame_index, int marker_index, const char *text);

#endif
