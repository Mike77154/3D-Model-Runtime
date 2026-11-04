/* A C89, heap-free damage skeleton audit; 8-byte EMR headers may carry
 * rotations without the hierarchy used to render vertices. */
#include "re3_codec.h"
#include <stdio.h>
static re3c_model model;
int main(int argc,char **argv){
 re3c_u32 status,b,c,k,total=0u;re3c_pose pose;int r;
 if(argc!=2){fprintf(stderr,"usage: re3emd_damage_contract file.EMD\n");return 2;}
 re3c_model_init(&model);
 r=re3c_open_file(&model,argv[1]);
 if(r!=RE3C_OK){fprintf(stderr,"open failed: %s\n",re3c_result_string(r));return 1;}
 r=re3c_damage_rig_status(&model,&status);
 if(r!=RE3C_OK){fprintf(stderr,"contract failed %d\n",r);return 1;}
 b=RE3C_BANK_DAMAGE;
 printf("file=%s\n",argv[1]);
 printf("normal_joints=%u damage_joints=%u damage_clips=%u damage_frames=%u\n",
 (unsigned)model.banks[RE3C_BANK_A].joint_count,
 (unsigned)model.banks[b].joint_count,
 (unsigned)model.banks[b].clip_count,
 (unsigned)model.banks[b].physical_frame_count);
 printf("damage_emr_offset=%u hierarchy_source=%u inherited=%u\n",
 (unsigned)model.banks[b].frame_offset,
 (unsigned)model.banks[b].hierarchy_source,
 (unsigned)model.banks[b].hierarchy_inherited);
 printf("binding=%s\n",status==RE3C_DAMAGE_RIG_ABSENT?"ABSENT":
 status==RE3C_DAMAGE_RIG_INHERITED?"STRUCTURAL_INHERITANCE":"UNRESOLVED_DO_NOT_AUTO_RETARGET");
 puts("clip,keys,decoded");
 for(c=0u;c<model.banks[b].clip_count;c++){
  re3c_u32 success=0u;
  for(k=0u;k<model.banks[b].clips[c].key_count;k++){
   r=re3c_decode_pose_by_key(&model,b,c,k,&pose);
   if(r==RE3C_OK)success++;else {fprintf(stderr,"decode failed clip %u key %u: %d\n",(unsigned)c,(unsigned)k,r);return 1;}
  }
  total+=success;
  printf("%u,%u,%u\n",(unsigned)c,(unsigned)model.banks[b].clips[c].key_count,(unsigned)success);
 }
 printf("total_decoded=%u\n",(unsigned)total);
 re3c_model_close(&model);return 0;
}
