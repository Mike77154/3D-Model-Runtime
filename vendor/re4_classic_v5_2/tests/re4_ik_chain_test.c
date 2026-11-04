/* IK discovery and constrained contact tests: C89, caller memory, no heap. */
#include "re4c3d/re4c3d.h"
#include <string.h>
#include <stdio.h>
#define CK(c) do {if(!(c)){fprintf(stderr,"IK chain test failure at %d\n",__LINE__);return 1;}}while(0)
static void pose_matrix(re4_skel_matrix *m,re4_q16 x,re4_q16 y,re4_q16 z)
{
    re4_u32 j;for(j=0;j<12U;++j)m->m[j]=0;
    m->m[0]=m->m[5]=m->m[10]=65536L;
    m->m[3]=x;m->m[7]=y;m->m[11]=z;
}
int main(void)
{
    re4_pmd_model model;re4_pmd_skeleton bones[4];re4_pmd_name names[4];
    re4_fcv clip;re4_fcv_block channels[3];re4_ik_chain chains[3];
    re4_skel_pose reference,animated;re4_skel_matrix rest[4],pose[4];
    re4_u32 n=0,valid=0,applied=0,skipped=0,i;re4_q16 before,after;
    const char *id[] = {"born__00","born__01","born__02","born__03"};
    memset(&model,0,sizeof(model));memset(&clip,0,sizeof(clip));
    memset(channels,0,sizeof(channels));memset(bones,0,sizeof(bones));
    for(i=0;i<4U;++i){bones[i].parent=(re4_i32)i-1;names[i].name.ptr=id[i];names[i].name.len=8UL;}
    model.skeleton_count=4;model.skeletons=bones;model.skeleton_names=names;model.skeleton_name_count=4;
    channels[0].bone_type=0x10;channels[0].bone_no=0;channels[0].data_type=4; /* Z+ */
    channels[1].bone_type=0x20;channels[1].bone_no=2;channels[1].data_type=0; /* Y+, missing 2 links */
    channels[2].bone_type=2;channels[2].bone_no=1;
    clip.blocks=channels;clip.block_count=3;
    CK(re4_ik_discover(&model,&clip,chains,3,&n,&valid)==RE4_OK);
    CK(n==2U && valid==1U);
    CK(chains[0].root==0 && chains[0].joint==1 && chains[0].effector==2);
    CK(chains[0].topology_ok && !chains[1].topology_ok);
    CK(re4_ik_discover(&model,&clip,chains,1,&n,&valid)==RE4_ERR_NO_SPACE);
    pose_matrix(&rest[0],0,0,0);pose_matrix(&rest[1],65536,0,0);
    pose_matrix(&rest[2],65536,65536,0);pose_matrix(&rest[3],65536,131072,0);
    pose_matrix(&pose[0],0,0,0);pose_matrix(&pose[1],65536,0,0);
    pose_matrix(&pose[2],131072,0,0);pose_matrix(&pose[3],196608,0,0);
    reference.world=rest;reference.bone_count=4;
    animated.world=pose;animated.bone_count=4;
    before=(pose[2].m[3]-rest[2].m[3])+(rest[2].m[7]-pose[2].m[7]);
    CK(re4_ik_lock_contacts(&model,chains,2,&reference,&animated,64,13000,&applied,&skipped)==RE4_OK);
    after=pose[2].m[3]-rest[2].m[3];if(after<0)after=-after;
    {re4_q16 dy=pose[2].m[7]-rest[2].m[7];if(dy<0)dy=-dy;after+=dy;}
    CK(applied==1U && skipped==1U);
    CK(after<before);
    CK(re4_skel_ik_ccd_axis(&model,&animated,0,1,2,rest[2].m+3,1,10000,3,0)==RE4_ERR_BAD_FORMAT);
    printf("IK chain scan + constrained contact OK before=%ld after=%ld\n",(long)before,(long)after);
    return 0;
}
