#include "hlfp_bundle.h"
#include <stdio.h>

#define MAIN_CAP (8U * 1024U * 1024U)
#define TEX_CAP (8U * 1024U * 1024U)
#define SEQ_CAP (8U * 1024U * 1024U)
static hlfp_u8 main_buf[MAIN_CAP];
static hlfp_u8 tex_buf[TEX_CAP];
static hlfp_u8 seq_buf[SEQ_CAP];
static hlfp_mdl mdl;
static hlfp_mdl_mesh_expanded expanded;
static hlfp_mdl_pose_set pose;
static hlfp_error err;

static int read_disk(void *user,const char*path,hlfp_u8 *buf,size_t cap,size_t *out_size) {
 FILE *f;long bytes;size_t got;
 (void)user;
 f=fopen(path,"rb");if(!f)return HLFP_ERR_MISSING_DATA;
 if(fseek(f,0,SEEK_END)!=0 || (bytes=ftell(f))<0 || (unsigned long)bytes>cap){fclose(f);return HLFP_ERR_CAPACITY;}
 rewind(f);got=fread(buf,1,(size_t)bytes,f);fclose(f);
 if(got!=(size_t)bytes)return HLFP_ERR_IO;
 *out_size=got;return HLFP_OK;
}
int main(int argc,char**argv) {
 hlfp_bundle_buffers w;
 hlfp_bundle_stats stat;
 int rc,i,mesh_ok=0,anim_ok=0;
 if(argc<2) {fprintf(stderr,"usage: bundle_probe file.mdl\n");return 2;}
 w.main_bytes=main_buf;w.main_capacity=MAIN_CAP;
 w.texture_bytes=tex_buf;w.texture_capacity=TEX_CAP;
 w.sequence_pool=seq_buf;w.sequence_capacity=SEQ_CAP;
 rc=hlfp_bundle_load(argv[1],read_disk,0,&w,&mdl,&stat,&err);
 if(rc!=HLFP_OK){fprintf(stderr,"BUNDLE_ERROR %d %s : %s\n",rc,err.message,argv[1]);return 1;}
 for(i=0;i<mdl.num_meshes;++i) {
  if(mdl.meshes[i].numtris==0)continue;
  rc=hlfp_mdl_expand_mesh(&mdl,mdl.meshes[i].parent_model,i,&expanded,&err);
  if(rc!=HLFP_OK){fprintf(stderr,"MESH_ERROR %d %s : %s\n",rc,err.message,argv[1]);return 3;}
  ++mesh_ok;
 }
 for(i=0;i<mdl.num_seqdesc;++i) {
  if(mdl.seqdesc[i].numframes <= 0 || mdl.seqdesc[i].numblends <= 0)continue;
  rc=hlfp_mdl_decode_sequence_frame(&mdl,i,0,mdl.seqdesc[i].numframes-1,&pose,&err);
  if(rc!=HLFP_OK){fprintf(stderr,"ANIM_ERROR %d %s : %s\n",rc,err.message,argv[1]);return 4;}
  ++anim_ok;
 }
 printf("ok=1 texture_external=%d seqgroup_external=%d meshes=%d animations=%d path=%s\n",stat.has_external_textures,stat.sequence_files_loaded,mesh_ok,anim_ok,argv[1]);
 return 0;
}
