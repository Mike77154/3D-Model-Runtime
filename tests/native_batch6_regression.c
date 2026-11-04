/* Wave 6: native providers and regressions, strict ISO C89 test harness. */
#include "model_runtime89.h"
#include <stdio.h>
#include <string.h>
static unsigned char memory[192u*1024u*1024u];
static unsigned long triangles,bones;
static int ontri(void*u,const modelrt_triangle_view*t){(void)u;(void)t;triangles++;return 0;}
static int onbone(void*u,const modelrt_bone_view*b){(void)u;(void)b;bones++;return 0;}
int main(int argc,char**argv){modelrt_runtime rt;modelrt_handle h;modelrt_visit_sink sink;int i,rc,vr;modelrt_animation_info ai;modelrt_runtime_init(&rt);rc=modelrt_register_builtin_providers(&rt);if(rc||rt.provider_count!=55u){printf("REGISTRY_FAIL count=%lu rc=%d\n",(unsigned long)rt.provider_count,rc);return 2;}printf("REGISTRY_OK %lu\n",(unsigned long)rt.provider_count);memset(&sink,0,sizeof(sink));sink.on_triangle=ontri;sink.on_bone=onbone;for(i=1;i<argc;i++){modelrt_handle_init(&h,memory,sizeof(memory));rc=modelrt_open_file(&rt,&h,argv[i]);if(rc!=MODELRT_OK){printf("REJECT %s rc=%d %s\n",argv[i],rc,modelrt_last_error(&rt));continue;}triangles=bones=0u;vr=modelrt_visit(&h,MODELRT_ANIM_BIND_POSE,0,&sink,0);printf("VISIT %s codec=%lu name=%s tri=%lu bones=%lu rc=%d anim=%lu used=%lu\n",argv[i],(unsigned long)h.info.codec_id,modelrt_get_provider_name(&h),triangles,bones,vr,(unsigned long)h.info.animation_count,(unsigned long)h.workspace_used);if(vr==MODELRT_OK&&h.info.animation_count>0u&&modelrt_animation_info_get(&h,0,&ai)==MODELRT_OK){printf("ANIMATION %s frames=%lu\n",argv[i],(unsigned long)ai.frame_count);}modelrt_close(&h);}return 0;}
