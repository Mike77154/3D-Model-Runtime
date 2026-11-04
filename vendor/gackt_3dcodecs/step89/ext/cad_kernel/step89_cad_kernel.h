#ifndef STEP89_CAD_KERNEL_H
#define STEP89_CAD_KERNEL_H

/*
   step89 CAD kernel base layer.

   C89, no heap, no float/double. This module converts the extracted STEP89
   geometry tables into a small indexed topological model that later modules can
   use for sewing, healing, tessellation, booleans, offsets and PMI anchoring.

   It is intentionally conservative: it does not pretend to be a full CAD kernel
   yet. It creates stable handles, checks references, computes integer/fixed
   bounding boxes, classifies analytic geometry, and exposes loop/edge topology.
*/

#include "step89.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef STEP89_CAD_MAX_VERTICES
#define STEP89_CAD_MAX_VERTICES STEP89_MAX_VERTICES
#endif

#ifndef STEP89_CAD_MAX_EDGES
#define STEP89_CAD_MAX_EDGES STEP89_MAX_EDGE_CURVES
#endif

#ifndef STEP89_CAD_MAX_ORIENTED_USES
#define STEP89_CAD_MAX_ORIENTED_USES STEP89_MAX_ORIENTED_EDGES
#endif

#ifndef STEP89_CAD_MAX_LOOPS
#define STEP89_CAD_MAX_LOOPS STEP89_MAX_EDGE_LOOPS
#endif

#ifndef STEP89_CAD_MAX_FACES
#define STEP89_CAD_MAX_FACES STEP89_MAX_FACES
#endif

#ifndef STEP89_CAD_MAX_SHELLS
#define STEP89_CAD_MAX_SHELLS 256
#endif

#define STEP89_CAD_FLAG_MISSING_GEOM       0x0001u
#define STEP89_CAD_FLAG_OPEN_LOOP          0x0002u
#define STEP89_CAD_FLAG_NONMANIFOLD        0x0004u
#define STEP89_CAD_FLAG_ZERO_EDGE          0x0008u
#define STEP89_CAD_FLAG_UNSUPPORTED_SURF   0x0010u
#define STEP89_CAD_FLAG_REVERSED_USE       0x0020u

#define STEP89_CAD_ORIENT_UNKNOWN -1
#define STEP89_CAD_ORIENT_FALSE    0
#define STEP89_CAD_ORIENT_TRUE     1

typedef enum step89_cad_entity_kind_e {
    STEP89_CAD_KIND_UNKNOWN = 0,
    STEP89_CAD_KIND_VERTEX = 1,
    STEP89_CAD_KIND_EDGE = 2,
    STEP89_CAD_KIND_LOOP = 3,
    STEP89_CAD_KIND_FACE = 4,
    STEP89_CAD_KIND_SHELL = 5,
    STEP89_CAD_KIND_SOLID = 6
} step89_cad_entity_kind;

typedef enum step89_cad_surface_class_e {
    STEP89_CAD_SURF_UNKNOWN = 0,
    STEP89_CAD_SURF_PLANE = 1,
    STEP89_CAD_SURF_CYLINDER = 2,
    STEP89_CAD_SURF_CONE = 3,
    STEP89_CAD_SURF_SPHERE = 4,
    STEP89_CAD_SURF_TORUS = 5,
    STEP89_CAD_SURF_NURBS = 6
} step89_cad_surface_class;

typedef struct step89_cad_bbox_s {
    int valid;
    step89_fixed min_x;
    step89_fixed min_y;
    step89_fixed min_z;
    step89_fixed max_x;
    step89_fixed max_y;
    step89_fixed max_z;
} step89_cad_bbox;

typedef struct step89_cad_vertex_s {
    int ref;
    int point_ref;
    step89_fixed x;
    step89_fixed y;
    step89_fixed z;
    unsigned flags;
} step89_cad_vertex;

typedef struct step89_cad_edge_s {
    int ref;
    int start_vertex_ref;
    int end_vertex_ref;
    int curve_ref;
    int same_sense;
    int use_count;
    unsigned flags;
    step89_cad_bbox bbox;
} step89_cad_edge;

typedef struct step89_cad_oriented_use_s {
    int ref;
    int edge_ref;
    int orientation;
    int start_vertex_ref;
    int end_vertex_ref;
    unsigned flags;
} step89_cad_oriented_use;

typedef struct step89_cad_loop_s {
    int ref;
    int first_use_index;
    int use_count;
    int closed;
    unsigned flags;
    step89_cad_bbox bbox;
} step89_cad_loop;

typedef struct step89_cad_face_s {
    int ref;
    int surface_ref;
    int same_sense;
    int first_loop_ref;
    int loop_count;
    step89_cad_surface_class surface_class;
    unsigned flags;
    step89_cad_bbox bbox;
} step89_cad_face;

typedef struct step89_cad_shell_s {
    int ref;
    int first_face_index;
    int face_count;
    int closed;
    unsigned flags;
    step89_cad_bbox bbox;
} step89_cad_shell;

typedef struct step89_cad_model_s {
    int vertex_count;
    step89_cad_vertex vertices[STEP89_CAD_MAX_VERTICES];

    int edge_count;
    step89_cad_edge edges[STEP89_CAD_MAX_EDGES];

    int oriented_use_count;
    step89_cad_oriented_use oriented_uses[STEP89_CAD_MAX_ORIENTED_USES];

    int loop_count;
    step89_cad_loop loops[STEP89_CAD_MAX_LOOPS];

    int face_count;
    step89_cad_face faces[STEP89_CAD_MAX_FACES];

    int shell_count;
    step89_cad_shell shells[STEP89_CAD_MAX_SHELLS];

    step89_cad_bbox bbox;

    int missing_ref_count;
    int open_loop_count;
    int nonmanifold_edge_count;
    int zero_edge_count;
    int unsupported_surface_count;
    int warning_count;
} step89_cad_model;

void step89_cad_init(step89_cad_model *cad);
step89_result step89_cad_build(step89_cad_model *cad, const step89_model *src);
step89_result step89_cad_validate_topology(step89_cad_model *cad);

const step89_cad_vertex *step89_cad_find_vertex(const step89_cad_model *cad, int ref);
const step89_cad_edge *step89_cad_find_edge(const step89_cad_model *cad, int ref);
const step89_cad_loop *step89_cad_find_loop(const step89_cad_model *cad, int ref);
const step89_cad_face *step89_cad_find_face(const step89_cad_model *cad, int ref);

void step89_cad_bbox_init(step89_cad_bbox *b);
void step89_cad_bbox_add_point(step89_cad_bbox *b, step89_fixed x, step89_fixed y, step89_fixed z);
void step89_cad_bbox_merge(step89_cad_bbox *dst, const step89_cad_bbox *src);

int step89_cad_curve_is_analytic(const step89_model *src, int curve_ref);
int step89_cad_surface_is_analytic(const step89_model *src, int surface_ref);
step89_cad_surface_class step89_cad_surface_classify(const step89_model *src, int surface_ref);

#ifdef __cplusplus
}
#endif

#endif /* STEP89_CAD_KERNEL_H */
