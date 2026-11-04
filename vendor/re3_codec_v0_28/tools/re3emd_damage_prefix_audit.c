/* v0.28: trace-supported damage-channel prefix experiment. C89, no heap. */
#include "re3_codec.h"
#include <stdio.h>
static re3c_model model;
int main(int argc,char **argv){
 re3c_pose base,damage,overlaid;
 re3c_mat34fx mats[RE3C_MAX_PARTS];
 re3c_u32 c,k,j,total=0u,vertices=0u,clip_count,tail;
 int r;
 if(argc!=2){fprintf(stderr,"usage: %s model.EMD\n",argv[0]);return 2;}
 re3c_model_init(&model);r=re3c_open_file(&model,argv[1]);
 if(r!=RE3C_OK){fprintf(stderr,"open: %s\n",re3c_result_string(r));return 1;}
 if(!model.banks[RE3C_BANK_DAMAGE].present){puts("no damage bank");return 2;}
 if(!model.banks[RE3C_BANK_A].clip_count){puts("no normal pose");return 2;}
 r=re3c_decode_pose_by_key(&model,RE3C_BANK_A,0,0,&base);
 if(r!=RE3C_OK)return 1;
 if(base.joint_count<model.banks[RE3C_BANK_DAMAGE].joint_count){
  puts("UNSUPPORTED damage channels exceed normal nodes; explicit analysis required");
  re3c_model_close(&model);return 2;
 }
 tail=(re3c_u32)base.joint_count-(re3c_u32)model.banks[RE3C_BANK_DAMAGE].joint_count;
 clip_count=model.banks[RE3C_BANK_DAMAGE].clip_count;
 for(c=0u;c<clip_count;c++)for(k=0u;k<model.banks[RE3C_BANK_DAMAGE].clips[c].key_count;k++){
  r=re3c_decode_pose_by_key(&model,RE3C_BANK_DAMAGE,c,k,&damage);
  if(r!=RE3C_OK)return 1;
  r=re3c_damage_overlay_prefix(&model,&base,&damage,&overlaid);
  if(r!=RE3C_OK){fprintf(stderr,"prefix fail clip=%u key=%u: %s\n",(unsigned)c,(unsigned)k,re3c_result_string(r));return 1;}
  for(j=damage.joint_count;j<overlaid.joint_count;j++){
   if(overlaid.rot_x[j]!=base.rot_x[j]||overlaid.rot_y[j]!=base.rot_y[j]||overlaid.rot_z[j]!=base.rot_z[j])return 3;
  }
  r=re3c_build_joint_matrices(&model.banks[RE3C_BANK_A],&overlaid,RE3C_ROOT_MODEL_SPACE,mats);
  if(r!=RE3C_OK){fprintf(stderr,"matrix fail clip=%u key=%u: %s\n",(unsigned)c,(unsigned)k,re3c_result_string(r));return 1;}
  for(j=0u;j<model.body.part_count;j++){
   const re3c_mesh_part *p=&model.body.parts[j];re3c_vec3fx v;
   if(!p->vertex_count||!re3c_part_should_render_default(p))continue;
   r=re3c_animated_body_vertex(&model,RE3C_BANK_A,mats,j,0u,&v);
   if(r!=RE3C_OK){fprintf(stderr,"skin fail clip=%u key=%u part=%u\n",(unsigned)c,(unsigned)k,(unsigned)j);return 1;}
   vertices++;
  }
  total++;
 }
 printf("OK clips=%u keys=%u preserved_tail_nodes=%u skinned_samples=%u\n",(unsigned)clip_count,(unsigned)total,(unsigned)tail,(unsigned)vertices);
 re3c_model_close(&model);return 0;
}
