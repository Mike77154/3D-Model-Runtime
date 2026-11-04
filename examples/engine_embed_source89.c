/* Embedding ModelRuntime89 Source codec in any game engine. Strict C89.
 * Requires a Source .mdl with corresponding .vvd and .dx90.vtx files. */
#include "model_runtime89.h"
#include "modelrt_native89.h"
#include <stdio.h>
#include <string.h>
static unsigned char arena[176u*1024u*1024u];
static unsigned int triangles = 0u;
static int on_triangle(void *user,const modelrt_triangle_view *tri) {
    (void)user; (void)tri;
    ++triangles;
    return 0;
}
int main(int argc,char **argv) {
    modelrt_runtime rt;
    modelrt_handle handle;
    modelrt_visit_sink sink;
    int rc;
    const hl2m_mdl *source_model;
    if(argc!=2){fprintf(stderr,"usage: engine_embed_source89 file.mdl\n");return 2;}
    modelrt_runtime_init(&rt);
    if(modelrt_register_builtin_providers(&rt)!=MODELRT_OK)return 3;
    modelrt_handle_init(&handle,arena,sizeof(arena));
    rc=modelrt_open_file(&rt,&handle,argv[1]);
    if(rc!=MODELRT_OK){fprintf(stderr,"%s\n",modelrt_last_error(&rt));return 4;}
    source_model=modelrt_native_source_mdl(&handle);
    if(!source_model){fprintf(stderr,"This file was not handled by Source HL2.\n");return 5;}
    memset(&sink,0,sizeof(sink));sink.on_triangle=on_triangle;
    rc=modelrt_visit(&handle,MODELRT_ANIM_BIND_POSE,0,&sink,0);
    printf("Source native name=%s triangles=%u bones=%d visit=%d\n",
           source_model->name,triangles,source_model->bone_count,rc);
    modelrt_close(&handle);
    return rc==MODELRT_OK?0:6;
}
