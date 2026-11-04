/* Regression for Source SDK CalcBoneQuaternion/Position when STUDIO_ANIM_DELTA
   has neither compressed rotation nor position channels. */
#include "hl2modelib.h"
#include <string.h>
#include <stdio.h>
static hl2m_mdl mdl;
static hl2m_pose_matrices pose;
int main(void)
{
    static const hl2m_u8 channel[]={0,16,0,0};
    int r;
    memset(&mdl,0,sizeof(mdl));
    mdl.bone_count=1;
    mdl.bones[0].parent=-1;
    mdl.bones[0].quat.w=HL2M_FIXED_ONE;
    mdl.bones[0].pos.x=15*HL2M_FIXED_ONE;
    mdl.bones[0].pos.y=2*HL2M_FIXED_ONE;
    mdl.anim_count=1;
    mdl.anims[0].numframes=1;
    mdl.anims[0].first_anim_offset=0;
    r=hl2m_native_sample_anim_matrices89(channel,sizeof(channel),&mdl,0,0,&pose,0);
    if(r!=HL2M_OK){printf("delta read error %d\n",r);return 1;}
    if(pose.world[0].m[0][3]!=0 || pose.world[0].m[1][3]!=0) return 2;
    if(pose.world[0].m[0][0]!=HL2M_FIXED_ONE)return 3;
    if(pose.world[0].m[1][1]!=HL2M_FIXED_ONE)return 4;
    puts("SDK delta identity/zero: OK"); return 0;
}
