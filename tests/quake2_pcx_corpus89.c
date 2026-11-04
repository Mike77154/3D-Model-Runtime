/* Quake II PCX corpus audit via public runtime image API. C89, no heap. */
#include "model_runtime89.h"
#include <stdio.h>
#include <string.h>
static unsigned char encoded[2u*1024u*1024u];
static unsigned char rgba[4u*1024u*1024u];
int main(int argc,char**argv){int i,bad=0,rc;modelrt_runtime rt;
 modelrt_image_info info;modelrt_runtime_init(&rt);
 puts("file,rc,width,height,decoded_bytes,decoder");
 for(i=1;i<argc;i++){
  memset(&info,0,sizeof(info));
  rc=modelrt_image_decode_file(&rt,argv[i],encoded,sizeof(encoded),rgba,sizeof(rgba),&info);
  printf("%s,%d,%lu,%lu,%lu,%lu\n",argv[i],rc,(unsigned long)info.width,
   (unsigned long)info.height,(unsigned long)info.rgba_bytes,(unsigned long)info.decoder);
  if(rc!=MODELRT_OK||info.decoder!=MODELRT_IMAGE_PCX)bad++;
 }
 fprintf(stderr,"Q2_PCX_READ=%d FAIL=%d\n",argc-1,bad);return bad?1:0;
}
