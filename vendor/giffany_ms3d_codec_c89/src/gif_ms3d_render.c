#include "gif_ms3d_render.h"
#include <string.h>
static int mat_pass(const ms3d_model *m,int id)
{
    const ms3d_material *mat;
    if(id<0||id>=(int)m->num_materials)return MS3D_RENDER_OPAQUE;
    mat=&m->materials[id];
    if(m->transparency_mode==MS3D_TRANSPARENCY_ALPHA_TEST &&
       (mat->alphamap[0]!='\0'||mat->transparency<MS3D_FX_ONE))
        return MS3D_RENDER_ALPHA_TEST;
    if(mat->transparency<MS3D_FX_ONE||mat->alphamap[0]!='\0')
        return MS3D_RENDER_ALPHA_BLEND;
    return MS3D_RENDER_OPAQUE;
}
int ms3d_render_material_pass(const ms3d_model *m,int id,
                              ms3d_material_pass *out)
{
    const ms3d_material *mat;
    if(!m||!out)return MS3D_E_NULL;
    if(id>= (int)m->num_materials||id < -1)return MS3D_E_BAD_INDEX;
    memset(out,0,sizeof(*out));
    out->pass=mat_pass(m,id);
    out->opacity=MS3D_FX_ONE;
    if(id>=0){
        mat=&m->materials[id];out->opacity=mat->transparency;
        if(out->opacity<0L)out->opacity=0L;
        if(out->opacity>MS3D_FX_ONE)out->opacity=MS3D_FX_ONE;
        out->diffuse_path=mat->texture;
        out->alpha_path=mat->alphamap;
    } else {out->diffuse_path="";out->alpha_path="";}
    out->alpha_ref=out->pass==MS3D_RENDER_ALPHA_TEST ? m->alpha_ref:0L;
    out->blend_enabled=out->pass==MS3D_RENDER_ALPHA_BLEND;
    out->depth_write=out->pass!=MS3D_RENDER_ALPHA_BLEND;
    return MS3D_OK;
}
/* Depth comparison for far-to-near sorting. Ties prefer original index. */
static int farther(const ms3d_render_queue *q,ms3d_u16 a,ms3d_u16 b)
{
    if(q->depth[a]!=q->depth[b])return q->depth[a]>q->depth[b];
    return a<b;
}
/* Sort just the blended tail with insertion-free shell sort. */
static void sort_tail(ms3d_render_queue *q,unsigned first,unsigned count)
{
    unsigned gap,i,j;ms3d_u16 tmp;
    for(gap=count/2U;gap>0U;gap/=2U){
        for(i=first+gap;i<first+count;++i){
            tmp=q->triangle[i];j=i;
            while(j>=first+gap&&farther(q,tmp,q->triangle[j-gap])){
                q->triangle[j]=q->triangle[j-gap];j-=gap;
            }
            q->triangle[j]=tmp;
        }
    }
}
int ms3d_render_build_queue(const ms3d_model *m,
    const ms3d_uv_bindings *bindings,const ms3d_skin_vertices *vertices,
    const ms3d_fx front[3],ms3d_render_queue *queue)
{
    unsigned i,k,j,idx;
    ms3d_fx center[3],depth;
    unsigned count[3]={0U,0U,0U},offset[3],next[3];
    int passes[MS3D_MAX_TRIANGLES],pass;
    if(!m||!bindings||!vertices||!front||!queue)return MS3D_E_NULL;
    if(m->num_triangles>MS3D_MAX_TRIANGLES)return MS3D_E_LIMIT_TRIANGLES;
    for(i=0U;i<m->num_triangles;++i){
        pass=mat_pass(m,bindings->material[i]);
        if(pass<0||pass>2)return MS3D_E_BAD_INDEX;
        passes[i]=pass;++count[pass];
        for(k=0U;k<3U;++k){
            ms3d_fx sum=0L;
            for(j=0U;j<3U;++j){
                idx=m->triangles[i].vertex_indices[j];
                if(idx>=m->num_vertices)return MS3D_E_BAD_INDEX;
                sum=ms3d_fx_add_sat(sum,vertices->xyz[idx][k]);
            }
            center[k]=sum/3L;
        }
        depth=0L;
        for(k=0U;k<3U;++k)depth=ms3d_fx_add_sat(depth,ms3d_fx_mul(center[k],front[k]));
        queue->depth[i]=depth;
    }
    offset[0]=0U;offset[1]=count[0];offset[2]=count[0]+count[1];
    for(i=0U;i<3U;++i)next[i]=offset[i];
    for(i=0U;i<m->num_triangles;++i)
        queue->triangle[next[passes[i]]++]=(ms3d_u16)i;
    queue->opaque_count=(ms3d_u16)count[0];
    queue->cutout_count=(ms3d_u16)count[1];
    queue->blend_count=(ms3d_u16)count[2];
    if(m->transparency_mode==MS3D_TRANSPARENCY_DEPTH_SORT && count[2]>1U)
        sort_tail(queue,offset[2],count[2]);
    return MS3D_OK;
}
