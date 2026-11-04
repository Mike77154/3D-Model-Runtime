#include "../src/gs3d_internal.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if(!(x)){printf("POSE FAIL line %d\n",__LINE__);return 1;} }while(0)
static int test_math(void){
    GS3D_Mat a,b,c;GS3D_Vec v,w;
    CHECK(gs_qmul(65536L,98304L)==98304L);
    CHECK(gs_qmul(-98304L,131072L)==-196608L);
    CHECK(gs_qmul(-2147483647L-1L,65536L)==(-2147483647L-1L));
    CHECK(gs_qmul(2147483647L,2147483647L)==2147483647L);
    gs_mat_translate(&a,131072L,196608L,262144L);
    gs_mat_translate(&b,-65536L,65536L,-131072L);
    gs_mat_mul(&c,&a,&b);
    CHECK(c.m[12]==65536L && c.m[13]==262144L && c.m[14]==131072L);
    memset(&v,0,sizeof(v));v.x=65536L;v.y=131072L;v.z=196608L;
    gs_mat_point(&c,&v,&w);
    CHECK(w.x==131072L&&w.y==393216L&&w.z==327680L);
    return 0;
}
static int test_animated(void){
    int r;GS3D_Vec v,w;unsigned int g;
    CHECK(gs3d_open("tests/fixtures/mdl7_bone_keys.mdl")==0);
    CHECK(gs3d_find_frame(0U,"walk")==0);
    CHECK(gs3d_find_frame(0U,"missing")==-1);
    CHECK(gs3d_build_pose(0U,0U)==0);
    CHECK(gs3d_skin_matrix[0].m[12]==196608L);
    CHECK(gs3d_skin_matrix[0].m[13]==262144L);
    CHECK(gs3d_skin_matrix[0].m[14]==327680L);
    for(g=0U;g<gs3d_model.groups;g++){
        CHECK(gs3d_apply_pose(g,0U)==0);
        v=gs3d_model.v[gs3d_model.group[g].vertex_first+1UL];
        /* frame vertex override (2,1,0) preserves attachment to bone 0 */
        CHECK(v.bone==0U && v.x==327680L && v.y==327680L && v.z==327680L);
        CHECK(gs3d_apply_pose(g,0U)==0); /* idempotence, no cumulative drift */
        v=gs3d_model.v[gs3d_model.group[g].vertex_first+1UL];
        CHECK(v.x==327680L && v.y==327680L);
    }
    gs3d_close();
    CHECK(gs3d_open("tests/fixtures/mdl7_hierarchy.mdl")==0);
    CHECK(gs3d_model.bones==2U);
    CHECK(gs3d_find_frame(0U,"pose")==0);
    CHECK(gs3d_apply_pose(0U,0U)==0);
    v=gs3d_model.v[1];CHECK(v.bone==1U);
    CHECK(v.x==262144L && v.y==327680L && v.z==327680L);
    CHECK(gs3d_transform_point(1U,&v,&w)==0); /* interface available */
    CHECK(gs3d_apply_pose(0U,0U)==0); /* no double skinning */
    v=gs3d_model.v[1];CHECK(v.x==262144L && v.y==327680L);
    gs3d_close();
    r=gs3d_open("tests/fixtures/mdl7_bad_deformer_header.mdl");
    CHECK(r!=GS3D_OK);
    CHECK(gs3d_transform_point(1U,&v,&w)==GS3D_ERR_RANGE);
    return 0;
}
int main(void){
    CHECK(test_math()==0);CHECK(test_animated()==0);
    puts("POSE TESTS PASSED: Q16 arithmetic, key matrices, joint hierarchy, frame overrides, 2 groups, idempotence, reject malformed deformer data");
    return 0;
}
