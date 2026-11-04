/* C89 real-corpus check (EM30): the delta hypothesis must preserve local
 * vertex positioning and differ from the prior absolute-frame interpretation.
 * No assertion of engine-level fidelity. */
#include "re3_codec.h"
#include <stdio.h>
static re3c_model m;
static re3c_mat34fx mats[RE3C_MAX_PARTS];
int main(int argc,char**argv){re3c_pose pose;re3c_vec3fx rest,raw,delta,center,offset;re3c_u32 p=3u,v=0u;int r;
 if(argc!=2){fprintf(stderr,"usage: test_morph_delta EM30.EMD\n");return 2;}
 re3c_model_init(&m);r=re3c_open_file(&m,argv[1]);if(r!=RE3C_OK)return 1;
 if(m.body.part_count<=p||m.body.parts[p].morph_frame_count<=1u)return 1;
 r=re3c_decode_pose_by_key(&m,RE3C_BANK_A,4u,0u,&pose);if(r!=RE3C_OK)return 1;
 r=re3c_build_joint_matrices(&m.banks[RE3C_BANK_A],&pose,RE3C_ROOT_MODEL_SPACE,mats);if(r!=RE3C_OK)return 1;
 r=re3c_animated_body_vertex(&m,RE3C_BANK_A,mats,p,v,&rest);if(r!=RE3C_OK)return 1;
 r=re3c_animated_body_vertex_morph(&m,RE3C_BANK_A,mats,p,v,1u,&raw);if(r!=RE3C_OK)return 1;
 r=re3c_animated_body_vertex_morph_delta(&m,RE3C_BANK_A,mats,p,v,1u,&delta);if(r!=RE3C_OK)return 1;
 if(delta.x==raw.x&&delta.y==raw.y&&delta.z==raw.z){puts("FAIL morph delta identical to absolute");return 1;}
 r=re3c_morph_center_translation(&m,p,1u,&offset);if(r!=RE3C_OK)return 1;
 r=re3c_animated_body_vertex_morph_offset(&m,RE3C_BANK_A,mats,p,v,1u,&offset,&center);if(r!=RE3C_OK)return 1;
 if(re3c_animated_body_vertex_morph_delta(&m,RE3C_BANK_A,mats,p,v,100u,&delta)!=RE3C_ERR_RANGE){puts("FAIL range");return 1;}
 if(re3c_animated_body_vertex_morph_delta(&m,RE3C_BANK_A,mats,p,v,RE3C_MORPH_REST,&delta)!=RE3C_OK||delta.x!=rest.x||delta.y!=rest.y||delta.z!=rest.z){puts("FAIL rest");return 1;}
 if(re3c_morph_center_translation(&m,p,RE3C_MORPH_REST,&offset)!=RE3C_OK||offset.x||offset.y||offset.z){puts("FAIL rest offset");return 1;}
 printf("PASS morph hypotheses on EM30: raw_x=%d recentered_x=%d delta_x=%d rest_x=%d\n",re3c_fx_to_int(raw.x),re3c_fx_to_int(center.x),re3c_fx_to_int(delta.x),re3c_fx_to_int(rest.x));
 re3c_model_close(&m);return 0;}
