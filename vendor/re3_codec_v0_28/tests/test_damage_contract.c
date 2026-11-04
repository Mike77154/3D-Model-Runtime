/* Contract must never equate channel-count mismatch with a safe skinning map. */
#include "re3_codec.h"
#include <stdio.h>
static re3c_model m;
int main(void){re3c_u32 s=99u;int r;
 re3c_model_init(&m);
 if(re3c_damage_rig_status(&m,&s)!=RE3C_ERR_NOT_OPEN)return 1;
 m.is_open=1u;
 if(re3c_damage_rig_status(&m,&s)!=RE3C_OK||s!=RE3C_DAMAGE_RIG_ABSENT)return 2;
 m.banks[RE3C_BANK_A].present=1u;
 m.banks[RE3C_BANK_A].joint_count=17u;
 m.banks[RE3C_BANK_DAMAGE].present=1u;
 m.banks[RE3C_BANK_DAMAGE].joint_count=15u;
 m.banks[RE3C_BANK_DAMAGE].hierarchy_inherited=1u;
 m.banks[RE3C_BANK_DAMAGE].hierarchy_source=255u;
 r=re3c_damage_rig_status(&m,&s);
 if(r!=RE3C_OK||s!=RE3C_DAMAGE_RIG_UNRESOLVED)return 3;
 m.banks[RE3C_BANK_DAMAGE].joint_count=17u;
 m.banks[RE3C_BANK_DAMAGE].hierarchy_source=RE3C_BANK_A;
 r=re3c_damage_rig_status(&m,&s);
 if(r!=RE3C_OK||s!=RE3C_DAMAGE_RIG_INHERITED)return 4;
 puts("PASS damage rig safety contract");return 0;
}
