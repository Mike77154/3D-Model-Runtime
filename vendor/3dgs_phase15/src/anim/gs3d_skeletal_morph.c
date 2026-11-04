#include "../gs3d_internal.h"
/* Interpolate raw MDL7 per-frame vertex overrides, without applying skinning
   until the final bone pose is available. Never accumulate transformations. */
#if GS3D_WITH_MDL7
static GS3D_Vec first_frame[GS3D_MAX_VERTS];
int gs3d_skeletal_sample_raw_vertices(unsigned int group,unsigned int fa,
                                     unsigned int fb,long alpha){
    unsigned long i,base,count;int r;GS3D_Group *g;
    if(group>=gs3d_model.groups)return GS3D_ERR_RANGE;
    g=&gs3d_model.group[group];base=g->vertex_first;count=g->vertex_count;
    if(count>GS3D_MAX_VERTS-base)return GS3D_ERR_CAPACITY;
    r=gs_mdl7_frame(group,fa);if(r)return r;
    if(alpha==0L||fa==fb)return GS3D_OK;
    for(i=0UL;i<count;i++)first_frame[i]=gs3d_model.v[base+i];
    r=gs_mdl7_frame(group,fb);if(r)return r;
    if(alpha==65536L)return GS3D_OK;
    for(i=0UL;i<count;i++){
        GS3D_Vec *v=&gs3d_model.v[base+i];const GS3D_Vec *a=&first_frame[i];
        v->x=gs_addsat(gs_qmul(a->x,65536L-alpha),gs_qmul(v->x,alpha));
        v->y=gs_addsat(gs_qmul(a->y,65536L-alpha),gs_qmul(v->y,alpha));
        v->z=gs_addsat(gs_qmul(a->z,65536L-alpha),gs_qmul(v->z,alpha));
        if(alpha<32768L)v->normal=a->normal;
        /* Retain the static skin attachment rather than treating vertindex
           in the frame override as a new bone number. */
    }
    return GS3D_OK;
}
#else
int gs3d_skeletal_sample_raw_vertices(unsigned int g,unsigned int a,unsigned int b,long t){
    (void)g;(void)a;(void)b;(void)t;return GS3D_ERR_UNSUPPORTED;
}
#endif
