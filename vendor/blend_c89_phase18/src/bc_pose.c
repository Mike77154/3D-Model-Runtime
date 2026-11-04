/* Export serialized pose channel LRS defaults: not evaluated animation.
   bPose is referenced by Object.pose; chanbase is a ListBase of bPoseChannel. */
#include "bc.h"
#include <string.h>
#define BC_POSE_LIMIT 8192U
static BC_U64 bc_pose_seen[BC_POSE_LIMIT];
static int bc_pose_vec(BC_File *b,unsigned int at,const char *field,unsigned int n,FILE *out)
{
  const BC_Field *f=bc_get_field(b,"bPoseChannel",field);
  unsigned int j,bits;
  int q;
  if(!f || at>2147483647U-f->offset ||
     f->offset>2147483647U-4U*n)return BC_UNSUPPORTED;
  for(j=0U;j<n;j++){
    if(bc_read_word(b,at+f->offset+4U*j,&bits)!=BC_OK ||
       bc_f32_q16(bits,&q)!=BC_OK)return BC_FAILURE;
    if(fputc('\t',out)==EOF)return BC_FAILURE;
    bc_print_q16(out,q);
  }
  return ferror(out)?BC_FAILURE:BC_OK;
}
static int bc_pose_export_one(BC_File *b,FILE *out,unsigned int object_at,unsigned int object_index,unsigned int *count)
{
  const BC_Struct *pose=bc_get_struct(b,"bPose"),*chan=bc_get_struct(b,"bPoseChannel");
  const BC_Field *cb=bc_get_field(b,"bPose","chanbase"),*fst=bc_get_field(b,"ListBase","first"),*nm=bc_get_field(b,"bPoseChannel","name");
  BC_U64 ptr,head,next;
  unsigned int at,ca,avail,i,n,k,mode;
  unsigned char mbytes[2];
  char name[97];
  if(!pose || !pose->valid || !chan || !chan->valid || !cb || !fst || !nm)return BC_UNSUPPORTED;
  if(bc_read_struct_ptr(b,object_at,"Object","pose",&ptr)!=BC_OK)return BC_UNSUPPORTED;
  if(ptr.lo==0U && ptr.hi==0U)return BC_OK;
  if(bc_resolve(b,ptr,b->type_len[pose->type],NULL,&at,&avail)!=BC_OK ||
     at>2147483647U-cb->offset || at+cb->offset>2147483647U-fst->offset)return BC_FAILURE;
  if(bc_read_pointer(b,at+cb->offset+fst->offset,&head)!=BC_OK)return BC_FAILURE;
  n=0U;
  while(head.lo || head.hi){
    if(n>=BC_POSE_LIMIT)return BC_FAILURE;
    for(i=0U;i<n;i++)if(bc_same64(bc_pose_seen[i],head))return BC_FAILURE;
    bc_pose_seen[n]=head;
    if(bc_resolve(b,head,b->type_len[chan->type],NULL,&ca,&avail)!=BC_OK)return BC_FAILURE;
    if(nm->offset>=b->type_len[chan->type])return BC_FAILURE;
    k=b->type_len[chan->type]-nm->offset;
    if(k>96U)k=96U;
    if(bc_at(b,ca+nm->offset,k,(unsigned char *)name)!=BC_OK)return BC_FAILURE;
    name[k]=0;
    for(i=0U;i<k && name[i];i++)if(name[i]=='\t'||name[i]=='\r'||name[i]=='\n')name[i]='_';
    name[i]=0;
    if(fprintf(out,"%u\t%u\t%s",object_index,n,name)<0)return BC_FAILURE;
    if(bc_pose_vec(b,ca,"loc",3U,out)!=BC_OK || bc_pose_vec(b,ca,"quat",4U,out)!=BC_OK ||
       bc_pose_vec(b,ca,"scale",3U,out)!=BC_OK)return BC_FAILURE;
    /* Phase09: append optional stored Euler angles + signed rotation mode.
       These attributes were absent in some historical/fixture SDNA structs. */
    if(bc_get_field(b,"bPoseChannel","eul")){
      if(bc_pose_vec(b,ca,"eul",3U,out)!=BC_OK)return BC_FAILURE;
    }else if(fputs("\t0\t0\t0",out)==EOF)return BC_FAILURE;
    {
      const BC_Field *mf=bc_get_field(b,"bPoseChannel","rotmode");
      mode=0U;
      if(mf){
        if(mf->offset>2147483647U-ca ||
           bc_at(b,ca+mf->offset,2U,mbytes)!=BC_OK)return BC_FAILURE;
        mode=bc_u16(mbytes,b->endian_le);
      }
      if(mode>32767U){
        if(fprintf(out,"\t-%u\n",65536U-mode)<0)return BC_FAILURE;
      }else if(fprintf(out,"\t%u\n",mode)<0)return BC_FAILURE;
    }
    if(bc_read_struct_ptr(b,ca,"bPoseChannel","next",&next)!=BC_OK)return BC_FAILURE;
    head=next;n++;
  }
  *count+=n;
  return BC_OK;
}
int bc_export_pose(BC_File *b,FILE *out)
{
  const BC_Struct *obj=bc_get_struct(b,"Object");
  const BC_Block *blk;
  unsigned int i,j,stride,object_index=0U,count=0U,at;
  int r;
  if(!obj || !obj->valid || !bc_get_field(b,"Object","pose"))return BC_UNSUPPORTED;
  stride=b->type_len[obj->type];if(!stride)return BC_UNSUPPORTED;
  if(fputs("# object\tchannel\tname\tloc_x\tloc_y\tloc_z\tquat_w\tquat_x\tquat_y\tquat_z\tscale_x\tscale_y\tscale_z\teul_x\teul_y\teul_z\trotmode\n",out)==EOF)return BC_FAILURE;
  for(i=0U;i<b->nblocks;i++){
    blk=&b->blocks[i];
    if(blk->sdna>=b->nstructs || b->structs[blk->sdna].type!=obj->type ||
       blk->code[0]!='O' || blk->code[1]!='B')continue;
    if(blk->count>blk->size/stride)return BC_FAILURE;
    for(j=0U;j<blk->count;j++){
      at=blk->offset+j*stride;
      r=bc_pose_export_one(b,out,at,object_index,&count);
      if(r!=BC_OK)return r;
      object_index++;
    }
  }
  return count && !ferror(out)?BC_OK:BC_UNSUPPORTED;
}
