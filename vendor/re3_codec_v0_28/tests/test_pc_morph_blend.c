#include "re3_codec.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static re3c_model m;
static re3c_mat34fx mat[RE3C_MAX_PARTS];
static re3c_u16 w[512];
static re3c_i32 sar12(re3c_i32 x){return x>=0?x/4096:-((-x+4095)/4096);}
int main(int argc,char**argv){re3c_u32 part,v,t,nt=0;int rc;re3c_vec3fx a,b,d,c;re3c_pose pose;
 if(argc!=2){fprintf(stderr,"usage: %s EM30.EMD\n",argv[0]);return 2;}
 re3c_model_init(&m);rc=re3c_open_file(&m,argv[1]);if(rc!=RE3C_OK)return 1;
 for(part=0;part<m.body.part_count;part++){
  rc=re3c_morph_track_for_part(&m,part,&t);if(rc==RE3C_ERR_NOT_PRESENT)continue;
  if(rc!=RE3C_OK)return 1;
  nt=(re3c_u32)m.binary01_morph.tracks[t].frame_count-1u;
  if(nt>512u)return 1;
  memset(w,0,sizeof(w));v=0u;
  rc=re3c_morph_blend_local_vertex(&m,part,v,w,nt,&a);if(rc!=RE3C_OK)return 1;
  rc=re3c_morph_vertex(&m,part,0u,v,&b);if(rc!=RE3C_OK)return 1;
  if(a.x!=b.x||a.y!=b.y||a.z!=b.z){puts("FAIL initial morph");return 1;}
  if(nt){
   w[0]=4096u;rc=re3c_morph_blend_local_vertex(&m,part,v,w,nt,&a);if(rc!=RE3C_OK)return 1;
   rc=re3c_morph_vertex(&m,part,1u,v,&d);if(rc!=RE3C_OK)return 1;
   if(a.x!=b.x+d.x||a.y!=b.y+d.y||a.z!=b.z+d.z){puts("FAIL full additive");return 1;}
   w[0]=2048u;rc=re3c_morph_blend_local_vertex(&m,part,v,w,nt,&c);if(rc!=RE3C_OK)return 1;
   if(c.x!=b.x+sar12((d.x/4096)*2048)*4096||c.y!=b.y+sar12((d.y/4096)*2048)*4096||c.z!=b.z+sar12((d.z/4096)*2048)*4096){puts("FAIL half-weight");return 1;}
   w[0]=4097u;if(re3c_morph_blend_local_vertex(&m,part,v,w,nt,&c)!=RE3C_ERR_RANGE){puts("FAIL weight range");return 1;}
  }
  printf("PASS part=%u weights=%u\n",(unsigned)part,(unsigned)nt);
 }
 rc=re3c_decode_pose_by_key(&m,RE3C_BANK_A,0u,0u,&pose);if(rc!=RE3C_OK)return 1;
 rc=re3c_build_joint_matrices(&m.banks[RE3C_BANK_A],&pose,RE3C_ROOT_MODEL_SPACE,mat);if(rc!=RE3C_OK)return 1;
 for(part=0;part<m.body.part_count;part++){
  rc=re3c_morph_track_for_part(&m,part,&t);if(rc!=RE3C_OK)continue;
  nt=(re3c_u32)m.binary01_morph.tracks[t].frame_count-1u;
  memset(w,0,sizeof(w));if(nt)w[0]=4096u;
  rc=re3c_animated_body_vertex_morph_blend(&m,RE3C_BANK_A,mat,part,0u,w,nt,&a);
  if(rc!=RE3C_OK){printf("FAIL animated part=%u rc=%d\n",(unsigned)part,rc);return 1;}
 }
 re3c_model_close(&m);puts("PASS weighted morph blend and animation");return 0;
}
