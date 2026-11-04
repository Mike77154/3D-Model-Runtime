/* Phase10: serialized Object graph, Armature modifier targets, raw local TRS.
 * This is a READ-ONLY audit, NOT evaluated depsgraph state. C89, static memory. */
#include "bc.h"
#include "bc_rig_q16.h"
#include <string.h>
#include <limits.h>

#define BC_OBJ_CAP 4096U
#define BC_OBJ_MOD_CAP 4096U
#define BC_OBJ_NAME 96U

typedef struct BC_ObjNode {
  BC_U64 old,data,parent,armature;
  char name[BC_OBJ_NAME];
  unsigned int at,mods,arms,other;
  unsigned int unresolved, unsupported_deform;
  int transform_ok;
  BC_RigMat local;
} BC_ObjNode;
static BC_ObjNode bc_objects[BC_OBJ_CAP];
static BC_U64 bc_mod_visited[BC_OBJ_MOD_CAP];

static int bc_obj_zero(BC_U64 v) {return v.lo==0U && v.hi==0U;}
static int bc_obj_index(BC_U64 ptr,unsigned int n)
{
  unsigned int i;
  if(bc_obj_zero(ptr))return -1;
  for(i=0U;i<n;i++)if(bc_same64(ptr,bc_objects[i].old))return (int)i;
  return -1;
}
static int bc_obj_offset(BC_File *b,unsigned int at,const char *structure,
                         const char *field,unsigned int *value)
{
  const BC_Field *f=bc_get_field(b,structure,field);
  const BC_Struct *s=bc_get_struct(b,structure);
  unsigned int len;
  if(!f || !s || !s->valid)return BC_UNSUPPORTED;
  len=b->type_len[s->type];
  if(f->offset>len ||at>UINT_MAX-f->offset)return BC_FAILURE;
  *value=at+f->offset;
  return BC_OK;
}
static int bc_obj_name(BC_File *b,unsigned int at,char name[BC_OBJ_NAME])
{
  const BC_Field *id=bc_get_field(b,"Object","id");
  const BC_Field *f=bc_get_field(b,"ID","name");
  const BC_Struct *st=bc_get_struct(b,"ID");
  unsigned int offset,n,i;
  if(!id||!f||!st||!st->valid)return BC_UNSUPPORTED;
  n=b->type_len[st->type];
  if(f->offset>=n ||id->offset>UINT_MAX-f->offset ||
     at>UINT_MAX-id->offset-f->offset)return BC_FAILURE;
  offset=at+id->offset+f->offset;
  n-=f->offset;if(n>=BC_OBJ_NAME)n=BC_OBJ_NAME-1U;
  if(bc_at(b,offset,n,(unsigned char *)name)!=BC_OK)return BC_FAILURE;
  name[n]=0;
  for(i=0U;i<n&&name[i];i++){
    if(name[i]=='\t'||name[i]=='\n'||name[i]=='\r')name[i]='_';
  }
  if(i==n)return BC_FAILURE;
  /* The ID prefix is two bytes, for example OB, ME, AR. */
  if(name[0]=='O' && name[1]=='B')memmove(name,name+2U,strlen(name+2U)+1U);
  return BC_OK;
}
static int bc_obj_local(BC_File *b,unsigned int at,BC_RigMat *mat)
{
  int loc[3],scale[3],rot[3],quat[4],mode;
  const char *fields[3]={"loc","scale","rot"};
  unsigned int off,bits,i,j;
  for(j=0U;j<3U;j++){
    if(bc_obj_offset(b,at,"Object",fields[j],&off)!=BC_OK){
      /* Blender's saved SDNA still calls object scale 'size' in many files. */
      if(j!=1U ||bc_obj_offset(b,at,"Object","size",&off)!=BC_OK)
        return BC_UNSUPPORTED;
    }
    for(i=0U;i<3U;i++){
      if(off>UINT_MAX-4U*i ||bc_read_word(b,off+i*4U,&bits)!=BC_OK)
        return BC_FAILURE;
      if(j==0U){if(bc_f32_q16(bits,&loc[i])!=BC_OK)return BC_UNSUPPORTED;}
      if(j==1U){if(bc_f32_q16(bits,&scale[i])!=BC_OK)return BC_UNSUPPORTED;}
      if(j==2U){if(bc_f32_q16(bits,&rot[i])!=BC_OK)return BC_UNSUPPORTED;}
    }
  }
  if(bc_obj_offset(b,at,"Object","rotmode",&off)!=BC_OK)return BC_UNSUPPORTED;
  {
    unsigned char raw[2];
    if(bc_at(b,off,2U,raw)!=BC_OK)return BC_FAILURE;
    mode=(int)(short)bc_u16(raw,b->endian_le);
  }
  if(mode>=1 && mode<=6){
    if(bc_rig_euler_quat(rot,(unsigned int)mode,quat)!=BC_RIG_OK)return BC_UNSUPPORTED;
  }else if(mode==0){
    if(bc_obj_offset(b,at,"Object","quat",&off)!=BC_OK)return BC_UNSUPPORTED;
    for(i=0U;i<4U;i++){
      if(off>UINT_MAX-4U*i ||bc_read_word(b,off+i*4U,&bits)!=BC_OK ||
         bc_f32_q16(bits,&quat[i])!=BC_OK)return BC_UNSUPPORTED;
    }
  }else return BC_UNSUPPORTED;
  return bc_rig_mat_from_trs(loc,quat,scale,mat)==BC_RIG_OK?BC_OK:BC_UNSUPPORTED;
}
static int bc_obj_modifiers(BC_File *b,BC_ObjNode *node)
{
  const BC_Field *first=bc_get_field(b,"ListBase","first");
  unsigned int at,i,avail;
  BC_U64 cur,nxt,target;
  const BC_Block *blk;
  const char *type;
  if(!first || bc_obj_offset(b,node->at,"Object","modifiers",&at)!=BC_OK)
    return BC_UNSUPPORTED;
  if(at>UINT_MAX-first->offset ||
     bc_read_pointer(b,at+first->offset,&cur)!=BC_OK)return BC_FAILURE;
  while(!bc_obj_zero(cur)){
    if(node->mods>=BC_OBJ_MOD_CAP)return BC_FAILURE;
    for(i=0U;i<node->mods;i++)if(bc_same64(cur,bc_mod_visited[i]))return BC_FAILURE;
    bc_mod_visited[node->mods]=cur;
    if(bc_resolve(b,cur,(unsigned int)b->ptr_size,&blk,&at,&avail)!=BC_OK)
      return BC_FAILURE;
    if(blk->sdna>=b->nstructs)return BC_FAILURE;
    type=bc_type(b,b->structs[blk->sdna].type);
    if(!type)return BC_FAILURE;
    if(strcmp(type,"ArmatureModifierData")==0){
      if(bc_read_struct_ptr(b,at,"ArmatureModifierData","object",&target)!=BC_OK)
        return BC_FAILURE;
      if(bc_obj_zero(target))node->unresolved++;
      else if(node->arms && !bc_same64(node->armature,target))node->unresolved++;
      else node->armature=target;
      {
        const BC_Field *flags=bc_get_field(b,"ArmatureModifierData","deformflag");
        const BC_Field *group=bc_get_field(b,"ArmatureModifierData","defgrp_name");
        unsigned char raw[2],name[64];
        unsigned int f;
        if(!flags ||!group ||avail<64U ||flags->offset>avail-2U ||
           group->offset>avail-64U ||
           bc_at(b,at+flags->offset,2U,raw)!=BC_OK ||
           bc_at(b,at+group->offset,64U,name)!=BC_OK)return BC_FAILURE;
        f=bc_u16(raw,b->endian_le);
        /* Only vertex-group linear skinning, without envelope/dual-quaternion,
           inversion or modifier-level influence group, is implemented. */
        if(f!=1U ||name[0]!=0U)node->unsupported_deform++;
      }
      node->arms++;
    }else node->other++;
    if(bc_read_struct_ptr(b,at,"ModifierData","next",&nxt)!=BC_OK)
      return BC_FAILURE;
    node->mods++;
    cur=nxt;
  }
  return BC_OK;
}
static int bc_obj_scan(BC_File *b,unsigned int *total)
{
  const BC_Struct *st=bc_get_struct(b,"Object");
  unsigned int i,j,stride,base;
  const BC_Block *p;
  BC_ObjNode *n;
  if(!st||!st->valid)return BC_UNSUPPORTED;
  stride=b->type_len[st->type];if(!stride)return BC_UNSUPPORTED;
  *total=0U;
  for(i=0U;i<b->nblocks;i++){
    p=&b->blocks[i];
    if(p->code[0]!='O'||p->code[1]!='B'||
       p->sdna>=b->nstructs ||b->structs[p->sdna].type!=st->type)continue;
    if(p->count>p->size/stride ||p->count>BC_OBJ_CAP-*total)return BC_FAILURE;
    for(j=0U;j<p->count;j++){
      BC_U64 old=p->old;
      base=p->offset+j*stride;
      /* Old serialized address + element displacement, two 32-bit limbs. */
      if(old.lo>UINT_MAX-j*stride)old.hi++;
      old.lo+=j*stride;
      n=&bc_objects[*total];memset(n,0,sizeof(*n));
      n->at=base;n->old=old;
      if(bc_obj_name(b,base,n->name)!=BC_OK ||
         bc_read_struct_ptr(b,base,"Object","data",&n->data)!=BC_OK ||
         bc_read_struct_ptr(b,base,"Object","parent",&n->parent)!=BC_OK)
        return BC_FAILURE;
      n->transform_ok=(bc_obj_local(b,base,&n->local)==BC_OK);
      (*total)++;
    }
  }
  return *total?BC_OK:BC_UNSUPPORTED;
}
static int bc_obj_data_kind(BC_File *b,BC_U64 ptr,char dst[3])
{
  const BC_Block *blk;
  unsigned int at,unused;
  if(bc_obj_zero(ptr)){dst[0]='-';dst[1]=0;return BC_OK;}
  if(bc_resolve(b,ptr,1U,&blk,&at,&unused)!=BC_OK)return BC_FAILURE;
  dst[0]=blk->code[0];dst[1]=blk->code[1];dst[2]=0;
  return BC_OK;
}
int bc_export_object_links(BC_File *b,FILE *out)
{
  unsigned int total,i,k;
  int result,pidx,aidx;
  char kind[3];
  BC_ObjNode *obj;
  result=bc_obj_scan(b,&total);
  if(result!=BC_OK)return result;
  if(fputs("# id\tname\tdata_code\tparent_id\tarmature_id\tmodifiers\tarmature_modifiers\tother_modifiers\tunresolved\tlocal_trs_valid\tlocal_matrix_row_major_Q16\n",out)==EOF)return BC_FAILURE;
  for(i=0U;i<total;i++){
    obj=&bc_objects[i];
    if(bc_obj_modifiers(b,obj)!=BC_OK)return BC_FAILURE;
    pidx=bc_obj_index(obj->parent,total);
    aidx=bc_obj_index(obj->armature,total);
    if(!bc_obj_zero(obj->parent) && pidx<0)obj->unresolved++;
    if(!bc_obj_zero(obj->armature)){
      if(aidx<0)obj->unresolved++;
      else{
        char target[3];
        if(bc_obj_data_kind(b,bc_objects[aidx].data,target)!=BC_OK ||
           strcmp(target,"AR")!=0)obj->unresolved++;
      }
    }
    if(bc_obj_data_kind(b,obj->data,kind)!=BC_OK)return BC_FAILURE;
    if(fprintf(out,"%u\t%s\t%s\t%d\t%d\t%u\t%u\t%u\t%u\t%d",i,
        obj->name,kind,pidx,aidx,obj->mods,obj->arms,obj->other,
        obj->unresolved,obj->transform_ok)<0)return BC_FAILURE;
    for(k=0U;k<16U;k++){
      if(fputc('\t',out)==EOF)return BC_FAILURE;
      if(obj->transform_ok)bc_print_q16(out,obj->local.m[k]);
      else if(fputs("NA",out)==EOF)return BC_FAILURE;
    }
    if(fputc('\n',out)==EOF)return BC_FAILURE;
  }
  return ferror(out)?BC_FAILURE:BC_OK;
}

/* Exactly one mesh + one rig with an ordinary Armature modifier.
   No parent constraints or other modifiers: the offline evaluator cannot
   represent those without Blender's dependency graph. */
static int bc_obj_has_constraints(BC_File *b,const BC_ObjNode *obj)
{
  const BC_Field *first=bc_get_field(b,"ListBase","first");
  unsigned int at;
  BC_U64 ptr;
  if(!bc_get_field(b,"Object","constraints"))return 0;
  if(!first ||bc_obj_offset(b,obj->at,"Object","constraints",&at)!=BC_OK ||
     at>UINT_MAX-first->offset ||
     bc_read_pointer(b,at+first->offset,&ptr)!=BC_OK)return 1;
  return !bc_obj_zero(ptr);
}
int bc_object_playback_spaces(BC_File *b,BC_RigMat *mesh,BC_RigMat *arm)
{
  unsigned int total,i,me=0U,ar=0U;
  int bound=-1,rig=-1,r;
  char kind[3];
  r=bc_obj_scan(b,&total);
  if(r==BC_UNSUPPORTED)return BC_UNSUPPORTED;
  if(r!=BC_OK)return BC_FAILURE;
  for(i=0U;i<total;i++){
    BC_ObjNode *o=&bc_objects[i];
    if(bc_obj_modifiers(b,o)!=BC_OK)return BC_FAILURE;
    if(o->arms){
      if(bound>=0 ||o->arms!=1U ||o->other ||o->unsupported_deform ||
         !bc_obj_zero(o->parent) ||!o->transform_ok ||o->unresolved)
        return BC_FAILURE;
      if(bc_obj_data_kind(b,o->data,kind)!=BC_OK ||strcmp(kind,"ME")!=0)
        return BC_FAILURE;
      bound=(int)i;
      rig=bc_obj_index(o->armature,total);
      if(rig<0)return BC_FAILURE;
    }
  }
  if(bound<0)return BC_FAILURE;
  if(bc_obj_has_constraints(b,&bc_objects[bound]) ||
     !bc_obj_zero(bc_objects[rig].parent) ||
     !bc_objects[rig].transform_ok ||
     bc_obj_has_constraints(b,&bc_objects[rig]) ||
     bc_obj_data_kind(b,bc_objects[rig].data,kind)!=BC_OK ||
     strcmp(kind,"AR")!=0)return BC_FAILURE;
  for(i=0U;i<b->nblocks;i++){
    if(b->blocks[i].code[0]=='M'&&b->blocks[i].code[1]=='E')me+=b->blocks[i].count;
    if(b->blocks[i].code[0]=='A'&&b->blocks[i].code[1]=='R')ar+=b->blocks[i].count;
  }
  if(me!=1U ||ar!=1U)return BC_FAILURE;
  *mesh=bc_objects[bound].local;
  *arm=bc_objects[rig].local;
  return BC_OK;
}

/* Phase13: explicit ID-block identity mapping, NOT matching bone/object names. */
static int bc_obj_data_serial(BC_File *b,BC_U64 old,const char *code,
                              const char *typename,unsigned int *index)
{
  const BC_Struct *st=bc_get_struct(b,typename);
  unsigned int stride,i,j,idx=0U;
  if(!st ||!st->valid)return BC_UNSUPPORTED;
  stride=b->type_len[st->type];if(!stride)return BC_UNSUPPORTED;
  for(i=0U;i<b->nblocks;i++){
    const BC_Block *blk=&b->blocks[i];
    if(blk->code[0]!=code[0] ||blk->code[1]!=code[1] ||
       blk->sdna>=b->nstructs ||b->structs[blk->sdna].type!=st->type)continue;
    if(blk->count>blk->size/stride ||blk->count>BC_OBJ_CAP-idx)return BC_FAILURE;
    for(j=0U;j<blk->count;j++){
      BC_U64 ptr=blk->old;unsigned int step=j*stride,carry=ptr.lo+step<ptr.lo;
      if(carry &&ptr.hi==UINT_MAX)return BC_FAILURE;
      ptr.lo+=step;ptr.hi+=carry;
      if(bc_same64(ptr,old)){*index=idx;return BC_OK;}
      idx++;
    }
  }
  return BC_UNSUPPORTED;
}
int bc_object_rig_pairs(BC_File *b,BC_RigPair *pairs,unsigned int capacity,unsigned int *count)
{
  unsigned int total,i,j,mesh_id,arm_id;
  int rig,r;
  char kind[3];
  *count=0U;
  r=bc_obj_scan(b,&total);if(r!=BC_OK)return r;
  for(i=0U;i<total;i++){
    BC_ObjNode *mesh=&bc_objects[i],*arm;
    if(bc_obj_modifiers(b,mesh)!=BC_OK)return BC_FAILURE;
    if(!mesh->arms)continue;
    if(mesh->arms!=1U ||mesh->other ||mesh->unsupported_deform ||mesh->unresolved ||
       !mesh->transform_ok ||!bc_obj_zero(mesh->parent) ||
       bc_obj_has_constraints(b,mesh))return BC_UNSUPPORTED;
    if(bc_obj_data_kind(b,mesh->data,kind)!=BC_OK ||strcmp(kind,"ME"))return BC_UNSUPPORTED;
    rig=bc_obj_index(mesh->armature,total);if(rig<0)return BC_FAILURE;
    arm=&bc_objects[rig];
    if(!arm->transform_ok ||!bc_obj_zero(arm->parent) ||
       bc_obj_has_constraints(b,arm) ||
       bc_obj_data_kind(b,arm->data,kind)!=BC_OK ||strcmp(kind,"AR"))return BC_UNSUPPORTED;
    if(bc_obj_data_serial(b,mesh->data,"ME","Mesh",&mesh_id)!=BC_OK ||
       bc_obj_data_serial(b,arm->data,"AR","bArmature",&arm_id)!=BC_OK)return BC_UNSUPPORTED;
    if(*count>=capacity)return BC_UNSUPPORTED;
    for(j=0U;j<*count;j++)if(pairs[j].mesh_index==mesh_id)return BC_UNSUPPORTED;
    pairs[*count].mesh_index=mesh_id;pairs[*count].arm_index=arm_id;
    pairs[*count].arm_object_at=arm->at;
    pairs[*count].arm_object_index=(unsigned int)rig;
    pairs[*count].mesh_local=mesh->local;
    pairs[*count].arm_local=arm->local;
    ++(*count);
  }
  return *count?BC_OK:BC_UNSUPPORTED;
}

/* Auditable per-mesh mappings for independently evaluated rigs. */
int bc_export_rig_pairs(BC_File *b,FILE *out)
{
  static BC_RigPair pairs[256];
  unsigned int count,i,k;
  if(bc_object_rig_pairs(b,pairs,256U,&count)!=BC_OK)return BC_UNSUPPORTED;
  if(fputs("# mesh_id\tarmature_data_id\tarmature_object_id\tmesh_matrix_Q16\tarmature_matrix_Q16\n",out)==EOF)
    return BC_FAILURE;
  for(i=0U;i<count;i++){
    if(fprintf(out,"%u\t%u\t%u",pairs[i].mesh_index,pairs[i].arm_index,
               pairs[i].arm_object_index)<0)return BC_FAILURE;
    for(k=0U;k<16U;k++){
      if(fputc('\t',out)==EOF)return BC_FAILURE;
      bc_print_q16(out,pairs[i].mesh_local.m[k]);
    }
    for(k=0U;k<16U;k++){
      if(fputc('\t',out)==EOF)return BC_FAILURE;
      bc_print_q16(out,pairs[i].arm_local.m[k]);
    }
    if(fputc('\n',out)==EOF)return BC_FAILURE;
  }
  return ferror(out)?BC_FAILURE:BC_OK;
}
