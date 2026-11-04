#include "model_runtime89.h"
#include <stdio.h>
#include <string.h>
#define OK(v) do{if(!(v)){fprintf(stderr,"fail %d: %s\n",__LINE__,#v);return 1;}}while(0)
int main(int argc,char**argv){
 modelrt_frag_audit a;int rc;
 if(argc!=3)return 2;
 rc=modelrt_frag_r16_audit(argv[1],&a);
 printf("X R16 audit rc=%d mesh=%lu vertices=%lu tri=%lu\n",rc,(unsigned long)a.meshes,(unsigned long)a.vertices,(unsigned long)a.triangles);
 OK(rc==MODELRT_OK && a.format==MODELRT_FRAG_AUDIT_X && a.meshes>0 && a.triangles>0);
 rc=modelrt_frag_r16_audit(argv[2],&a);
 printf("DBO R16 audit rc=%d mesh=%lu vertices=%lu\n",rc,(unsigned long)a.meshes,(unsigned long)a.vertices);
 OK(rc==MODELRT_OK && a.format==MODELRT_FRAG_AUDIT_DBO && a.meshes>0);
 return 0;
}
