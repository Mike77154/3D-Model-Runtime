#include "../include/model_runtime89.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned char workspace[8u*1024u*1024u];
static unsigned long tr_count,mesh_count,mat_count;
static int invalid;
static int triangle(void *v,const modelrt_triangle_view *t)
{
 int k;(void)v;
 for(k=0;k<3;k++){
  if(!(t->v[k].flags&MODELRT_VTX_NORMAL)||!(t->v[k].flags&MODELRT_VTX_UV0))invalid++;
 }
 ++tr_count;return 0;
}
static int mesh(void *v,const modelrt_mesh_info *m)
{(void)v;(void)m;++mesh_count;return 0;}
static int mat(void *v,const modelrt_material_view *m)
{(void)v;if(m->texture_index!=-1)invalid++;++mat_count;return 0;}
int main(int argc,char **argv)
{
 modelrt_runtime rt;modelrt_handle h;modelrt_visit_sink sink;
 const modelrt_model_info *info;modelrt_animation_info a;
 int rc;unsigned long ntris,nmeshes,nclips;
 if(argc!=5)return 2;
 ntris=strtoul(argv[2],0,10);nmeshes=strtoul(argv[3],0,10);nclips=strtoul(argv[4],0,10);
 modelrt_runtime_init(&rt);if(modelrt_register_builtin_providers(&rt)!=MODELRT_OK)return 3;
 modelrt_handle_init(&h,workspace,sizeof(workspace));rc=modelrt_open_file(&rt,&h,argv[1]);
 if(rc!=MODELRT_OK){printf("FAIL HOD2 open: %s\n",modelrt_last_error(&rt));return 4;}
 info=modelrt_get_info(&h);
 if(!info||info->codec_id!=MODELRT_CODEC_HOD2||info->mesh_count!=nmeshes||info->animation_count!=nclips)return 5;
 memset(&sink,0,sizeof(sink));sink.on_triangle=triangle;sink.on_mesh_begin=mesh;sink.on_material=mat;
 if(modelrt_visit(&h,MODELRT_ANIM_BIND_POSE,0,&sink,0)!=MODELRT_OK)return 6;
 if(tr_count!=ntris||mesh_count!=nmeshes||invalid)return 7;
 if(nclips){
  if(modelrt_animation_info_get(&h,0,&a)!=MODELRT_OK||a.frame_count!=4||a.ticks_per_second!=60||!(a.flags&MODELRT_ANIM_UNBOUND))return 8;
  if(modelrt_visit(&h,0,0,&sink,0)!=MODELRT_ERR_UNSUPPORTED)return 9;
 }else if(ntris && mat_count==0)return 10;
 printf("PASS HOD2 %s triangles=%lu meshes=%lu materials=%lu clips=%lu\n",argv[1],tr_count,mesh_count,mat_count,nclips);
 modelrt_close(&h);return 0;
}
