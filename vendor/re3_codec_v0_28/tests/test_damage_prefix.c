#include "re3_codec.h"
#include <stdio.h>
#include <string.h>
static re3c_model model;
static re3c_pose normal,damage,out;
int main(void){re3c_u32 j;int r;
 re3c_model_init(&model);model.is_open=1u;
 model.banks[RE3C_BANK_A].present=1u;
 model.banks[RE3C_BANK_A].joint_count=17u;
 model.banks[RE3C_BANK_DAMAGE].present=1u;
 model.banks[RE3C_BANK_DAMAGE].joint_count=15u;
 memset(&normal,0,sizeof(normal));memset(&damage,0,sizeof(damage));
 normal.joint_count=17u;damage.joint_count=15u;
 for(j=0u;j<17u;j++){normal.rot_x[j]=111;normal.rot_y[j]=222;normal.rot_z[j]=333;}
 for(j=0u;j<15u;j++){damage.rot_x[j]=(re3c_i16)(j+1u);damage.rot_y[j]=(re3c_i16)(j+2u);damage.rot_z[j]=(re3c_i16)(j+3u);}
 damage.speed.x=4096;damage.origin_y=2048;
 r=re3c_damage_overlay_prefix(&model,&normal,&damage,&out);
 if(r!=RE3C_OK){fprintf(stderr,"overlay: %s\n",re3c_result_string(r));return 1;}
 for(j=0;j<17u;j++){
  if(out.rot_x[j]!=(re3c_i16)(j<15u?j+1u:111u) ||
     out.rot_y[j]!=(re3c_i16)(j<15u?j+2u:222u) ||
     out.rot_z[j]!=(re3c_i16)(j<15u?j+3u:333u))return 2;
 }
 if(out.speed.x!=4096||out.origin_y!=2048||out.joint_count!=17u)return 3;
 if(re3c_damage_overlay_prefix(&model,&normal,&damage,&damage)!=RE3C_ERR_BAD_ARGUMENT)return 4;
 model.banks[RE3C_BANK_DAMAGE].joint_count=18u;
 if(re3c_damage_overlay_prefix(&model,&normal,&damage,&out)==RE3C_OK)return 5;
 puts("damage prefix tests PASS: overlay first 15; preserve trailing 2; reject alias and overflow");
 return 0;
}
