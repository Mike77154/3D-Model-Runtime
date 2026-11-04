/* C89 runtime regression: IQM interpolation patch must preserve other visitors. */
#include "model_runtime89.h"
#include <stdio.h>
#include <string.h>
static unsigned char arena[64u*1024u*1024u];
static unsigned long triangles;
static int on_triangle(void *ctx,const modelrt_triangle_view *t)
{(void)ctx;(void)t;triangles++;return 0;}
int main(int argc,char**argv)
{
    modelrt_runtime rt;modelrt_handle h;modelrt_visit_sink sink;
    unsigned long expected[5]={12ul,4212ul,12ul,654ul,3732ul};
    int j,rc;
    if(argc!=6)return 2;
    modelrt_runtime_init(&rt);
    if(modelrt_register_builtin_providers(&rt)!=MODELRT_OK||rt.provider_count!=75u)return 3;
    memset(&sink,0,sizeof(sink));sink.on_triangle=on_triangle;
    for(j=0;j<5;j++){
        modelrt_handle_init(&h,arena,sizeof(arena));
        rc=modelrt_open_file(&rt,&h,argv[j+1]);
        if(rc){fprintf(stderr,"OPEN FAILED %d %s\n",rc,argv[j+1]);return 4;}
        triangles=0ul;
        rc=modelrt_visit(&h,MODELRT_ANIM_BIND_POSE,0,&sink,0);
        if(rc||triangles!=expected[j]){
            fprintf(stderr,"VISIT FAILED %d %s %lu expected %lu\n",
                    rc,argv[j+1],triangles,expected[j]);return 5;
        }
        printf("CROSS-CODEC PASS %s triangles=%lu\n",argv[j+1],triangles);
        modelrt_close(&h);
    }
    return 0;
}
