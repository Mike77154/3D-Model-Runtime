#include "../src/modelrt_internal.h"
#include <stdio.h>
#include <string.h>
static unsigned char workspace[192u*1024u*1024u];
static int bone(void*u,const modelrt_bone_view*b){int *n=(int*)u;(void)b;(*n)++;return 0;}
int main(int argc,char**argv){modelrt_runtime rt;modelrt_handle h;modelrt_visit_sink sink;int rc,vr,n=0;if(argc!=2)return 2;modelrt_runtime_init(&rt);modelrt_handle_init(&h,workspace,sizeof(workspace));rc=modelrt_provider_native_bvh_bravure89.open(&rt,&h,argv[1]);printf("Bravure open=%d bones=%lu frames?\n",rc,(unsigned long)h.info.bone_count);if(rc)return 1;h.provider=&modelrt_provider_native_bvh_bravure89;h.info.codec_id=80u;h.is_open=1;memset(&sink,0,sizeof(sink));sink.on_bone=bone;vr=modelrt_visit(&h,0,0,&sink,&n);printf("Bravure visit=%d bones=%d\n",vr,n);return vr?3:0;}
