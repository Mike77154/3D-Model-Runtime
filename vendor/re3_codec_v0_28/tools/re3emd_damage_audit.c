#include "re3_codec.h"
#include <stdio.h>
static re3c_model m;
static re3c_mat34fx matrices[RE3C_MAX_PARTS];
int main(int argc,char**argv){re3c_u32 c,k,tested=0,transformed=0;int r;const re3c_anim_bank *a,*d;re3c_pose src,ret;re3c_vec3fx v;
 if(argc!=2)return 2;
 re3c_model_init(&m);r=re3c_open_file(&m,argv[1]);if(r!=RE3C_OK)return 1;
 a=&m.banks[0];d=&m.banks[2];if(!d->present||!d->clip_count){puts("NO_DAMAGE");return 0;}
 if(a->joint_count==d->joint_count&&d->hierarchy_source!=255u){puts("SAME_HIERARCHY");return 0;}
 if(a->joint_count!=11u||d->joint_count!=15u){puts("UNMAPPED");return 0;}
 for(c=0;c<d->clip_count;c++)for(k=0;k<d->clips[c].key_count;k++){
  r=re3c_decode_pose_by_key(&m,2u,c,k,&src);if(r!=RE3C_OK)return 3;
  r=re3c_retarget_zombie_damage_15_to_11(&m,&src,&ret);if(r!=RE3C_OK){printf("UNMAPPED_%d\n",r);return 0;}
  r=re3c_build_joint_matrices(a,&ret,RE3C_ROOT_MODEL_SPACE,matrices);if(r!=RE3C_OK)return 4;
  if(m.body.part_count&&m.body.parts[0].vertex_count){r=re3c_animated_body_vertex(&m,0,matrices,0,0,&v);if(r!=RE3C_OK)return 5;transformed++;}
  tested++;
 }
 printf("MAPPED,%u,%u\n",(unsigned)tested,(unsigned)transformed);
 return 0;
}
