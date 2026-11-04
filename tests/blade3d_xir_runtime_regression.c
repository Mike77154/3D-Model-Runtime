/* Runtime75: XIR1 test container geometry; native XMDL remains unimplemented.
 * All inputs here are generated/synthetic; strict C89.
 */
#include "model_runtime89.h"
#include <stdio.h>
#include <string.h>
static unsigned char arena[32768];
typedef struct test_count_s {unsigned long tris;int bad;int reject;} test_count;
#define CHECK(x) do{if(!(x)){fprintf(stderr,"Blade3D regression failed at line %d: %s\n",__LINE__,#x);return 1;}}while(0)
static int receive_triangle(void *u,const modelrt_triangle_view *t){
 test_count *c=(test_count*)u;
 if(t->mesh_index!=0u || t->material_index!=-1 ||
    !(t->v[0].flags & MODELRT_VTX_NORMAL) || !(t->v[0].flags & MODELRT_VTX_UV0) ||
    t->v[0].normal.z!=MODELRT_Q16_ONE || t->v[1].position.x!=MODELRT_Q16_ONE ||
    t->v[1].uv0.x!=MODELRT_Q16_ONE || t->v[2].position.y!=MODELRT_Q16_ONE ||
    t->v[2].uv0.y!=MODELRT_Q16_ONE)c->bad=1;
 c->tris++;
 return c->reject?1:0;
}
int main(int argc,char **argv){
 modelrt_runtime rt;modelrt_handle h;modelrt_visit_sink sink;test_count c;
 int rc,i;
 if(argc!=8){fprintf(stderr,"usage: test good.xir badindex.xir truncated.xir badcount.xir candidate.xmdl invalid.xmdl nan.xir\n");return 2;}
 modelrt_runtime_init(&rt);CHECK(modelrt_register_builtin_providers(&rt)==MODELRT_OK);
 modelrt_handle_init(&h,arena,sizeof(arena));
 CHECK(modelrt_open_file(&rt,&h,argv[1])==MODELRT_OK);
 CHECK(h.info.codec_id==MODELRT_CODEC_BLADE_XIR);
 CHECK(h.info.mesh_count==1u && (h.info.capabilities&MODELRT_CAP_GEOMETRY)!=0u);
 memset(&sink,0,sizeof(sink));sink.on_triangle=receive_triangle;
 memset(&c,0,sizeof(c));
 CHECK(modelrt_visit(&h,MODELRT_ANIM_BIND_POSE,0,&sink,&c)==MODELRT_OK);
 CHECK(c.tris==2UL && !c.bad);
 CHECK(modelrt_visit(&h,0,0,&sink,&c)==MODELRT_ERR_UNSUPPORTED);
 c.reject=1;c.tris=0;
 CHECK(modelrt_visit(&h,MODELRT_ANIM_BIND_POSE,0,&sink,&c)==MODELRT_ERR_CALLBACK);
 modelrt_close(&h);
 for(i=2;i<=4;i++){
  modelrt_handle_init(&h,arena,sizeof(arena));
  rc=modelrt_open_file(&rt,&h,argv[i]);
  CHECK(rc!=MODELRT_OK);
 }
 modelrt_handle_init(&h,arena,sizeof(arena));
 CHECK(modelrt_open_file(&rt,&h,argv[5])==MODELRT_ERR_UNSUPPORTED);
 CHECK(strstr(modelrt_last_error(&rt),"XMDL")!=0);
 modelrt_handle_init(&h,arena,sizeof(arena));
 CHECK(modelrt_open_file(&rt,&h,argv[6])!=MODELRT_OK);
 modelrt_handle_init(&h,arena,sizeof(arena));
 CHECK(modelrt_open_file(&rt,&h,argv[7])!=MODELRT_OK);
 puts("PASS XIR1 2 triangles UV+normals; invalid indices, truncated/overcount rejected; XMDL probe-only");
 return 0;
}
