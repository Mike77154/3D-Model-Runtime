/* C89 native BLEND provider regression. Two 2.79 synthetic Phase13/16 scenes. */
#include "model_runtime89.h"
#include <stdio.h>
#include <string.h>
static unsigned char arena[24u*1024u*1024u];
static unsigned char small[1024u];
typedef struct Probe {unsigned int meshes,tris,bad;int saw_pos;} Probe;
static int begin(void *p,const modelrt_mesh_info *m){Probe *s=(Probe*)p;
 if(m->index!=s->meshes || m->vertex_count!=3u||m->triangle_count!=1u)s->bad++;
 s->meshes++;return 0;
}
static int tri(void *p,const modelrt_triangle_view *t){Probe *s=(Probe*)p;
 if(t->mesh_index>1u||t->material_index!=-1)s->bad++;
 if(t->v[0].position.x==MODELRT_Q16_ONE || t->v[0].position.y==2*MODELRT_Q16_ONE)s->saw_pos=1;
 s->tris++;return 0;
}
#define CHECK(v) do {if(!(v)){fprintf(stderr,"BLEND check %d: %s\n",__LINE__,#v);return 1;}} while(0)
int main(int argc,char **argv){
 modelrt_runtime rt;modelrt_handle h;modelrt_visit_sink callbacks;Probe p;int i;
 if(argc!=3)return 2;
 modelrt_runtime_init(&rt);CHECK(modelrt_register_builtin_providers(&rt)==MODELRT_OK);
 for(i=1;i<3;i++){
  modelrt_handle_init(&h,small,sizeof(small));
  CHECK(modelrt_open_file(&rt,&h,argv[i])==MODELRT_ERR_NO_SPACE);
  modelrt_handle_init(&h,arena,sizeof(arena));
  CHECK(modelrt_open_file(&rt,&h,argv[i])==MODELRT_OK);
  CHECK(h.info.codec_id==MODELRT_CODEC_BLEND);
  CHECK(h.info.mesh_count==2u);
  CHECK(h.info.animation_count==0u);
  memset(&p,0,sizeof(p));memset(&callbacks,0,sizeof(callbacks));
  callbacks.on_mesh_begin=begin;callbacks.on_triangle=tri;
  CHECK(modelrt_visit(&h,MODELRT_ANIM_BIND_POSE,0,&callbacks,&p)==MODELRT_OK);
  CHECK(p.meshes==2u && p.tris==2u && !p.bad && p.saw_pos);
  CHECK(modelrt_visit(&h,0,0,&callbacks,&p)==MODELRT_ERR_UNSUPPORTED);
  modelrt_close(&h);
 }
 puts("PASS Blender Phase18 direct C89 native geometry; 2 synthetic 2-mesh scenes; no fabricated animations");return 0;
}
