/* RE3 damage-bank comparison; ISO C89, no heap, no third-party libraries.
 * Usage: re3emd_damage_compare first.EMD [second.EMD]
 * Each model's separate 15-channel damage EMR may omit its own rig.
 */
#include "re3_codec.h"
#include <stdio.h>
static re3c_model a,b;
static int show(const re3c_model *m,const char *path){
 const re3c_anim_bank *d=&m->banks[RE3C_BANK_DAMAGE];
 re3c_u32 c,k,decoded=0u;int r;re3c_pose p;
 printf("file,%s\nnormal_joints,%u\ndamage_present,%u\n",path,(unsigned)m->banks[RE3C_BANK_A].joint_count,(unsigned)d->present);
 if(!d->present)return RE3C_OK;
 printf("damage_joints,%u\ndamage_hierarchy_source,%u\ndamage_clips,%u\ndamage_keys,%u\n",
  (unsigned)d->joint_count,(unsigned)d->hierarchy_source,(unsigned)d->clip_count,(unsigned)d->key_count);
 printf("is_nemesis_rest_rig,%u\nis_gravedigger_rest_rig,%u\n",
  (unsigned)(re3c_damage_family_check(m,RE3C_DAMAGE_FAMILY_NEMESIS)==RE3C_OK),
  (unsigned)(re3c_damage_family_check(m,RE3C_DAMAGE_FAMILY_GRAVEDIGGER)==RE3C_OK));
 for(c=0u;c<d->clip_count;c++){
  printf("clip,%u,keys,%u\n",(unsigned)c,(unsigned)d->clips[c].key_count);
  for(k=0u;k<d->clips[c].key_count;k++){
   r=re3c_decode_pose_by_key(m,RE3C_BANK_DAMAGE,c,k,&p);
   if(r!=RE3C_OK)return r;
   decoded++;
  }
 }
 printf("decoded_damage_keys,%u\n",(unsigned)decoded);
 return RE3C_OK;
}
int main(int argc,char **argv){int r,eq;
 if(argc!=2&&argc!=3){fprintf(stderr,"usage: %s file.EMD [other.EMD]\n",argv[0]);return 2;}
 re3c_model_init(&a);re3c_model_init(&b);
 r=re3c_open_file(&a,argv[1]);if(r!=RE3C_OK){fprintf(stderr,"open A: %s\n",re3c_result_string(r));return 1;}
 r=show(&a,argv[1]);if(r!=RE3C_OK){fprintf(stderr,"decode A: %s\n",re3c_result_string(r));return 1;}
 if(argc==3){
  r=re3c_open_file(&b,argv[2]);if(r!=RE3C_OK){fprintf(stderr,"open B: %s\n",re3c_result_string(r));return 1;}
  r=show(&b,argv[2]);if(r!=RE3C_OK){fprintf(stderr,"decode B: %s\n",re3c_result_string(r));return 1;}
  r=re3c_damage_sections_equal(&a,&b,&eq);
  if(r!=RE3C_OK){fprintf(stderr,"compare damage: %s\n",re3c_result_string(r));return 1;}
  printf("damage_EDD_EMR_byte_identical,%d\n",eq);
  r=re3c_normal_rigs_equal(&a,&b,&eq);
  if(r!=RE3C_OK)return 1;
  printf("normal_rest_rig_identical,%d\n",eq);
 }
 re3c_model_close(&a);re3c_model_close(&b);return 0;
}
