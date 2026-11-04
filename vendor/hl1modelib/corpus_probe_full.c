#include "hlfp.h"
#include <stdio.h>
#include <string.h>

#define MAX_BYTES (32U*1024U*1024U)
static unsigned char blob[MAX_BYTES];
static unsigned char extra[MAX_BYTES];
static unsigned char seqbuf[MAX_BYTES];
static hlfp_mdl mdl;
static hlfp_mdl_mesh_expanded mesh;
static hlfp_mdl_pose_set pose;
static hlfp_error err;
static int read_file(const char *p, unsigned char *b, size_t *len) {
 FILE *fp=fopen(p,"rb");long n;
 if(!fp) return 0;
 if(fseek(fp,0,SEEK_END)!=0 || (n=ftell(fp))<0 || (unsigned long)n>MAX_BYTES){fclose(fp);return 0;}
 rewind(fp);*len=fread(b,1,(size_t)n,fp);fclose(fp);return *len==(size_t)n;
}
int main(int argc, char **argv) {
 size_t n,e,s;int rc,pr,ex=0,ani=0,atts=0,attq=0;int j,mm=0,first=0;char p[8192];char *dot;int badseq=0,goodseq=0,meshok=0,meshbad=0;int maxverts=0,maxfaces=0;int f,bl,steps,nframes,nblends;
 if(argc<2 || !read_file(argv[1],blob,&n)) return 1;
 hlfp_mdl_init(&mdl);rc=hlfp_mdl_parse_main(blob,n,&mdl,&err);pr=rc;
 if(rc==HLFP_OK){
  strcpy(p,argv[1]);dot=strrchr(p,'.');
  if(dot && mdl.header.textureindex==0){strcpy(dot,"T.mdl");if(read_file(p,extra,&e)){atts=hlfp_mdl_attach_texture_file(&mdl,extra,e,&err)==0?1:-1;}}
  if(dot && mdl.num_seqgroups>1){strcpy(dot,"01.mdl");if(read_file(p,seqbuf,&s)){attq=hlfp_mdl_attach_seqgroup_file(&mdl,1,seqbuf,s,&err)==0?1:-1;}}
  for(j=0;j<mdl.num_meshes;++j) {
   if(mdl.meshes[j].numtris>0) {
    ex=hlfp_mdl_expand_mesh(&mdl,mdl.meshes[j].parent_model,j,&mesh,&err);
    if(ex) {meshbad++;if(!first){mm=j;first=1;} } else {meshok++; if(mesh.num_vertices>maxverts)maxverts=mesh.num_vertices;if(mesh.num_triangles>maxfaces)maxfaces=mesh.num_triangles;}
   }
  }
  for(j=0;j<mdl.num_seqdesc;++j) {
   if(mdl.seqdesc[j].numframes>0 && mdl.seqdesc[j].numblends>0){
    nframes=mdl.seqdesc[j].numframes;nblends=mdl.seqdesc[j].numblends;
    for(bl=0;bl<nblends;bl+=(nblends>1?nblends-1:1)) {
     for(steps=0;steps<3;steps++) {
      f=(steps==0)?0:(steps==1?nframes/2:nframes-1);
      ani=hlfp_mdl_decode_sequence_frame(&mdl,j,bl,f,&pose,&err);
      if(ani){badseq++;if(badseq<=2)fprintf(stderr,"ANIM_ERROR %d seq=%d blend=%d frame=%d/%d %s | %s\n",ani,j,bl,f,nframes,err.message,argv[1]);}
      else goodseq++;
     }
    }
   }
  }
 }
 printf("%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%s\n",pr,atts,attq,meshok,meshbad,goodseq,badseq,mdl.num_bones,mdl.num_seqdesc,mdl.num_models,mdl.num_meshes,mdl.num_textures,maxverts,maxfaces,first,argv[1]);
 if(first) fprintf(stderr,"BAD_MESH %d %s\n",mm,argv[1]);
 return 0;
}
