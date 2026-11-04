#ifndef IFC3D_INTERNAL_H
#define IFC3D_INTERNAL_H

#include "ifc3d.h"

int ifc3d_is_space(int c);
int ifc3d_is_alpha(int c);
int ifc3d_is_digit(int c);
int ifc3d_is_name_char(int c);
int ifc3d_toupper_ascii(int c);
void ifc3d_copy_upper(char *dst, int dst_len, const char *src, int src_len);
int ifc3d_strieq(const char *a, const char *b);
void ifc3d_set_diag(ifc3d_diag *diag, int code, long offset, const char *msg);

const char *ifc3d_skip_ws(const char *p, const char *end);
const char *ifc3d_skip_ws_comments(const char *p, const char *end);
int ifc3d_match_kw(const char *p, const char *end, const char *kw);
const char *ifc3d_find_kw(const char *p, const char *end, const char *kw);

ifc3d_status ifc3d_arg_get(const char *args, int len, int index, const char **out, int *out_len);
int ifc3d_arg_is_omitted(const char *arg, int len);
ifc3d_status ifc3d_arg_parse_ref(const char *arg, int len, long *out_ref);
ifc3d_status ifc3d_arg_parse_ref_list(const char *arg, int len, long *refs, int max_refs, int *out_count);
ifc3d_status ifc3d_arg_parse_fixed_list3(const char *arg, int len, ifc3d_vec3 *out);
ifc3d_status ifc3d_parse_nested_point_list3(ifc3d_model *m, ifc3d_entity *e, const char *arg, int len, ifc3d_diag *diag);
ifc3d_status ifc3d_parse_nested_tri_indices(ifc3d_model *m, ifc3d_entity *e, const char *arg, int len, ifc3d_diag *diag);
ifc3d_status ifc3d_arg_parse_string_token(const char *arg, int len, char *dst, int dst_len);
int ifc3d_arg_parse_bool(const char *arg, int len, int default_value);

ifc3d_status ifc3d_step_parse(const char *text, long len, ifc3d_model *m, ifc3d_diag *diag);
int ifc3d_type_from_name(const char *name);

ifc3d_status ifc3d_mesh_add_triangle(ifc3d_mesh *mesh, ifc3d_vec3 a, ifc3d_vec3 b, ifc3d_vec3 c, ifc3d_diag *diag);
ifc3d_status ifc3d_mesh_add_quad(ifc3d_mesh *mesh, ifc3d_vec3 a, ifc3d_vec3 b, ifc3d_vec3 c, ifc3d_vec3 d, ifc3d_diag *diag);

#endif
