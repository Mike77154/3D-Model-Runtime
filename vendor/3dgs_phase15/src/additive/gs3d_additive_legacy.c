#include "../gs3d_internal.h"
/* MDL3/4/5 reference-relative per-vertex morphs. No bones, weights, or heap.
   Each source point decoded directly from original file; base is the only
   frame actually placed in gs3d_model.v, and then an additive delta follows. */
#if GS3D_WITH_LEGACY
static long sub(long a,long b){
    if(b==(-2147483647L-1L))return a>=0L?2147483647L:gs_addsat(a,2147483647L);
    return gs_addsat(a,-b);
}
static int sample_vertex(const GS3D_Group *g,unsigned long vertex,
                         const GS3D_ClipSample *sample,GS3D_Vec *out){
    GS3D_Vec a,b;GS3D_Frame const *f,*h;int r;long alpha;
    if(sample->frame_a>=g->frame_count||sample->frame_b>=g->frame_count)
        return GS3D_ERR_RANGE;
    f=&g->frames[sample->frame_a];h=&g->frames[sample->frame_b];
    r=gs_decode_vertex_legacy(f->offset+vertex*f->packed_stride,
                              f->packed_stride,&a);if(r)return r;
    if(sample->alpha_q16==0L||sample->frame_a==sample->frame_b){*out=a;return GS3D_OK;}
    r=gs_decode_vertex_legacy(h->offset+vertex*h->packed_stride,
                              h->packed_stride,&b);if(r)return r;
    alpha=sample->alpha_q16;
    out->x=gs_addsat(gs_qmul(a.x,65536L-alpha),gs_qmul(b.x,alpha));
    out->y=gs_addsat(gs_qmul(a.y,65536L-alpha),gs_qmul(b.y,alpha));
    out->z=gs_addsat(gs_qmul(a.z,65536L-alpha),gs_qmul(b.z,alpha));
    out->bone=65535U;out->normal=alpha<32768L?a.normal:b.normal;
    return GS3D_OK;
}
int gs3d_additive_legacy(unsigned int group,const GS3D_AdditiveRequest *req){
    const GS3D_Clip *c[3];GS3D_ClipSample v[3];GS3D_Group *g;
    unsigned long i;long w;GS3D_Vec add,ref,*base;unsigned int j;int r;
    if(!req||!gs_is_open||group>=gs3d_model.groups)return GS3D_ERR_RANGE;
    if(gs3d_model.format<3||gs3d_model.format>5)return GS3D_ERR_UNSUPPORTED;
    if(req->strength_q16<0L||req->strength_q16>65536L)return GS3D_ERR_RANGE;
    c[0]=&req->base;c[1]=&req->additive;c[2]=&req->reference;
    g=&gs3d_model.group[group];
    for(j=0U;j<3U;j++){
        if(c[j]->group!=group||c[j]->generation!=c[0]->generation||
           c[j]->track!=GS3D_CLIP_TRACK_MORPH||c[j]->mode>GS3D_CLIP_PINGPONG||
           c[j]->count==0U||c[j]->ticks_per_frame==0UL||
           (unsigned long)c[j]->first+c[j]->count>g->frame_count)
            return GS3D_ERR_RANGE;
    }
    if(!gs3d_family_supports(req->base.generation,gs3d_model.format))
        return GS3D_ERR_UNSUPPORTED;
    r=gs3d_clip_resolve(c[0],req->base_ticks,&v[0]);if(r)return r;
    r=gs3d_clip_resolve(c[1],req->additive_ticks,&v[1]);if(r)return r;
    r=gs3d_clip_resolve(c[2],req->reference_ticks,&v[2]);if(r)return r;
    /* Apply base (morph sampler has one static frame scratch). */
    r=gs3d_sample_morph_q16(group,v[0].frame_a,v[0].frame_b,v[0].alpha_q16);
    if(r)return r;
    w=req->strength_q16;
    for(i=0UL;i<g->vertex_count;i++){
        r=sample_vertex(g,i,&v[1],&add);if(r)return r;
        r=sample_vertex(g,i,&v[2],&ref);if(r)return r;
        base=&gs3d_model.v[g->vertex_first+i];
        base->x=gs_addsat(base->x,gs_qmul(sub(add.x,ref.x),w));
        base->y=gs_addsat(base->y,gs_qmul(sub(add.y,ref.y),w));
        base->z=gs_addsat(base->z,gs_qmul(sub(add.z,ref.z),w));
    }
    return GS3D_OK;
}
#else
int gs3d_additive_legacy(unsigned int group,const GS3D_AdditiveRequest *req){
    (void)group;(void)req;return GS3D_ERR_UNSUPPORTED;
}
#endif
