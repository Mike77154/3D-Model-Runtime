#include "../gs3d_internal.h"
/* Two-pose vertex morph sampler, deliberately separate from skeletal skinning.
   C89/Q16.16, one static scratch vertex per input vertex, no heap. */
static GS3D_Vec gs_morph_first[GS3D_MAX_VERTS];

int gs3d_sample_morph_q16(unsigned int group,unsigned int frame_a,
                           unsigned int frame_b,long alpha_q16){
    GS3D_Group *g;unsigned long i,base,count;int r;
    if(!gs_is_open||group>=gs3d_model.groups)return GS3D_ERR_RANGE;
    g=&gs3d_model.group[group];
    if(frame_a>=g->frame_count||frame_b>=g->frame_count||
       alpha_q16<0L||alpha_q16>GS3D_Q16_ONE)return GS3D_ERR_RANGE;
    /* Morph-only API must never silently drop skeletal transforms. */
    if(gs3d_model.format==7 &&
       (g->frames[frame_a].trans_count||g->frames[frame_b].trans_count||
        gs_deformer_info[group].kind==GS3D_DEFORM_WEIGHTED_BONE))
        return GS3D_ERR_UNSUPPORTED;
    if(alpha_q16==0L||frame_a==frame_b)return gs3d_apply_frame(group,frame_a);
    if(alpha_q16==GS3D_Q16_ONE)return gs3d_apply_frame(group,frame_b);
    base=g->vertex_first;count=g->vertex_count;
    if(count>GS3D_MAX_VERTS-base)return GS3D_ERR_CAPACITY;
    r=gs3d_apply_frame(group,frame_a);if(r)return r;
    for(i=0UL;i<count;i++)gs_morph_first[i]=gs3d_model.v[base+i];
    r=gs3d_apply_frame(group,frame_b);if(r)return r;
    for(i=0UL;i<count;i++){
        GS3D_Vec *v=&gs3d_model.v[base+i];
        long b=GS3D_Q16_ONE-alpha_q16;
        GS3D_Vec *a=&gs_morph_first[i];
        v->x=gs_addsat(gs_qmul(a->x,b),gs_qmul(v->x,alpha_q16));
        v->y=gs_addsat(gs_qmul(a->y,b),gs_qmul(v->y,alpha_q16));
        v->z=gs_addsat(gs_qmul(a->z,b),gs_qmul(v->z,alpha_q16));
        /* Packed normal selector uses nearest endpoint, no false quaternion interpolation. */
        if(alpha_q16<32768L)v->normal=a->normal;
    }
    return GS3D_OK;
}
