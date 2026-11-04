/* Real-corpus-independent API guards, ISO C89. */
#include "re3_codec.h"
#include <stdio.h>
static re3c_model a,b;
static re3c_pose pose,out;
int main(void){int eq=-1,r;
 re3c_model_init(&a);re3c_model_init(&b);
 if(re3c_damage_family_check(&a,RE3C_DAMAGE_FAMILY_NEMESIS)!=RE3C_ERR_NOT_OPEN)return 1;
 if(re3c_damage_sections_equal(&a,&b,&eq)!=RE3C_ERR_NOT_OPEN)return 2;
 a.is_open=b.is_open=1u;a.banks[0].present=b.banks[0].present=1u;
 a.banks[0].joint_count=b.banks[0].joint_count=2u;
 a.banks[0].joints[0].parent=b.banks[0].joints[0].parent=-1;
 a.banks[0].joints[1].parent=b.banks[0].joints[1].parent=0;
 r=re3c_normal_rigs_equal(&a,&b,&eq);if(r!=RE3C_OK||eq!=1)return 3;
 b.banks[0].joints[1].local_pos.x=4096;
 r=re3c_normal_rigs_equal(&a,&b,&eq);if(r!=RE3C_OK||eq!=0)return 4;
 if(re3c_damage_family_check(&a,RE3C_DAMAGE_FAMILY_NEMESIS)!=RE3C_ERR_NOT_PRESENT)return 5;
 if(re3c_damage_family_check(&a,RE3C_DAMAGE_FAMILY_GRAVEDIGGER)!=RE3C_ERR_NOT_PRESENT)return 6;
 if(re3c_damage_family_check(&a,999u)!=RE3C_ERR_BAD_ARGUMENT)return 7;
 pose.joint_count=15u;
 if(re3c_retarget_nemesis_damage_experimental(&a,&pose,&out)!=RE3C_ERR_NOT_PRESENT)return 8;
 if(re3c_retarget_gravedigger_damage_experimental(&a,&pose,&out)!=RE3C_ERR_NOT_PRESENT)return 9;
 puts("PASS damage family guards and normal rig comparison");
 return 0;
}
