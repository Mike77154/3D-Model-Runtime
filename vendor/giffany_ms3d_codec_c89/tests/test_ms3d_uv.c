#include "gif_ms3d_uv.h"
#include <stdio.h>
#include <string.h>
static ms3d_model m;
static ms3d_uv_bindings b;
static ms3d_corner_normals normals;
static int actual_mat=-2;
static int cb(void *user,ms3d_u16 i,ms3d_u8 c,const ms3d_draw_corner *corner)
{
    (void)user;(void)c;
    if(i==2U)actual_mat=corner->material_index;
    return MS3D_OK;
}
int main(void)
{
    ms3d_fx u,v;
    memset(&m,0,sizeof(m));
    m.num_triangles=3U;m.num_groups=2U;m.num_materials=2U;
    m.groups[0].triangle_start=0U;m.groups[0].triangle_count=1U;
    m.groups[0].material_index=1;
    m.groups[1].triangle_start=1U;m.groups[1].triangle_count=1U;
    m.groups[1].material_index=0;
    m.num_group_triangle_indices=2U;
    m.group_triangle_indices[0]=2U;m.group_triangle_indices[1]=0U;
    m.triangles[2].group_index=1U; /* stale hint, should be group 0 */
    m.triangles[1].group_index=1U; /* fallback */
    if(ms3d_uv_build_bindings(&m,&b)!=MS3D_OK)return 1;
    if(b.material[2]!=1 || b.group[2]!=0 || b.material[0]!=0 ||
       b.material[1]!=0||b.group[1]!=1)return 2;
    if(b.assigned_count!=2UL)return 3;
    m.triangles[0].s[1]=MS3D_FX_ONE/4L;
    m.triangles[0].t[1]=MS3D_FX_ONE/8L;
    ms3d_uv_native(&m.triangles[0],1U,&u,&v);
    if(u!=MS3D_FX_ONE/4L||v!=MS3D_FX_ONE/8L)return 4;
    ms3d_uv_obj(&m.triangles[0],1U,&u,&v);
    if(u!=MS3D_FX_ONE/4L||v!=7L*MS3D_FX_ONE/8L)return 5;
    {
        ms3d_shaded_sink sink;
        sink.user=0;sink.corner=cb;
        m.num_vertices=3U;
        m.triangles[0].vertex_indices[0]=0U;
        m.triangles[0].vertex_indices[1]=1U;
        m.triangles[0].vertex_indices[2]=2U;
        m.triangles[1].vertex_indices[0]=0U;
        m.triangles[1].vertex_indices[1]=1U;
        m.triangles[1].vertex_indices[2]=2U;
        m.triangles[2].vertex_indices[0]=0U;
        m.triangles[2].vertex_indices[1]=1U;
        m.triangles[2].vertex_indices[2]=2U;
        if(ms3d_emit_uv_corners(&m,&normals,&b,&sink)!=MS3D_OK)return 6;
        if(actual_mat!=1)return 7;
    }
    printf("test_ms3d_uv PASS\n");return 0;
}
