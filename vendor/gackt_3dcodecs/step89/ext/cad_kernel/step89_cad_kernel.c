#include "step89_cad_kernel.h"

static void ck_zero(void *p, int n);
static void ck_edge_bbox(step89_cad_model *cad, step89_cad_edge *e);
static const step89_point *ck_find_point_for_vertex(const step89_model *src, int vertex_ref, int *point_ref);
static int ck_find_edge_index(const step89_cad_model *cad, int ref);
static int ck_find_vertex_index(const step89_cad_model *cad, int ref);
static int ck_find_face_bound_loop_ref(const step89_model *src, int bound_ref);
static void ck_resolve_oriented_use(step89_cad_model *cad, int use_index);
static void ck_resolve_loop(step89_cad_model *cad, int loop_index, const step89_model *src);
static void ck_resolve_face(step89_cad_model *cad, int face_index, const step89_model *src);
static int ck_same_undirected_edge(const step89_cad_edge *a, const step89_cad_edge *b);

static void ck_zero(void *p, int n)
{
    unsigned char *q;
    int i;
    q = (unsigned char *)p;
    for (i = 0; i < n; ++i) q[i] = 0u;
}

void step89_cad_bbox_init(step89_cad_bbox *b)
{
    if (!b) return;
    b->valid = 0;
    b->min_x = 0;
    b->min_y = 0;
    b->min_z = 0;
    b->max_x = 0;
    b->max_y = 0;
    b->max_z = 0;
}

void step89_cad_bbox_add_point(step89_cad_bbox *b, step89_fixed x, step89_fixed y, step89_fixed z)
{
    if (!b) return;
    if (!b->valid) {
        b->valid = 1;
        b->min_x = x;
        b->min_y = y;
        b->min_z = z;
        b->max_x = x;
        b->max_y = y;
        b->max_z = z;
        return;
    }
    if (x < b->min_x) b->min_x = x;
    if (y < b->min_y) b->min_y = y;
    if (z < b->min_z) b->min_z = z;
    if (x > b->max_x) b->max_x = x;
    if (y > b->max_y) b->max_y = y;
    if (z > b->max_z) b->max_z = z;
}

void step89_cad_bbox_merge(step89_cad_bbox *dst, const step89_cad_bbox *src)
{
    if (!dst || !src || !src->valid) return;
    step89_cad_bbox_add_point(dst, src->min_x, src->min_y, src->min_z);
    step89_cad_bbox_add_point(dst, src->max_x, src->max_y, src->max_z);
}

void step89_cad_init(step89_cad_model *cad)
{
    if (!cad) return;
    ck_zero(cad, (int)sizeof(*cad));
    step89_cad_bbox_init(&cad->bbox);
}

const step89_cad_vertex *step89_cad_find_vertex(const step89_cad_model *cad, int ref)
{
    int i;
    if (!cad) return 0;
    for (i = 0; i < cad->vertex_count; ++i) {
        if (cad->vertices[i].ref == ref) return &cad->vertices[i];
    }
    return 0;
}

const step89_cad_edge *step89_cad_find_edge(const step89_cad_model *cad, int ref)
{
    int i;
    if (!cad) return 0;
    for (i = 0; i < cad->edge_count; ++i) {
        if (cad->edges[i].ref == ref) return &cad->edges[i];
    }
    return 0;
}

const step89_cad_loop *step89_cad_find_loop(const step89_cad_model *cad, int ref)
{
    int i;
    if (!cad) return 0;
    for (i = 0; i < cad->loop_count; ++i) {
        if (cad->loops[i].ref == ref) return &cad->loops[i];
    }
    return 0;
}

const step89_cad_face *step89_cad_find_face(const step89_cad_model *cad, int ref)
{
    int i;
    if (!cad) return 0;
    for (i = 0; i < cad->face_count; ++i) {
        if (cad->faces[i].ref == ref) return &cad->faces[i];
    }
    return 0;
}

static int ck_find_edge_index(const step89_cad_model *cad, int ref)
{
    int i;
    if (!cad) return -1;
    for (i = 0; i < cad->edge_count; ++i) {
        if (cad->edges[i].ref == ref) return i;
    }
    return -1;
}

static int ck_find_vertex_index(const step89_cad_model *cad, int ref)
{
    int i;
    if (!cad) return -1;
    for (i = 0; i < cad->vertex_count; ++i) {
        if (cad->vertices[i].ref == ref) return i;
    }
    return -1;
}

static int ck_find_face_bound_loop_ref(const step89_model *src, int bound_ref)
{
    int i;
    if (!src) return 0;
    for (i = 0; i < src->face_bound_count; ++i) {
        if (src->face_bounds[i].ref == bound_ref) return src->face_bounds[i].loop_ref;
    }
    return 0;
}

static const step89_point *ck_find_point_for_vertex(const step89_model *src, int vertex_ref, int *point_ref)
{
    int i;
    if (point_ref) *point_ref = 0;
    if (!src) return 0;
    for (i = 0; i < src->vertex_count; ++i) {
        if (src->vertices[i].ref == vertex_ref) {
            if (point_ref) *point_ref = src->vertices[i].point_ref;
            return step89_find_point(src, src->vertices[i].point_ref);
        }
    }
    return 0;
}

static void ck_edge_bbox(step89_cad_model *cad, step89_cad_edge *e)
{
    const step89_cad_vertex *va;
    const step89_cad_vertex *vb;
    step89_cad_bbox_init(&e->bbox);
    va = step89_cad_find_vertex(cad, e->start_vertex_ref);
    vb = step89_cad_find_vertex(cad, e->end_vertex_ref);
    if (!va || !vb) {
        e->flags |= STEP89_CAD_FLAG_MISSING_GEOM;
        ++cad->missing_ref_count;
        return;
    }
    step89_cad_bbox_add_point(&e->bbox, va->x, va->y, va->z);
    step89_cad_bbox_add_point(&e->bbox, vb->x, vb->y, vb->z);
    if (va->x == vb->x && va->y == vb->y && va->z == vb->z) {
        e->flags |= STEP89_CAD_FLAG_ZERO_EDGE;
        ++cad->zero_edge_count;
    }
}

step89_cad_surface_class step89_cad_surface_classify(const step89_model *src, int surface_ref)
{
    const step89_surface *s;
    s = step89_find_surface(src, surface_ref);
    if (!s) return STEP89_CAD_SURF_UNKNOWN;
    if (s->kind == STEP89_SURFACE_PLANE) return STEP89_CAD_SURF_PLANE;
    if (s->kind == STEP89_SURFACE_CYLINDRICAL) return STEP89_CAD_SURF_CYLINDER;
    if (s->kind == STEP89_SURFACE_CONICAL) return STEP89_CAD_SURF_CONE;
    if (s->kind == STEP89_SURFACE_SPHERICAL) return STEP89_CAD_SURF_SPHERE;
    if (s->kind == STEP89_SURFACE_TOROIDAL) return STEP89_CAD_SURF_TORUS;
    if (s->kind == STEP89_SURFACE_BSPLINE) return STEP89_CAD_SURF_NURBS;
    return STEP89_CAD_SURF_UNKNOWN;
}

int step89_cad_curve_is_analytic(const step89_model *src, int curve_ref)
{
    const step89_curve *c;
    c = step89_find_curve(src, curve_ref);
    if (!c) return 0;
    return c->kind == STEP89_CURVE_LINE || c->kind == STEP89_CURVE_CIRCLE || c->kind == STEP89_CURVE_ELLIPSE;
}

int step89_cad_surface_is_analytic(const step89_model *src, int surface_ref)
{
    step89_cad_surface_class sc;
    sc = step89_cad_surface_classify(src, surface_ref);
    return sc == STEP89_CAD_SURF_PLANE || sc == STEP89_CAD_SURF_CYLINDER ||
           sc == STEP89_CAD_SURF_CONE || sc == STEP89_CAD_SURF_SPHERE ||
           sc == STEP89_CAD_SURF_TORUS;
}

static void ck_resolve_oriented_use(step89_cad_model *cad, int use_index)
{
    step89_cad_oriented_use *u;
    step89_cad_edge *e;
    int ei;
    if (!cad || use_index < 0 || use_index >= cad->oriented_use_count) return;
    u = &cad->oriented_uses[use_index];
    ei = ck_find_edge_index(cad, u->edge_ref);
    if (ei < 0) {
        u->flags |= STEP89_CAD_FLAG_MISSING_GEOM;
        ++cad->missing_ref_count;
        return;
    }
    e = &cad->edges[ei];
    ++e->use_count;
    if (u->orientation == STEP89_CAD_ORIENT_FALSE) {
        u->start_vertex_ref = e->end_vertex_ref;
        u->end_vertex_ref = e->start_vertex_ref;
        u->flags |= STEP89_CAD_FLAG_REVERSED_USE;
    } else {
        u->start_vertex_ref = e->start_vertex_ref;
        u->end_vertex_ref = e->end_vertex_ref;
    }
}

static void ck_resolve_loop(step89_cad_model *cad, int loop_index, const step89_model *src)
{
    step89_cad_loop *l;
    int i;
    int ui;
    int next_ui;
    int closed;
    const step89_cad_edge *e;
    (void)src;
    if (!cad || loop_index < 0 || loop_index >= cad->loop_count) return;
    l = &cad->loops[loop_index];
    step89_cad_bbox_init(&l->bbox);
    closed = 1;
    if (l->use_count <= 0) closed = 0;
    for (i = 0; i < l->use_count; ++i) {
        ui = l->first_use_index + i;
        if (ui < 0 || ui >= cad->oriented_use_count) {
            closed = 0;
            l->flags |= STEP89_CAD_FLAG_MISSING_GEOM;
            ++cad->missing_ref_count;
        } else {
            e = step89_cad_find_edge(cad, cad->oriented_uses[ui].edge_ref);
            if (e) step89_cad_bbox_merge(&l->bbox, &e->bbox);
            if (cad->oriented_uses[ui].flags & STEP89_CAD_FLAG_MISSING_GEOM) closed = 0;
            next_ui = l->first_use_index + ((i + 1) % l->use_count);
            if (next_ui >= 0 && next_ui < cad->oriented_use_count) {
                if (cad->oriented_uses[ui].end_vertex_ref != cad->oriented_uses[next_ui].start_vertex_ref) closed = 0;
            }
        }
    }
    l->closed = closed;
    if (!closed) {
        l->flags |= STEP89_CAD_FLAG_OPEN_LOOP;
        ++cad->open_loop_count;
    }
}

static void ck_resolve_face(step89_cad_model *cad, int face_index, const step89_model *src)
{
    step89_cad_face *f;
    int i;
    if (!cad || face_index < 0 || face_index >= cad->face_count) return;
    f = &cad->faces[face_index];
    f->surface_class = step89_cad_surface_classify(src, f->surface_ref);
    if (f->surface_class == STEP89_CAD_SURF_UNKNOWN) {
        f->flags |= STEP89_CAD_FLAG_UNSUPPORTED_SURF;
        ++cad->unsupported_surface_count;
    }
    step89_cad_bbox_init(&f->bbox);
    for (i = 0; i < f->loop_count; ++i) {
        const step89_cad_loop *l;
        l = step89_cad_find_loop(cad, f->first_loop_ref + i);
        if (l) step89_cad_bbox_merge(&f->bbox, &l->bbox);
    }
}

static int ck_same_undirected_edge(const step89_cad_edge *a, const step89_cad_edge *b)
{
    if (!a || !b) return 0;
    if (a->start_vertex_ref == b->start_vertex_ref && a->end_vertex_ref == b->end_vertex_ref) return 1;
    if (a->start_vertex_ref == b->end_vertex_ref && a->end_vertex_ref == b->start_vertex_ref) return 1;
    return 0;
}

step89_result step89_cad_build(step89_cad_model *cad, const step89_model *src)
{
    int i;
    int j;
    int p_ref;
    const step89_point *p;
    if (!cad || !src) return STEP89_ERR_NULL;
    step89_cad_init(cad);
    if (src->vertex_count > STEP89_CAD_MAX_VERTICES) return STEP89_ERR_LIMIT;
    if (src->edge_curve_count > STEP89_CAD_MAX_EDGES) return STEP89_ERR_LIMIT;
    if (src->oriented_edge_count > STEP89_CAD_MAX_ORIENTED_USES) return STEP89_ERR_LIMIT;
    if (src->edge_loop_count > STEP89_CAD_MAX_LOOPS) return STEP89_ERR_LIMIT;
    if (src->face_count > STEP89_CAD_MAX_FACES) return STEP89_ERR_LIMIT;

    for (i = 0; i < src->vertex_count; ++i) {
        step89_cad_vertex *v;
        v = &cad->vertices[cad->vertex_count];
        v->ref = src->vertices[i].ref;
        v->point_ref = src->vertices[i].point_ref;
        p = step89_find_point(src, v->point_ref);
        if (!p) {
            v->flags |= STEP89_CAD_FLAG_MISSING_GEOM;
            ++cad->missing_ref_count;
        } else {
            v->x = p->x;
            v->y = p->y;
            v->z = p->z;
            step89_cad_bbox_add_point(&cad->bbox, v->x, v->y, v->z);
        }
        ++cad->vertex_count;
    }

    for (i = 0; i < src->edge_curve_count; ++i) {
        step89_cad_edge *e;
        e = &cad->edges[cad->edge_count];
        e->ref = src->edge_curves[i].ref;
        e->start_vertex_ref = src->edge_curves[i].start_vertex_ref;
        e->end_vertex_ref = src->edge_curves[i].end_vertex_ref;
        e->curve_ref = src->edge_curves[i].curve_ref;
        e->same_sense = src->edge_curves[i].same_sense;
        p = ck_find_point_for_vertex(src, e->start_vertex_ref, &p_ref);
        if (!p || ck_find_vertex_index(cad, e->start_vertex_ref) < 0) {
            e->flags |= STEP89_CAD_FLAG_MISSING_GEOM;
            ++cad->missing_ref_count;
        }
        p = ck_find_point_for_vertex(src, e->end_vertex_ref, &p_ref);
        if (!p || ck_find_vertex_index(cad, e->end_vertex_ref) < 0) {
            e->flags |= STEP89_CAD_FLAG_MISSING_GEOM;
            ++cad->missing_ref_count;
        }
        if (!step89_find_curve(src, e->curve_ref)) {
            e->flags |= STEP89_CAD_FLAG_MISSING_GEOM;
            ++cad->missing_ref_count;
        }
        ck_edge_bbox(cad, e);
        ++cad->edge_count;
    }

    for (i = 0; i < src->oriented_edge_count; ++i) {
        step89_cad_oriented_use *u;
        u = &cad->oriented_uses[cad->oriented_use_count];
        u->ref = src->oriented_edges[i].ref;
        u->edge_ref = src->oriented_edges[i].edge_curve_ref;
        u->orientation = src->oriented_edges[i].orientation;
        ++cad->oriented_use_count;
        ck_resolve_oriented_use(cad, cad->oriented_use_count - 1);
    }

    for (i = 0; i < src->edge_loop_count; ++i) {
        step89_cad_loop *l;
        l = &cad->loops[cad->loop_count];
        l->ref = src->edge_loops[i].ref;
        l->first_use_index = src->edge_loops[i].first_oriented_edge_ref;
        l->use_count = src->edge_loops[i].oriented_edge_count;
        /* step89_geom stores the first oriented edge reference, not an index. Map it. */
        l->first_use_index = -1;
        for (j = 0; j < cad->oriented_use_count; ++j) {
            if (cad->oriented_uses[j].ref == src->edge_loops[i].first_oriented_edge_ref) {
                l->first_use_index = j;
                break;
            }
        }
        if (l->first_use_index < 0) {
            l->flags |= STEP89_CAD_FLAG_MISSING_GEOM;
            ++cad->missing_ref_count;
        }
        ++cad->loop_count;
        ck_resolve_loop(cad, cad->loop_count - 1, src);
    }

    for (i = 0; i < src->face_count; ++i) {
        step89_cad_face *f;
        f = &cad->faces[cad->face_count];
        f->ref = src->faces[i].ref;
        f->surface_ref = src->faces[i].surface_ref;
        f->same_sense = src->faces[i].same_sense;
        f->first_loop_ref = ck_find_face_bound_loop_ref(src, src->faces[i].first_bound_ref);
        f->loop_count = src->faces[i].bound_count;
        if (f->first_loop_ref == 0) {
            f->flags |= STEP89_CAD_FLAG_MISSING_GEOM;
            ++cad->missing_ref_count;
        }
        ++cad->face_count;
        ck_resolve_face(cad, cad->face_count - 1, src);
        step89_cad_bbox_merge(&cad->bbox, &f->bbox);
    }

    return step89_cad_validate_topology(cad);
}

step89_result step89_cad_validate_topology(step89_cad_model *cad)
{
    int i;
    int j;
    if (!cad) return STEP89_ERR_NULL;
    cad->nonmanifold_edge_count = 0;
    for (i = 0; i < cad->edge_count; ++i) {
        if (cad->edges[i].use_count > 2) {
            cad->edges[i].flags |= STEP89_CAD_FLAG_NONMANIFOLD;
            ++cad->nonmanifold_edge_count;
        }
        for (j = i + 1; j < cad->edge_count; ++j) {
            if (ck_same_undirected_edge(&cad->edges[i], &cad->edges[j])) {
                if (cad->edges[i].ref != cad->edges[j].ref) {
                    cad->edges[i].flags |= STEP89_CAD_FLAG_NONMANIFOLD;
                    cad->edges[j].flags |= STEP89_CAD_FLAG_NONMANIFOLD;
                    ++cad->nonmanifold_edge_count;
                }
            }
        }
    }
    cad->warning_count = cad->missing_ref_count + cad->open_loop_count +
                         cad->nonmanifold_edge_count + cad->zero_edge_count +
                         cad->unsupported_surface_count;
    if (cad->warning_count > 0) return STEP89_ERR_SYNTAX;
    return STEP89_OK;
}
