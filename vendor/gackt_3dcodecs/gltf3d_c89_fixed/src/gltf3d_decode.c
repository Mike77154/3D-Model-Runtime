#include "gltf3d_decode.h"
#include "gltf3d_util.h"

static void dv_zero(gltf3d_decoded_vertex *v)
{
    int i;
    v->flags = 0UL;
    for (i = 0; i < 3; ++i) { v->position[i] = 0; v->normal[i] = 0; }
    for (i = 0; i < 4; ++i) { v->tangent[i] = 0; v->color0[i] = 0; v->joints0[i] = 0UL; v->weights0[i] = 0; }
    v->texcoord0[0] = v->texcoord0[1] = 0;
    v->texcoord1[0] = v->texcoord1[1] = 0;
    v->color0[3] = GLTF3D_FX_ONE;
}

static int read_attr_fx(const gltf3d_asset *a, int ai, unsigned long v, int comps, gltf3d_fx *dst)
{
    int c;
    int r;
    if (ai < 0) return GLTF3D_OK;
    for (c = 0; c < comps; ++c) {
        r = gltf3d_accessor_read_fixed(a, ai, v, c, &dst[c]);
        if (r < 0) return r;
    }
    return GLTF3D_OK;
}

static int read_attr_u32(const gltf3d_asset *a, int ai, unsigned long v, int comps, unsigned long *dst)
{
    int c;
    int r;
    gltf3d_fx fx;
    if (ai < 0) return GLTF3D_OK;
    for (c = 0; c < comps; ++c) {
        r = gltf3d_accessor_read_fixed(a, ai, v, c, &fx);
        if (r < 0) return r;
        dst[c] = (unsigned long)(fx >> GLTF3D_FX_SHIFT);
    }
    return GLTF3D_OK;
}

int gltf3d_decode_primitive_stats(const gltf3d_asset *a, int prim_index, gltf3d_decode_stats *stats)
{
    const gltf3d_primitive *p;
    int i;
    if (!a || !stats || prim_index < 0 || prim_index >= a->primitive_count) return GLTF3D_ERR_BAD_ARG;
    p = &a->primitives[prim_index];
    stats->vertex_count = gltf3d_primitive_vertex_count(a, prim_index);
    stats->index_count = gltf3d_primitive_index_count(a, prim_index);
    stats->triangle_count = gltf3d_primitive_triangle_count(a, prim_index);
    stats->attribute_mask = 0UL;
    for (i = 0; i < GLTF3D_ATTR_COUNT; ++i) if (p->attributes[i] >= 0) stats->attribute_mask |= (1UL << i);
    stats->mode = p->mode;
    stats->material = p->material;
    return GLTF3D_OK;
}

int gltf3d_decode_primitive_vertices(const gltf3d_asset *a, int prim_index,
                                     gltf3d_decoded_vertex *vertices,
                                     unsigned long vertex_capacity,
                                     unsigned long *vertex_count)
{
    const gltf3d_primitive *p;
    unsigned long n;
    unsigned long i;
    int r;
    if (!a || !vertices || !vertex_count || prim_index < 0 || prim_index >= a->primitive_count) return GLTF3D_ERR_BAD_ARG;
    p = &a->primitives[prim_index];
    n = gltf3d_primitive_vertex_count(a, prim_index);
    if (vertex_capacity < n) return GLTF3D_ERR_RANGE;
    for (i = 0UL; i < n; ++i) {
        dv_zero(&vertices[i]);
        if (p->attributes[GLTF3D_ATTR_POSITION] >= 0) {
            r = read_attr_fx(a, p->attributes[GLTF3D_ATTR_POSITION], i, 3, vertices[i].position); if (r < 0) return r;
            vertices[i].flags |= GLTF3D_DV_HAS_POSITION;
        }
        if (p->attributes[GLTF3D_ATTR_NORMAL] >= 0) {
            r = read_attr_fx(a, p->attributes[GLTF3D_ATTR_NORMAL], i, 3, vertices[i].normal); if (r < 0) return r;
            vertices[i].flags |= GLTF3D_DV_HAS_NORMAL;
        }
        if (p->attributes[GLTF3D_ATTR_TANGENT] >= 0) {
            r = read_attr_fx(a, p->attributes[GLTF3D_ATTR_TANGENT], i, 4, vertices[i].tangent); if (r < 0) return r;
            vertices[i].flags |= GLTF3D_DV_HAS_TANGENT;
        }
        if (p->attributes[GLTF3D_ATTR_TEXCOORD_0] >= 0) {
            r = read_attr_fx(a, p->attributes[GLTF3D_ATTR_TEXCOORD_0], i, 2, vertices[i].texcoord0); if (r < 0) return r;
            vertices[i].flags |= GLTF3D_DV_HAS_TEXCOORD0;
        }
        if (p->attributes[GLTF3D_ATTR_TEXCOORD_1] >= 0) {
            r = read_attr_fx(a, p->attributes[GLTF3D_ATTR_TEXCOORD_1], i, 2, vertices[i].texcoord1); if (r < 0) return r;
            vertices[i].flags |= GLTF3D_DV_HAS_TEXCOORD1;
        }
        if (p->attributes[GLTF3D_ATTR_COLOR_0] >= 0) {
            int ec;
            ec = a->accessors[p->attributes[GLTF3D_ATTR_COLOR_0]].elem_count;
            if (ec > 4) ec = 4;
            r = read_attr_fx(a, p->attributes[GLTF3D_ATTR_COLOR_0], i, ec, vertices[i].color0); if (r < 0) return r;
            vertices[i].flags |= GLTF3D_DV_HAS_COLOR0;
        }
        if (p->attributes[GLTF3D_ATTR_JOINTS_0] >= 0) {
            r = read_attr_u32(a, p->attributes[GLTF3D_ATTR_JOINTS_0], i, 4, vertices[i].joints0); if (r < 0) return r;
            vertices[i].flags |= GLTF3D_DV_HAS_JOINTS0;
        }
        if (p->attributes[GLTF3D_ATTR_WEIGHTS_0] >= 0) {
            r = read_attr_fx(a, p->attributes[GLTF3D_ATTR_WEIGHTS_0], i, 4, vertices[i].weights0); if (r < 0) return r;
            vertices[i].flags |= GLTF3D_DV_HAS_WEIGHTS0;
        }
    }
    *vertex_count = n;
    return GLTF3D_OK;
}

int gltf3d_decode_primitive_triangle_indices(const gltf3d_asset *a, int prim_index,
                                             unsigned long *indices,
                                             unsigned long index_capacity,
                                             unsigned long *index_count)
{
    unsigned long tri_count;
    unsigned long t;
    unsigned long outn;
    int r;
    if (!a || !indices || !index_count) return GLTF3D_ERR_BAD_ARG;
    tri_count = gltf3d_primitive_triangle_count(a, prim_index);
    outn = tri_count * 3UL;
    if (index_capacity < outn) return GLTF3D_ERR_RANGE;
    for (t = 0UL; t < tri_count; ++t) {
        r = gltf3d_primitive_read_triangle(a, prim_index, t, indices + t * 3UL);
        if (r < 0) return r;
    }
    *index_count = outn;
    return GLTF3D_OK;
}
