#include "hlfp_bundle.h"
#include "hlfp_softmdl_plus.h"
#include <stdio.h>
#include <string.h>
#define CAP (12U * 1024U * 1024U)
static hlfp_u8 main_data[CAP],texture_data[CAP],sequence_data[CAP];
static hlfp_mdl mdl;
static hlfp_error err;
static hlfp_soft_scratch scratch;
static hlfp_u8 pixels[96*96*4];
static hlfp_i32 depth[96*96];
static hlfp_trans89_triangle tri_store[16384];
static hlfp_trans89_queue queue;
static int rf(void *u,const char *p,hlfp_u8 *b,size_t cap,size_t *sz) {
 FILE *f;long k;size_t r;(void)u;
 f=fopen(p,"rb");if(!f)return HLFP_ERR_MISSING_DATA;
 if(fseek(f,0,SEEK_END)!=0 || (k=ftell(f))<=0 || (unsigned long)k>cap){fclose(f);return HLFP_ERR_CAPACITY;}
 rewind(f);r=fread(b,1,(size_t)k,f);fclose(f);
 if(r!=(size_t)k)return HLFP_ERR_IO;
 *sz=r;return HLFP_OK;
}
int main(int argc,char **argv) {
 hlfp_bundle_buffers bu;hlfp_bundle_stats st;
 int i,bp,nmesh=0,full=0,flat=0,mask=0,chrome=0,alpha=0,add=0,rc;
 int render_rc=0,drawn=0,dim=96;
 hlfp_soft_framebuffer fb;hlfp_soft_camera cam;hlfp_soft2_options opt;
 hlfp_p89_camera view;
 if(argc<2) return 2;
 bu.main_bytes=main_data;bu.main_capacity=sizeof(main_data);
 bu.texture_bytes=texture_data;bu.texture_capacity=sizeof(texture_data);
 bu.sequence_pool=sequence_data;bu.sequence_capacity=sizeof(sequence_data);
 rc=hlfp_bundle_load(argv[1],rf,0,&bu,&mdl,&st,&err);
 if(rc!=HLFP_OK) {fprintf(stderr,"LOAD_FAIL:%s\n",err.message);return 3;}
 for(i=0;i<mdl.num_textures;i++) {
  int f=mdl.textures[i].flags;
  if(f&HLFP_STUDIO_NF_FULLBRIGHT)full++;
  if(f&HLFP_STUDIO_NF_FLATSHADE)flat++;
  if(f&HLFP_STUDIO_NF_CHROME)chrome++;
  if(f&HLFP_STUDIO_NF_MASKED)mask++;
  if(f&HLFP_STUDIO_NF_ALPHA)alpha++;
  if(f&HLFP_STUDIO_NF_ADDITIVE)add++;
 }
 for(bp=0;bp<mdl.num_bodyparts;bp++){
  int model;
  if(hlfp_goldsrc_model_for_bodypart(&mdl,0,bp,&model,&err)==HLFP_OK && model>=0 && model<mdl.num_models) nmesh+=mdl.models[model].nummesh;
 }
 if(argc>2 && strcmp(argv[2],"tiny")==0)dim=12;
 if(argc>2 && mdl.num_seqdesc>0 && mdl.num_bones>0 && nmesh>0) {
  hlfp_soft_options clr;
  hlfp_soft2_options_init(&opt);opt.skeleton_overlay=HLFP_FALSE;opt.wire_overlay=HLFP_FALSE;
  opt.chrome_view_dependent=HLFP_TRUE;
  opt.enable_directional_light=HLFP_TRUE;
  opt.chrome_per_bone=HLFP_TRUE;
  opt.smooth_vertex_light=HLFP_TRUE;
  opt.light_direction.z=-HLFP_FIXED_ONE;
  opt.light_direction.y=0;
  opt.light_lambert=HLFP_FIXED_ONE + HLFP_FIXED_ONE/2;
  opt.chrome_eye.x=0;opt.chrome_eye.y=0;opt.chrome_eye.z=160*HLFP_FIXED_ONE;
  hlfp_trans89_init(&queue,tri_store,16384U);
  hlfp_soft_options_init(&clr);
  fb.width=dim;fb.height=dim;fb.stride_bytes=dim*4;fb.rgba=pixels;fb.zbuffer=depth;
  hlfp_soft_camera_init(&cam,dim,dim);cam.scale=HLFP_FIXED_ONE;
  hlfp_p89_camera_init(&view);
  view.eye.z=512*HLFP_FIXED_ONE;
  view.forward.z=-HLFP_FIXED_ONE;
  view.focal=512*HLFP_FIXED_ONE;
  view.near_z=8*HLFP_FIXED_ONE;
  opt.perspective=&view;
  opt.full_frustum_clipping=HLFP_TRUE;
  opt.perspective_far_z=8192*HLFP_FIXED_ONE;
  if(hlfp_soft_clear(&fb,&clr,&err)!=HLFP_OK)render_rc=4;
  else {
   render_rc=hlfp_soft3_render_mdl_sequence_subframe_textured(&mdl,0,0,0,
         HLFP_FIXED_ONE/2,0,&fb,&cam,&opt,&scratch,&queue,&err);
   if(render_rc==HLFP_OK){for(i=0;i<dim*dim*4;i+=4) if(pixels[i]||pixels[i+1]||pixels[i+2])drawn++;}
  }
 }
 printf("%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%lu,%s\n",
 mdl.num_textures,mdl.num_seqdesc,mdl.num_bodyparts,nmesh,full,flat,mask,chrome,alpha,add,st.has_external_textures,render_rc,drawn,(unsigned long)queue.count,argv[1]);
 if(render_rc!=0)fprintf(stderr,"RENDER_FAIL code=%d %s path=%s\n",render_rc,err.message,argv[1]);
 return render_rc==0?0:1;
}
