#include "model_runtime89.h"
#include <stdio.h>
#include <string.h>
static unsigned char memory[96u*1024u*1024u];
static unsigned long tris=0,bones=0;
static int triangle(void*u,const modelrt_triangle_view *t){(void)u;(void)t;tris++;return 0;}
static int bone(void*u,const modelrt_bone_view *b){(void)u;(void)b;bones++;return 0;}
int main(int argc,char **argv){modelrt_runtime rt;modelrt_handle h;modelrt_visit_sink sink;int i,rc,vr;
 modelrt_runtime_init(&rt);rc=modelrt_register_builtin_providers(&rt);
 printf("REGISTRY %lu rc=%d\n",(unsigned long)rt.provider_count,rc);
 if(rc||rt.provider_count!=70u)return 2;
 memset(&sink,0,sizeof(sink));sink.on_triangle=triangle;sink.on_bone=bone;
 for(i=1;i<argc;i++){modelrt_handle_init(&h,memory,sizeof(memory));
   rc=modelrt_open_file(&rt,&h,argv[i]);
   if(rc){printf("REJECT %s %d %s\n",argv[i],rc,modelrt_last_error(&rt));continue;}
   tris=bones=0;vr=modelrt_visit(&h,MODELRT_ANIM_BIND_POSE,0,&sink,0);
   printf("VISIT %s codec=%lu tri=%lu bones=%lu anim=%lu rc=%d bytes=%lu\n",argv[i],(unsigned long)h.info.codec_id,tris,bones,(unsigned long)h.info.animation_count,vr,(unsigned long)h.workspace_used);
   modelrt_close(&h);
 }
 return 0;
}
