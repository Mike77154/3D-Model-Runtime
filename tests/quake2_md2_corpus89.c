/* Quake 2 corpus MD2 smoke and all-frames regression. C89, fixed storage. */
#include "model_runtime89.h"
#include <stdio.h>
#include <string.h>
static unsigned char workspace[24u*1024u*1024u];
static unsigned long triangles;
static unsigned long uv_corners;
static unsigned long normal_corners;
static int triangle_cb(void*u,const modelrt_triangle_view*v){int i;(void)u;triangles++;for(i=0;i<3;i++){if(v->v[i].flags&MODELRT_VTX_UV0)uv_corners++;if(v->v[i].flags&MODELRT_VTX_NORMAL)normal_corners++;}return 0;}
int main(int argc,char**argv){modelrt_runtime rt;modelrt_handle h;modelrt_visit_sink sink;modelrt_animation_info a;int k,rc,err=0;unsigned long total_frames=0;modelrt_runtime_init(&rt);rc=modelrt_register_builtin_providers(&rt);if(rc){fprintf(stderr,"REGISTER_FAIL %d\n",rc);return 2;}memset(&sink,0,sizeof(sink));sink.on_triangle=triangle_cb;puts("file,open_rc,codec,triangles,frames,visited,uv_corners,normal_corners,failed");for(k=1;k<argc;k++){unsigned long expect=0,nframes=0,visited=0;int failed=0;modelrt_u32 j;triangles=uv_corners=normal_corners=0;modelrt_handle_init(&h,workspace,sizeof(workspace));rc=modelrt_open_file(&rt,&h,argv[k]);if(rc==0){if(h.info.codec_id!=MODELRT_CODEC_GACKT_MD2)failed=1;else {int vrc=modelrt_visit(&h,MODELRT_ANIM_BIND_POSE,0,&sink,0);if(vrc||triangles==0||uv_corners!=3*triangles)failed=1;expect=triangles;if(modelrt_animation_info_get(&h,0,&a)!=MODELRT_OK)failed=1;else{nframes=a.frame_count;for(j=0;j<a.frame_count;j++){triangles=0;vrc=modelrt_visit_animation_frame(&h,0,j,&sink,0);if(vrc||triangles!=expect){failed=1;break;}visited++;}}}modelrt_close(&h);}else failed=1;printf("%s,%d,%d,%lu,%lu,%lu,%lu,%lu,%d\n",argv[k],rc,rc==0?(int)MODELRT_CODEC_GACKT_MD2:0,expect,nframes,visited,uv_corners,normal_corners,failed);if(failed)err++;total_frames+=visited;}fprintf(stderr,"QUAKE2_CORPUS_MODELS=%d FAILED=%d FRAMES_VISITED=%lu\n",argc-1,err,total_frames);return err?1:0;}
