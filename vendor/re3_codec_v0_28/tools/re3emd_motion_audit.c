/* Animation corpus audit. The sampled frames are checked, not the full clip. */
#include "re3_codec.h"
#include <stdio.h>
static re3c_model m;
static re3c_pose pose;
static re3c_mat34fx mats[RE3C_MAX_PARTS];
int main(int argc,char**argv){
 re3c_u32 bi,ci,k,pi,vi,nkeys,idx,pose_count=0u,vertex_count=0u,clips=0u,skipped=0u;
 int r;
 if(argc!=2){fprintf(stderr,"usage: re3emd_motion_audit input.EMD\n");return 2;}
 re3c_model_init(&m);r=re3c_open_file(&m,argv[1]);
 if(r!=RE3C_OK){fprintf(stderr,"open: %s\n",re3c_result_string(r));return 1;}
 for(bi=0u;bi<RE3C_BANK_COUNT;bi++){
  const re3c_anim_bank*b=&m.banks[bi];if(!b->present)continue;
  if(!m.banks[RE3C_BANK_A].present||b->joint_count!=m.banks[RE3C_BANK_A].joint_count){skipped+=b->clip_count;continue;}
  for(ci=0u;ci<b->clip_count;ci++){
   nkeys=b->clips[ci].key_count;if(!nkeys)continue;clips++;
   for(k=0u;k<3u;k++){
    idx=k==0u?0u:(k==1u?nkeys/2u:nkeys-1u);
    r=re3c_decode_pose_by_key(&m,bi,ci,idx,&pose);
    if(r==RE3C_ERR_NOT_PRESENT){skipped++;continue;}
    if(r!=RE3C_OK){fprintf(stderr,"pose error bank=%u clip=%u key=%u r=%d\n",(unsigned)bi,(unsigned)ci,(unsigned)idx,r);return 1;}
    r=re3c_build_joint_matrices(b,&pose,RE3C_ROOT_MODEL_SPACE,mats);
    if(r!=RE3C_OK){fprintf(stderr,"matrix error bank=%u clip=%u r=%d\n",(unsigned)bi,(unsigned)ci,r);return 1;}
    pose_count++;
    for(pi=0u;pi<m.body.part_count;pi++){
     const re3c_mesh_part*p=&m.body.parts[pi];if(!re3c_part_should_render_default(p))continue;
     if(p->part_class==RE3C_PART_EXTRA_UNBOUND||p->part_class==RE3C_PART_DYNAMIC_MORPH)continue;
     for(vi=0u;vi<p->vertex_count;vi+=(p->vertex_count/8u)+1u){
      re3c_vec3fx result;r=re3c_animated_body_vertex(&m,bi,mats,pi,vi,&result);
      if(r!=RE3C_OK){fprintf(stderr,"vertex error bank=%u clip=%u part=%u vertex=%u r=%d\n",(unsigned)bi,(unsigned)ci,(unsigned)pi,(unsigned)vi,r);return 1;}
      vertex_count++;
     }
    }
   }
  }
 }
 printf("OK clips=%u sampled_poses=%u sampled_vertices=%u skipped_incompatible_clips_or_keys=%u\n",(unsigned)clips,(unsigned)pose_count,(unsigned)vertex_count,(unsigned)skipped);
 re3c_model_close(&m);return 0;
}
