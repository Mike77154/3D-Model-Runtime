#include "../include/model_runtime89.h"
#include <stdio.h>
#include <string.h>

static unsigned char arena[32768];
static unsigned char rgba[4UL*1048576UL];
typedef struct scan_s {unsigned long tris, meshes, bones;} scan;
static int start_mesh(void *user,const modelrt_mesh_info *m){scan *s=(scan*)user;(void)m;s->meshes++;return 0;}
static int on_tri(void *user,const modelrt_triangle_view *t){scan *s=(scan*)user;(void)t;s->tris++;return 0;}
static int on_bone(void *user,const modelrt_bone_view *b){scan *s=(scan*)user;(void)b;s->bones++;return 0;}
static const modelrt_visit_sink callbacks={0,start_mesh,0,on_tri,on_bone,0,0,0};
int main(int argc,char **argv){
    modelrt_runtime rt;
    modelrt_handle handle;
    const modelrt_model_info *inf;
    modelrt_animation_info a;
    modelrt_texture_info t;
    scan got;
    int rc,i;
    modelrt_u32 w,h;
    size_t n;
    if(argc!=2 && argc!=3){fprintf(stderr,"expected .mdl file [file|legacy]\n");return 2;}
    if(argc==3){
        modelrt_u32 mode;
        if(strcmp(argv[2],"file")==0)mode=MODELRT_3DGS_POSE_FILE_SPACE;
        else if(strcmp(argv[2],"legacy")==0)mode=MODELRT_3DGS_POSE_HIERARCHY;
        else return 11;
        if(modelrt_3dgs_set_pose_mode(mode)!=MODELRT_OK ||
           modelrt_3dgs_get_pose_mode()!=mode)return 12;
    }
    modelrt_runtime_init(&rt);
    rc=modelrt_register_builtin_providers(&rt);
    if(rc){fprintf(stderr,"register error %d\n",rc);return 3;}
    modelrt_handle_init(&handle,arena,sizeof(arena));
    rc=modelrt_open_file(&rt,&handle,argv[1]);
    if(rc){fprintf(stderr,"open error %d: %s\n",rc,modelrt_last_error(&rt));return 4;}
    inf=modelrt_get_info(&handle);
    printf("codecid=%u groups=%u skins=%u bones=%u anims=%u\n",
           (unsigned)inf->codec_id,(unsigned)inf->mesh_count,
           (unsigned)inf->texture_count,(unsigned)inf->bone_count,
           (unsigned)inf->animation_count);
    if(inf->codec_id!=106u){return 5;}
    memset(&got,0,sizeof(got));
    rc=modelrt_visit(&handle,MODELRT_ANIM_BIND_POSE,0,&callbacks,&got);
    if(rc){fprintf(stderr,"visit failed %d\n",rc);return 6;}
    printf("triangles=%lu meshes=%lu bones=%lu\n",got.tris,got.meshes,got.bones);
    if(!got.meshes)return 7;
    for(i=0;i<modelrt_animation_count(&handle);++i){
        rc=modelrt_animation_info_get(&handle,i,&a);
        if(rc)return 8;
        if(a.frame_count){
            memset(&got,0,sizeof(got));
            rc=modelrt_visit_animation_frame(&handle,i,a.frame_count-1,&callbacks,&got);
            if(rc){fprintf(stderr,"frame visit error %d index %d\n",rc,i);return 9;}
        }
    }
    for(i=0;i<modelrt_texture_count(&handle);++i){
        rc=modelrt_texture_info_get(&handle,i,&t);
        if(rc)return 10;
        rc=modelrt_texture_decode_rgba(&handle,i,rgba,sizeof(rgba),&w,&h,&n);
        if(!rc)printf("skin%d %ux%u rgba=%lu\n",i,(unsigned)w,(unsigned)h,(unsigned long)n);
        else printf("skin%d unresolved (%d)\n",i,rc);
    }
    modelrt_close(&handle);
    return 0;
}
