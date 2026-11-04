#include "re4c3d/re4_motion_audit.h"
#include <string.h>
static int equal_fold(const char *a,const char *b)
{
 int x,y;if(!a||!b)return 0;
 do {x=(unsigned char)*a++;y=(unsigned char)*b++;
 if(x>='A'&&x<='Z')x+='a'-'A';
 if(y>='A'&&y<='Z')y+='a'-'A';
 if(x!=y)return 0;} while(x);
 return 1;
}
re4_err re4_motion_audit_fcv(re4_span src,re4_endian en,
                             re4_arena *scratch,re4_motion_audit *out)
{
 re4_fcv f;re4_err err;re4_size mark;re4_u32 i,j;
 re4_fcv_axis_view ax[3];
 if(!scratch||!out||!src.data)return RE4_ERR_NULL;
 memset(out,0,sizeof(*out));mark=scratch->pos;
 err=re4_fcv_read(src,en,scratch,&f);
 if(err!=RE4_OK){scratch->pos=mark;return err;}
 out->max_frame=f.max_time;out->blocks=f.block_count;
 for(i=0;i<f.block_count;++i){
  const re4_fcv_block *b=&f.blocks[i];
  err=re4_fcv_block_axis_views_e(src,b,en,&ax[0],&ax[1],&ax[2]);
  if(err!=RE4_OK){scratch->pos=mark;memset(out,0,sizeof(*out));return err;}
  for(j=0;j<3U;++j)out->keys+=(re4_u32)ax[j].tag_count;
  switch(b->bone_type){
   case 2U:++out->rotation_blocks;break;
   case 1U:case 64U:++out->root_blocks;break;
   case 4U:case 16U:case 32U:case 160U:++out->ik_blocks;break;
   case 8U:case 128U:++out->other_special_blocks;break;
   default:++out->unknown_blocks;break;
  }
 }
 out->nonrotation_blocks=out->blocks-out->rotation_blocks;
 scratch->pos=mark;
 return RE4_OK;
}
re4_err re4_motion_exact_fcv(re4_span a,re4_span b,re4_endian en,
                             re4_arena *scratch,int *identical)
{
 re4_motion_audit meta;re4_err er;
 if(!scratch||!identical||!a.data||!b.data)return RE4_ERR_NULL;
 *identical=0;
 er=re4_motion_audit_fcv(a,en,scratch,&meta);if(er!=RE4_OK)return er;
 er=re4_motion_audit_fcv(b,en,scratch,&meta);if(er!=RE4_OK)return er;
 if(a.size!=b.size)return RE4_OK;
 *identical=(memcmp(a.data,b.data,(size_t)a.size)==0);
 return RE4_OK;
}
int re4_motion_source_policy(const char *target,const char *source,
                             int identical,int allow_shared)
{
 if(!target||!source||!target[0]||!source[0])return RE4_MOTION_DENIED;
 if(equal_fold(target,source))return RE4_MOTION_NATIVE;
 if(allow_shared && identical==1)return RE4_MOTION_SHARED_IDENTICAL;
 return RE4_MOTION_UNVERIFIED;
}
