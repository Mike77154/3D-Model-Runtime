/* v19 native provider smoke/regression: compile without bundled game assets.
 * Supply a legal NAB or special HMD on the command line for real-corpus run.
 */
#include "../include/model_runtime89.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GS_TEST_ARENA (16u*1024u*1024u)
static unsigned char arena[GS_TEST_ARENA];
static unsigned char rgba[2048u*2048u*4u];
static unsigned long tris, meshes, mats, joints, uv_errors, normal_errors;
static modelrt_q16 first_x, first_y, first_z;
static int first_seen;
static int on_tri(void *ctx, const modelrt_triangle_view *t)
{
    unsigned int k;
    (void)ctx;
    if (!first_seen) {
        first_seen=1;
        first_x=t->v[0].position.x;first_y=t->v[0].position.y;first_z=t->v[0].position.z;
    }
    for (k=0u;k<3u;++k) {
        if(t->v[k].uv0.x<0||t->v[k].uv0.x>65536||
           t->v[k].uv0.y<0||t->v[k].uv0.y>65536)++uv_errors;
        if(!(t->v[k].flags&MODELRT_VTX_NORMAL))++normal_errors;
    }
    ++tris;return 0;
}
static int on_mat(void *ctx, const modelrt_material_view *m)
{(void)ctx;(void)m;++mats;return 0;}
static int on_mesh(void *ctx, const modelrt_mesh_info *m)
{(void)ctx;(void)m;++meshes;return 0;}
static int on_part(void *ctx, const modelrt_rigid_part_view *p)
{(void)ctx;(void)p;++joints;return 0;}
int main(int argc, char **argv)
{
    modelrt_runtime rt;
    modelrt_handle h;
    modelrt_visit_sink sink;
    modelrt_animation_info anim;
    modelrt_texture_info tex;
    const modelrt_model_info *info;
    modelrt_u32 w,ht;
    size_t bytes;
    int rc,i;
    unsigned int sum_clips;
    if(argc<2||argc>5){
        fputs("Usage: gunsurvivor_runtime_regression asset.NAB|asset.NAC|asset.HMD [triangles] [textures] [clips]\n",stderr);
        return 2;
    }
    modelrt_runtime_init(&rt);
    if (modelrt_register_builtin_providers(&rt)!=MODELRT_OK) return 3;
    modelrt_handle_init(&h,arena,sizeof(arena));
    rc=modelrt_open_file(&rt,&h,argv[1]);
    if (rc!=MODELRT_OK) {printf("FAIL open '%s': %s\n",argv[1],modelrt_last_error(&rt));return 4;}
    info=modelrt_get_info(&h);
    if(!info||info->codec_id!=MODELRT_CODEC_GUNSURVIVOR)return 5;
    memset(&sink,0,sizeof(sink));
    sink.on_triangle=on_tri;sink.on_material=on_mat;
    sink.on_mesh_begin=on_mesh;sink.on_rigid_part=on_part;
    if(modelrt_visit(&h,MODELRT_ANIM_BIND_POSE,0,&sink,0)!=MODELRT_OK)return 6;
    if((tris==0u && info->mesh_count!=0u)||meshes!=info->mesh_count||
       mats!=info->material_count||uv_errors||normal_errors)return 7;
    if(argc>=3&&(unsigned long)strtoul(argv[2],0,10)!=tris)return 8;
    if(argc>=4&&(unsigned long)strtoul(argv[3],0,10)!=info->texture_count)return 9;
    if(argc>=5&&(unsigned long)strtoul(argv[4],0,10)!=info->animation_count)return 10;
    for(i=0;i<(int)info->texture_count;++i){
        if(modelrt_texture_info_get(&h,i,&tex)!=MODELRT_OK||tex.kind!=MODELRT_TEXTURE_EMBEDDED)return 11;
        if(modelrt_texture_decode_rgba(&h,i,rgba,sizeof(rgba),&w,&ht,&bytes)!=MODELRT_OK)return 12;
        if(w!=tex.width||ht!=tex.height||bytes!=tex.rgba_bytes)return 13;
    }
    sum_clips=0u;
    for(i=0;i<(int)info->animation_count;++i){
        if(modelrt_animation_info_get(&h,i,&anim)!=MODELRT_OK)return 14;
        if(!(anim.flags&MODELRT_ANIM_UNBOUND)||anim.ticks_per_second)return 15;
        if(!anim.source_key_count)return 16;
        ++sum_clips;
    }
    if(sum_clips && modelrt_visit(&h,0,0,&sink,0)!=MODELRT_ERR_UNSUPPORTED)return 17;
    printf("PASS %s tris=%lu meshes=%lu tex=%u clips=%u parts=%lu firstQ16=(%ld,%ld,%ld)\n",
        argv[1],tris,meshes,(unsigned)info->texture_count,
        (unsigned)info->animation_count,joints,(long)first_x,(long)first_y,(long)first_z);
    modelrt_close(&h);return 0;
}
