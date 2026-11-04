#include "model_runtime89.h"
#include <stdio.h>
static unsigned char mem[96u*1024u*1024u];
int main(int argc,char **argv){modelrt_runtime rt;modelrt_handle h;int i,rc;
 if(argc!=3)return 2;
 modelrt_runtime_init(&rt);
 if(modelrt_register_builtin_providers(&rt))return 3;
 for(i=1;i<3;i++){
  modelrt_handle_init(&h,mem,sizeof(mem));rc=modelrt_open_file(&rt,&h,argv[i]);
  printf("%s rc=%d codec=%u triangles_mesh_count=%u err=%s\n",argv[i],rc,(unsigned)h.info.codec_id,(unsigned)h.info.mesh_count,modelrt_last_error(&rt));
  if(rc==MODELRT_OK){if(h.info.codec_id!=(i==1?MODELRT_CODEC_DIRECTX_X:MODELRT_CODEC_NATIVE_DBO))return 4;modelrt_close(&h);}
  else return 5;
 }
 return 0;}
