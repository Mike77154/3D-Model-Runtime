#ifndef STEP89_H
#define STEP89_H

/*
   step89 - tiny STEP/STP ISO 10303-21 reader, C89, fixed point, no heap.

   Design rules:
   - No malloc/realloc/free and no hidden heap use.
   - No float/double. Decimal numbers are parsed into signed fixed point.
   - Caller owns the STEP text buffer; raw entity spans point into that buffer.
   - Intended for import pipelines, validation tools, embedded/retro tooling.

   This is a Part 21 parser plus a practical extractor for common CAD/B-rep
   entity records. It is not a complete EXPRESS validator or CAD kernel.
*/

#include <stddef.h>
#include <limits.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef STEP89_MAX_ENTITIES
#define STEP89_MAX_ENTITIES 8192
#endif

#ifndef STEP89_MAX_SCHEMA_CHARS
#define STEP89_MAX_SCHEMA_CHARS 128
#endif

#ifndef STEP89_MAX_ENTITY_NAME
#define STEP89_MAX_ENTITY_NAME 64
#endif

#ifndef STEP89_MAX_POINTS
#define STEP89_MAX_POINTS 4096
#endif

#ifndef STEP89_MAX_DIRECTIONS
#define STEP89_MAX_DIRECTIONS 4096
#endif

#ifndef STEP89_MAX_VECTORS
#define STEP89_MAX_VECTORS 2048
#endif

#ifndef STEP89_MAX_AXIS2
#define STEP89_MAX_AXIS2 2048
#endif

#ifndef STEP89_MAX_VERTICES
#define STEP89_MAX_VERTICES 4096
#endif

#ifndef STEP89_MAX_CURVES
#define STEP89_MAX_CURVES 4096
#endif

#ifndef STEP89_MAX_SURFACES
#define STEP89_MAX_SURFACES 4096
#endif

#ifndef STEP89_MAX_FACE_BOUNDS
#define STEP89_MAX_FACE_BOUNDS 4096
#endif

#ifndef STEP89_MAX_EDGE_LOOPS
#define STEP89_MAX_EDGE_LOOPS 2048
#endif

#ifndef STEP89_MAX_ORIENTED_EDGES
#define STEP89_MAX_ORIENTED_EDGES 8192
#endif

#ifndef STEP89_MAX_EDGE_CURVES
#define STEP89_MAX_EDGE_CURVES 8192
#endif

#ifndef STEP89_MAX_FACES
#define STEP89_MAX_FACES 4096
#endif

#ifndef STEP89_MAX_ENTITY_HISTO
#define STEP89_MAX_ENTITY_HISTO 256
#endif

#define STEP89_FIXED_SCALE 1000000L
#define STEP89_FIXED_ONE   1000000L

#if LONG_MAX < 2147483647L
#error step89 requires a compiler where long is at least 32 bits.
#endif

typedef long step89_fixed;

typedef enum step89_result_e {
    STEP89_OK = 0,
    STEP89_ERR_NULL = -1,
    STEP89_ERR_NOT_STEP = -2,
    STEP89_ERR_OVERFLOW = -3,
    STEP89_ERR_SYNTAX = -4,
    STEP89_ERR_LIMIT = -5
} step89_result;

typedef enum step89_curve_kind_e {
    STEP89_CURVE_UNKNOWN = 0,
    STEP89_CURVE_LINE = 1,
    STEP89_CURVE_CIRCLE = 2,
    STEP89_CURVE_ELLIPSE = 3,
    STEP89_CURVE_BSPLINE = 4
} step89_curve_kind;

typedef enum step89_surface_kind_e {
    STEP89_SURFACE_UNKNOWN = 0,
    STEP89_SURFACE_PLANE = 1,
    STEP89_SURFACE_CYLINDRICAL = 2,
    STEP89_SURFACE_CONICAL = 3,
    STEP89_SURFACE_SPHERICAL = 4,
    STEP89_SURFACE_TOROIDAL = 5,
    STEP89_SURFACE_BSPLINE = 6
} step89_surface_kind;

typedef struct step89_entity_s {
    int id;
    char name[STEP89_MAX_ENTITY_NAME];
    int args_begin;
    int args_len;
    unsigned line;
    unsigned flags;
} step89_entity;

typedef struct step89_histo_s {
    char name[STEP89_MAX_ENTITY_NAME];
    int count;
} step89_histo;

typedef struct step89_point_s {
    int ref;
    step89_fixed x;
    step89_fixed y;
    step89_fixed z;
} step89_point;

typedef struct step89_direction_s {
    int ref;
    step89_fixed x;
    step89_fixed y;
    step89_fixed z;
} step89_direction;

typedef struct step89_vector_s {
    int ref;
    int direction_ref;
    step89_fixed magnitude;
} step89_vector;

typedef struct step89_axis2_s {
    int ref;
    int location_ref;
    int axis_ref;
    int ref_direction_ref;
} step89_axis2;

typedef struct step89_vertex_s {
    int ref;
    int point_ref;
} step89_vertex;

typedef struct step89_curve_s {
    int ref;
    step89_curve_kind kind;
    int a_ref;
    int b_ref;
    int c_ref;
    step89_fixed r0;
    step89_fixed r1;
    int degree;
    int control_count;
} step89_curve;

typedef struct step89_surface_s {
    int ref;
    step89_surface_kind kind;
    int axis_ref;
    step89_fixed r0;
    step89_fixed r1;
    int degree_u;
    int degree_v;
    int control_count;
} step89_surface;

typedef struct step89_face_bound_s {
    int ref;
    int loop_ref;
    int orientation; /* -1 unknown, 0 false, 1 true */
    int is_outer;
} step89_face_bound;

typedef struct step89_edge_loop_s {
    int ref;
    int first_oriented_edge_ref;
    int oriented_edge_count;
} step89_edge_loop;

typedef struct step89_oriented_edge_s {
    int ref;
    int edge_curve_ref;
    int orientation;
} step89_oriented_edge;

typedef struct step89_edge_curve_s {
    int ref;
    int start_vertex_ref;
    int end_vertex_ref;
    int curve_ref;
    int same_sense;
} step89_edge_curve;

typedef struct step89_face_s {
    int ref;
    int first_bound_ref;
    int bound_count;
    int surface_ref;
    int same_sense;
} step89_face;

typedef struct step89_model_s {
    const char *text;
    int text_len;

    char schema[STEP89_MAX_SCHEMA_CHARS];
    char implementation_level[32];

    int entity_count;
    step89_entity entities[STEP89_MAX_ENTITIES];

    int histo_count;
    step89_histo histo[STEP89_MAX_ENTITY_HISTO];

    int point_count;
    step89_point points[STEP89_MAX_POINTS];

    int direction_count;
    step89_direction directions[STEP89_MAX_DIRECTIONS];

    int vector_count;
    step89_vector vectors[STEP89_MAX_VECTORS];

    int axis2_count;
    step89_axis2 axis2[STEP89_MAX_AXIS2];

    int vertex_count;
    step89_vertex vertices[STEP89_MAX_VERTICES];

    int curve_count;
    step89_curve curves[STEP89_MAX_CURVES];

    int surface_count;
    step89_surface surfaces[STEP89_MAX_SURFACES];

    int face_bound_count;
    step89_face_bound face_bounds[STEP89_MAX_FACE_BOUNDS];

    int edge_loop_count;
    step89_edge_loop edge_loops[STEP89_MAX_EDGE_LOOPS];

    int oriented_edge_count;
    step89_oriented_edge oriented_edges[STEP89_MAX_ORIENTED_EDGES];

    int edge_curve_count;
    step89_edge_curve edge_curves[STEP89_MAX_EDGE_CURVES];

    int face_count;
    step89_face faces[STEP89_MAX_FACES];

    int unsupported_brep_count;
    int complex_entity_count;
    int warning_count;
    int error_line;
    int error_pos;
    char error_message[96];
} step89_model;

void step89_init(step89_model *m);
step89_result step89_parse(step89_model *m, const char *text, int text_len);
step89_result step89_extract_geometry(step89_model *m);

const step89_entity *step89_find_entity(const step89_model *m, int ref);
const step89_point *step89_find_point(const step89_model *m, int ref);
const step89_direction *step89_find_direction(const step89_model *m, int ref);
const step89_axis2 *step89_find_axis2(const step89_model *m, int ref);
const step89_curve *step89_find_curve(const step89_model *m, int ref);
const step89_surface *step89_find_surface(const step89_model *m, int ref);


typedef struct step89_writer_s {
    char *buf;
    int cap;
    int pos;
    int error;
    int next_id;
} step89_writer;

void step89_writer_init(step89_writer *w, char *buf, int cap);
int step89_writer_begin(step89_writer *w, const char *schema, const char *name);
int step89_writer_end(step89_writer *w);
int step89_writer_raw_entity(step89_writer *w, int id, const char *entity_name, const char *args);
int step89_writer_point3(step89_writer *w, int id, step89_fixed x, step89_fixed y, step89_fixed z);
int step89_writer_direction3(step89_writer *w, int id, step89_fixed x, step89_fixed y, step89_fixed z);
int step89_writer_axis2_placement_3d(step89_writer *w, int id, int location_ref, int axis_ref, int ref_direction_ref);
int step89_writer_plane(step89_writer *w, int id, int axis2_ref);

int step89_name_eq(const char *a, const char *b);
step89_fixed step89_fixed_from_int(long v);
long step89_fixed_to_milli(step89_fixed v);
void step89_fixed_to_ascii(step89_fixed v, char *out, int out_cap);

#ifdef __cplusplus
}
#endif

#endif /* STEP89_H */
