/* Test all damage keys through an explicit experimental body-rig projection.
 * C89, no heap, integer-only. Checks execution, not Capcom equivalence. */
#include "re3_codec.h"
#include <stdio.h>
#include <string.h>
static re3c_model model;
static re3c_mat34fx mats[RE3C_MAX_PARTS];
int main(int argc,char **argv){
 re3c_u32 family,clip,key,part,v,checked=0u;int r;re3c_i32 max_span=0;
 if(argc!=3){fprintf(stderr,"usage: %s model.EMD nemesis|grave\n",argv[0]);return 2;}
 family=strcmp(argv[2],"nemesis")==0?RE3C_DAMAGE_FAMILY_NEMESIS:(strcmp(argv[2],"grave")==0?RE3C_DAMAGE_FAMILY_GRAVEDIGGER:0u);
 if(!family)return 2;
 re3c_model_init(&model);r=re3c_open_file(&model,argv[1]);
 if(r!=RE3C_OK){fprintf(stderr,"open: %s\n",re3c_result_string(r));return 1;}
 r=re3c_damage_family_check(&model,family);
 if(r!=RE3C_OK){fprintf(stderr,"unrecognized rig for this projection\n");return 1;}
 puts("clip,keys,checked_vertices,largest_axis_span_model_units,failed");
 for(clip=0u;clip<model.banks[RE3C_BANK_DAMAGE].clip_count;clip++){
  re3c_u32 count=model.banks[RE3C_BANK_DAMAGE].clips[clip].key_count,clip_checked=0u,fail=0u;
  max_span=0;
  for(key=0u;key<count;key++){
   re3c_pose src,dst;
   re3c_i32 minx=0,miny=0,minz=0,maxx=0,maxy=0,maxz=0;
   int first=1;
   r=re3c_decode_pose_by_key(&model,RE3C_BANK_DAMAGE,clip,key,&src);
   if(r!=RE3C_OK){fail++;break;}
   if(family==RE3C_DAMAGE_FAMILY_NEMESIS)r=re3c_retarget_nemesis_damage_experimental(&model,&src,&dst);
   else r=re3c_retarget_gravedigger_damage_experimental(&model,&src,&dst);
   if(r!=RE3C_OK){fail++;break;}
   r=re3c_build_joint_matrices(&model.banks[RE3C_BANK_A],&dst,RE3C_ROOT_MODEL_SPACE,mats);
   if(r!=RE3C_OK){fail++;break;}
   for(part=0u;part<model.body.part_count;part++){
    const re3c_mesh_part *p=&model.body.parts[part];
    if(!re3c_part_should_render_default(p)||p->part_class==RE3C_PART_EXTRA_UNBOUND||p->part_class==RE3C_PART_DYNAMIC_MORPH)continue;
    for(v=0u;v<p->vertex_count;v++){
     re3c_vec3fx pos;re3c_i32 x,y,z;
     r=re3c_animated_body_vertex(&model,RE3C_BANK_A,mats,part,v,&pos);
     if(r!=RE3C_OK){fail++;break;}
     x=re3c_fx_to_int(pos.x);y=re3c_fx_to_int(pos.y);z=re3c_fx_to_int(pos.z);
     if(first){minx=maxx=x;miny=maxy=y;minz=maxz=z;first=0;}
     else {if(x<minx)minx=x;if(x>maxx)maxx=x;if(y<miny)miny=y;if(y>maxy)maxy=y;if(z<minz)minz=z;if(z>maxz)maxz=z;}
     clip_checked++;
    }
    if(fail)break;
   }
   if(fail)break;
   {
    re3c_i32 dx=maxx-minx,dy=maxy-miny,dz=maxz-minz;
    if(dx>max_span)max_span=dx;
    if(dy>max_span)max_span=dy;
    if(dz>max_span)max_span=dz;
   }
  }
  printf("%u,%u,%u,%d,%u\n",(unsigned)clip,(unsigned)count,(unsigned)clip_checked,(int)max_span,(unsigned)fail);
  checked+=clip_checked;
  if(fail){fprintf(stderr,"damage stress failed at clip %u\n",(unsigned)clip);return 1;}
 }
 fprintf(stderr,"verified %u transformed vertices over all decoded damage keys (experimental maps)\n",(unsigned)checked);
 re3c_model_close(&model);return 0;
}
