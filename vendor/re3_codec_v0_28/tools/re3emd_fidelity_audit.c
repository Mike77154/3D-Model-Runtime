/* RE3 classic family-specific structural/animation fidelity audit.
 * C89, heap free, fixed-point. No external dependencies.
 * This is a diagnostics tool, not a claim of pixel-identical engine behavior. */
#include "re3_codec.h"
#include <stdio.h>
#include <string.h>

static re3c_model model;
static re3c_tim_vram tim;
static re3c_pose pose;
static re3c_mat34fx joint_mats[RE3C_MAX_PARTS];

int main(int argc,char **argv){
 re3c_u32 b,c,k,p,v,checked=0u,renderable=0u,hidden=0u;
 re3c_u32 morph_visible=0u,morph_hidden=0u,poses=0u,keys=0u;
 re3c_u32 mismatched=0u,missing_pose=0u,tex_tests=0u,tex_failed=0u;
 re3c_u32 compatible_clips=0u,incompatible_clips=0u;
 int r;
 if(argc!=3){fprintf(stderr,"usage: %s model.EMD texture.TIM\n",argv[0]);return 2;}
 re3c_model_init(&model);
 r=re3c_open_file(&model,argv[1]);
 if(r!=RE3C_OK){fprintf(stderr,"open: %s\n",re3c_result_string(r));return 1;}
 re3c_tim_init(&tim);
 r=re3c_tim_load_file(&tim,argv[2]);
 if(r!=RE3C_OK){fprintf(stderr,"TIM: %s\n",re3c_result_string(r));return 1;}
 for(p=0u;p<model.body.part_count;p++){
  const re3c_mesh_part*part=&model.body.parts[p];
  int visible=re3c_part_should_render_default(part) &&
   part->part_class!=RE3C_PART_EXTRA_UNBOUND &&
   part->part_class!=RE3C_PART_DYNAMIC_MORPH;
  if(visible)renderable++;else hidden++;
  if(part->morph_track>=0){if(visible)morph_visible++;else morph_hidden++;}
  if(visible){
   for(v=0u;v<part->face_count;v++){
    const re3c_face*face=&model.body.faces[part->first_face+v];
    re3c_u32 a;
    for(a=0;a<face->vertex_count;a++){
     re3c_u32 color;
     r=re3c_tim_sample_argb(&tim,face->page,face->clut,face->uv[a].u,face->uv[a].v,&color);
     tex_tests++;if(r!=RE3C_OK)tex_failed++;
    }
   }
  }
 }
 for(b=0u;b<RE3C_BANK_COUNT;b++){
  const re3c_anim_bank*ab=&model.banks[b];
  if(!ab->present)continue;
  if(!model.banks[RE3C_BANK_A].present||
     ab->joint_count!=model.banks[RE3C_BANK_A].joint_count){
   incompatible_clips+=ab->clip_count;
   mismatched++;
   continue;
  }
  for(c=0u;c<ab->clip_count;c++){
   const re3c_clip*clip=&ab->clips[c];
   if(!clip->key_count)continue;
   compatible_clips++;
   for(k=0u;k<clip->key_count;k++){
    re3c_u32 j;
    r=re3c_decode_pose_by_key(&model,b,c,k,&pose);
    keys++;
    if(r==RE3C_ERR_NOT_PRESENT){missing_pose++;continue;}
    if(r!=RE3C_OK){
     fprintf(stderr,"bad pose bank=%u clip=%u key=%u rc=%d\n",
       (unsigned)b,(unsigned)c,(unsigned)k,r);
     return 1;
    }
    r=re3c_build_joint_matrices(ab,&pose,RE3C_ROOT_MODEL_SPACE,joint_mats);
    if(r!=RE3C_OK){fprintf(stderr,"bad matrices bank=%u clip=%u key=%u\n",
     (unsigned)b,(unsigned)c,(unsigned)k);return 1;}
    poses++;
    /* Check actual animated transform for one vertex of each rendered body part.
       The renderer separately smoke-tests all triangles/UVs. */
    for(j=0u;j<model.body.part_count;j++){
     const re3c_mesh_part*part=&model.body.parts[j];
     re3c_vec3fx position;
     if(!part->vertex_count||!re3c_part_should_render_default(part)||
        part->part_class==RE3C_PART_EXTRA_UNBOUND||
        part->part_class==RE3C_PART_DYNAMIC_MORPH)continue;
     r=re3c_animated_body_vertex(&model,b,joint_mats,j,0u,&position);
     if(r!=RE3C_OK){fprintf(stderr,"bad body vertex bank=%u clip=%u key=%u part=%u\n",
      (unsigned)b,(unsigned)c,(unsigned)k,(unsigned)j);return 1;}
     checked++;
    }
   }
  }
 }
 printf("model,parts,render_parts,hidden_parts,morph_tracks,visible_morph_parts,hidden_morph_parts,normal_joints,compatible_clips,incompatible_clips,incompatible_banks,total_keys,decoded_poses,missing_pose_keys,animated_vertices,texture_corners,texture_errors\n");
 printf("%s,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u\n",argv[1],
  (unsigned)model.body.part_count,(unsigned)renderable,(unsigned)hidden,
  (unsigned)model.binary01_morph.track_count,(unsigned)morph_visible,(unsigned)morph_hidden,
  (unsigned)model.banks[RE3C_BANK_A].joint_count,(unsigned)compatible_clips,
  (unsigned)incompatible_clips,(unsigned)mismatched,(unsigned)keys,(unsigned)poses,
  (unsigned)missing_pose,(unsigned)checked,(unsigned)tex_tests,(unsigned)tex_failed);
 re3c_model_close(&model);
 return tex_failed?1:0;
}
