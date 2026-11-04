#include "hlfp.h"
#include <stdio.h>
#include <string.h>

#define MAX_BYTES (32U*1024U*1024U)
static unsigned char blob[MAX_BYTES];
static hlfp_mdl mdl;
static hlfp_mdl_mesh_expanded mesh;
static hlfp_mdl_pose_set pose;
static hlfp_error err;

int main(int argc, char **argv) {
 FILE *fp; long n;size_t rd;int rc,pr,ex,ani;int j,mm,found;
 if(argc<2) return 1;
 fp=fopen(argv[1],"rb");if(!fp){puts("IO_ERR");return 2;}
 if(fseek(fp,0,SEEK_END)!=0 || (n=ftell(fp))<0 || (unsigned long)n>MAX_BYTES){fclose(fp); puts("TOO_BIG");return 2;}
 rewind(fp);rd=fread(blob,1,(size_t)n,fp);fclose(fp);if(rd!=(size_t)n){puts("READ_ERR");return 2;}
 hlfp_mdl_init(&mdl);
 rc=hlfp_mdl_parse_main(blob,(size_t)n,&mdl,&err);pr=rc;ex=0;ani=0;
 if(rc==HLFP_OK){
  mm=0;found=0;
  for(j=0;j<mdl.num_meshes;++j){if(mdl.meshes[j].numtris>0){mm=j;found=1;break;}}
  if(found) {ex=hlfp_mdl_expand_mesh(&mdl,mdl.meshes[mm].parent_model,mm,&mesh,&err);if(ex!=0) fprintf(stderr,"MESH_ERROR %d mesh=%d numtris=%d triindex=%d nv=%d nn=%d first=%d: %s | %s\n",ex,mm,mdl.meshes[mm].numtris,mdl.meshes[mm].triindex,mdl.models[mdl.meshes[mm].parent_model].numverts,mdl.models[mdl.meshes[mm].parent_model].numnorms,mdl.models[mdl.meshes[mm].parent_model].first_mesh,err.message,argv[1]);}
  for(j=0;j<mdl.num_seqdesc;++j){if(mdl.seqdesc[j].numframes>0 && mdl.seqdesc[j].numblends>0){ani=hlfp_mdl_decode_sequence_frame(&mdl,j,0,0,&pose,&err);break;}}
 }
 printf("%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%s\n",pr,ex,ani,mdl.num_bones,mdl.num_seqdesc,mdl.num_models,mdl.num_meshes,mdl.num_textures,mdl.num_skinfamilies,mdl.num_events,argv[1]);
 return 0;
}
