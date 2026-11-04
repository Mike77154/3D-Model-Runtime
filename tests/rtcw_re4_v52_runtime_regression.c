/* Copyright waived CC0-1.0. Synthetic RTCW frame and RE4 SEQ bridge test.
   No commercial assets. C89 strict, no heap, no floats. */
#include "model_runtime89.h"
#include "modelrt_re4seq89.h"
#include <stdio.h>
#include <string.h>
static unsigned char workspace[40u*1024u*1024u];
static unsigned long triangles;
static modelrt_q16 first_x, first_y, first_z;
static int triangle(void *u,const modelrt_triangle_view *v)
{
    (void)u;
    if(triangles==0UL){first_x=v->v[1].position.x;first_y=v->v[1].position.y;first_z=v->v[1].position.z;}
    ++triangles;return 0;
}
#define CHECK(v) do{if(!(v)){fprintf(stderr,"FAIL %s line %d\n",#v,__LINE__);return 1;}}while(0)
int main(int argc,char **argv)
{
    modelrt_runtime rt;
    modelrt_handle h;
    modelrt_visit_sink sink;
    modelrt_animation_info ai;
    modelrt_re4_seq_key key;
    modelrt_re4_seq_rule rule;
    modelrt_re4_seq_visual_state vis;
    modelrt_u32 num;
    int i;
    modelrt_q16 ax,ay,az;
    if(argc!=5)return 2;
    modelrt_runtime_init(&rt);
    CHECK(modelrt_register_builtin_providers(&rt)==MODELRT_OK);
    memset(&sink,0,sizeof(sink));sink.on_triangle=triangle;
    for(i=1;i<=3;i++){
        modelrt_handle_init(&h,workspace,sizeof(workspace));
        if(modelrt_open_file(&rt,&h,argv[i])!=MODELRT_OK){
            fprintf(stderr,"Failed to open %s: %s\n",argv[i],modelrt_last_error(&rt));return 3;
        }
        if(i==1)CHECK(h.info.codec_id==MODELRT_CODEC_RTCW);
        CHECK(h.info.mesh_count>=1u);
        CHECK(modelrt_animation_info_get(&h,0,&ai)==MODELRT_OK);
        CHECK(ai.frame_count==2u);
        triangles=0UL;
        CHECK(modelrt_visit_animation_frame(&h,0,0u,&sink,0)==MODELRT_OK);
        CHECK(triangles==1UL);
        ax=first_x;ay=first_y;az=first_z;
        triangles=0UL;
        CHECK(modelrt_visit_animation_frame(&h,0,1u,&sink,0)==MODELRT_OK);
        CHECK(triangles==1UL);
        CHECK(ax!=first_x||ay!=first_y||az!=first_z);
        printf("PASS %s / frames=2 / provider=%u\n",argv[i],(unsigned)h.info.codec_id);
        modelrt_close(&h);
    }
    modelrt_handle_init(&h,workspace,sizeof(workspace));
    CHECK(modelrt_open_file(&rt,&h,argv[4])==MODELRT_OK);
    CHECK(h.info.codec_id==MODELRT_CODEC_RE4_CLASSIC);
    CHECK(modelrt_re4_seq_key_count(&h,&num)==MODELRT_OK && num==1u);
    CHECK(modelrt_re4_seq_key_at(&h,0u,&key)==MODELRT_OK);
    CHECK(key.code==5 && key.time_q6==64);
    CHECK(modelrt_animation_info_get(&h,0,&ai)==MODELRT_OK);
    CHECK(ai.source_key_count==1u && (ai.flags & MODELRT_ANIM_UNBOUND));
    memset(&rule,0,sizeof(rule));rule.code=5u;rule.action=MODELRT_RE4_SEQ_SHOW;rule.target=3u;
    CHECK(modelrt_re4_seq_seek(&h,128u,128u,2u,&rule,1u,1,0u,0u,&vis)==MODELRT_OK);
    CHECK(vis.visible_mask==(1u<<3) && vis.applied_events==1u);
    CHECK(modelrt_re4_seq_seek(&h,128u,128u,2u,0,0u,1,0u,0u,&vis)!=MODELRT_OK);
    puts("PASS RE4 v5.2 SEQ deterministic authored visual mapping");
    modelrt_close(&h);
    return 0;
}
