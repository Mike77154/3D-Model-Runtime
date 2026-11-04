#include "re3_codec.h"
#include <stdio.h>
#include <string.h>
static re3c_model model;
int main(void){
 static const re3c_i16 parents[11]={-1,0,0,2,0,4,0,6,7,6,9};
 static const re3c_i16 xyz[11][3]={
 {0,-1995,0},{-43,-646,0},{-46,-648,-343},{13,581,-61},
 {-46,-648,343},{13,572,61},{0,0,0},{0,253,-176},
 {0,730,-1},{0,253,176},{0,730,1}};
 static const re3c_u16 map[11]={0u,1u,2u,3u,5u,6u,8u,9u,10u,12u,13u};
 re3c_pose src,out;re3c_u32 i;int r;
 memset(&model,0,sizeof(model));memset(&src,0,sizeof(src));
 model.is_open=1u;model.banks[0].present=1u;
 model.banks[0].joint_count=11u;model.banks[0].hierarchy_source=0u;
 src.joint_count=15u;
 for(i=0u;i<11u;i++){
  re3c_joint *j=&model.banks[0].joints[i];j->parent=parents[i];
  j->local_pos.x=re3c_fx_from_int(xyz[i][0]);
  j->local_pos.y=re3c_fx_from_int(xyz[i][1]);
  j->local_pos.z=re3c_fx_from_int(xyz[i][2]);
 }
 for(i=0u;i<15u;i++)src.rot_x[i]=(re3c_i16)(i*71u);
 r=re3c_retarget_zombie_damage_15_to_11(&model,&src,&out);
 if(r!=RE3C_OK)return 1;
 for(i=0u;i<11u;i++)if(out.rot_x[i]!=src.rot_x[map[i]])return 2;
 model.banks[0].joints[6].parent=1;
 if(re3c_retarget_zombie_damage_15_to_11(&model,&src,&out)!=RE3C_ERR_NOT_PRESENT)return 3;
 model.banks[0].joints[6].parent=0;
 src.joint_count=14u;
 if(re3c_retarget_zombie_damage_15_to_11(&model,&src,&out)!=RE3C_ERR_NOT_PRESENT)return 4;
 puts("PASS guarded 15-to-11 zombie damage remapping");return 0;
}
