#include "c3dae.h"
#include <stdio.h>
#include <stdlib.h>
static c3dae_context ctx;
static c3dae_model model;
static c3dae_vertex verts[30000];
static c3dae_triangle tri[40000];
static c3dae_mesh meshes[128];
static char bytes[1024*1024];
int main(int argc,char**argv){ FILE*f; size_t n;int rc;if(argc<2)return 2;f=fopen(argv[1],"rb");if(!f)return 1;n=fread(bytes,1,sizeof(bytes)-1,f);fclose(f);bytes[n]=0;c3dae_context_init(&ctx);c3dae_model_init(&model,verts,30000,tri,40000,meshes,128);rc=c3dae_load_memory(&ctx,bytes,(c3dae_u32)n,&model);printf("rc=%d ctx_err=%d %s v=%lu t=%lu mesh=%lu sources=%u data=%lu msg=%s\n",rc,ctx.error_code,c3dae_error_string(rc),(unsigned long)model.vertex_count,(unsigned long)model.triangle_count,(unsigned long)model.mesh_count,ctx.source_count,(unsigned long)ctx.source_data_count,ctx.error_message);return 0;}
