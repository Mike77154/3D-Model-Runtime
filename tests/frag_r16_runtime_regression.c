/* CC0 synthetic FragFormats R16 runtime smoke tests, strict C89. */
#include "model_runtime89.h"
#include <stdio.h>
#include <string.h>
static unsigned char arena[128u*1024u];
typedef struct Count {unsigned long triangles;int bad;} Count;
static int on_triangle(void *u,const modelrt_triangle_view *t){
 Count *s=(Count*)u;
 if(t->mesh_index||!(t->v[0].flags&MODELRT_VTX_NORMAL)||
    !(t->v[1].flags&MODELRT_VTX_UV0)||
    t->v[1].position.x!=MODELRT_Q16_ONE||
    t->v[2].position.y!=MODELRT_Q16_ONE||
    t->v[1].uv0.x!=MODELRT_Q16_ONE||
    t->v[0].normal.z!=MODELRT_Q16_ONE)s->bad=1;
 s->triangles++;return 0;
}
#define CHECK(x) do{if(!(x)){fprintf(stderr,"R16 check %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(int argc,char**argv){
 modelrt_runtime rt;modelrt_handle h;modelrt_visit_sink sink;modelrt_q16 affine[16],inv[16];
 int i,rc;Count count;
 if(argc!=6)return 2;
 modelrt_runtime_init(&rt);CHECK(modelrt_register_builtin_providers(&rt)==MODELRT_OK);
 for(i=1;i<=2;i++){
  modelrt_handle_init(&h,arena,sizeof(arena));
  CHECK(modelrt_open_file(&rt,&h,argv[i])==MODELRT_OK);
  CHECK(h.info.codec_id==MODELRT_CODEC_FRAG_ARG);
  CHECK(h.info.mesh_count==1u);
  memset(&sink,0,sizeof(sink));sink.on_triangle=on_triangle;
  memset(&count,0,sizeof(count));
  CHECK(modelrt_visit(&h,MODELRT_ANIM_BIND_POSE,0,&sink,&count)==MODELRT_OK);
  CHECK(count.triangles==1u && count.bad==0);
  CHECK(modelrt_visit(&h,0,0,&sink,&count)==MODELRT_ERR_UNSUPPORTED);
  modelrt_close(&h);
 }
 modelrt_handle_init(&h,arena,sizeof(arena));
 CHECK(modelrt_open_file(&rt,&h,argv[3])!=MODELRT_OK);
 modelrt_handle_init(&h,arena,sizeof(arena));
 CHECK(modelrt_open_file(&rt,&h,argv[4])!=MODELRT_OK);
 modelrt_handle_init(&h,arena,sizeof(arena));
 CHECK(modelrt_open_file(&rt,&h,argv[5])==MODELRT_ERR_UNSUPPORTED);
 CHECK(strstr(modelrt_last_error(&rt),"UGH")!=0);
 for(i=0;i<16;i++)affine[i]=((i%5)==0)?MODELRT_Q16_ONE:0;
 affine[12]=2*MODELRT_Q16_ONE;
 rc=modelrt_frag_affine_inverse_q16(affine,inv);CHECK(rc==MODELRT_OK);
 CHECK(inv[12]==-2*MODELRT_Q16_ONE);
 affine[0]=0;CHECK(modelrt_frag_affine_inverse_q16(affine,inv)!=MODELRT_OK);
 puts("PASS ARG 1.1/1.2 native triangles; corrupted ARG rejected; UGH unimplemented; R16 inverse");
 return 0;
}
