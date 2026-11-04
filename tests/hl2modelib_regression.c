#include "../include/model_runtime89.h"
#include <stdio.h>
#include <string.h>
/* Build peak ~120 MiB with full native Source 1M vertex capacity. */
static unsigned char workspace[176u*1024u*1024u];
typedef struct cap_s { unsigned long tri; modelrt_vec3 first; } cap;
static int on_tri(void *u,const modelrt_triangle_view *t){ cap *c=(cap*)u;if(!c->tri)c->first=t->v[0].position;c->tri++;return 0;}
int main(int argc,char **argv) {
 modelrt_runtime rt;modelrt_handle h;modelrt_visit_sink sink;modelrt_animation_info info;modelrt_texture_info tex;
 modelrt_u32 width,height;size_t used;static modelrt_u8 pixels[256];cap c;int rc;
 if(argc!=3)return 2;
 modelrt_runtime_init(&rt);if(modelrt_register_builtin_providers(&rt)!=MODELRT_OK)return 3;
 if(rt.provider_count!=11u){printf("provider count=%u\n",(unsigned)rt.provider_count);return 4;}
 modelrt_handle_init(&h,workspace,sizeof(workspace));rc=modelrt_open_file(&rt,&h,argv[1]);
 if(rc!=MODELRT_OK){printf("HL2 failed: %s\n",modelrt_last_error(&rt));return 5;}
 if(h.info.codec_id!=MODELRT_CODEC_SOURCE_HL2){printf("incorrect codec=%u\n",h.info.codec_id);return 6;}
 memset(&sink,0,sizeof(sink));memset(&c,0,sizeof(c));sink.on_triangle=on_tri;
 rc=modelrt_visit(&h,MODELRT_ANIM_BIND_POSE,0,&sink,&c);
 if(rc!=MODELRT_OK || !c.tri){printf("visit %d tris %lu\n",rc,c.tri);return 7;}
 rc=modelrt_texture_info_get(&h,0,&tex);
 if(rc!=MODELRT_OK || tex.kind!=MODELRT_TEXTURE_NATIVE_FILE){printf("VTF path not found: %d %u\n",rc,tex.kind);return 11;}
 width=height=0u;used=0u;
 rc=modelrt_texture_decode_rgba(&h,0,pixels,sizeof(pixels),&width,&height,&used);
 if(rc!=MODELRT_OK || width!=4u || height!=4u || used!=64u || pixels[0]!=255u || pixels[1]!=0u){printf("VTF decode wrong: %d\n",rc);return 12;}
 printf("SOURCE native VMT->VTF OK: %ux%u first=(%u,%u,%u,%u)\n",width,height,pixels[0],pixels[1],pixels[2],pixels[3]);
 printf("SOURCE HL2 OK: meshes=%u verts-tri=%lu bones=%u anims=%u\n",(unsigned)h.info.mesh_count,c.tri,(unsigned)h.info.bone_count,(unsigned)h.info.animation_count);
 if(h.info.animation_count){rc=modelrt_animation_info_get(&h,0,&info);printf("animation metadata=%d frames=%u\n",rc,info.frame_count);}
 modelrt_close(&h);
 modelrt_handle_init(&h,workspace,sizeof(workspace));rc=modelrt_open_file(&rt,&h,argv[2]);
 if(rc!=MODELRT_OK || h.info.codec_id!=MODELRT_CODEC_SOURCE_HL2){printf("SMD failed %s\n",modelrt_last_error(&rt));return 8;}
 memset(&c,0,sizeof(c));rc=modelrt_visit(&h,-1,0,&sink,&c);
 if(rc!=MODELRT_OK || !c.tri){printf("SMD visit failed %d\n",rc);return 9;}
 printf("SOURCE SMD OK: triangles=%lu\n",c.tri);modelrt_close(&h);
 return 0;
}
