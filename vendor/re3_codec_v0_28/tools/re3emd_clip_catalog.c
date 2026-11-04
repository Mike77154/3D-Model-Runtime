/* RE3 EMD clip catalogue: C89 strict, fixed-size static, no allocations.
 * All keys are decoded. Geometry suitability is sampled (first, middle, last).
 * This tool never guesses gameplay event names from clip numbers. */
#include "re3_codec.h"
#include <stdio.h>
#include <string.h>
static re3c_model model;
static re3c_pose pose;
static re3c_mat34fx matrices[RE3C_MAX_PARTS];
static re3c_u32 sample_keys[3];
static const char *safety(const re3c_anim_bank *b,const re3c_anim_bank *main_bank){
 if(!main_bank->present)return "no_main_bank";
 if(b->joint_count!=main_bank->joint_count)return "needs_retarget";
 if(b->hierarchy_source==255u)return "hierarchy_unavailable";
 return "testable";
}
int main(int argc,char **argv){
 re3c_u32 bi,ci,ki,p,k,verified=0u,failures=0u,totalclips=0u,renderable=0u;
 FILE *f=stdout;int r,ret=0;
 if(argc!=2&&argc!=3){fprintf(stderr,"usage: re3emd_clip_catalog model.EMD [output.csv]\n");return 2;}
 re3c_model_init(&model);r=re3c_open_file(&model,argv[1]);if(r!=RE3C_OK){fprintf(stderr,"open: %s\n",re3c_result_string(r));return 1;}
 if(argc==3){f=fopen(argv[2],"wb");if(!f){fprintf(stderr,"cannot open output\n");re3c_model_close(&model);return 1;}}
 fputs("bank,clip,keys,joints,decoded_keys,pose_errors,preview_sampled,preview_errors,render_status\n",f);
 for(bi=0u;bi<RE3C_BANK_COUNT;bi++){
  const re3c_anim_bank *b=&model.banks[bi];if(!b->present)continue;
  for(ci=0u;ci<b->clip_count;ci++){
   const re3c_clip *c=&b->clips[ci];re3c_u32 ok=0u,bad=0u,prev_ok=0u,prev_bad=0u,s;
   const char *status=safety(b,&model.banks[RE3C_BANK_A]);
   totalclips++;
   for(ki=0u;ki<c->key_count;ki++){
    r=re3c_decode_pose_by_key(&model,bi,ci,ki,&pose);
    if(r==RE3C_OK)ok++;else bad++;
   }
   verified+=ok;failures+=bad;
   if(!strcmp(status,"testable")&&c->key_count>0u){
    sample_keys[0]=0u;sample_keys[1]=c->key_count/2u;sample_keys[2]=c->key_count-1u;
    for(s=0u;s<3u;s++){
     r=re3c_decode_pose_by_key(&model,bi,ci,sample_keys[s],&pose);
     if(r==RE3C_OK)r=re3c_build_joint_matrices(b,&pose,RE3C_ROOT_MODEL_SPACE,matrices);
     if(r==RE3C_OK){
      for(p=0u;p<model.body.part_count;p++){
       const re3c_mesh_part *part=&model.body.parts[p];
       if(!re3c_part_should_render_default(part))continue;
       if(part->part_class==RE3C_PART_EXTRA_UNBOUND||part->part_class==RE3C_PART_DYNAMIC_MORPH)continue;
       for(k=0u;k<part->vertex_count;k+=(part->vertex_count/8u)+1u){
        re3c_vec3fx v;
        r=re3c_animated_body_vertex(&model,bi,matrices,p,k,&v);
        if(r!=RE3C_OK)break;
       }
       if(r!=RE3C_OK)break;
      }
     }
     if(r==RE3C_OK)prev_ok++;else prev_bad++;
    }
    if(prev_bad==0u)renderable++;
   }
   fprintf(f,"%u,%u,%u,%u,%u,%u,%u,%u,%s\n",(unsigned)bi,(unsigned)ci,
    (unsigned)c->key_count,(unsigned)b->joint_count,(unsigned)ok,(unsigned)bad,
    (unsigned)prev_ok,(unsigned)prev_bad,status);
   if(bad!=0u||prev_bad!=0u)ret=1;
  }
 }
 if(f!=stdout&&fclose(f)!=0)ret=1;
 fprintf(stderr,"clips=%u decoded_keys=%u failures=%u safe_preview_clips=%u\n",
  (unsigned)totalclips,(unsigned)verified,(unsigned)failures,(unsigned)renderable);
 re3c_model_close(&model);return ret;
}
