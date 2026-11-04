/* v0.23: independent all-vertex test of extracted binary01 Q12 morphs.
 * ISO C89, integer only, static memory; optional corpus CSV mode via shell. */
#include "re3_codec.h"
#include <stdio.h>
#include <string.h>
static re3c_model m;
static re3c_u16 weights[256];
static re3c_i32 floor_q12(re3c_i32 v,re3c_u32 w){
 re3c_i32 p=v*(re3c_i32)w;
 if(p>=0)return p/4096;
 return -( ((-p)/4096) + (((-p)%4096)!=0 ? 1 : 0) );
}
int main(int argc,char**argv){
 re3c_u32 part,t,k,windex,n,v,track_count=0u,vertex_count=0u,zero_mismatches=0u;
 re3c_u32 tested=0u,overflow=0u,invalid=0u,full_different=0u,multi_tested=0u;
 int r;
 if(argc!=2){fprintf(stderr,"usage: %s file.EMD\n",argv[0]);return 2;}
 re3c_model_init(&m);
 r=re3c_open_file(&m,argv[1]);if(r!=RE3C_OK){fprintf(stderr,"open fail %s\n",re3c_result_string(r));return 1;}
 for(part=0u;part<m.body.part_count;part++){
  const re3c_mesh_part *p=&m.body.parts[part];
  r=re3c_morph_track_for_part(&m,part,&t);if(r==RE3C_ERR_NOT_PRESENT)continue;
  if(r!=RE3C_OK)return 1;
  n=(re3c_u32)m.binary01_morph.tracks[t].frame_count-1u;
  if(n>256u)return 1;
  track_count++;
  for(v=0u;v<p->vertex_count;v++){
   const re3c_vec3fx*orig=&m.body.vertices[p->first_vertex+v].pos;
   re3c_vec3fx base,actual,delta;
   vertex_count++;
   r=re3c_morph_vertex(&m,part,0u,v,&base);if(r!=RE3C_OK)return 1;
   if(orig->x!=base.x||orig->y!=base.y||orig->z!=base.z)zero_mismatches++;
   for(k=0u;k<n;k++){
    r=re3c_morph_vertex(&m,part,k+1u,v,&delta);if(r!=RE3C_OK)return 1;
    for(windex=0u;windex<=4u;windex++){
     re3c_u32 i,w=windex*1024u;
     re3c_i32 xx,yy,zz;
     for(i=0u;i<n;i++)weights[i]=0u;
     weights[k]=(re3c_u16)w;
     xx=base.x/4096+floor_q12(delta.x/4096,w);
     yy=base.y/4096+floor_q12(delta.y/4096,w);
     zz=base.z/4096+floor_q12(delta.z/4096,w);
     r=re3c_morph_blend_local_vertex(&m,part,v,weights,n,&actual);
     if(xx>32767||xx< -32768||yy>32767||yy< -32768||zz>32767||zz< -32768){
       if(r!=RE3C_ERR_OVERFLOW){invalid++;}else overflow++;
       continue;
     }
     if(r!=RE3C_OK||actual.x!=xx*4096||actual.y!=yy*4096||actual.z!=zz*4096)invalid++;
     tested++;
     if(windex==4u&&(orig->x+delta.x!=actual.x||orig->y+delta.y!=actual.y||orig->z+delta.z!=actual.z))full_different++;
    }
   }
   if(n>=2u){
    re3c_vec3fx delta_a,delta_b;
    re3c_i32 ex,ey,ez;
    for(k=0u;k<n;k++)weights[k]=0u;
    weights[0]=1024u;weights[1]=2048u;
    if(re3c_morph_vertex(&m,part,1u,v,&delta_a)!=RE3C_OK)return 1;
    if(re3c_morph_vertex(&m,part,2u,v,&delta_b)!=RE3C_OK)return 1;
    ex=base.x/4096+floor_q12(delta_a.x/4096,1024u)+floor_q12(delta_b.x/4096,2048u);
    ey=base.y/4096+floor_q12(delta_a.y/4096,1024u)+floor_q12(delta_b.y/4096,2048u);
    ez=base.z/4096+floor_q12(delta_a.z/4096,1024u)+floor_q12(delta_b.z/4096,2048u);
    r=re3c_morph_blend_local_vertex(&m,part,v,weights,n,&actual);
    if(ex>32767||ex< -32768||ey>32767||ey< -32768||ez>32767||ez< -32768){
      if(r!=RE3C_ERR_OVERFLOW)invalid++;
      else overflow++;
    } else {
      if(r!=RE3C_OK||actual.x!=ex*4096||actual.y!=ey*4096||actual.z!=ez*4096)invalid++;
      multi_tested++;
    }
   }
  }
 }
 printf("file=%s tracks=%u base_vertices=%u frame0_mesh_diff=%u tested=%u overflows=%u mismatches=%u full_vs_v21_delta_diff=%u multi_stream_tested=%u\n",
  argv[1],(unsigned)track_count,(unsigned)vertex_count,(unsigned)zero_mismatches,(unsigned)tested,(unsigned)overflow,(unsigned)invalid,(unsigned)full_different,(unsigned)multi_tested);
 re3c_model_close(&m);return invalid?1:0;
}
