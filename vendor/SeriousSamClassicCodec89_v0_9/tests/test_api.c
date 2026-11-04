#include "ss89.h"
#include <stdio.h>

static SS89_Model model;
static SS89_Vertex frame[SS89_MAX_VERTICES];
#define CHECK(x) do {if(!(x)){fprintf(stderr,"failed line %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(int argc,char **argv)
{
    int rc;
    if(argc<2)return 2;
    rc=ss89_open(&model,argv[1]);CHECK(rc==SS89_OK);
    CHECK(model.vertices==4U);CHECK(model.frames==2U);
    CHECK(model.triangles_count==2U);CHECK(model.anim_count==2U);
    rc=ss89_decode_anim_key(&model,1U,1U,frame,SS89_MAX_VERTICES);
    CHECK(rc==SS89_OK);
    CHECK(frame[2].x==12*SS89_Q16_ONE);
    CHECK(frame[2].y==22*SS89_Q16_ONE);
    CHECK(frame[2].z==30*SS89_Q16_ONE);
    CHECK(model.triangles[0].v[0]==0U);
    CHECK(model.triangles[0].v[1]==1U);
    CHECK(model.triangles[0].v[2]==2U);
    CHECK(model.texvertices[2].u_mex==1024);
    CHECK(model.texvertices[2].v_mex==1024);
    CHECK(ss89_decode_frame(&model,99U,frame,SS89_MAX_VERTICES)==SS89_ERR_RANGE);
    CHECK(ss89_decode_frame(&model,0U,frame,1U)==SS89_ERR_RANGE);
    CHECK(ss89_select_mip(&model,9U)==SS89_ERR_RANGE);
    ss89_close(&model);
    puts("API test PASSED");return 0;
}
