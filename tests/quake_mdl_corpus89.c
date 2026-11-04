/* Quake IDPO v6 corpus regression: C89, no heap. Accept any number of .mdl paths.
 * Prints one CSV row per file (not the copyrighted model content). */
#include "modelrt_native_batch6_89.h"
#include <stdio.h>
#include <string.h>
static unsigned char arena[192u*1024u*1024u];
static modelrt_u8 palette[768];
static modelrt_u8 rgba[1024u*1024u*4u];
static modelrt_u32 ntri, nnormal, nuv, nseam, nmaterials;
static int tri_cb(void*u,const modelrt_triangle_view*tr){int j;(void)u;ntri++;for(j=0;j<3;j++){if(tr->v[j].flags&MODELRT_VTX_NORMAL)nnormal++;if(tr->v[j].flags&MODELRT_VTX_UV0)nuv++;if(tr->v[j].uv0.x>=65536)nseam++;}return 0;}
static int mat_cb(void*u,const modelrt_material_view*m){(void)u;(void)m;nmaterials++;return 0;}
static void clear_counts(void){ntri=nnormal=nuv=nseam=nmaterials=0u;}
int main(int argc,char**argv){modelrt_runtime rt;modelrt_handle h;modelrt_visit_sink s;
 int k,rc,fail=0,nok=0; unsigned long total_frames=0,total_tri=0,total_skins=0,groups=0;
 modelrt_runtime_init(&rt);if(modelrt_register_builtin_providers(&rt)!=0)return 2;
 memset(&s,0,sizeof(s));s.on_triangle=tri_cb;s.on_material=mat_cb;
 for(k=0;k<768;k++)palette[k]=(unsigned char)(k%256);
 puts("file,open_rc,codec,triangles,animations,skin_images,group_animations,frames_visited,normal_corners,uv_corners,seam_corners,failed");
 for(k=1;k<argc;k++){
  const qmdl_model *m;modelrt_u32 i,groupcnt=0,skins=0,frames=0;
  int bad=0,tc,rr;modelrt_animation_info ai;modelrt_texture_info ti;
  modelrt_handle_init(&h,arena,sizeof(arena));rc=modelrt_open_file(&rt,&h,argv[k]);
  if(rc){printf("%s,%d,0,0,0,0,0,0,0,0,0,1\n",argv[k],rc);fail++;continue;}
  m=modelrt_native_wave6_quake(&h);
  if(!m||h.info.codec_id!=MODELRT_CODEC_NATIVE_QUAKE_MDL){bad=1;goto report;}
  clear_counts();rr=modelrt_visit(&h,MODELRT_ANIM_BIND_POSE,0,&s,0);
  if(rr||ntri!=m->header.numtris||!nuv||!nmaterials)bad=1;
  tc=(int)ntri;
  for(i=0u;i<m->header.numskins;i++)skins+=m->skins[i].num_images;
  if(modelrt_texture_count(&h)!=(int)skins)bad=1;
  for(i=0u;i<skins;i++){
   const modelrt_u8*pixels=0;modelrt_u32 w=0,he=0;
   if(modelrt_texture_info_get(&h,(int)i,&ti)!=MODELRT_OK||ti.kind!=MODELRT_TEXTURE_EMBEDDED)bad=1;
   if(modelrt_quake_skin_indices89(&h,(int)i,&pixels,&w,&he)!=MODELRT_OK||!pixels||w!=m->header.skinwidth||he!=m->header.skinheight)bad=1;
   if(modelrt_quake_skin_rgba_palette89(&h,(int)i,palette,sizeof(palette),rgba,sizeof(rgba))!=MODELRT_OK)bad=1;
   if(pixels && rgba[0]!=palette[(unsigned int)pixels[0]*3u])bad=1;
  }
  for(i=0u;i<m->header.numframes;i++){
   modelrt_u32 f,lim;modelrt_u32 expected=m->frames[i].num_subframes;
   if(modelrt_animation_info_get(&h,(int)i,&ai)!=MODELRT_OK||ai.frame_count!=expected)bad=1;
   if(expected>1u){groupcnt++;if(modelrt_visit(&h,(int)i,65536,&s,0)!=MODELRT_OK)bad=1;}
   /* Walk all authored native subframes: no inferred FPS or interpolation. */
   lim=expected;
   for(f=0u;f<lim;f++){
    clear_counts();rr=modelrt_visit_animation_frame(&h,(int)i,f,&s,0);
    if(rr||ntri!=(modelrt_u32)tc)bad=1;
    frames++;
   }
  }
  clear_counts();rr=modelrt_visit(&h,MODELRT_ANIM_BIND_POSE,0,&s,0);if(rr)bad=1;
  total_tri+=(unsigned long)tc;total_frames+=frames;total_skins+=skins;groups+=groupcnt;
report:
  printf("%s,%d,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%d\n",argv[k],rc,(unsigned long)h.info.codec_id,(unsigned long)ntri,(unsigned long)h.info.animation_count,(unsigned long)skins,(unsigned long)groupcnt,(unsigned long)frames,(unsigned long)nnormal,(unsigned long)nuv,(unsigned long)nseam,bad);
  if(bad)fail++;else nok++;
  modelrt_close(&h);
 }
 fprintf(stderr,"QUAKE_CORPUS_OK=%d FAIL=%d FRAMES=%lu TRIANGLES=%lu SKINS=%lu GROUPS=%lu\n",nok,fail,total_frames,total_tri,total_skins,groups);
 return fail?1:0;
}
