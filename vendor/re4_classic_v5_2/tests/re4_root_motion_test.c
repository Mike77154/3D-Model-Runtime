/* Deterministic C89 root track + translation tests. */
#include "re4c3d/re4c3d.h"
#include <string.h>
#include <stdio.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"root test fail line %d\n",__LINE__);return 1;}}while(0)
/* Two keys at times 0,10, each axis: f32(value, in tangent, out tangent).
   x 0->1, y 0->0, z 0->2. All tangent values are zero. */
static void putu16(re4_u8 *p,re4_u16 v){p[0]=(re4_u8)v;p[1]=(re4_u8)(v>>8);}
static void putf32(re4_u8 *p,re4_u32 bits){p[0]=(re4_u8)bits;p[1]=(re4_u8)(bits>>8);p[2]=(re4_u8)(bits>>16);p[3]=(re4_u8)(bits>>24);}
static void axis(re4_u8 *p,re4_u32 last)
{
    memset(p,0,30U);putu16(p,2U);putu16(p+2U,0U);putu16(p+4U,10U);
    putf32(p+18U,last);
}
int main(void)
{
    re4_u8 bytes[90];re4_span src;re4_fcv clip;re4_fcv_block block[2];
    re4_root_track root;re4_q16 xyz[3],zero[3]={0,0,0},off[3];
    re4_skel_matrix matrices[2];re4_skel_pose pose;re4_u32 i;
    memset(bytes,0,sizeof(bytes));axis(bytes,0x3f800000UL);
    axis(bytes+30,0UL);axis(bytes+60,0x40000000UL);
    memset(&clip,0,sizeof(clip));memset(block,0,sizeof(block));
    src.data=bytes;src.size=sizeof(bytes);
    block[0].bone_type=1;block[0].bone_no=0;block[0].data_type=0;
    clip.blocks=block;clip.block_count=1;clip.endian=RE4_ENDIAN_LITTLE;
    CHECK(re4_root_find(&clip,&root)==RE4_OK && root.present);
    CHECK(re4_root_sample(src,&clip,&root,0,xyz)==RE4_OK);
    CHECK(xyz[0]==0 && xyz[1]==0 && xyz[2]==0);
    CHECK(re4_root_sample(src,&clip,&root,10,xyz)==RE4_OK);
    CHECK(xyz[0]==65536 && xyz[1]==0 && xyz[2]==131072);
    CHECK(re4_root_relative(src,&clip,&root,0,10,off)==RE4_OK);
    CHECK(off[0]==65536 && off[1]==0 && off[2]==131072);
    CHECK(re4_root_relative(src,&clip,&root,10,0,off)==RE4_OK);
    CHECK(off[0]==-65536 && off[2]==-131072);
    for(i=0;i<2U;++i){memset(&matrices[i],0,sizeof(matrices[i]));matrices[i].m[0]=matrices[i].m[5]=matrices[i].m[10]=65536;}
    pose.world=matrices;pose.bone_count=2;
    CHECK(re4_root_translate(&pose,zero)==RE4_OK);
    CHECK(re4_root_translate(&pose,xyz)==RE4_OK);
    CHECK(matrices[0].m[3]==65536 && matrices[1].m[11]==131072);
    CHECK(matrices[1].m[0]==65536);
    /* Atomic overflow: none of the bones may be partially updated. */
    matrices[1].m[3]=RE4_I32_MAX;
    CHECK(re4_root_translate(&pose,xyz)==RE4_ERR_RANGE);
    CHECK(matrices[0].m[3]==65536 && matrices[1].m[3]==RE4_I32_MAX);
    block[1]=block[0];clip.block_count=2;
    CHECK(re4_root_find(&clip,&root)==RE4_ERR_BAD_FORMAT);
    clip.block_count=1;block[0].bone_type=2;
    CHECK(re4_root_find(&clip,&root)==RE4_OK && !root.present);
    CHECK(re4_root_sample(src,&clip,&root,10,xyz)==RE4_OK);
    CHECK(xyz[0]==0 && xyz[1]==0 && xyz[2]==0);
    printf("root track sampling, relative offsets, atomic translation OK\n");
    return 0;
}
