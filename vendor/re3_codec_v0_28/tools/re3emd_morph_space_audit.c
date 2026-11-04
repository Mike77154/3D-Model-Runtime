/* RE3 v0.21: separate absolute frame-0 geometry from delta-like later frames.
 * Evidence report only: this DOES NOT prove the game's runtime semantics.
 * Strict C89, bounded, no heap, no floating point. */
#include "re3_codec.h"
#include <stdio.h>
static re3c_model model;
static re3c_u32 abs32(re3c_i32 n){return (re3c_u32)(n<0?-n:n);}
int main(int argc,char **argv){re3c_u32 t,f,v;unsigned long frames=0ul,vertices=0ul,frame0_matches=0ul,delta_candidates=0ul;int r;FILE *out=stdout;
 if(argc!=2&&argc!=3){fprintf(stderr,"usage: re3emd_morph_space_audit file.EMD [report.csv]\n");return 2;}
 re3c_model_init(&model);r=re3c_open_file(&model,argv[1]);if(r!=RE3C_OK){fprintf(stderr,"open %s\n",re3c_result_string(r));return 1;}
 if(argc==3){out=fopen(argv[2],"wb");if(!out){fprintf(stderr,"output\n");return 1;}}
 fputs("part,frame,vertices,frame0_equal,base_cx,base_cy,base_cz,morph_cx,morph_cy,morph_cz,delta_candidate\n",out);
 for(t=0;t<model.binary01_morph.track_count;t++){
  const re3c_morph_track *tr=&model.binary01_morph.tracks[t];
  re3c_u32 p=(re3c_u32)tr->part_index,match0=0u,n=tr->vertex_count;
  re3c_i32 bx=0,by=0,bz=0;
  if(!n||n>256u)return 1;
  for(v=0;v<n;v++){const re3c_vec3fx*base=&model.body.vertices[model.body.parts[p].first_vertex+v].pos;
   bx+=re3c_fx_to_int(base->x);by+=re3c_fx_to_int(base->y);bz+=re3c_fx_to_int(base->z);
  }
  bx/=(re3c_i32)n;by/=(re3c_i32)n;bz/=(re3c_i32)n;
  for(f=0;f<tr->frame_count;f++){
   re3c_i32 mx=0,my=0,mz=0;re3c_u32 matched=0u;int candidate=0;
   for(v=0;v<n;v++){
    re3c_vec3fx morph;const re3c_vec3fx*base=&model.body.vertices[model.body.parts[p].first_vertex+v].pos;
    r=re3c_morph_vertex(&model,p,f,v,&morph);if(r!=RE3C_OK)return 1;
    mx+=re3c_fx_to_int(morph.x);my+=re3c_fx_to_int(morph.y);mz+=re3c_fx_to_int(morph.z);
    if(morph.x==base->x&&morph.y==base->y&&morph.z==base->z)matched++;
   }
   mx/=(re3c_i32)n;my/=(re3c_i32)n;mz/=(re3c_i32)n;
   /* Only a heuristic for strongly centered displacement values. */
   if(f>0u){re3c_u32 ob=abs32(bx)+abs32(by)+abs32(bz),od=abs32(mx)+abs32(my)+abs32(mz);
    if(ob>512u && od*4u+128u<ob){candidate=1;delta_candidates++;}
   } else {match0=matched;frame0_matches+=matched;}
   fprintf(out,"%u,%u,%u,%u,%d,%d,%d,%d,%d,%d,%d\n",(unsigned)p,(unsigned)f,(unsigned)n,(unsigned)(f==0u?matched:0u),(int)bx,(int)by,(int)bz,(int)mx,(int)my,(int)mz,candidate);
   frames++;vertices+=n;
  }
  (void)match0;
 }
 if(argc==3)fclose(out);
 fprintf(stderr,"morph_space_audit tracks=%u frames=%lu vertices=%lu frame0_matches=%lu centered_candidates=%lu\n",(unsigned)model.binary01_morph.track_count,frames,vertices,frame0_matches,delta_candidates);
 re3c_model_close(&model);return 0;
}
