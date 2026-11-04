#include "../include/gs3d.h"
#include <stdio.h>
#define ASSERT(x) do{if(!(x)){printf("PHASE06 FAIL at %d: %s\n",__LINE__,#x);return 1;}}while(0)
static int legacy_fraction(void){
    ASSERT(gs3d_open("tests/fixtures/mdl3_morph_fraction.mdl")==0);
    ASSERT(gs3d_vendor_sample_morph(4U,0U,0U,1U,32768L)==0);
    ASSERT(gs3d_model.v[1].x==5L*65536L);
    ASSERT(gs3d_vendor_sample_morph(4U,0U,0U,1U,16384L)==0);
    ASSERT(gs3d_model.v[1].x==3L*65536L);
    ASSERT(gs3d_vendor_sample_morph(4U,0U,0U,1U,65536L)==0);
    ASSERT(gs3d_model.v[1].x==9L*65536L);
    ASSERT(gs3d_vendor_sample_morph(4U,0U,0U,1U,32768L)==0);
    ASSERT(gs3d_model.v[1].x==5L*65536L); /* repeated sampling never accumulates */
    ASSERT(gs3d_vendor_sample_morph(4U,0U,0U,1U,-1L)==GS3D_ERR_RANGE);
    ASSERT(gs3d_vendor_sample_morph(5U,0U,0U,1U,32768L)==GS3D_ERR_UNSUPPORTED);
    return 0;
}
static int mdl7_fraction(void){
    ASSERT(gs3d_open("tests/fixtures/mdl7_morph_fraction.mdl")==0);
    ASSERT(gs3d_vendor_sample_morph(6U,0U,0U,1U,32768L)==0);
    ASSERT(gs3d_model.v[1].x==5L*65536L);
    ASSERT(gs3d_vendor_sample_morph(8U,0U,0U,1U,32768L)==0);
    ASSERT(gs3d_model.v[1].x==5L*65536L);
    ASSERT(gs3d_vendor_sample_morph(4U,0U,0U,1U,32768L)==GS3D_ERR_UNSUPPORTED);
    return 0;
}
static int multi_groups(void){
    GS3D_MaterialInfo mat;
    unsigned int i;
    ASSERT(gs3d_open("tests/fixtures/mdl7_eight_groups_26.mdl")==0);
    ASSERT(gs3d_model.groups==8U && gs3d_model.vertices==496UL && gs3d_model.faces==960UL);
    for(i=0U;i<8U;i++){
        ASSERT(gs3d_model.group[i].vertex_stride==26U);
        ASSERT(gs3d_model.group[i].vertex_count==62UL);
        ASSERT(gs3d_model.v[gs3d_model.group[i].vertex_first].z==(long)i*65536L);
        if(i==0U){ASSERT(gs3d_model.group[i].skin_count==3U);
                   ASSERT(gs3d_get_material(i,0U,&mat)==0 && mat.is_explicit==1U);}
        else ASSERT(gs3d_model.group[i].skin_count==0U);
    }
    return 0;
}
static int multi_weighted(void){
    GS3D_WeightSet w;GS3D_DeformerInfo d;
    unsigned int i;
    ASSERT(gs3d_open("tests/fixtures/mdl7_two_weighted_groups.mdl")==0);
    ASSERT(gs3d_model.groups==2U && gs3d_model.bones==2U);
    ASSERT(gs3d_model.vertices==6UL);
    for(i=0U;i<2U;i++){
        ASSERT(gs3d_get_deformer_info(i,&d)==0);
        ASSERT(d.kind==GS3D_DEFORM_WEIGHTED_BONE && d.weighted_vertices==1UL);
        ASSERT(gs3d_get_vertex_weights(gs3d_model.group[i].vertex_first+1UL,&w)==0);
        ASSERT(w.count==2U && w.item[0].weight_q16==32768L);
        ASSERT(gs3d_vendor_deform(8U,i,0U)==0);
        ASSERT(gs3d_model.v[gs3d_model.group[i].vertex_first+1UL].x==294912L);
    }
    ASSERT(gs3d_vendor_sample_morph(8U,0U,0U,0U,32768L)==GS3D_ERR_UNSUPPORTED);
    return 0;
}
int main(void){
    ASSERT(legacy_fraction()==0);
    ASSERT(mdl7_fraction()==0);
    ASSERT(multi_groups()==0);
    ASSERT(multi_weighted()==0);
    puts("PHASE06 PASS: fractional morphs, 8 groups (26-byte vertices), independent dual weighted groups");
    return 0;
}
