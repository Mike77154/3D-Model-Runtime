#include "hl2m_master89.h"
#include <stdio.h>
#include <string.h>
static hl2m_u8 mdlbytes[1024];
static hl2m_mdl mdl;
static hl2m_poseparam121 desc[4];
static void put32(unsigned int o,unsigned int v)
{ mdlbytes[o]=(hl2m_u8)v;mdlbytes[o+1]=(hl2m_u8)(v>>8);
  mdlbytes[o+2]=(hl2m_u8)(v>>16);mdlbytes[o+3]=(hl2m_u8)(v>>24); }
static int near(hl2m_fixed a,hl2m_fixed b)
{ return a>=b-8 && a<=b+8; }
int main(void)
{
    hl2m_fixed v,x,y,normalized[2],physical;
    unsigned int count=0u;int r;
    memset(mdlbytes,0,sizeof(mdlbytes));memset(&mdl,0,sizeof(mdl));
    memcpy(mdlbytes,"IDST",4u);
    put32(300u,2u);put32(304u,400u);
    put32(400u,40u);put32(404u,0u);
    put32(408u,0xc2b40000u);put32(412u,0x42b40000u);put32(416u,0u);
    put32(420u,40u);put32(424u,0u);
    put32(428u,0xc3340000u);put32(432u,0x43340000u);
    put32(436u,0x43b40000u); /* loop=360, start=-180 end=180 */
    memcpy(mdlbytes+440u,"pitch",6u);memcpy(mdlbytes+460u,"yaw",4u);
    r=hl2m_poseparams_read121(mdlbytes,sizeof(mdlbytes),desc,4u,&count);
    if(r || count!=2u || strcmp(desc[0].name,"pitch") ||
       strcmp(desc[1].name,"yaw") || desc[0].start!=-90*65536 ||
       desc[1].start!=-180*65536 || desc[1].loop!=360*65536){
        puts("FAIL mdl pose descriptors");return 1;
    }
    r=hl2m_pose_physical_from_control121(&desc[0],49152,&v);
    if(r || !near(v,45*65536)){printf("FAIL normalized 0.75 -> +45 deg: r=%d v=%ld expected=%d start=%ld end=%ld\n",r,(long)v,45*65536,(long)desc[0].start,(long)desc[0].end);return 2;}
    r=hl2m_pose_control_from_physical121(&desc[0],45*65536,&v);
    if(r || !near(v,49152)){puts("FAIL +45 deg -> 0.75");return 3;}
    r=hl2m_pose_control_from_physical121(&desc[1],200*65536,&v);
    if(r || !near(v,((20*65536)/360))){
        puts("FAIL 360deg loop (200 -> -160)");return 4;
    }
    /* 2 axis independent selection; child sequence can use another index. */
    mdl.sequence_count=2;
    mdl.sequences[0].groupsize[0]=3;mdl.sequences[0].groupsize[1]=1;
    mdl.sequences[0].paramindex[0]=0;mdl.sequences[0].paramindex[1]=-1;
    mdl.sequences[1].groupsize[0]=5;mdl.sequences[1].groupsize[1]=1;
    mdl.sequences[1].paramindex[0]=1;mdl.sequences[1].paramindex[1]=-1;
    normalized[0]=49152;normalized[1]=16384;
    r=hl2m_pose_sequence_axes121(&mdl,0,desc,2u,normalized,2u,&x,&y);
    if(r || !near(x,45*65536)){puts("FAIL primary seq index 0");return 5;}
    r=hl2m_pose_sequence_axes121(&mdl,1,desc,2u,normalized,2u,&x,&y);
    if(r || !near(x,-90*65536)){puts("FAIL child seq index 1");return 6;}
    /* Read rejects malformed descriptor range and missing control. */
    r=hl2m_poseparams_read121(mdlbytes,403u,desc,4u,&count);
    if(r==HL2M_OK){puts("FAIL truncated pose table accepted");return 7;}
    r=hl2m_pose_sequence_axes121(&mdl,1,desc,2u,normalized,1u,&x,&y);
    if(r==HL2M_OK){puts("FAIL missing controller accepted");return 8;}
    physical=12345;
    r=hl2m_pose_physical_from_control121(&desc[0],65537,&physical);
    if(r==HL2M_OK || physical!=12345){puts("FAIL normalized range validation");return 9;}
    puts("pose121 PASS: real MDL descriptor layout, normalized/local mapping, looping, per-sequence indices, malformed tables");
    return 0;
}
