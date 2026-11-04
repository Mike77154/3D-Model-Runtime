/* Test native vendors without GACKT-LANG or intermediate files. C89. */
#include "model_runtime89.h"
#include <stdio.h>
#include <string.h>
static unsigned char workspace[96u*1024u*1024u];
static unsigned long tris;
static int tri_cb(void *u,const modelrt_triangle_view *v){(void)u;(void)v;tris++;return 0;}
int main(int argc,char**argv){
    modelrt_runtime rt;modelrt_handle h;modelrt_visit_sink sink;
    int i,rc,vr;modelrt_u32 cid;
    if(argc<2){puts("usage: native_wave4_regression path...");return 2;}
    modelrt_runtime_init(&rt);
    rc=modelrt_register_builtin_providers(&rt);
    if(rc || rt.provider_count != 35u){printf("FAIL registry=%lu rc=%d\n",(unsigned long)rt.provider_count,rc);return 2;}
    memset(&sink,0,sizeof(sink));sink.on_triangle=tri_cb;
    for(i=1;i<argc;i++){
        modelrt_handle_init(&h,workspace,sizeof(workspace));
        rc=modelrt_open_file(&rt,&h,argv[i]);
        if(rc){printf("REJECT %s code=%d (%s)\n",argv[i],rc,modelrt_last_error(&rt));continue;}
        tris=0u;cid=h.info.codec_id;
        vr=modelrt_visit(&h,MODELRT_ANIM_BIND_POSE,0,&sink,0);
        printf("VISIT %s codec=%lu triangles=%lu visit_rc=%d\n",argv[i],(unsigned long)cid,tris,vr);
        if(cid==MODELRT_CODEC_NATIVE_MD3 && vr==MODELRT_OK){
            modelrt_animation_info a;
            if(modelrt_animation_info_get(&h,0,&a)==MODELRT_OK && a.frame_count>0u){
                unsigned long base=tris;tris=0u;
                rc=modelrt_visit_animation_frame(&h,0,a.frame_count-1u,&sink,0);
                printf("MD3 last frame: frames=%lu triangles=%lu rc=%d\n",
                  (unsigned long)a.frame_count,tris,rc);
                if(rc!=MODELRT_OK || (base && !tris))return 4;
            }
        }
        modelrt_close(&h);
    }
    return 0;
}
