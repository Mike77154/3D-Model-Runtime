/* RE4 model runtime integration, synthetic PMD+FCV, no game content. */
#include "model_runtime89.h"
#include "modelrt_re4classic89.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do {if(!(x)){printf("FAIL line %d\n",__LINE__);return 1;}}while(0)
static unsigned char pm_mem[4u*1024u*1024u];
static unsigned char fcv_mem[2u*1024u*1024u];
static unsigned char part_mem[4u*1024u*1024u];
static unsigned char pose_scratch[128u*1024u];
static unsigned char bind_mem[128u*1024u];
static unsigned int ntri;
static unsigned int lastmesh;
static modelrt_q16 xyz[3];
static int triangle(void *u,const modelrt_triangle_view *t){
    (void)u;
    if(ntri==0){xyz[0]=t->v[0].position.x;xyz[1]=t->v[0].position.y;xyz[2]=t->v[0].position.z;}
    lastmesh=(unsigned int)t->mesh_index;
    ++ntri;return 0;
}
int main(int argc,char **argv){
    modelrt_runtime rt;modelrt_handle pm,fc,pt,*parts[1],*source;modelrt_animation_binding bound;
    modelrt_visit_sink sink;int rc;modelrt_u32 count;
    if(argc!=3)return 2;
    modelrt_runtime_init(&rt);CHECK(modelrt_register_builtin_providers(&rt)==MODELRT_OK);
    modelrt_handle_init(&pm,pm_mem,sizeof(pm_mem));
    modelrt_handle_init(&fc,fcv_mem,sizeof(fcv_mem));
    CHECK(modelrt_open_file(&rt,&pm,argv[1])==MODELRT_OK);
    CHECK(pm.info.codec_id==MODELRT_CODEC_RE4_CLASSIC && pm.info.bone_count==1);
    CHECK((pm.info.capabilities&MODELRT_CAP_ANIM_ATTACH_TARGET)!=0u);
    CHECK(modelrt_open_file(&rt,&fc,argv[2])==MODELRT_OK);
    CHECK((fc.info.capabilities&MODELRT_CAP_ANIM_ATTACH_SOURCE)!=0u);
    modelrt_animation_binding_init(&bound,bind_mem,sizeof(bind_mem));
    rc=modelrt_animation_bind(&bound,&pm,&fc);
    if(rc){printf("BIND returned %d\n",rc);return 3;}
    CHECK(modelrt_animation_binding_get_info(&bound)->matched_track_count==1u);
    memset(&sink,0,sizeof(sink));sink.on_triangle=triangle;
    ntri=0;CHECK(modelrt_visit(&pm,MODELRT_ANIM_BIND_POSE,0,&sink,0)==MODELRT_OK);
    CHECK(ntri==1 && xyz[0]==65536 && xyz[1]==0);
    ntri=0;CHECK(modelrt_re4_visit_fcv_frame(&bound,5u,&sink,0)==MODELRT_OK);
    CHECK(ntri==1);
    printf("SKINNED x=%ld y=%ld\n",(long)xyz[0],(long)xyz[1]);
    CHECK(xyz[0]>56000 && xyz[0]<61000 && xyz[1]>28000 && xyz[1]<35000);
    ntri=0;CHECK(modelrt_visit_bound_animation(&bound,0,5*65536,&sink,0)==MODELRT_OK);
    CHECK(ntri==1 && xyz[1]>28000);
    CHECK(modelrt_re4_visit_fcv_frame(&bound,11u,&sink,0)==MODELRT_ERR_BAD_ARG);
    modelrt_handle_init(&pt,part_mem,sizeof(part_mem));
    CHECK(modelrt_open_file(&rt,&pt,argv[1])==MODELRT_OK);
    parts[0]=&pt;
    ntri=0;
    CHECK(modelrt_re4_visit_character_frame(&bound,parts,1u,5u,
          pose_scratch,sizeof(pose_scratch),&sink,0)==MODELRT_OK);
    CHECK(ntri==2 && lastmesh==1 && xyz[1]>28000);
    CHECK(modelrt_re4_character_texture_source(&pm,parts,1u,1u,
          &source,&count)==MODELRT_OK && source==&pt && count==0u);
    modelrt_close(&pt);
    modelrt_animation_unbind(&bound);
    CHECK(modelrt_re4_visit_fcv_frame(&bound,5u,&sink,0)==MODELRT_ERR_BAD_ARG);
    modelrt_close(&fc);modelrt_close(&pm);
    count=0; (void)count;
    puts("RE4 PMD/FCV native animated runtime PASS");
    return 0;
}
