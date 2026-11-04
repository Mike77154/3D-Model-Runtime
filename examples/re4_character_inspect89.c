/* RE4 native character inspector: caller-owned buffers, C89, Q16, no heap.
 * Usage: re4_character_inspect89 master.pmd motion.fcv frame [part1.pmd ...]
 * This inspects a renderable runtime stream; it does not repackage game data.
 */
#include "model_runtime89.h"
#include "modelrt_re4classic89.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define RE4_PARTS_MAX 6
#define RE4_MODEL_BYTES (32u*1024u*1024u)
#define RE4_FCV_BYTES (16u*1024u*1024u)
static unsigned char master_mem[RE4_MODEL_BYTES];
static unsigned char clip_mem[RE4_FCV_BYTES];
static unsigned char part_mem[RE4_PARTS_MAX][RE4_MODEL_BYTES];
static unsigned char binding_mem[128u*1024u];
static unsigned char part_scratch[128u*1024u];
static modelrt_handle part[RE4_PARTS_MAX];
static modelrt_handle *part_list[RE4_PARTS_MAX];
static unsigned long triangles,meshes,materials,bones;
static modelrt_i32 low[3],high[3];
static int mesh_begin(void *ctx,const modelrt_mesh_info *m){
    (void)ctx;(void)m;++meshes;return 0;
}
static int material(void *ctx,const modelrt_material_view *m){
    (void)ctx;(void)m;++materials;return 0;
}
static int bone(void *ctx,const modelrt_bone_view *b){
    (void)ctx;(void)b;++bones;return 0;
}
static int tri(void *ctx,const modelrt_triangle_view *t){
    unsigned int i;modelrt_i32 v[3];(void)ctx;
    for(i=0;i<3u;++i){
        v[0]=t->v[i].position.x;v[1]=t->v[i].position.y;
        v[2]=t->v[i].position.z;
        if(triangles==0u&&i==0u){
            low[0]=high[0]=v[0];low[1]=high[1]=v[1];low[2]=high[2]=v[2];
        }
        if(v[0]<low[0])low[0]=v[0];if(v[0]>high[0])high[0]=v[0];
        if(v[1]<low[1])low[1]=v[1];if(v[1]>high[1])high[1]=v[1];
        if(v[2]<low[2])low[2]=v[2];if(v[2]>high[2])high[2]=v[2];
    }
    ++triangles;return 0;
}
int main(int argc,char **argv){
    modelrt_runtime runtime;modelrt_handle master,clip;
    modelrt_animation_binding bound;modelrt_visit_sink sink;
    unsigned long native_tick;char *end;int rc,i,count;
    if(argc<4||argc>4+RE4_PARTS_MAX){
        fprintf(stderr,"Usage: %s master.pmd motion.fcv native_frame [part.pmd ...]\n",argv[0]);return 2;
    }
    count=argc-4;
    native_tick=strtoul(argv[3],&end,10);
    if(!argv[3][0]||*end||native_tick>65535ul){
        fprintf(stderr,"Invalid native FCV frame (0..65535)\n");return 2;
    }
    modelrt_runtime_init(&runtime);
    rc=modelrt_register_builtin_providers(&runtime);
    if(rc!=MODELRT_OK)return 2;
    modelrt_handle_init(&master,master_mem,sizeof(master_mem));
    modelrt_handle_init(&clip,clip_mem,sizeof(clip_mem));
    rc=modelrt_open_file(&runtime,&master,argv[1]);
    if(rc!=MODELRT_OK){fprintf(stderr,"PMD open: %s\n",modelrt_last_error(&runtime));return 3;}
    rc=modelrt_open_file(&runtime,&clip,argv[2]);
    if(rc!=MODELRT_OK){fprintf(stderr,"FCV open: %s\n",modelrt_last_error(&runtime));return 4;}
    for(i=0;i<count;++i){
        modelrt_handle_init(&part[i],part_mem[i],sizeof(part_mem[i]));
        rc=modelrt_open_file(&runtime,&part[i],argv[i+4]);
        if(rc!=MODELRT_OK){
            fprintf(stderr,"Part %d open: %s\n",i,modelrt_last_error(&runtime));return 5;
        }
        part_list[i]=&part[i];
    }
    modelrt_animation_binding_init(&bound,binding_mem,sizeof(binding_mem));
    rc=modelrt_animation_bind(&bound,&master,&clip);
    if(rc!=MODELRT_OK){fprintf(stderr,"PMD/FCV bone binding failed: %d\n",rc);return 6;}
    memset(&sink,0,sizeof(sink));sink.on_triangle=tri;sink.on_mesh_begin=mesh_begin;
    sink.on_material=material;sink.on_bone=bone;
    rc=modelrt_re4_visit_character_frame(&bound,part_list,(modelrt_u32)count,
                (modelrt_u32)native_tick,part_scratch,sizeof(part_scratch),&sink,0);
    if(rc!=MODELRT_OK){fprintf(stderr,"Character frame failed: %d\n",rc);return 7;}
    printf("RE4 native frame %lu: meshes=%lu materials=%lu bones=%lu triangles=%lu\n",
           native_tick,meshes,materials,bones,triangles);
    if(triangles)printf("Bounds Q16: x[%ld,%ld] y[%ld,%ld] z[%ld,%ld]\n",
      (long)low[0],(long)high[0],(long)low[1],(long)high[1],(long)low[2],(long)high[2]);
    modelrt_animation_unbind(&bound);
    for(i=0;i<count;++i)modelrt_close(&part[i]);
    modelrt_close(&clip);modelrt_close(&master);
    return 0;
}
