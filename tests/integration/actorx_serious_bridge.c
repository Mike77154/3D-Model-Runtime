/* Strict C89 integration regression. Invocation: TEST serious.mdl triangle.psk triangle.psa */
#include "model_runtime89.h"
#include "modelrt_native89.h"
#include <stdio.h>
#include <string.h>

static unsigned char workspace_a[64u*1024u*1024u];
static unsigned char workspace_b[64u*1024u*1024u];
static unsigned char bind_mem[2u*1024u*1024u];
static unsigned long tri_count;
static modelrt_vec3 last_first_vertex;
static int on_tri(void *u, const modelrt_triangle_view *t)
{
    (void)u;
    if (t->v[0].flags & MODELRT_VTX_UV0) { /* view has authored UV */ }
    last_first_vertex=t->v[0].position;
    ++tri_count;
    return 0;
}
static int visit(modelrt_handle *h, int index, modelrt_q16 time)
{
    modelrt_visit_sink sink;
    int rc;
    memset(&sink,0,sizeof(sink));sink.on_triangle=on_tri;
    tri_count=0u;
    rc=modelrt_visit(h,index,time,&sink,0);
    if (rc != MODELRT_OK || tri_count == 0u) return 0;
    return 1;
}
int main(int argc,char **argv)
{
    modelrt_runtime runtime;
    modelrt_handle model,anim;
    modelrt_animation_binding binding;
    modelrt_visit_sink sink;
    modelrt_animation_info info;
    modelrt_vec3 pos0;
    int rc;
    if (argc!=4) { puts("usage: test serious.mdl triangle.psk triangle.psa");return 2; }
    modelrt_runtime_init(&runtime);
    rc=modelrt_register_builtin_providers(&runtime);
    if(rc!=MODELRT_OK||runtime.provider_count!=10u) { puts("REGISTRY FAIL");return 3; }
    modelrt_handle_init(&model,workspace_a,sizeof(workspace_a));
    rc=modelrt_open_file(&runtime,&model,argv[1]);
    if(rc!=MODELRT_OK || model.info.codec_id!=MODELRT_CODEC_SERIOUSSAM || !modelrt_native_serioussam(&model) || !visit(&model,-1,0) || !visit(&model,0,0)) {
        printf("SERIOUS FAIL %s\n",modelrt_last_error(&runtime));return 4;
    }
    printf("SERIOUS OK: %lu tris\n",tri_count);
    modelrt_close(&model);
    modelrt_handle_init(&model,workspace_a,sizeof(workspace_a));
    rc=modelrt_open_file(&runtime,&model,argv[2]);
    if(rc!=MODELRT_OK || model.info.codec_id!=MODELRT_CODEC_ACTORX || !modelrt_native_psk(&model) || !visit(&model,-1,0)) {
        printf("PSK FAIL %s\n",modelrt_last_error(&runtime));return 5;
    }
    printf("PSK OK: %lu tris\n",tri_count);
    modelrt_handle_init(&anim,workspace_b,sizeof(workspace_b));
    rc=modelrt_open_file(&runtime,&anim,argv[3]);
    if(rc!=MODELRT_OK || !modelrt_native_psa(&anim) || modelrt_animation_info_get(&anim,0,&info)!=MODELRT_OK) {
        printf("PSA FAIL %s\n",modelrt_last_error(&runtime));return 6;
    }
    if (info.frame_count<2u) { puts("PSA FRAME FAIL");return 7; }
    modelrt_animation_binding_init(&binding,bind_mem,sizeof(bind_mem));
    rc=modelrt_animation_bind(&binding,&model,&anim);
    if(rc!=MODELRT_OK || modelrt_animation_bound_count(&binding)!=1) {
        printf("BIND FAIL %d\n",rc);return 8;
    }
    memset(&sink,0,sizeof(sink));sink.on_triangle=on_tri;
    tri_count=0u;
    rc=modelrt_visit_bound_animation(&binding,0,0,&sink,0);
    if(rc!=MODELRT_OK || tri_count!=1u) {printf("PSA FIRST FRAME FAIL %d\n",rc);return 9;}
    pos0=last_first_vertex;
    tri_count=0u;
    rc=modelrt_visit_bound_animation(&binding,0,2184,&sink,0);
    if(rc!=MODELRT_OK || tri_count!=1u) {printf("PSA SECOND FRAME FAIL %d\n",rc);return 10;}
    if (pos0.x==last_first_vertex.x && pos0.y==last_first_vertex.y &&
        pos0.z==last_first_vertex.z) { puts("PSA ANIMATED VERTEX DID NOT MOVE");return 11; }
    puts("PSA BIND + SKIN OK (vertex changed at second frame)");
    modelrt_animation_unbind(&binding);
    modelrt_close(&anim);modelrt_close(&model);
    return 0;
}
