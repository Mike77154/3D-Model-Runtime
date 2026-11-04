/* v0.19 explicit state tests. C89, bounded, no heap. */
#include "re3_codec.h"
#include <stdio.h>
#include <string.h>
static re3c_model model;
static re3c_visual_state state;
static re3c_mat34fx matrices[RE3C_MAX_PARTS];
int main(int argc,char **argv){
 re3c_u32 p;int r;re3c_u32 old;
 re3c_pose pose;re3c_vec3fx rest,one,two;
 re3c_model_init(&model);
 /* The static fixture intentionally only exercises state validation. */
 model.is_open=1u;model.body.part_count=3u;
 model.body.parts[0].part_flags=RE3C_PART_FLAG_RENDER_DEFAULT;
 model.body.parts[1].part_flags=RE3C_PART_FLAG_RENDER_DEFAULT;
 model.body.parts[1].part_class=RE3C_PART_DYNAMIC_MORPH;
 model.body.parts[2].part_flags=0u;
 model.binary01_morph.present=1u;model.binary01_morph.track_count=1u;
 model.binary01_morph.tracks[0].present=1u;
 model.binary01_morph.tracks[0].part_index=1u;
 model.binary01_morph.tracks[0].frame_count=3u;
 r=re3c_visual_state_init(&model,&state,0);
 if(r!=RE3C_OK||state.visible[0]!=1u||state.visible[1]!=0u||state.visible[2]!=0u)return 1;
 for(p=0u;p<3u;p++)if(state.morph_frame[p]!=RE3C_MORPH_REST)return 1;
 if(re3c_visual_state_set_visibility(&model,&state,1u,1)!=RE3C_OK||state.visible[1]!=1u)return 1;
 if(re3c_visual_state_set_visibility(&model,&state,3u,1)!=RE3C_ERR_RANGE)return 1;
 if(re3c_visual_state_set_visibility(&model,&state,1u,2)!=RE3C_ERR_BAD_ARGUMENT||state.visible[1]!=1u)return 1;
 if(re3c_visual_state_set_morph(&model,&state,1u,2u)!=RE3C_OK||state.morph_frame[1]!=2u)return 1;
 if(re3c_visual_state_set_morph(&model,&state,1u,3u)!=RE3C_ERR_RANGE||state.morph_frame[1]!=2u)return 1;
 if(re3c_visual_state_set_morph(&model,&state,0u,1u)!=RE3C_ERR_NOT_PRESENT)return 1;
 if(re3c_visual_state_set_morph(&model,&state,1u,RE3C_MORPH_REST)!=RE3C_OK)return 1;
 if(re3c_visual_state_init(&model,&state,1)!=RE3C_OK||state.visible[1]!=1u)return 1;
 model.is_open=0u;
 if(re3c_visual_state_init(&model,&state,0)!=RE3C_ERR_NOT_OPEN)return 1;
 puts("PASS visual state synthetic validation");
 if(argc<2)return 0;
 re3c_model_init(&model);
 r=re3c_open_file(&model,argv[1]);if(r!=RE3C_OK){puts("FAIL corpus open");return 1;}
 r=re3c_visual_state_init(&model,&state,0);if(r!=RE3C_OK)return 1;
 if(model.body.part_count<=21u||model.body.parts[1].morph_frame_count<2u||model.body.parts[16].morph_frame_count<8u){puts("FAIL unexpected EM36 format");return 1;}
 if(state.visible[16]!=0u)return 1;
 if(re3c_visual_state_set_visibility(&model,&state,16u,1)!=RE3C_OK)return 1;
 if(re3c_visual_state_set_morph(&model,&state,1u,1u)!=RE3C_OK)return 1;
 if(re3c_visual_state_set_morph(&model,&state,16u,7u)!=RE3C_OK)return 1;
 old=state.morph_frame[16];
 if(re3c_visual_state_set_morph(&model,&state,16u,8u)!=RE3C_ERR_RANGE||state.morph_frame[16]!=old)return 1;
 r=re3c_decode_pose_by_key(&model,RE3C_BANK_A,1u,0u,&pose);if(r!=RE3C_OK)return 1;
 r=re3c_build_joint_matrices(&model.banks[RE3C_BANK_A],&pose,RE3C_ROOT_MODEL_SPACE,matrices);if(r!=RE3C_OK)return 1;
 r=re3c_animated_body_vertex(&model,RE3C_BANK_A,matrices,1u,0u,&rest);if(r!=RE3C_OK)return 1;
 r=re3c_animated_body_vertex_state(&model,RE3C_BANK_A,matrices,&state,1u,0u,&one);if(r!=RE3C_OK)return 1;
 r=re3c_animated_body_vertex_state(&model,RE3C_BANK_A,matrices,&state,16u,0u,&two);if(r!=RE3C_OK)return 1;
 if(rest.x==one.x&&rest.y==one.y&&rest.z==one.z){puts("FAIL state morph no vertex change");return 1;}
 if(re3c_visual_state_set_morph(&model,&state,1u,RE3C_MORPH_REST)!=RE3C_OK)return 1;
 r=re3c_animated_body_vertex_state(&model,RE3C_BANK_A,matrices,&state,1u,0u,&one);if(r!=RE3C_OK)return 1;
 if(rest.x!=one.x||rest.y!=one.y||rest.z!=one.z)return 1;
 puts("PASS EM36 independent per-part morph/visibility and revert");
 re3c_model_close(&model);
 return 0;
}
