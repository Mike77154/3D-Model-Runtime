#include "../gs3d_internal.h"
/* Two independent morph poses evaluated on their OWN clocks. Final vertex
   blending is valid here: legacy formats have no skeletal attachment. */
static GS3D_Vec gs_blend_source_vertices[GS3D_MAX_VERTS];
int gs3d_blend_morph(unsigned int group,const GS3D_Clip *a,
    const GS3D_ClipSample *sa,const GS3D_Clip *b,
    const GS3D_ClipSample *sb,long alpha){
    GS3D_Group *g;unsigned long i,base,count;long inv;int r;
    if(!a||!sa||!b||!sb||group>=gs3d_model.groups||
       alpha<0L||alpha>GS3D_Q16_ONE)return GS3D_ERR_RANGE;
    if(alpha==0L)return gs3d_vendor_sample_morph(a->generation,group,
        sa->frame_a,sa->frame_b,sa->alpha_q16);
    if(alpha==GS3D_Q16_ONE)return gs3d_vendor_sample_morph(b->generation,group,
        sb->frame_a,sb->frame_b,sb->alpha_q16);
    g=&gs3d_model.group[group];base=g->vertex_first;count=g->vertex_count;
    if(base>GS3D_MAX_VERTS||count>GS3D_MAX_VERTS-base)return GS3D_ERR_CAPACITY;
    r=gs3d_vendor_sample_morph(a->generation,group,
        sa->frame_a,sa->frame_b,sa->alpha_q16);if(r)return r;
    for(i=0UL;i<count;i++)gs_blend_source_vertices[i]=gs3d_model.v[base+i];
    r=gs3d_vendor_sample_morph(b->generation,group,
        sb->frame_a,sb->frame_b,sb->alpha_q16);if(r)return r;
    inv=GS3D_Q16_ONE-alpha;
    for(i=0UL;i<count;i++){
        GS3D_Vec *dst=&gs3d_model.v[base+i];
        const GS3D_Vec *old=&gs_blend_source_vertices[i];
        dst->x=gs_addsat(gs_qmul(old->x,inv),gs_qmul(dst->x,alpha));
        dst->y=gs_addsat(gs_qmul(old->y,inv),gs_qmul(dst->y,alpha));
        dst->z=gs_addsat(gs_qmul(old->z,inv),gs_qmul(dst->z,alpha));
        if(alpha<32768L)dst->normal=old->normal;
    }
    return GS3D_OK;
}
