/* Explicit morph/retarget tests. ISO C89, static objects, no heap. */
#include "re3_codec.h"
#include <stdio.h>
#include <string.h>
static re3c_model model;
static re3c_pose src_pose,dst_pose;
static re3c_anim_bank target;
static re3c_mat34fx matrices[RE3C_MAX_PARTS];
static re3c_u16 map[RE3C_MAX_PARTS];
static int same_point(const re3c_vec3fx*a,const re3c_vec3fx*b){return a->x==b->x&&a->y==b->y&&a->z==b->z;}
int main(int argc,char **argv){re3c_u32 i;int r;re3c_vec3fx v0,v1,v2;unsigned changed=0u;
 memset(&src_pose,0,sizeof(src_pose));memset(&target,0,sizeof(target));
 src_pose.joint_count=3u;src_pose.rot_x[0]=111;src_pose.rot_x[1]=222;src_pose.rot_x[2]=333;src_pose.speed.x=1234;src_pose.origin_y=512;
 target.present=1u;target.joint_count=4u;target.hierarchy_source=0u;
 for(i=0u;i<RE3C_MAX_PARTS;i++)map[i]=RE3C_JOINT_NEUTRAL;
 map[0]=2u;map[1]=0u;map[2]=RE3C_JOINT_NEUTRAL;map[3]=1u;
 r=re3c_retarget_pose_explicit(&src_pose,&target,map,&dst_pose);
 if(r!=RE3C_OK||dst_pose.joint_count!=4u||dst_pose.rot_x[0]!=333||dst_pose.rot_x[1]!=111||dst_pose.rot_x[2]!=0||dst_pose.rot_x[3]!=222||dst_pose.speed.x!=1234||dst_pose.origin_y!=512){puts("FAIL retarget explicit");return 1;}
 map[3]=3u;dst_pose.rot_x[3]=999;
 if(re3c_retarget_pose_explicit(&src_pose,&target,map,&dst_pose)!=RE3C_ERR_RANGE||dst_pose.rot_x[3]!=999){puts("FAIL retarget range/atomicity");return 1;}
 map[3]=1u;target.hierarchy_source=255u;
 if(re3c_retarget_pose_explicit(&src_pose,&target,map,&dst_pose)!=RE3C_ERR_NOT_PRESENT){puts("FAIL retarget missing target rig");return 1;}
 /* A no-hierarchy bank is forbidden even if joint count matches. */
 src_pose.joint_count=4u;target.joints[0].parent=-1;
 if(re3c_build_joint_matrices(&target,&src_pose,RE3C_ROOT_MODEL_SPACE,matrices)!=RE3C_ERR_NOT_PRESENT){puts("FAIL empty damage hierarchy");return 1;}
 if(argc>1){
  re3c_pose pose;const re3c_anim_bank *b; re3c_u32 part,vi,frames=0u;
  re3c_model_init(&model);r=re3c_open_file(&model,argv[1]);if(r!=RE3C_OK){puts("FAIL corpus EMD open");return 1;}
  b=&model.banks[RE3C_BANK_A];r=re3c_decode_pose_by_key(&model,RE3C_BANK_A,1u,0u,&pose);if(r!=RE3C_OK){puts("FAIL corpus pose");return 1;}
  r=re3c_build_joint_matrices(b,&pose,RE3C_ROOT_MODEL_SPACE,matrices);if(r!=RE3C_OK){puts("FAIL corpus matrices");return 1;}
  for(part=0u;part<model.body.part_count;part++){
   re3c_u32 ti; if(re3c_morph_track_for_part(&model,part,&ti)!=RE3C_OK)continue;
   for(vi=0u;vi<model.body.parts[part].vertex_count;vi++){
    r=re3c_animated_body_vertex(&model,RE3C_BANK_A,matrices,part,vi,&v0);if(r!=RE3C_OK)return 1;
    r=re3c_animated_body_vertex_morph(&model,RE3C_BANK_A,matrices,part,vi,RE3C_MORPH_REST,&v1);
    if(r!=RE3C_OK||!same_point(&v0,&v1)){puts("FAIL rest backward compatibility");return 1;}
    r=re3c_animated_body_vertex_morph(&model,RE3C_BANK_A,matrices,part,vi,1u,&v2);
    if(r!=RE3C_OK){puts("FAIL morph frame 1");return 1;}
    if(!same_point(&v0,&v2))changed++;
   }
   frames++;
  }
  if(frames==0u||changed==0u){puts("FAIL expected morph variation");return 1;}
  /* Known EM36 two-frame part; reject invalid morph frame. */
  if(model.body.parts[1].morph_frame_count>0u){
   r=re3c_animated_body_vertex_morph(&model,RE3C_BANK_A,matrices,1u,0u,model.body.parts[1].morph_frame_count,&v2);
   if(r!=RE3C_ERR_RANGE){puts("FAIL morph bounds");return 1;}
  }
  printf("PASS corpus morph tracks=%u changed_vertices=%u\n",frames,changed);
  re3c_model_close(&model);
 }
 puts("PASS explicit retarget, hierarchy guard, morph-state skinning");return 0;
}
