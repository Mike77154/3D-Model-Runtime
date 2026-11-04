#include "../../include/gs3d_vtk_bridge.h"
#include <string.h>
int gs3d_vtk_group(unsigned int group,unsigned long *nv,unsigned long *nf,
                   unsigned long *nuv,unsigned int *skins,unsigned int *frames){
    const GS3D_Group *g;
    if(group>=gs3d_model.groups)return GS3D_ERR_RANGE;
    g=&gs3d_model.group[group];
    if(nv)*nv=g->vertex_count;
    if(nf)*nf=g->face_count;
    if(nuv)*nuv=g->uv_count;
    if(skins)*skins=g->skin_count;
    if(frames)*frames=g->frame_count;
    return GS3D_OK;
}
int gs3d_vtk_vertex(unsigned int group,unsigned long index,long xyz[3],unsigned short *bone){
    const GS3D_Group *g;const GS3D_Vec *v;
    if(!xyz||group>=gs3d_model.groups)return GS3D_ERR_RANGE;
    g=&gs3d_model.group[group];if(index>=g->vertex_count)return GS3D_ERR_RANGE;
    v=&gs3d_model.v[g->vertex_first+index];
    xyz[0]=v->x;xyz[1]=v->y;xyz[2]=v->z;
    if(bone)*bone=v->bone;
    return GS3D_OK;
}
int gs3d_vtk_uv(unsigned int group,unsigned long index,long uv[2]){
    const GS3D_Group *g;const GS3D_UV *p;
    if(!uv||group>=gs3d_model.groups)return GS3D_ERR_RANGE;
    g=&gs3d_model.group[group];if(index>=g->uv_count)return GS3D_ERR_RANGE;
    p=&gs3d_model.uv[g->uv_first+index];uv[0]=p->u;uv[1]=p->v;
    return GS3D_OK;
}
int gs3d_vtk_triangle(unsigned int group,unsigned long index,
                       unsigned long vi[3],unsigned long ti[3],unsigned int *material){
    const GS3D_Group *g;const GS3D_Face *f;unsigned int k;
    if(!vi||!ti||group>=gs3d_model.groups)return GS3D_ERR_RANGE;
    g=&gs3d_model.group[group];if(index>=g->face_count)return GS3D_ERR_RANGE;
    f=&gs3d_model.face[g->face_first+index];
    for(k=0U;k<3U;k++){
        if(f->vi[k]>=g->vertex_count)return GS3D_ERR_CORRUPT;
        if(g->uv_count&&f->ti[k]>=g->uv_count)return GS3D_ERR_CORRUPT;
        vi[k]=f->vi[k];ti[k]=g->uv_count?f->ti[k]:0UL;
    }
    if(material)*material=f->material;
    return GS3D_OK;
}
int gs3d_vtk_skin(unsigned int group,unsigned int skin,unsigned long *w,
                   unsigned long *h,const unsigned char **rgba){
    int r;if(!rgba)return GS3D_ERR_RANGE;
    *rgba=(const unsigned char *)0;
    r=gs3d_decode_skin(group,skin,w,h);if(r)return r;
    *rgba=gs3d_rgba;return GS3D_OK;
}
int gs3d_vtk_bone(unsigned int bone,char name[33],
                  unsigned short *parent,long xyz[3]){
    const GS3D_Bone *p;
    if(bone>=gs3d_model.bones)return GS3D_ERR_RANGE;
    p=&gs3d_model.bone[bone];
    if(name){memcpy(name,p->name,33U);name[32]='\0';}
    if(parent)*parent=p->parent;
    if(xyz){xyz[0]=p->xyz[0];xyz[1]=p->xyz[1];xyz[2]=p->xyz[2];}
    return GS3D_OK;
}
int gs3d_vtk_weights(unsigned int group,unsigned long local,
                     unsigned short *bones,long *weights,
                     unsigned int capacity,unsigned int *count){
    unsigned int i;GS3D_WeightSet w;int r;const GS3D_Group *g;
    if(group>=gs3d_model.groups||!count)return GS3D_ERR_RANGE;
    g=&gs3d_model.group[group];if(local>=g->vertex_count)return GS3D_ERR_RANGE;
    r=gs3d_get_vertex_weights(g->vertex_first+local,&w);if(r)return r;
    *count=w.count;
    if(capacity<w.count)return GS3D_ERR_CAPACITY;
    if(w.count&&(!bones||!weights))return GS3D_ERR_RANGE;
    for(i=0U;i<w.count;i++){bones[i]=w.item[i].bone;weights[i]=w.item[i].weight_q16;}
    return GS3D_OK;
}
int gs3d_vtk_frame_name(unsigned int group,unsigned int frame,char out[17]){
    if(!out||group>=gs3d_model.groups)return GS3D_ERR_RANGE;
    if(frame>=gs3d_model.group[group].frame_count)return GS3D_ERR_RANGE;
    memcpy(out,gs3d_model.group[group].frames[frame].name,17U);
    out[16]='\0';return GS3D_OK;
}
int gs3d_vtk_audit(unsigned int group,unsigned long *degenerate,
                   unsigned long *weighted,unsigned long *influences){
    const GS3D_Group *g;unsigned long i,deg=0UL,wt=0UL,n=0UL;
    GS3D_Face *f;GS3D_WeightSet w;int r;
    if(group>=gs3d_model.groups)return GS3D_ERR_RANGE;
    g=&gs3d_model.group[group];
    for(i=0UL;i<g->face_count;i++){
        f=&gs3d_model.face[g->face_first+i];
        if(f->vi[0]>=g->vertex_count||f->vi[1]>=g->vertex_count||
           f->vi[2]>=g->vertex_count)return GS3D_ERR_CORRUPT;
        if(f->vi[0]==f->vi[1]||f->vi[0]==f->vi[2]||f->vi[1]==f->vi[2])deg++;
    }
    for(i=0UL;i<g->vertex_count;i++){
        r=gs3d_get_vertex_weights(g->vertex_first+i,&w);if(r)return r;
        if(w.kind==GS3D_DEFORM_WEIGHTED_BONE&&w.count){wt++;n+=w.count;}
    }
    if(degenerate)*degenerate=deg;
    if(weighted)*weighted=wt;
    if(influences)*influences=n;
    return GS3D_OK;
}

int gs3d_vtk_skin_info(unsigned int group,unsigned int skin,GS3D_TextureInfo *out){
    return gs3d_texture_info(group,skin,out);
}
