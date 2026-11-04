#include "../gs3d_internal.h"
/* Experimental MDL7 layout: deformer records follow vertex-frame blocks
   within groupdata_size. Not confused with subsequent MED editor lumps.
   All record sizes checked BEFORE decoding any weight. */
int gs7_read_deformers(GS3D_Group *g,unsigned int group,unsigned long *cursor,
                       unsigned long group_end,unsigned int count,unsigned int maxweights){
    unsigned int i;unsigned long cur=*cursor,recend,e,w,ne,nw;
    unsigned char h[16],elem[28],entry[8];GS3D_DeformerInfo *di;
    int r;unsigned long bone,vertex;long q;
    if(group>=gs3d_model.groups||count>32U)
        return GS3D_ERR_CAPACITY;
    di=&gs_deformer_info[group];di->declared_deformers=(unsigned char)count;
    di->declared_max_weights=(unsigned char)maxweights;
    if(count==0U){
        di->kind=gs3d_model.bones?GS3D_DEFORM_RIGID_BONE:GS3D_DEFORM_NONE;
        return GS3D_OK;
    }
    if(maxweights>GS3D_MAX_WEIGHTS_PER_VERTEX)return GS3D_ERR_CAPACITY;
    if(!maxweights||!gs3d_model.bones)return GS3D_ERR_CORRUPT;
    for(i=0U;i<count;i++){
        if(cur>group_end||group_end-cur<16UL||gs_get(cur,h,16UL))return GS3D_ERR_RANGE;
        if(h[0]!=0U||h[1]!=0U)return GS3D_ERR_UNSUPPORTED;
        if(gs_u32(h+4)!=(unsigned long)group)return GS3D_ERR_CORRUPT;
        ne=gs_u32(h+8);
        if(ne>GS3D_MAX_BONES)return GS3D_ERR_CAPACITY;
        if(gs_u32(h+12)<16UL||gs_addmul(cur,1UL,gs_u32(h+12),&recend))return GS3D_ERR_CORRUPT;
        if(recend>group_end)return GS3D_ERR_RANGE;
        cur+=16UL;
        for(e=0UL;e<ne;e++){
            if(cur>recend||recend-cur<28UL||gs_get(cur,elem,28UL))return GS3D_ERR_RANGE;
            bone=gs_u32(elem);nw=gs_u32(elem+24);
            if(bone>=gs3d_model.bones||nw>g->vertex_count)return GS3D_ERR_CORRUPT;
            cur+=28UL;
            for(w=0UL;w<nw;w++){
                unsigned long bits;
                if(cur>recend||recend-cur<8UL||gs_get(cur,entry,8UL))return GS3D_ERR_RANGE;
                vertex=gs_u32(entry);bits=gs_u32(entry+4);
                /* finite IEEE754 binary32 in [0,1], reject NaN/Inf/negative */
                if(vertex>=g->vertex_count||bits>0x3F800000UL)return GS3D_ERR_CORRUPT;
                q=gs3d_ieee_q16(bits);
                r=gs_weights_append(g->vertex_first+vertex,(unsigned int)bone,q);
                if(r)return r;
                di->explicit_influences++;
                if(gs_vertex_weights[g->vertex_first+vertex].count>maxweights)return GS3D_ERR_CAPACITY;
                cur+=8UL;
            }
        }
        if(cur!=recend)return GS3D_ERR_UNSUPPORTED;
        di->parsed_deformers++;
    }
    *cursor=cur;di->kind=GS3D_DEFORM_WEIGHTED_BONE;
    return gs_weights_finish_group(group);
}
