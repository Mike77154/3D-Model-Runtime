#ifndef NFF89_H
#define NFF89_H

/*
   nff89 - tiny C89, fixed-point, no-heap parser for:
   - Eric Haines / SPD NFF scene files
   - ENFF extensions: light intensity, ambient Ka, Bezier patches, NURBS
   - Sense8 / WorldToolKit Neutral ASCII NFF geometry files

   Design limits:
   - no malloc/realloc/free; caller owns all arrays
   - no float/double; all scalar numbers are signed Q16.16 fixed point
   - C89-compatible public API
*/

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef NFF89_NAME_MAX
#define NFF89_NAME_MAX 64
#endif

#ifndef NFF89_TOKEN_MAX
#define NFF89_TOKEN_MAX 128
#endif

#ifndef NFF89_MAX_POLY_VERTS
#define NFF89_MAX_POLY_VERTS 32
#endif

#ifndef NFF89_MAX_BEZIER_CP
#define NFF89_MAX_BEZIER_CP 64
#endif

#ifndef NFF89_MAX_NURBS_KNOTS
#define NFF89_MAX_NURBS_KNOTS 96
#endif

#ifndef NFF89_MAX_NURBS_CP
#define NFF89_MAX_NURBS_CP 96
#endif

#ifndef NFF89_MAX_TRIMS
#define NFF89_MAX_TRIMS 8
#endif

#ifndef NFF89_MAX_TRIM_KNOTS
#define NFF89_MAX_TRIM_KNOTS 64
#endif

#ifndef NFF89_MAX_TRIM_CP
#define NFF89_MAX_TRIM_CP 64
#endif

#define NFF89_FIX_SHIFT 16
#define NFF89_FIX_ONE   65536L

#define NFF89_OK                  0
#define NFF89_ERR_BAD_ARG        -1
#define NFF89_ERR_BAD_TOKEN      -2
#define NFF89_ERR_EOF            -3
#define NFF89_ERR_FORMAT         -4
#define NFF89_ERR_CAPACITY       -5
#define NFF89_ERR_RANGE          -6

#define NFF89_FORMAT_UNKNOWN      0
#define NFF89_FORMAT_HAINES       1
#define NFF89_FORMAT_SENSE8       2
#define NFF89_FORMAT_ENFF         3

#define NFF89_POLY_PATCH          1U
#define NFF89_POLY_DOUBLE_SIDED   2U
#define NFF89_POLY_HAS_TEXTURE    4U
#define NFF89_POLY_HAS_ID         8U
#define NFF89_POLY_HAS_PORTAL    16U
#define NFF89_POLY_AUTO_NORMAL   32U

#define NFF89_LIGHT_HAS_COLOR     1U
#define NFF89_LIGHT_HAS_INTENSITY 2U

#define NFF89_TEX_PLAIN           0
#define NFF89_TEX_SHADED          1
#define NFF89_TEX_TRANSPARENT     2

#define NFF89_NURBS_RATIONAL      1U

typedef long nff89_fix;

typedef struct nff89_vec3_s {
    nff89_fix x;
    nff89_fix y;
    nff89_fix z;
} nff89_vec3;

typedef struct nff89_vec4_s {
    nff89_fix x;
    nff89_fix y;
    nff89_fix z;
    nff89_fix w;
} nff89_vec4;

typedef struct nff89_color_s {
    nff89_fix r;
    nff89_fix g;
    nff89_fix b;
} nff89_color;

typedef struct nff89_view_s {
    nff89_vec3 from;
    nff89_vec3 at;
    nff89_vec3 up;
    nff89_fix angle;
    nff89_fix hither;
    long xres;
    long yres;
} nff89_view;

typedef struct nff89_material_s {
    nff89_color color;
    nff89_fix kd;
    nff89_fix ks;
    nff89_fix shine;
    nff89_fix transmittance;
    nff89_fix ior;
    nff89_fix ka;
    unsigned has_ka;
} nff89_material;

typedef struct nff89_light_s {
    nff89_vec3 pos;
    nff89_fix intensity;
    nff89_color color;
    unsigned flags;
    long line;
} nff89_light;

typedef struct nff89_sphere_s {
    nff89_vec3 center;
    nff89_fix radius;
    nff89_material mat;
    long line;
} nff89_sphere;

typedef struct nff89_cone_s {
    nff89_vec3 base;
    nff89_fix base_radius;
    nff89_vec3 apex;
    nff89_fix apex_radius;
    nff89_material mat;
    long line;
} nff89_cone;

typedef struct nff89_poly_s {
    unsigned vertex_count;
    nff89_vec3 vertices[NFF89_MAX_POLY_VERTS];
    nff89_vec3 normals[NFF89_MAX_POLY_VERTS];
    unsigned flags;
    nff89_material mat;
    unsigned color12;
    unsigned color24;
    char texture[NFF89_NAME_MAX];
    int texture_type;
    nff89_fix tex_rot;
    nff89_fix tex_scale;
    nff89_fix tex_trans_u;
    nff89_fix tex_trans_v;
    unsigned tex_mirror;
    long id;
    char portal[NFF89_NAME_MAX];
    long object_index;
    long line;
} nff89_poly;

typedef struct nff89_bezier_s {
    int u_degree;
    int v_degree;
    unsigned cp_count;
    nff89_vec3 cp[NFF89_MAX_BEZIER_CP];
    nff89_material mat;
    long line;
} nff89_bezier;

typedef struct nff89_trim_s {
    int order;
    unsigned knot_count;
    unsigned cp_count;
    nff89_fix knots[NFF89_MAX_TRIM_KNOTS];
    nff89_vec4 cp[NFF89_MAX_TRIM_CP];
} nff89_trim;

typedef struct nff89_nurbs_s {
    unsigned flags;
    int u_degree;
    int v_degree;
    unsigned u_knot_count;
    unsigned v_knot_count;
    unsigned u_point_count;
    unsigned v_point_count;
    nff89_fix u_knots[NFF89_MAX_NURBS_KNOTS];
    nff89_fix v_knots[NFF89_MAX_NURBS_KNOTS];
    unsigned cp_count;
    nff89_vec4 cp[NFF89_MAX_NURBS_CP];
    unsigned trim_count;
    nff89_trim trims[NFF89_MAX_TRIMS];
    nff89_material mat;
    long line;
} nff89_nurbs;

typedef struct nff89_object_s {
    char name[NFF89_NAME_MAX];
    unsigned shading_on;
    unsigned first_poly;
    unsigned poly_count;
    long line;
} nff89_object;

typedef struct nff89_diag_s {
    long line;
    char last_token[NFF89_TOKEN_MAX];
    int last_error;
    unsigned warnings;
    unsigned overflow_count;
    unsigned skipped_unknown;
    unsigned truncated_polygons;
    unsigned truncated_curves;
} nff89_diag;

typedef struct nff89_scene_s {
    int format;
    unsigned has_view;
    unsigned has_background;
    nff89_view view;
    nff89_color background;
    nff89_material current_material;

    nff89_light *lights;
    unsigned lights_count;
    unsigned lights_max;

    nff89_sphere *spheres;
    unsigned spheres_count;
    unsigned spheres_max;

    nff89_cone *cones;
    unsigned cones_count;
    unsigned cones_max;

    nff89_poly *polys;
    unsigned polys_count;
    unsigned polys_max;

    nff89_bezier *beziers;
    unsigned beziers_count;
    unsigned beziers_max;

    nff89_nurbs *nurbs;
    unsigned nurbs_count;
    unsigned nurbs_max;

    nff89_object *objects;
    unsigned objects_count;
    unsigned objects_max;

    nff89_diag diag;
} nff89_scene;

typedef struct nff89_vertex_s {
    nff89_vec3 pos;
    nff89_vec3 norm;
    unsigned has_norm;
    unsigned auto_norm;
} nff89_vertex;

typedef int (*nff89_triangle_cb)(void *user,
                                const nff89_poly *poly,
                                const nff89_vec3 *a,
                                const nff89_vec3 *b,
                                const nff89_vec3 *c);

void nff89_init_scene(nff89_scene *scene);
void nff89_set_buffers(nff89_scene *scene,
                       nff89_light *lights, unsigned lights_max,
                       nff89_sphere *spheres, unsigned spheres_max,
                       nff89_cone *cones, unsigned cones_max,
                       nff89_poly *polys, unsigned polys_max,
                       nff89_bezier *beziers, unsigned beziers_max,
                       nff89_nurbs *nurbs, unsigned nurbs_max,
                       nff89_object *objects, unsigned objects_max);
void nff89_reset_counts(nff89_scene *scene);

int nff89_parse(nff89_scene *scene, const char *text, long length);
int nff89_parse_haines(nff89_scene *scene, const char *text, long length);
int nff89_parse_sense8(nff89_scene *scene, const char *text, long length,
                       nff89_vertex *scratch_vertices,
                       unsigned scratch_vertices_max);

int nff89_parse_fix_token(const char *token, nff89_fix *out_value);
int nff89_parse_int_token(const char *token, long *out_value);
long nff89_fix_to_int(nff89_fix value);
nff89_fix nff89_int_to_fix(long value);
nff89_fix nff89_fix_mul(nff89_fix a, nff89_fix b);
nff89_fix nff89_fix_div(nff89_fix a, nff89_fix b);
void nff89_fix_to_string(nff89_fix value, char *dst, unsigned dst_size, unsigned decimals);

unsigned nff89_tri_count(const nff89_scene *scene);
int nff89_emit_triangles(const nff89_scene *scene, nff89_triangle_cb cb, void *user);

const char *nff89_format_name(int format);
const char *nff89_error_name(int code);

#ifdef __cplusplus
}
#endif

#endif
