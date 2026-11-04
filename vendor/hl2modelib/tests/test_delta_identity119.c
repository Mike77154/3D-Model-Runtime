/* Regression: unanimated DELTA bone must not reuse its bind quaternion. */
#include "hl2modelib.h"
#include <string.h>
#include <stdio.h>
static hl2m_u8 bytes[32];
static hl2m_mdl mdl;
static hl2m_pose_matrices pose;
int main(void)
{
 int r,i,j,k;
 memset(&mdl,0,sizeof(mdl));memset(bytes,0,sizeof(bytes));
 mdl.bone_count=2;mdl.anim_count=1;
 mdl.bones[0].parent=-1;mdl.bones[1].parent=0;
 for(i=0;i<2;i++){
    mdl.bones[i].quat.x=0;mdl.bones[i].quat.y=0;
    mdl.bones[i].quat.z=46340;mdl.bones[i].quat.w=46340;
    mdl.bones[i].pos.x=3*HL2M_FIXED_ONE;
 }
 mdl.anims[0].flags=HL2M_SEQ_DELTA89;
 mdl.anims[0].numframes=2;
 mdl.anims[0].first_anim_offset=8;
 bytes[8]=255; /* empty channel chain: terminate */
 r=hl2m_native_sample_anim_matrices_external89(bytes,sizeof(bytes),NULL,0,
     &mdl,0,0,&pose,NULL);
 if(r){printf("FAIL delta sample code=%d\n",r);return 1;}
 for(i=0;i<2;i++)for(j=0;j<3;j++)for(k=0;k<4;k++){
    hl2m_fixed want=(j==k)?HL2M_FIXED_ONE:0;
    if(pose.local[i].m[j][k]!=want){
       printf("FAIL unanimated DELTA bone=%d [%d][%d] got=%d expected=%d\n",
          i,j,k,pose.local[i].m[j][k],want);return 2;
    }
 }
 puts("delta119: unanimated bones keep neutral transform even with non-identity bind quaternions PASS");
 return 0;
}
