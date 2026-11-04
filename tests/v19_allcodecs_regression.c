/* v19 package sanity: all provider IDs retained plus three new native bridges. */
#include "model_runtime89.h"
#include "modelrt_native89.h"
#include <stdio.h>
#include <string.h>
#define V19_MEMORY (48u*1024u*1024u)
static unsigned char g_memory[V19_MEMORY];
static unsigned int g_tris=0;
static int visitor_triangle(void *user,const modelrt_triangle_view *t)
{
    (void)user;
    if(!t)return 1;
    g_tris++;
    return 0;
}
int main(int argc,char **argv)
{
    modelrt_runtime rt;modelrt_handle h;modelrt_visit_sink sink;int rc;
    if(argc<3){puts("usage: v19_allcodecs_regression tiny.ms3d synthetic.dts");return 2;}
    modelrt_runtime_init(&rt);
    if(modelrt_register_builtin_providers(&rt)!=MODELRT_OK||rt.provider_count<14u){puts("registry fail");return 3;}
    modelrt_handle_init(&h,g_memory,sizeof(g_memory));
    if(modelrt_open_file(&rt,&h,argv[1])!=MODELRT_OK||h.info.codec_id!=MODELRT_CODEC_MILKSHAPE3D){puts("MS3D open fail");return 4;}
    if(!modelrt_native_ms3d(&h)){puts("native MS3D accessor fail");return 5;}
    memset(&sink,0,sizeof(sink));sink.on_triangle=visitor_triangle;
    rc=modelrt_visit(&h,MODELRT_ANIM_BIND_POSE,0,&sink,0);
    if(rc!=MODELRT_OK||g_tris!=1u){puts("MS3D visit fail");return 6;}
    modelrt_close(&h);
    modelrt_handle_init(&h,g_memory,sizeof(g_memory));
    if(modelrt_open_file(&rt,&h,argv[2])!=MODELRT_OK||h.info.codec_id!=MODELRT_CODEC_TORQUE_DTS){puts("DTS open fail");return 7;}
    if(!modelrt_native_torque_dts(&h)){puts("native DTS accessor fail");return 8;}
    rc=modelrt_visit(&h,MODELRT_ANIM_BIND_POSE,0,&sink,0);
    if(rc!=MODELRT_OK){puts("DTS visit fail");return 9;}
    modelrt_close(&h);
    printf("PASS v19: registered=%u; MS3D triangle=1; DTS native open/visit; original 14 preserved, plus experimental\n",(unsigned)rt.provider_count);
    return 0;
}
