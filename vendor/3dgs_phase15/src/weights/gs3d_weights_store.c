#include "../gs3d_internal.h"
GS3D_WeightSet gs_vertex_weights[GS3D_MAX_VERTS];
GS3D_DeformerInfo gs_deformer_info[GS3D_MAX_GROUPS];
void gs_weights_reset(void){
    memset(gs_vertex_weights,0,sizeof(gs_vertex_weights));
    memset(gs_deformer_info,0,sizeof(gs_deformer_info));
}
int gs_weights_append(unsigned long idx,unsigned int bone,long weight){
    GS3D_WeightSet *w;unsigned int i;
    if(idx>=gs3d_model.vertices||bone>=gs3d_model.bones||weight<0L||weight>65536L)return GS3D_ERR_CORRUPT;
    w=&gs_vertex_weights[idx];
    if(w->count>=GS3D_MAX_WEIGHTS_PER_VERTEX)return GS3D_ERR_CAPACITY;
    for(i=0U;i<w->count;i++)if(w->item[i].bone==bone)return GS3D_ERR_CORRUPT;
    w->item[w->count].bone=(unsigned short)bone;
    w->item[w->count].weight_q16=weight;
    w->count++;
    w->kind=GS3D_DEFORM_WEIGHTED_BONE;
    return GS3D_OK;
}
/* Divide a Q16.16 numerator by an integer denominator without 64-bit types. */
static long ratio_q16(unsigned long num,unsigned long den){
    unsigned long rem=num,frac=0UL;unsigned int i;
    if(!den)return 0L;
    if(num>=den)return 65536L;
    for(i=0U;i<16U;i++){
        rem*=2UL;frac*=2UL;
        if(rem>=den){rem-=den;frac++;}
    }
    return (long)frac;
}
int gs_weights_finish_group(unsigned int group){
    GS3D_Group *g;unsigned long v;unsigned int j,largest;unsigned long sum;
    long nsum;GS3D_WeightSet *w;GS3D_DeformerInfo *di;
    if(group>=gs3d_model.groups)return GS3D_ERR_RANGE;
    g=&gs3d_model.group[group];di=&gs_deformer_info[group];
    for(v=g->vertex_first;v<g->vertex_first+g->vertex_count;v++){
        w=&gs_vertex_weights[v];if(!w->count)continue;
        sum=0UL;largest=0U;
        for(j=0U;j<w->count;j++){
            if(w->item[j].weight_q16>w->item[largest].weight_q16)largest=j;
            sum+=(unsigned long)w->item[j].weight_q16;
        }
        if(!sum || sum>65536UL+1024UL)return GS3D_ERR_CORRUPT;
        nsum=0L;
        for(j=0U;j<w->count;j++){
            long norm=ratio_q16((unsigned long)w->item[j].weight_q16,sum);
            w->item[j].weight_q16=norm;nsum+=norm;
        }
        /* Assign the rounding remainder to the largest input influence. */
        w->item[largest].weight_q16+=65536L-nsum;
        di->weighted_vertices++;
    }
    return GS3D_OK;
}
