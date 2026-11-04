#include "model_runtime89.h"
#include <stdio.h>

static unsigned char workspace[32u * 1024u * 1024u];
static unsigned long tri_count = 0ul;
static int on_tri(void *u, const modelrt_triangle_view *t) { (void)u; (void)t; ++tri_count; return 0; }
int main(int argc, char **argv) {
    modelrt_runtime rt; modelrt_handle h; modelrt_visit_sink sink; int rc;
    if (argc < 2) return 2;
    modelrt_runtime_init(&rt); modelrt_register_builtin_providers(&rt);
    modelrt_handle_init(&h, workspace, sizeof(workspace));
    rc = modelrt_open_file(&rt, &h, argv[1]);
    if (rc != MODELRT_OK) { printf("open error: %s\n", modelrt_last_error(&rt)); return 3; }
    printf("provider=%s meshes=%u bones=%u anims=%u caps=%08x\n", modelrt_get_provider_name(&h),
      (unsigned)h.info.mesh_count,(unsigned)h.info.bone_count,(unsigned)h.info.animation_count,(unsigned)h.info.capabilities);
    sink.on_model_begin=0;sink.on_mesh_begin=0;sink.on_material=0;sink.on_triangle=on_tri;sink.on_bone=0;sink.on_mesh_end=0;sink.on_model_end=0;
    rc=modelrt_visit(&h,MODELRT_ANIM_BIND_POSE,0,&sink,0);
    printf("visit=%d triangles=%lu\n",rc,tri_count);
    return rc==MODELRT_OK?0:4;
}
