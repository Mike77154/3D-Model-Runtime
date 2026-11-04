/* Phase12: optional second UV set and post-group MDL7 deformers. ISO C89. */
#include "../include/gs3d.h"
#include <stdio.h>
#define CK(x) do { if(!(x)) {fprintf(stderr,"FAIL phase12 line %d\n",__LINE__);return 1;} }while(0)
int main(void){
    GS3D_WeightSet weights;
    GS3D_DeformerInfo inf;
    CK(gs3d_open("tests/fixtures/mdl7_unused_second_uv.mdl")==GS3D_OK);
    CK(gs3d_model.groups==1U && gs3d_model.group[0].uv_count==3UL);
    gs3d_close();
    CK(gs3d_open("tests/fixtures/mdl7_bad_second_uv.mdl")==GS3D_ERR_CORRUPT);
    CK(gs3d_open("tests/fixtures/mdl7_postgroup_weighted.mdl")==GS3D_OK);
    CK(gs3d_get_deformer_info(0U,&inf)==GS3D_OK);
    CK(inf.parsed_deformers==1U && inf.explicit_influences==2UL);
    CK(gs3d_get_vertex_weights(1UL,&weights)==GS3D_OK);
    CK(weights.count==2U);
    gs3d_close();
    CK(gs3d_open("tests/fixtures/mdl7_weighted_valid.mdl")==GS3D_OK);
    gs3d_close();
    puts("PHASE12 PASS: unused UV2, rejected invalid UV2, post-group weights, legacy in-group weights");
    return 0;
}
