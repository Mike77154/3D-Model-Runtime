/* Corpus reference-pose audit, C89 / fixed-point / heap-free. */
#include "re3_codec.h"
#include <stdio.h>
static re3c_model m;
static re3c_i32 abs32(re3c_i32 x){return x<0?-x:x;}
int main(int argc,char**argv){re3c_u32 p,v,translated=0u,optional=0u,seams=0u,owners=0u,unmapped=0u,maxabs=0u;int rc;
 if(argc!=2){fprintf(stderr,"usage: %s model.EMD\n",argv[0]);return 2;}
 re3c_model_init(&m);rc=re3c_open_file(&m,argv[1]);if(rc!=RE3C_OK){fprintf(stderr,"open %s: %s\n",argv[1],re3c_result_string(rc));return 1;}
 for(p=0;p<m.body.part_count;p++){const re3c_mesh_part*part=&m.body.parts[p];
  if(part->part_class==RE3C_PART_DYNAMIC_MORPH||part->part_class==RE3C_PART_EXTRA_UNBOUND)optional++;
  for(v=0;v<part->vertex_count;v++){re3c_vec3fx out,raw=m.body.vertices[part->first_vertex+v].pos;re3c_i32 ax,ay,az;re3c_i16 joint;re3c_u8 j0,j1,w;
   rc=re3c_reference_body_vertex(&m,p,v,&out);if(rc!=RE3C_OK){fprintf(stderr,"ref failed: part=%u vertex=%u code=%d\n",(unsigned)p,(unsigned)v,rc);return 1;}
   if(out.x!=raw.x||out.y!=raw.y||out.z!=raw.z)translated++;
   ax=abs32(re3c_fx_to_int(out.x));ay=abs32(re3c_fx_to_int(out.y));az=abs32(re3c_fx_to_int(out.z));
   if((re3c_u32)ax>maxabs)maxabs=(re3c_u32)ax;
   if((re3c_u32)ay>maxabs)maxabs=(re3c_u32)ay;
   if((re3c_u32)az>maxabs)maxabs=(re3c_u32)az;
   if(part->part_class==RE3C_PART_DAT_SKINNED){if(re3c_bind_seam_for_vertex(&m.bind_a,p,v,&j0,&j1,&w)==RE3C_OK)seams++;
    else if(re3c_bind_joint_for_vertex(&m.bind_a,p,v,0,&joint)==RE3C_OK||re3c_bind_joint_for_vertex(&m.bind_a,p,v,1,&joint)==RE3C_OK)owners++;
    else unmapped++;}
  }
 }
 if(maxabs>100000u){fprintf(stderr,"WARNING huge model coordinate %u\n",(unsigned)maxabs);return 1;}
 printf("OK file=%s parts=%u vertices=%u faces=%u translated=%u seam=%u owner=%u fallback=%u optional_parts=%u maxabs=%u\n",argv[1],(unsigned)m.body.part_count,(unsigned)m.body.vertex_count,(unsigned)m.body.face_count,(unsigned)translated,(unsigned)seams,(unsigned)owners,(unsigned)unmapped,(unsigned)optional,(unsigned)maxabs);
 re3c_model_close(&m);return 0;
}
