/* Self-contained no-Capcom-assets RE4 runtime regression. */
#include "model_runtime89.h"
#include "modelrt_re4classic89.h"
#include <stdio.h>
#include <string.h>
static unsigned char mem[32u*1024u*1024u];
static unsigned long tris;
static int tri(void *u,const modelrt_triangle_view *v){
    (void)u;
    if(tris==0ul && v->v[1].position.x!=65536)return 1;
    ++tris;return 0;
}
#define CHECK(c) do{if(!(c)){printf("FAIL line %d\n",__LINE__);return 1;}}while(0)
int main(int argc,char **argv){
    modelrt_runtime rt;modelrt_handle h;modelrt_visit_sink sink;
    modelrt_texture_info ti;modelrt_animation_info ai;
    modelrt_re4_fcv_track tr;modelrt_vec3 xyz;
    modelrt_u32 tracks=0,w=0,ht=0;size_t n=0;unsigned char pixels[8u*4u*4u];int rc;
    if(argc!=4)return 2;
    modelrt_runtime_init(&rt);
    CHECK(modelrt_register_builtin_providers(&rt)==MODELRT_OK);
    memset(&sink,0,sizeof(sink));sink.on_triangle=tri;
    modelrt_handle_init(&h,mem,sizeof(mem));
    rc=modelrt_open_file(&rt,&h,argv[1]);
    if(rc){printf("PMD open %d %s\n",rc,modelrt_last_error(&rt));return 3;}
    CHECK(h.info.codec_id==MODELRT_CODEC_RE4_CLASSIC);
    CHECK(h.info.mesh_count==1 && h.info.material_count==1 && h.info.bone_count==0);
    CHECK(modelrt_texture_info_get(&h,0,&ti)==MODELRT_OK);
    CHECK(ti.kind==MODELRT_TEXTURE_EXTERNAL && strstr(ti.path,"sample0.tga")!=0);
    tris=0;CHECK(modelrt_visit(&h,MODELRT_ANIM_BIND_POSE,0,&sink,0)==MODELRT_OK);
    CHECK(tris==1);
    modelrt_close(&h);
    modelrt_handle_init(&h,mem,sizeof(mem));
    rc=modelrt_open_file(&rt,&h,argv[2]);
    if(rc){printf("TPL open %d %s\n",rc,modelrt_last_error(&rt));return 4;}
    CHECK(h.info.codec_id==MODELRT_CODEC_RE4_CLASSIC && h.info.texture_count==1);
    CHECK(modelrt_texture_decode_rgba(&h,0,pixels,sizeof(pixels),&w,&ht,&n)==MODELRT_OK);
    CHECK(w==8 && ht==4 && n==128 && pixels[0]==127 && pixels[3]==255);
    modelrt_close(&h);
    modelrt_handle_init(&h,mem,sizeof(mem));
    rc=modelrt_open_file(&rt,&h,argv[3]);
    if(rc){printf("FCV open %d %s\n",rc,modelrt_last_error(&rt));return 5;}
    CHECK(h.info.codec_id==MODELRT_CODEC_RE4_CLASSIC && h.info.animation_count==1);
    CHECK(modelrt_animation_info_get(&h,0,&ai)==MODELRT_OK);
    CHECK((ai.flags&MODELRT_ANIM_UNBOUND)!=0u && ai.ticks_per_second==0u);
    CHECK(modelrt_visit(&h,0,0,&sink,0)==MODELRT_ERR_UNSUPPORTED);
    CHECK(modelrt_re4_fcv_track_count(&h,&tracks)==MODELRT_OK&&tracks==1);
    CHECK(modelrt_re4_fcv_track_info(&h,0,&tr)==MODELRT_OK&&tr.bone_type==1);
    CHECK(modelrt_re4_fcv_sample(&h,0,0,0,&xyz)==MODELRT_OK&&xyz.x==0);
    CHECK(modelrt_re4_fcv_sample(&h,0,0,1,&xyz)==MODELRT_OK&&xyz.y==0);
    puts("RE4 v4.1 PMD/TPL/FCV runtime regression PASS");
    return 0;
}
