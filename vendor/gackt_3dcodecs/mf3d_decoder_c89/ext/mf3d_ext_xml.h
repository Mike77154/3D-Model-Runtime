#ifndef MF3D_EXT_XML_H
#define MF3D_EXT_XML_H

#include "../src/mf3d.h"

#ifdef __cplusplus
extern "C" {
#endif

int mf3d_ext_tag_matches(const char *p, const char *local_name);
const char *mf3d_ext_next_tag(const char *p, const char *local_name);
const char *mf3d_ext_find_close(const char *p, const char *local_name);
void mf3d_ext_attr(const char *tag, const char *attr, char *out, int outcap);
void mf3d_ext_text_between(const char *begin, const char *end, char *out, int outcap);
int mf3d_ext_parse_hex_color(const char *s, mf3d_u8 *r, mf3d_u8 *g, mf3d_u8 *b, mf3d_u8 *a);
void mf3d_ext_identity(mf3d_fix *t12);
void mf3d_ext_parse_transform(const char *s, mf3d_fix *t12);
mf3d_fix mf3d_ext_mul_fix(mf3d_fix a, mf3d_fix b);
void mf3d_ext_apply_transform(const mf3d_fix *t12, const mf3d_vec3 *in_v, mf3d_vec3 *out_v);

#ifdef __cplusplus
}
#endif
#endif
