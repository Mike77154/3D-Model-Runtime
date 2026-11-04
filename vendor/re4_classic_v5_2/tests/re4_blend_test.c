/* RE4 local FCV crossfade endpoints, midpoint, malformed weights, root blend.
 * C89 without dynamic allocations or floating point. */
#include "re4c3d/re4c3d.h"
#include <string.h>
#include <stdio.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"blend test failed line %d\n",__LINE__);return 1;}}while(0)
static void p16(re4_u8 *p,re4_u16 v){p[0]=(re4_u8)v;p[1]=(re4_u8)(v>>8);}
static void p32(re4_u8 *p,re4_u32 v){p[0]=(re4_u8)v;p[1]=(re4_u8)(v>>8);p[2]=(re4_u8)(v>>16);p[3]=(re4_u8)(v>>24);}
static void make_fcv(re4_u8 bytes[120],re4_u32 last_z)
{
    re4_u32 k=16;memset(bytes,0,120U);
    p16(bytes,10U);bytes[2]=1U;bytes[3]=2U;
    p32(bytes+8U,120U);p32(bytes+12U,16U);
    p16(bytes+k,2U);k+=2U;p16(bytes+k,0U);p16(bytes+k+2U,10U);k+=4U;
    k+=24U;
    p16(bytes+k,2U);k+=2U;p16(bytes+k,0U);p16(bytes+k+2U,10U);k+=4U;
    k+=24U;
    p16(bytes+k,2U);k+=2U;p16(bytes+k,0U);p16(bytes+k+2U,10U);k+=4U;
    k+=12U;p32(bytes+k,last_z);
}
static void makebone(re4_pmd_skeleton *bone)
{
    memset(bone,0,sizeof(*bone));bone->parent=-1;
    bone->data[0]=bone->data[1]=bone->data[2]=65536;
    bone->data[6]=65536;
}
int main(void)
{
    re4_u8 raw_a[120],raw_b[120],mem[8192];
    re4_span sa,sb;re4_arena ar;re4_fcv ca,cb;
    re4_pmd_model model;re4_pmd_skeleton bone;re4_pmd_name name;
    re4_skel_matrix mat_a[1],mat_b[1],mat_mix[1],mat_old[1];
    re4_skel_pose pa,pb,pm,po;re4_q16 root[3];
    re4_fcv fa,fb;re4_fcv_block ba,bb;re4_root_track ta,tb;
    re4_u8 roots_a[90],roots_b[90];re4_span ra,rb;
    re4_u32 j;
    make_fcv(raw_a,0x3f800000UL);make_fcv(raw_b,0x40000000UL);
    sa.data=raw_a;sa.size=120;sb.data=raw_b;sb.size=120;
    re4_arena_init(&ar,mem,sizeof(mem));
    CHECK(re4_fcv_read(sa,RE4_ENDIAN_LITTLE,&ar,&ca)==RE4_OK);
    CHECK(re4_fcv_read(sb,RE4_ENDIAN_LITTLE,&ar,&cb)==RE4_OK);
    CHECK(re4_fcv_validate(sa,&ca)==RE4_OK);
    CHECK(re4_fcv_validate(sb,&cb)==RE4_OK);
    memset(&model,0,sizeof(model));makebone(&bone);
    name.name.ptr="born__00";name.name.len=8UL;
    model.skeleton_count=1;model.skeletons=&bone;
    model.skeleton_name_count=1;model.skeleton_names=&name;
    memset(&pa,0,sizeof(pa));memset(&pb,0,sizeof(pb));
    memset(&pm,0,sizeof(pm));memset(&po,0,sizeof(po));
    pa.world=mat_a;pb.world=mat_b;pm.world=mat_mix;po.world=mat_old;
    CHECK(re4_skel_evaluate(&model,sa,&ca,5U,&pa,1U)==RE4_OK);
    CHECK(re4_skel_evaluate(&model,sb,&cb,5U,&pb,1U)==RE4_OK);
    CHECK(re4_skel_evaluate_blend(&model,sa,&ca,5U,sb,&cb,5U,0,&po,1U)==RE4_OK);
    CHECK(!memcmp(&mat_a[0],&mat_old[0],sizeof(mat_a[0])));
    CHECK(re4_skel_evaluate_blend(&model,sa,&ca,5U,sb,&cb,5U,65536L,&po,1U)==RE4_OK);
    CHECK(!memcmp(&mat_b[0],&mat_old[0],sizeof(mat_b[0])));
    CHECK(re4_skel_evaluate_blend(&model,sa,&ca,5U,sb,&cb,5U,32768L,&pm,1U)==RE4_OK);
    CHECK(pm.applied_rotation_channels==2UL);
    /* midpoint rotation must be between the two angles (0.5 and 1 rad) */
    CHECK(mat_mix[0].m[4]>mat_a[0].m[4] && mat_mix[0].m[4]<mat_b[0].m[4]);
    CHECK(mat_mix[0].m[0]<mat_a[0].m[0] && mat_mix[0].m[0]>mat_b[0].m[0]);
    CHECK(re4_skel_evaluate_blend(&model,sa,&ca,5U,sb,&cb,5U,-1,&pm,1U)==RE4_ERR_BAD_FORMAT);
    CHECK(re4_skel_evaluate_blend(&model,sa,&ca,5U,sb,&cb,5U,65537L,&pm,1U)==RE4_ERR_BAD_FORMAT);
    /* Root tracks with x distance of 1 vs 3. Mid blend must be 2. */
    memset(roots_a,0,sizeof(roots_a));memset(roots_b,0,sizeof(roots_b));
    for(j=0;j<3U;++j){
        re4_u32 start=j*30U;
        p16(roots_a+start,2U);p16(roots_a+start+2U,0U);p16(roots_a+start+4U,10U);
        p16(roots_b+start,2U);p16(roots_b+start+2U,0U);p16(roots_b+start+4U,10U);
    }
    p32(roots_a+18,0x3f800000UL);
    p32(roots_b+18,0x40400000UL);
    ra.data=roots_a;ra.size=90;rb.data=roots_b;rb.size=90;
    memset(&fa,0,sizeof(fa));memset(&fb,0,sizeof(fb));
    memset(&ba,0,sizeof(ba));memset(&bb,0,sizeof(bb));
    ba.bone_type=1;ba.bone_no=0;bb=ba;
    fa.block_count=1;fa.blocks=&ba;fa.endian=RE4_ENDIAN_LITTLE;
    fb.block_count=1;fb.blocks=&bb;fb.endian=RE4_ENDIAN_LITTLE;
    CHECK(re4_root_find(&fa,&ta)==RE4_OK);
    CHECK(re4_root_find(&fb,&tb)==RE4_OK);
    CHECK(re4_root_blend_relative(ra,&fa,&ta,0,10,rb,&fb,&tb,0,10,32768,root)==RE4_OK);
    CHECK(root[0]==131072 && root[1]==0 && root[2]==0);
    CHECK(re4_root_blend_relative(ra,&fa,&ta,0,10,rb,&fb,&tb,0,10,0,root)==RE4_OK);
    CHECK(root[0]==65536);
    CHECK(re4_root_blend_relative(ra,&fa,&ta,0,10,rb,&fb,&tb,0,10,65536,root)==RE4_OK);
    CHECK(root[0]==196608);
    puts("v2.9 local crossfade and root mix OK");return 0;
}
