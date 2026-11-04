#ifndef NFF89_CAD_H
#define NFF89_CAD_H

/*
   nff89_cad - optional CAD/NURBS evaluator module for nff89.

   C89, no heap, no malloc/realloc/free, no float/double.
   Uses the nff89 Q16.16 fixed point scalar type.

   Implemented features:
   - clamped-domain NURBS surface point evaluation
   - rational and non-rational ENFF pn/pnn support
   - NURBS trim curve evaluation in UV space
   - Bezier patch evaluation by tensor de Casteljau
   - fixed-grid tessellation callback for NURBS and Bezier patches
*/

#include "nff89.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef NFF89_CAD_MAX_DEGREE
#define NFF89_CAD_MAX_DEGREE 12
#endif

#ifndef NFF89_CAD_MAX_GRID
#define NFF89_CAD_MAX_GRID 4096U
#endif

#ifndef NFF89_CAD_MAX_TRIM_POLY
#define NFF89_CAD_MAX_TRIM_POLY 1024U
#endif

#ifndef NFF89_CAD_MAX_TRIM_LOOPS
#define NFF89_CAD_MAX_TRIM_LOOPS NFF89_MAX_TRIMS
#endif

#define NFF89_CAD_OK               0
#define NFF89_CAD_ERR_BAD_ARG     -101
#define NFF89_CAD_ERR_DEGREE      -102
#define NFF89_CAD_ERR_KNOTS       -103
#define NFF89_CAD_ERR_CONTROL     -104
#define NFF89_CAD_ERR_RANGE       -105
#define NFF89_CAD_ERR_WEIGHT      -106
#define NFF89_CAD_ERR_CAPACITY    -107
#define NFF89_CAD_ERR_CALLBACK    -108
#define NFF89_CAD_ERR_TRIM        -109

typedef struct nff89_cad_uv_s {
    nff89_fix u;
    nff89_fix v;
} nff89_cad_uv;

typedef struct nff89_cad_vertex_s {
    nff89_vec3 pos;
    nff89_vec3 normal;
    nff89_cad_uv uv;
    unsigned has_normal;
} nff89_cad_vertex;

typedef struct nff89_cad_trim_poly_s {
    unsigned first;
    unsigned count;
    nff89_fix u_min;
    nff89_fix u_max;
    nff89_fix v_min;
    nff89_fix v_max;
    nff89_fix signed_area;
    unsigned closed;
} nff89_cad_trim_poly;

typedef int (*nff89_cad_tri_cb)(void *user,
                                const nff89_cad_vertex *a,
                                const nff89_cad_vertex *b,
                                const nff89_cad_vertex *c,
                                const nff89_nurbs *surface);

typedef int (*nff89_cad_bezier_tri_cb)(void *user,
                                       const nff89_cad_vertex *a,
                                       const nff89_cad_vertex *b,
                                       const nff89_cad_vertex *c,
                                       const nff89_bezier *patch);

int nff89_cad_validate_nurbs(const nff89_nurbs *surface);
int nff89_cad_nurbs_domain(const nff89_nurbs *surface,
                           nff89_fix *u_min, nff89_fix *u_max,
                           nff89_fix *v_min, nff89_fix *v_max);
int nff89_cad_eval_nurbs_surface(const nff89_nurbs *surface,
                                 nff89_fix u, nff89_fix v,
                                 nff89_vec3 *out_pos);
int nff89_cad_eval_nurbs_surface_vertex(const nff89_nurbs *surface,
                                        nff89_fix u, nff89_fix v,
                                        nff89_cad_vertex *out_vertex);
int nff89_cad_eval_trim_curve(const nff89_trim *trim,
                              nff89_fix u,
                              nff89_cad_uv *out_uv);
int nff89_cad_flatten_trim_curve(const nff89_trim *trim,
                                 unsigned samples,
                                 nff89_cad_uv *scratch_points,
                                 unsigned scratch_points_max,
                                 unsigned *out_count,
                                 nff89_cad_trim_poly *out_loop);
int nff89_cad_build_trim_polylines(const nff89_nurbs *surface,
                                   unsigned samples_per_trim,
                                   nff89_cad_uv *scratch_points,
                                   unsigned scratch_points_max,
                                   unsigned *out_point_count,
                                   nff89_cad_trim_poly *scratch_loops,
                                   unsigned scratch_loops_max,
                                   unsigned *out_loop_count);
int nff89_cad_point_in_trim_polylines(nff89_fix u,
                                      nff89_fix v,
                                      const nff89_cad_uv *points,
                                      const nff89_cad_trim_poly *loops,
                                      unsigned loop_count,
                                      unsigned *out_inside);
int nff89_cad_eval_bezier_patch(const nff89_bezier *patch,
                                nff89_fix u, nff89_fix v,
                                nff89_vec3 *out_pos);
int nff89_cad_eval_bezier_vertex(const nff89_bezier *patch,
                                 nff89_fix u, nff89_fix v,
                                 nff89_cad_vertex *out_vertex);
int nff89_cad_tessellate_nurbs(const nff89_nurbs *surface,
                               unsigned u_steps,
                               unsigned v_steps,
                               nff89_cad_vertex *scratch_grid,
                               unsigned scratch_grid_max,
                               nff89_cad_tri_cb cb,
                               void *user);
int nff89_cad_tessellate_nurbs_trimmed(const nff89_nurbs *surface,
                                       unsigned u_steps,
                                       unsigned v_steps,
                                       unsigned cell_subdiv,
                                       unsigned trim_samples_per_curve,
                                       nff89_cad_vertex *scratch_grid,
                                       unsigned scratch_grid_max,
                                       nff89_cad_uv *scratch_trim_points,
                                       unsigned scratch_trim_points_max,
                                       nff89_cad_trim_poly *scratch_trim_loops,
                                       unsigned scratch_trim_loops_max,
                                       nff89_cad_tri_cb cb,
                                       void *user);
int nff89_cad_tessellate_bezier(const nff89_bezier *patch,
                                unsigned u_steps,
                                unsigned v_steps,
                                nff89_cad_vertex *scratch_grid,
                                unsigned scratch_grid_max,
                                nff89_cad_bezier_tri_cb cb,
                                void *user);
int nff89_cad_tessellate_scene_nurbs(const nff89_scene *scene,
                                     unsigned u_steps,
                                     unsigned v_steps,
                                     nff89_cad_vertex *scratch_grid,
                                     unsigned scratch_grid_max,
                                     nff89_cad_tri_cb cb,
                                     void *user);
int nff89_cad_tessellate_scene_nurbs_trimmed(const nff89_scene *scene,
                                             unsigned u_steps,
                                             unsigned v_steps,
                                             unsigned cell_subdiv,
                                             unsigned trim_samples_per_curve,
                                             nff89_cad_vertex *scratch_grid,
                                             unsigned scratch_grid_max,
                                             nff89_cad_uv *scratch_trim_points,
                                             unsigned scratch_trim_points_max,
                                             nff89_cad_trim_poly *scratch_trim_loops,
                                             unsigned scratch_trim_loops_max,
                                             nff89_cad_tri_cb cb,
                                             void *user);

const char *nff89_cad_error_name(int code);

#ifdef __cplusplus
}
#endif

#endif
