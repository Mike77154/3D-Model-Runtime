/* C89 native-provider corpus harness. 
 * Positive test: +path/to/scene.ext (must visit >=1 triangle).
 * Negative test: -path/to/malformed.ext (must reject).
 * Ordinary argument: path/to/scene.ext (diagnostic only).
 */
#include "model_runtime89.h"
#include <stdio.h>
#include <string.h>

static unsigned char workspace[256u*1024u*1024u];
static unsigned long triangles;
static int on_triangle(void*u,const modelrt_triangle_view*t){(void)u;(void)t;triangles++;return 0;}
int main(int argc,char**argv){
 modelrt_runtime rt;modelrt_handle h;modelrt_visit_sink sink;int rc,visit,i,failures=0;const char*p;char expected;
 modelrt_runtime_init(&rt);rc=modelrt_register_builtin_providers(&rt);
 if(rc!=MODELRT_OK || rt.provider_count!=45u){printf("REGISTRY_FAIL count=%lu rc=%d\n",(unsigned long)rt.provider_count,rc);return 3;}
 memset(&sink,0,sizeof(sink));sink.on_triangle=on_triangle;
 if(argc<2){puts("usage: native_batch5_regression +valid.file -invalid.file [diagnostic.file ...]");return 0;}
 for(i=1;i<argc;i++){
  expected=0;p=argv[i];if(*p=='+'||*p=='-'){expected=*p;p++;}
  modelrt_handle_init(&h,workspace,sizeof(workspace));triangles=0u;
  rc=modelrt_open_file(&rt,&h,p);visit=rc;
  if(rc==MODELRT_OK)visit=modelrt_visit(&h,MODELRT_ANIM_BIND_POSE,0,&sink,0);
  printf("%s open=%d visit=%d codec=%lu triangles=%lu\n",p,rc,visit,(unsigned long)h.info.codec_id,triangles);
  if((expected=='+'&&(rc!=MODELRT_OK||visit!=MODELRT_OK||triangles==0u))||
     (expected=='-'&&rc==MODELRT_OK)){printf("FAIL expectation %c\n",expected);failures++;}
  modelrt_close(&h);
 }
 return failures?1:0;
}
