/* Keyframe preservation, endian-specific FCV reads, optional CCD IK test.
   All synthetic data. Portable C89, no heap and no libm. */
#include "re4c3d/re4c3d.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"motion catalog test failure on line %d\n",__LINE__);return 1;}}while(0)
static void u16be(re4_u8 *p,re4_u16 v){p[0]=(re4_u8)(v>>8);p[1]=(re4_u8)v;}
static void u32be(re4_u8 *p,re4_u32 v){p[0]=(re4_u8)(v>>24);p[1]=(re4_u8)(v>>16);p[2]=(re4_u8)(v>>8);p[3]=(re4_u8)v;}
static void u32le(re4_u8 *p,re4_u32 v){p[0]=(re4_u8)v;p[1]=(re4_u8)(v>>8);p[2]=(re4_u8)(v>>16);p[3]=(re4_u8)(v>>24);}
static void id(re4_skel_matrix *m,re4_q16 x,re4_q16 y,re4_q16 z)
{
 re4_u32 i;for(i=0;i<12U;++i)m->m[i]=0;m->m[0]=m->m[5]=m->m[10]=65536L;
 m->m[3]=x;m->m[7]=y;m->m[11]=z;
}
int main(void)
{
 re4_u8 fcvbuf[128],memory[8192],bits[8192];re4_u32 off,j,n=0;
 re4_span sp;re4_arena ar;re4_fcv f;re4_fcv_axis_view x,y,z;
 re4_q16 triple[3];re4_u16 t;re4_err err;
 re4_pmd_model model;re4_pmd_skeleton sk[3];re4_skel_matrix wm[3];
 re4_skel_pose pose;re4_q16 target[3],dist;
 memset(fcvbuf,0,sizeof(fcvbuf));
 /* one big-endian 0x80 FCV with 1:4:4 encoded axis keys */
 u16be(fcvbuf,12);fcvbuf[2]=1;fcvbuf[3]=4;fcvbuf[4]=0x80;fcvbuf[5]=0;
 u32le(fcvbuf+8,128);u32be(fcvbuf+12,16);
 off=16;
 for(j=0;j<3;++j){
  u16be(fcvbuf+off,2);off+=2;
  u16be(fcvbuf+off,0);u16be(fcvbuf+off+2,12);off+=4;
  fcvbuf[off]=0;off+=9;
  fcvbuf[off]=1;off+=9;
 }
 sp.data=fcvbuf;sp.size=sizeof(fcvbuf);
 re4_arena_init(&ar,memory,sizeof(memory));
 CHECK(re4_fcv_read(sp,RE4_ENDIAN_BIG,&ar,&f)==RE4_OK);
 CHECK(re4_fcv_validate(sp,&f)==RE4_OK);
 CHECK(re4_fcv_block_axis_views_e(sp,&f.blocks[0],RE4_ENDIAN_BIG,&x,&y,&z)==RE4_OK);
 CHECK(re4_fcv_axis_time_at_e(&z,1,RE4_ENDIAN_BIG,&t)==RE4_OK && t==12U);
 CHECK(re4_fcv_axis_key_q16(&x,1,RE4_ENDIAN_BIG,triple)==RE4_OK);
 CHECK(re4_fcv_keyframe_union(sp,&f,bits,sizeof(bits),&n)==RE4_OK && n==2U);
 CHECK((bits[0]&1U) && (bits[1]&(1U<<4)));
 CHECK(re4_fcv_block_sample_hermite_q16(sp,&f.blocks[0],6,RE4_ENDIAN_BIG,triple)==RE4_OK);
 /* Test IK joint ancestry and reduction of tip-target L1 error. */
 memset(&model,0,sizeof(model));memset(sk,0,sizeof(sk));
 sk[0].parent=-1;sk[1].parent=0;sk[2].parent=1;
 model.skeleton_count=3;model.skeletons=sk;
 id(&wm[0],0,0,0);id(&wm[1],65536,0,0);id(&wm[2],131072,0,0);
 memset(&pose,0,sizeof(pose));pose.world=wm;pose.bone_count=3;
 target[0]=65536;target[1]=65536;target[2]=0;
 err=re4_skel_ik_ccd_two_bone(&model,&pose,0,1,2,target,64,13107,&dist);
 CHECK(err==RE4_OK);
 CHECK(dist<65536L);
 {
    re4_q16 lower[2]={0,0},upper[2]={0,0},before;
    /* Reset test skeleton and require zero correction budget. */
    id(&wm[0],0,0,0);id(&wm[1],65536,0,0);id(&wm[2],131072,0,0);
    before=wm[2].m[3];
    CHECK(re4_skel_ik_ccd_axis_limited(&model,&pose,0,1,2,target,
        20,13107,2,lower,upper,&dist)==RE4_OK);
    CHECK(wm[2].m[3]==before && wm[2].m[7]==0);
    CHECK(dist==131072);
    lower[0]=-131072;lower[1]=-131072;
    upper[0]=131072;upper[1]=131072;
    CHECK(re4_skel_ik_ccd_axis_limited(&model,&pose,0,1,2,target,
        64,13107,2,lower,upper,&dist)==RE4_OK);
    CHECK(dist<131072);
    CHECK(re4_skel_ik_ccd_axis_limited(&model,&pose,0,1,2,target,
        20,13107,2,upper,lower,&dist)==RE4_ERR_BAD_FORMAT);
 }
 CHECK(re4_skel_ik_ccd_two_bone(&model,&pose,2,1,0,target,64,13107,&dist)==RE4_ERR_BAD_FORMAT);
 puts("motion keyframes endian+0x80+CCD IK tests OK");return 0;
}
