/* Regression for real PMDs exceeding previous 128-joint limitation. */
#include "re4c3d/re4c3d.h"
#include <string.h>
#include <stdio.h>
int main(void)
{
    static re4_pmd_skeleton b[200];
    static re4_skel_matrix world[RE4_SKEL_MAX_BONES];
    re4_skel_pose pose;re4_pmd_model model;re4_span empty;
    re4_u32 i,j;
    memset(&model,0,sizeof(model));memset(&pose,0,sizeof(pose));
    for(i=0;i<200U;++i){
        b[i].parent=(i? (re4_i32)(i-1U): -1);
        for(j=0;j<26U;++j)b[i].data[j]=0;
        b[i].data[6]=65536;
        b[i].data[7]=65536;
    }
    model.skeleton_count=200U;model.skeletons=b;
    pose.world=world;empty.data=0;empty.size=0;
    if(re4_skel_evaluate(&model,empty,0,0,&pose,RE4_SKEL_MAX_BONES)!=RE4_OK){
        fputs("200 joint pose failed\n",stderr);return 1;
    }
    if(pose.bone_count!=200U||pose.world[199].m[3]!=200L*65536L){
        fputs("200 joint hierarchy mismatch\n",stderr);return 1;
    }
    puts("200-joint C89 FK regression OK");return 0;
}
