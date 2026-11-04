#include "gif_ms3d_normals.h"
#include <string.h>

/* Single workspace: serial model conversion; ~336 KiB of scratch at
 * 32-bit long. Can be factored into a caller-owned scratch structure later.
 */
static ms3d_fx s_faces[MS3D_MAX_TRIANGLES][3];
static ms3d_fx s_sums[MS3D_MAX_VERTICES][3];
#define MS3D_NORMAL_HASH_CAP 32768U
static int s_hash[MS3D_NORMAL_HASH_CAP];
static ms3d_u16 s_representative[MS3D_MAX_VERTICES];

/* Hash equal *positions* only: triangle indices and UV seams remain intact. */
static unsigned int ms3d_position_hash(const ms3d_fx p[3])
{
    unsigned long h = 2166136261UL;
    unsigned j;
    for(j=0U;j<3U;++j) {
        h = (h ^ (unsigned long)p[j]) * 16777619UL;
    }
    return (unsigned int)(h % MS3D_NORMAL_HASH_CAP);
}
static int ms3d_build_representatives(const ms3d_model *m)
{
    unsigned i,slot,steps;
    const ms3d_fx *a,*b;
    memset(s_hash,0,sizeof(s_hash));
    for(i=0U;i<m->num_vertices;++i) {
        a=m->vertices[i].position;
        slot=ms3d_position_hash(a);
        for(steps=0U;steps<MS3D_NORMAL_HASH_CAP;++steps){
            if(s_hash[slot]==0) {
                s_hash[slot]=(int)i+1;
                s_representative[i]=(ms3d_u16)i;
                break;
            }
            b=m->vertices[(unsigned)s_hash[slot]-1U].position;
            if(a[0]==b[0] && a[1]==b[1] && a[2]==b[2]){
                s_representative[i]=(ms3d_u16)((unsigned)s_hash[slot]-1U);
                break;
            }
            slot=(slot+1U)%MS3D_NORMAL_HASH_CAP;
        }
        if(steps==MS3D_NORMAL_HASH_CAP)return MS3D_E_OVERFLOW;
    }
    return MS3D_OK;
}

/* Integer square root (floor): no 64-bit integer and no math library. */
static unsigned long ms3d_isqrt32(unsigned long value)
{
    unsigned long bit = 1UL << 30;
    unsigned long root = 0UL;
    while (bit > value) bit >>= 2;
    while (bit != 0UL) {
        if (value >= root + bit) {
            value -= root + bit;
            root = (root >> 1) + bit;
        } else root >>= 1;
        bit >>= 2;
    }
    return root;
}

/* Takes arbitrary signed fixed-point direction and returns unit Q16.16.
 * Shift until components <= 12000; then sum of three squares fits 32-bit.
 */
static int ms3d_unit(const ms3d_fx vec[3], ms3d_fx result[3])
{
    long x[3], ss;
    unsigned long mag;
    int i, nonzero;
    for (i = 0; i < 3; ++i) x[i] = vec[i];
    nonzero = 0;
    for (i = 0; i < 3; ++i)
        if (x[i] != 0L) nonzero = 1;
    if (!nonzero) return 0;
    while (x[0] > 12000L || x[0] < -12000L ||
           x[1] > 12000L || x[1] < -12000L ||
           x[2] > 12000L || x[2] < -12000L) {
        for (i = 0; i < 3; ++i) x[i] /= 2L;
    }
    /* Keep precision for microscopic (Q16) faces. */
    while (x[0] <= 6000L && x[0] >= -6000L &&
           x[1] <= 6000L && x[1] >= -6000L &&
           x[2] <= 6000L && x[2] >= -6000L) {
        for (i = 0; i < 3; ++i) x[i] *= 2L;
    }
    ss = x[0]*x[0] + x[1]*x[1] + x[2]*x[2];
    if (ss <= 0L) return 0;
    mag = ms3d_isqrt32((unsigned long)ss);
    if (mag == 0UL) return 0;
    for (i = 0; i < 3; ++i)
        result[i] = ms3d_fx_div(x[i], (ms3d_fx)mag);
    return 1;
}

static void ms3d_face_normal(const ms3d_model *m,
                             const ms3d_triangle *tri,
                             ms3d_fx out[3])
{
    ms3d_fx e1[3], e2[3], cross[3];
    const ms3d_fx *p0, *p1, *p2;
    int j;
    p0 = m->vertices[tri->vertex_indices[0]].position;
    p1 = m->vertices[tri->vertex_indices[1]].position;
    p2 = m->vertices[tri->vertex_indices[2]].position;
    for (j = 0; j < 3; ++j) {
        e1[j] = ms3d_fx_sub_sat(p1[j], p0[j]);
        e2[j] = ms3d_fx_sub_sat(p2[j], p0[j]);
    }
    /* Bound every cross-product to signed 32-bit. */
    while (e1[0] > 12000L || e1[0] < -12000L ||
           e1[1] > 12000L || e1[1] < -12000L ||
           e1[2] > 12000L || e1[2] < -12000L ||
           e2[0] > 12000L || e2[0] < -12000L ||
           e2[1] > 12000L || e2[1] < -12000L ||
           e2[2] > 12000L || e2[2] < -12000L) {
        for (j = 0; j < 3; ++j) {
            e1[j] /= 2L;
            e2[j] /= 2L;
        }
    }
    cross[0] = e1[1]*e2[2] - e1[2]*e2[1];
    cross[1] = e1[2]*e2[0] - e1[0]*e2[2];
    cross[2] = e1[0]*e2[1] - e1[1]*e2[0];
    if (!ms3d_unit(cross, out)) {
        /* Degenerate geometry: prefer the authored corner normal. */
        if (!ms3d_unit(tri->vertex_normals[0], out)) {
            out[0] = 0L; out[1] = 0L; out[2] = MS3D_FX_ONE;
        }
    }
}

int ms3d_build_corner_normals(const ms3d_model *m,
                              ms3d_corner_normals *out,
                              int mode)
{
    unsigned i, k, j, g, t, v;
    int rc;
    const ms3d_triangle *tri;
    ms3d_fx normal[3];
    if (m == (const ms3d_model *)0 || out == (ms3d_corner_normals *)0)
        return MS3D_E_NULL;
    if (mode < MS3D_NORMAL_SOURCE || mode > MS3D_NORMAL_SMOOTH_WELDED)
        return MS3D_E_BAD_COUNT;
    if (m->num_vertices > MS3D_MAX_VERTICES || m->num_triangles > MS3D_MAX_TRIANGLES)
        return MS3D_E_BAD_COUNT;
    for (i = 0U; i < m->num_triangles; ++i) {
        tri = &m->triangles[i];
        for (k = 0U; k < 3U; ++k) {
            if (tri->vertex_indices[k] >= m->num_vertices) return MS3D_E_BAD_INDEX;
        }
        if (mode == MS3D_NORMAL_SOURCE) {
            for (k = 0U; k < 3U; ++k)
                for (j = 0U; j < 3U; ++j)
                    out->xyz[i][k][j] = tri->vertex_normals[k][j];
        } else {
            ms3d_face_normal(m, tri, s_faces[i]);
            for (k = 0U; k < 3U; ++k)
                for (j = 0U; j < 3U; ++j)
                    out->xyz[i][k][j] = s_faces[i][j];
        }
    }
    if (mode == MS3D_NORMAL_SOURCE || mode == MS3D_NORMAL_FLAT)
        return MS3D_OK;
    if (mode == MS3D_NORMAL_SMOOTH_WELDED) {
        rc=ms3d_build_representatives(m);
        if(rc!=MS3D_OK)return rc;
    }
    /* Iterate 1..32, using one accumulator per vertex. Groups are IDs,
     * not bit masks in MS3D. Output preserves different corner normals
     * where smoothing group changes. A zero group stays flat.
     */
    for (g = 1U; g <= 32U; ++g) {
        memset(s_sums, 0, (size_t)m->num_vertices * sizeof(s_sums[0]));
        for (t = 0U; t < m->num_triangles; ++t) {
            tri = &m->triangles[t];
            if ((mode == MS3D_NORMAL_SMOOTH_ALL && g != 1U) ||
                ((mode == MS3D_NORMAL_SMOOTH_GROUP || mode == MS3D_NORMAL_SMOOTH_WELDED) && tri->smoothing_group != g))
                continue;
            for (k = 0U; k < 3U; ++k) {
                v = (unsigned)tri->vertex_indices[k];
                if(mode == MS3D_NORMAL_SMOOTH_WELDED)v=s_representative[v];
                for (j = 0U; j < 3U; ++j)
                    s_sums[v][j] = ms3d_fx_add_sat(s_sums[v][j], s_faces[t][j]);
            }
        }
        for (t = 0U; t < m->num_triangles; ++t) {
            tri = &m->triangles[t];
            if ((mode == MS3D_NORMAL_SMOOTH_ALL && g != 1U) ||
                ((mode == MS3D_NORMAL_SMOOTH_GROUP || mode == MS3D_NORMAL_SMOOTH_WELDED) && tri->smoothing_group != g))
                continue;
            for (k = 0U; k < 3U; ++k) {
                v = (unsigned)tri->vertex_indices[k];
                if(mode == MS3D_NORMAL_SMOOTH_WELDED)v=s_representative[v];
                if (ms3d_unit(s_sums[v], normal)) {
                    for (j = 0U; j < 3U; ++j)
                        out->xyz[t][k][j] = normal[j];
                }
            }
        }
    }
    return MS3D_OK;
}

int ms3d_emit_shaded_corners(const ms3d_model *model,
                              const ms3d_corner_normals *normals,
                              const ms3d_shaded_sink *sink)
{
    unsigned i, k, j, vi;
    ms3d_draw_corner dc;
    const ms3d_triangle *tri;
    int result;
    if (model == (const ms3d_model *)0 ||
        normals == (const ms3d_corner_normals *)0 ||
        sink == (const ms3d_shaded_sink *)0 ||
        sink->corner == (void *)0) return MS3D_E_NULL;
    for (i = 0U; i < model->num_triangles; ++i) {
        tri = &model->triangles[i];
        for (k = 0U; k < 3U; ++k) {
            vi = tri->vertex_indices[k];
            if (vi >= model->num_vertices) return MS3D_E_BAD_INDEX;
            for (j = 0U; j < 3U; ++j) {
                dc.position[j] = model->vertices[vi].position[j];
                dc.normal[j] = normals->xyz[i][k][j];
            }
            dc.uv[0] = tri->s[k];
            dc.uv[1] = tri->t[k];
            dc.source_vertex = (ms3d_u16)vi;
            dc.smoothing_group = tri->smoothing_group;
            dc.group_index = tri->group_index;
            dc.material_index = -1;
            if (tri->group_index < model->num_groups)
                dc.material_index = model->groups[tri->group_index].material_index;
            result = sink->corner(sink->user, (ms3d_u16)i, (ms3d_u8)k, &dc);
            if (result != MS3D_OK) return result;
        }
    }
    return MS3D_OK;
}
