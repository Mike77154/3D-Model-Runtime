/* Read-only PLD/PLW model and texture validation. C89, heap-free. */
#include "re3_packed.h"
#include <stdio.h>
#define SCRATCH_SIZE (256u*1024u)
static re3c_model model;
static re3c_tim_vram tim;
static re3c_u8 scratch[SCRATCH_SIZE];
int main(int argc,char**argv){re3p_info info;re3c_u32 i,p,f,corners=0u,sampled=0u;int r;const char *type;
 if(argc!=2){fprintf(stderr,"usage: re3packed_info file.PLD|file.PLW\n");return 2;}
 re3c_model_init(&model);r=re3p_open_model_file(&model,argv[1],&info);
 if(r!=RE3C_OK){fprintf(stderr,"open error: %s\n",re3c_result_string(r));return 1;}
 type=info.kind==RE3P_KIND_PLD?"PLD":"PLW";
 r=re3p_load_tim(argv[1],&info,scratch,sizeof(scratch),&tim);
 if(r!=RE3C_OK){fprintf(stderr,"TIM error: %s\n",re3c_result_string(r));return 1;}
 for(p=0;p<model.body.part_count;p++){
  const re3c_mesh_part *mp=&model.body.parts[p];
  for(f=0;f<mp->face_count;f++){
   const re3c_face *face=&model.body.faces[mp->first_face+f];
   re3c_u32 k;
   for(k=0u;k<face->vertex_count;k++){
    re3c_u32 argb;
    r=re3c_tim_sample_argb(&tim,face->page,face->clut,face->uv[k].u,face->uv[k].v,&argb);
    if(r!=RE3C_OK){fprintf(stderr,"pixel sample error part=%u face=%u corner=%u code=%d\n",(unsigned)p,(unsigned)f,(unsigned)k,r);return 1;}
    corners++;
   }
  }
  for(i=0;i<mp->vertex_count;i++){
   re3c_vec3fx v;
   r=re3c_reference_body_vertex(&model,p,i,&v);
   if(r!=RE3C_OK){fprintf(stderr,"vertex error: %d\n",r);return 1;}
   sampled++;
  }
 }
 printf("%s,%s,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u\n",argv[1],type,
  (unsigned)info.segment_count,(unsigned)model.body.part_count,
  (unsigned)model.body.vertex_count,(unsigned)model.body.face_count,
  (unsigned)model.banks[RE3C_BANK_A].present,
  (unsigned)model.banks[RE3C_BANK_A].joint_count,
  (unsigned)model.banks[RE3C_BANK_A].clip_count,
  (unsigned)corners,(unsigned)sampled,(unsigned)info.file_size,
  (unsigned)model.bind_a.present,(unsigned)model.bind_a.mapped_limb_count,
  (unsigned)model.bind_a.inverse_bind_valid);
 re3c_model_close(&model);return 0;
}
