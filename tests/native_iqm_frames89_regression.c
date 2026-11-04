/* IQM native pose-channel regression; no cooked format or engine renderer. */
#include "model_runtime89.h"
#include <stdio.h>
#include <string.h>
static unsigned char arena[32u*1024u*1024u];
static unsigned long triangles, bones;
static modelrt_q16 root_y;
static int on_tri(void *ctx,const modelrt_triangle_view*t){(void)ctx;(void)t;triangles++;return 0;}
static int on_bone(void *ctx,const modelrt_bone_view*b){(void)ctx;bones++;if(b->index==0u)root_y=b->position.y;return 0;}
int main(int argc,char**argv)
{
    modelrt_runtime rt;modelrt_handle h;modelrt_visit_sink sink;
    modelrt_animation_info ai;modelrt_animation_frame_info fi;
    modelrt_q16 first,last;
    int rc;
    if(argc!=3){fprintf(stderr,"Usage: %s valid.iqm invalid.iqm\n",argv[0]);return 2;}
    modelrt_runtime_init(&rt);
    rc=modelrt_register_builtin_providers(&rt);
    if(rc!=MODELRT_OK||rt.provider_count!=75u){fprintf(stderr,"REGISTRY FAIL %lu rc %d\n",(unsigned long)rt.provider_count,rc);return 3;}
    modelrt_handle_init(&h,arena,sizeof(arena));
    if(modelrt_open_file(&rt,&h,argv[1])!=MODELRT_OK || h.info.codec_id!=101u){fprintf(stderr,"OPEN FAIL %s\n",modelrt_last_error(&rt));return 4;}
    if(modelrt_animation_count(&h)!=1||modelrt_animation_info_get(&h,0,&ai)!=MODELRT_OK || ai.frame_count!=101u || ai.ticks_per_second==0u){fprintf(stderr,"ANIM INFO FAIL count=%d frames=%lu rate=%lu\n",modelrt_animation_count(&h),(unsigned long)ai.frame_count,(unsigned long)ai.ticks_per_second);return 5;}
    memset(&sink,0,sizeof(sink));sink.on_triangle=on_tri;sink.on_bone=on_bone;
    if(modelrt_animation_frame_info_get(&h,0,100u,&fi)!=MODELRT_OK) return 6;
    if(modelrt_visit_animation_frame(&h,0,0u,&sink,0)!=MODELRT_OK || bones!=75u || triangles!=2988u) {fprintf(stderr,"FRAME 0 FAIL bones=%lu triangles=%lu\n",bones,triangles);return 7;}
    first=root_y;triangles=bones=0;
    if(modelrt_visit_animation_frame(&h,0,100u,&sink,0)!=MODELRT_OK || bones!=75u || triangles!=2988u) {fprintf(stderr,"FRAME 100 FAIL bones=%lu triangles=%lu\n",bones,triangles);return 8;}
    last=root_y;
    if(first==last){fprintf(stderr,"FRAME DELTA FAIL root0=%ld root100=%ld\n",(long)first,(long)last);return 9;}
    if(modelrt_visit_animation_frame(&h,0,101u,&sink,0)==MODELRT_OK)return 10;
    printf("IQM FRAMES PASS provider=%lu clip=%s frames=%lu bones=%lu triangles=%lu fps=%lu root_y_q16=%ld,%ld delta=%ld\n",(unsigned long)h.info.codec_id,ai.name,(unsigned long)ai.frame_count,bones,triangles,(unsigned long)ai.ticks_per_second,(long)first,(long)last,(long)(last-first));
    modelrt_close(&h);
    modelrt_handle_init(&h,arena,sizeof(arena));
    if(modelrt_open_file(&rt,&h,argv[2])==MODELRT_OK){fprintf(stderr,"MALFORMED OPENED\n");return 11;}
    printf("IQM INVALID OFFSET REJECT PASS\n");
    return 0;
}
