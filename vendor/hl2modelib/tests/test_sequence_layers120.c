/* SDK regression: pose-key stride and autolayer physical pose control. */
#include "hl2m_master89.h"
#include <stdio.h>
#include <string.h>
static hl2m_u8 binary[1400];
static hl2m_mdl model;
static hl2m_seq_metadata89 metadata;
static void w32(unsigned int o,unsigned int x)
{binary[o]=(hl2m_u8)x;binary[o+1]=(hl2m_u8)(x>>8);binary[o+2]=(hl2m_u8)(x>>16);binary[o+3]=(hl2m_u8)(x>>24);}
static void f32(unsigned int o,unsigned int x){w32(o,x);}
int main(void)
{
    hl2m_seq_layer89 layer;hl2m_fixed cycle,w,t;
    int r;
    memset(binary,0,sizeof(binary));memset(&model,0,sizeof(model));
    model.bone_count=1;model.sequence_count=1;
    model.sequences[0].groupsize[0]=2;
    model.sequences[0].groupsize[1]=3;
    w32(192,512);w32(512+160,300);
    f32(812,0x00000000u);f32(816,0x3f800000u);
    f32(820,0x00000000u);f32(824,0x3e800000u);f32(828,0x3f800000u);
    r=hl2m_sequence_metadata89(binary,sizeof(binary),&model,0,&metadata);
    if(r || !metadata.has_posekeys || metadata.posekeys[0][1]!=HL2M_FIXED_ONE ||
       metadata.posekeys[1][1]!=HL2M_FIXED_ONE/4 ||
       metadata.posekeys[1][2]!=HL2M_FIXED_ONE){puts("FAIL posekey unequal grid");return 1;}
    r=hl2m_sequence_pose_axis89(&model.sequences[0],&metadata,1,
        HL2M_FIXED_ONE/4,&t);
    if(r || t<HL2M_FIXED_ONE/2-3 || t>HL2M_FIXED_ONE/2+3)
        {puts("FAIL posekey axis1 interpolation");return 2;}
    memset(&layer,0,sizeof(layer));
    layer.flags=HL2M_SEQ_LAYER_POSE89;
    layer.start=0;layer.peak=HL2M_FIXED_ONE/4;
    layer.tail=HL2M_FIXED_ONE/2;layer.end=HL2M_FIXED_ONE;
    cycle=HL2M_FIXED_ONE/8;
    r=hl2m_sequence_layer_envelope_control120(&layer,cycle,HL2M_FIXED_ONE,
        3*HL2M_FIXED_ONE/4,&t,&w);
    if(r || t!=cycle || w<HL2M_FIXED_ONE/2-3 || w>HL2M_FIXED_ONE/2+3)
        {puts("FAIL physical pose envelope or inherited cycle");return 3;}
    r=hl2m_sequence_layer_envelope_control120(&layer,cycle,HL2M_FIXED_ONE,
        HL2M_FIXED_ONE,&t,&w);
    if(r || w!=0 || t!=cycle){puts("FAIL pose end excluded");return 4;}
    layer.flags=0;
    r=hl2m_sequence_layer_envelope_control120(&layer,cycle,HL2M_FIXED_ONE,
        3*HL2M_FIXED_ONE/4,&t,&w);
    if(r || t!=cycle || w<HL2M_FIXED_ONE/2-3 || w>HL2M_FIXED_ONE/2+3)
        {puts("FAIL cycle regression");return 5;}
    puts("sequence120: Source posekey stride, unequal grid, STUDIO_AL_POSE, cycle inheritance PASS");
    return 0;
}
