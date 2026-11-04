#include "gif_ms3d_uv.h"
#include <string.h>

int ms3d_uv_build_bindings(const ms3d_model *m, ms3d_uv_bindings *o)
{
    unsigned i,j; unsigned long index;
    int material;
    if (!m || !o) return MS3D_E_NULL;
    if (m->num_triangles > MS3D_MAX_TRIANGLES ||
        m->num_groups > MS3D_MAX_GROUPS) return MS3D_E_BAD_COUNT;
    memset(o, 0, sizeof(*o));
    for (i=0U;i<m->num_triangles;++i) {
        o->group[i]=-1;
        o->material[i]=-1;
    }
    /* Authoritative triangle membership is the group triangle-index list.
       triangle.group_index is merely a secondary hint and is sometimes stale. */
    for (i=0U;i<m->num_groups;++i) {
        const ms3d_group *g=&m->groups[i];
        material=g->material_index;
        if (material < 0 || material >= (int)m->num_materials) material=-1;
        for (j=0U;j<g->triangle_count;++j) {
            index=(unsigned long)g->triangle_start+(unsigned long)j;
            if (index>=m->num_group_triangle_indices) return MS3D_E_BAD_INDEX;
            index=m->group_triangle_indices[index];
            if (index>=m->num_triangles) return MS3D_E_BAD_INDEX;
            if (o->group[index]!=-1 && o->group[index]!=(int)i) {
                o->conflict_count++;
                continue; /* first explicitly listed group wins */
            }
            if (o->group[index]==-1) o->assigned_count++;
            o->group[index]=(int)i;
            o->material[index]=material;
        }
    }
    for (i=0U;i<m->num_triangles;++i) {
        if (o->group[i]<0 && m->triangles[i].group_index<m->num_groups) {
            unsigned g=m->triangles[i].group_index;
            o->group[i]=(int)g;
            material=m->groups[g].material_index;
            if (material>=0 && material<(int)m->num_materials)
                o->material[i]=material;
        }
    }
    return MS3D_OK;
}

void ms3d_uv_native(const ms3d_triangle *t, ms3d_u8 corner,
                    ms3d_fx *u, ms3d_fx *v)
{
    if(!t||corner>2U||!u||!v)return;
    *u=t->s[corner];*v=t->t[corner];
}
void ms3d_uv_obj(const ms3d_triangle *t, ms3d_u8 corner,
                 ms3d_fx *u, ms3d_fx *v)
{
    if(!t||corner>2U||!u||!v)return;
    *u=t->s[corner];
    *v=ms3d_fx_sub_sat(MS3D_FX_ONE,t->t[corner]);
}

typedef struct ms3d_uv_bridge_s {
    const ms3d_uv_bindings *bindings;
    const ms3d_shaded_sink *sink;
} ms3d_uv_bridge;
static int ms3d_uv_rebind_corner(void *user,ms3d_u16 triangle,
                                  ms3d_u8 corner,const ms3d_draw_corner *src)
{
    ms3d_uv_bridge *bridge=(ms3d_uv_bridge *)user;
    ms3d_draw_corner corrected;
    int group;
    if(!bridge||!bridge->bindings||!bridge->sink||!bridge->sink->corner||!src)
        return MS3D_E_NULL;
    corrected=*src;
    group=bridge->bindings->group[triangle];
    corrected.group_index=group>=0 ? (ms3d_u8)group : 255U;
    corrected.material_index=bridge->bindings->material[triangle];
    return bridge->sink->corner(bridge->sink->user,triangle,corner,&corrected);
}
int ms3d_emit_uv_corners(const ms3d_model *model,
                         const ms3d_corner_normals *normals,
                         const ms3d_uv_bindings *bindings,
                         const ms3d_shaded_sink *sink)
{
    ms3d_uv_bridge bridge;ms3d_shaded_sink inner;
    if(!model||!normals||!bindings||!sink||!sink->corner)return MS3D_E_NULL;
    bridge.bindings=bindings;bridge.sink=sink;
    inner.user=&bridge;inner.corner=ms3d_uv_rebind_corner;
    return ms3d_emit_shaded_corners(model,normals,&inner);
}
int ms3d_emit_skinned_uv_corners(const ms3d_model *model,
                                 const ms3d_skin_pose *pose,
                                 const ms3d_skin_vertices *vertices,
                                 const ms3d_corner_normals *normals,
                                 const ms3d_uv_bindings *bindings,
                                 const ms3d_shaded_sink *sink)
{
    ms3d_uv_bridge bridge;ms3d_shaded_sink inner;
    if(!model||!pose||!vertices||!normals||!bindings||!sink||!sink->corner)
        return MS3D_E_NULL;
    bridge.bindings=bindings;bridge.sink=sink;
    inner.user=&bridge;inner.corner=ms3d_uv_rebind_corner;
    return ms3d_emit_skinned_corners(model,pose,vertices,normals,&inner);
}
