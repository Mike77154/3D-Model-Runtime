#include "model_runtime89.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned char memory[96u*1024u*1024u];
static unsigned long tris;
static unsigned long meshes;
static int on_begin(void *u,const modelrt_model_info*in){(void)u;(void)in;return 0;}
static int on_tri(void *u,const modelrt_triangle_view*v){(void)u;(void)v;tris++;return 0;}
static int on_mesh(void *u,const modelrt_mesh_info *v){(void)u;(void)v;meshes++;return 0;}
int main(int argc,char**argv)
{
 modelrt_runtime rt;modelrt_handle handle;modelrt_visit_sink sink;int i,rc,vr;const modelrt_model_info*inf;
 modelrt_runtime_init(&rt);rc=modelrt_register_builtin_providers(&rt);
 if(rc!=MODELRT_OK){printf("register fail %d\n",rc);return 2;}
 printf("registered=%lu\n",(unsigned long)rt.provider_count);
 for(i=1;i<argc;i++){
  modelrt_handle_init(&handle,memory,sizeof(memory));
  rc=modelrt_open_file(&rt,&handle,argv[i]);
  if(rc!=MODELRT_OK){printf("REJECT %s rc=%d %s\n",argv[i],rc,modelrt_last_error(&rt));continue;}
  inf=modelrt_get_info(&handle);tris=0u;meshes=0u;memset(&sink,0,sizeof(sink));
  sink.on_model_begin=on_begin;sink.on_triangle=on_tri;sink.on_mesh_begin=on_mesh;
  vr=modelrt_visit(&handle,MODELRT_ANIM_BIND_POSE,0,&sink,0);
  printf("VISIT %s provider=%s codec=%lu mesh=%lu tri=%lu info_mesh=%lu flags=%lu rc=%d\n",argv[i],
         modelrt_get_provider_name(&handle),(unsigned long)inf->codec_id,meshes,tris,
         (unsigned long)inf->mesh_count,(unsigned long)inf->capabilities,vr);
  modelrt_close(&handle);
 }
 return 0;
}
