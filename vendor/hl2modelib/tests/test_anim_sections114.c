/* Native Source block-0 section selection, deterministic no heap C89. */
#include "hl2modelib.h"
#include <stdio.h>
#include <string.h>
static hl2m_u8 buffer[1024];
static hl2m_mdl mdl;
static hl2m_ik_pose pose;
static hl2m_native_anim_report89 rep;
static void wr32(unsigned int off,unsigned int x)
{
    buffer[off]=(hl2m_u8)x;buffer[off+1u]=(hl2m_u8)(x>>8);
    buffer[off+2u]=(hl2m_u8)(x>>16);buffer[off+3u]=(hl2m_u8)(x>>24);
}
static void wr16(unsigned int off,int val)
{ unsigned int x=(unsigned int)val;buffer[off]=(hl2m_u8)x;buffer[off+1u]=(hl2m_u8)(x>>8); }
static void anim_chunk(int section,int value)
{
    unsigned int off=256u+200u+(unsigned int)section*32u;
    buffer[off]=0;buffer[off+1u]=4;wr16(off+2u,0);
    wr16(off+4u,6);wr16(off+6u,0);wr16(off+8u,0);
    buffer[off+10u]=2;buffer[off+11u]=2;
    wr16(off+12u,value);wr16(off+14u,value+1);
}
int main(void)
{
    int i,frame[3],want[3],r;
    memset(buffer,0,sizeof(buffer));memset(&mdl,0,sizeof(mdl));
    mdl.bone_count=1;mdl.anim_count=1;mdl.bones[0].parent=-1;
    mdl.bones[0].quat.w=HL2M_FIXED_ONE;
    mdl.bones[0].posscale_ieee32[0]=0x3F800000u;
    mdl.anims[0].numframes=6;mdl.anims[0].sectionframes=2;
    mdl.anims[0].sectionindex=120;mdl.anims[0].animblock=0;
    wr32(184u,256u);
    for(i=0;i<5;i++){
        wr32(256u+120u+(unsigned int)i*8u,0);
        wr32(256u+124u+(unsigned int)i*8u,200u+(unsigned int)i*32u);
        anim_chunk(i,5+i*10);
    }
    frame[0]=0;frame[1]=3;frame[2]=5;
    want[0]=5;want[1]=16;want[2]=45;
    for(i=0;i<3;i++){
        r=hl2m_native_sample_anim_frame89(buffer,sizeof(buffer),&mdl,0,frame[i],&pose,&rep);
        if(r!=HL2M_OK || pose.pos[0].x!=want[i]*HL2M_FIXED_ONE){
            fprintf(stderr,"section fail: frame=%d code=%d x=%d expected=%d\n",frame[i],r,pose.pos[0].x,want[i]*HL2M_FIXED_ONE);
            return 1;
        }
    }
    puts("PASS: embedded Source sections first, middle and separate last frame");return 0;
}
