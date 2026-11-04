/* Phase14: deliberately bounded pose-space Copy constraints.
   Only a same-Armature bPoseChannel target, full influence, no offsets,
   no mixed coordinate spaces. Serialized SDNA is used for all offsets.
   This is NOT Blender's full dependency graph / constraint solver.
   GPL Blender source consulted for layout; this code is independently written. */
#include "bc.h"
#include "bc_rig_q16.h"
#include "bc_ik2.h"
#include <string.h>
#include <limits.h>

#define C14_BONES 8192U
#define C14_LINKS 32U
#define C14_IK 3U
#define C14_COPY_ROT 8U
#define C14_COPY_LOC 9U
#define C14_COPY_TRANS 23U
#define C14_FLAG_OFF (1U<<9)
#define C14_FLAG_DISABLED (1U<<2)

typedef struct C14_Op { int target; unsigned int kind; int goal[3],pole[3]; } C14_Op;
static int c16_has_ik=0;
static BC_RigMat c16_arm_inverse;
static C14_Op c14_ops[C14_BONES];

static int c14_zero(BC_U64 p){return p.lo==0U && p.hi==0U;}
static int c14_at(BC_File *b,unsigned int base,const char *type,
                  const char *field,unsigned int len,unsigned int *out)
{
  const BC_Field *f=bc_get_field(b,type,field);
  const BC_Struct *s=bc_get_struct(b,type);
  unsigned int n;
  if(!f||!s||!s->valid)return BC_UNSUPPORTED;
  n=b->type_len[s->type];
  if(f->offset>n ||len>n-f->offset ||base>UINT_MAX-f->offset)return BC_FAILURE;
  *out=base+f->offset;
  return BC_OK;
}
static int c14_short(BC_File *b,unsigned int base,const char *t,
                     const char *fld,unsigned int *v)
{
  unsigned int at;unsigned char p[2];
  if(c14_at(b,base,t,fld,2U,&at)!=BC_OK ||bc_at(b,at,2U,p)!=BC_OK)
    return BC_FAILURE;
  *v=bc_u16(p,b->endian_le);return BC_OK;
}
static int c14_char(BC_File *b,unsigned int base,const char *t,
                    const char *fld,unsigned int *v)
{
  unsigned int at;unsigned char p;
  if(c14_at(b,base,t,fld,1U,&at)!=BC_OK ||bc_at(b,at,1U,&p)!=BC_OK)
    return BC_FAILURE;
  *v=p;return BC_OK;
}
static int c14_qfield(BC_File *b,unsigned int base,const char *t,
                      const char *fld,int *q)
{
  unsigned int at,bits;
  if(c14_at(b,base,t,fld,4U,&at)!=BC_OK ||
     bc_read_word(b,at,&bits)!=BC_OK ||bc_f32_q16(bits,q)!=BC_OK)return BC_FAILURE;
  return BC_OK;
}
static int c14_string(BC_File *b,unsigned int base,const char *t,
                      const char *fld,char out[65])
{
  unsigned int at,i;
  if(c14_at(b,base,t,fld,64U,&at)!=BC_OK ||
     bc_at(b,at,64U,(unsigned char *)out)!=BC_OK)return BC_FAILURE;
  out[64]=0;
  for(i=0U;i<64U &&out[i];i++)if(out[i]=='\r'||out[i]=='\n'||out[i]=='\t')return BC_FAILURE;
  return i==64U?BC_FAILURE:BC_OK;
}
static int c14_list_first(BC_File *b,unsigned int base,const char *t,
                          const char *fld,BC_U64 *p)
{
  unsigned int list,off;
  if(c14_at(b,base,t,fld,(unsigned int)b->ptr_size,&list)!=BC_OK ||
     c14_at(b,list,"ListBase","first",(unsigned int)b->ptr_size,&off)!=BC_OK ||
     bc_read_pointer(b,off,p)!=BC_OK)return BC_FAILURE;
  return BC_OK;
}
static int c14_resolve_typed(BC_File *b,BC_U64 ptr,const char *type,unsigned int *at)
{
  const BC_Struct *s=bc_get_struct(b,type);
  const BC_Block *block;unsigned int available;
  if(!s||!s->valid||bc_resolve(b,ptr,b->type_len[s->type],&block,at,&available)!=BC_OK ||
     block->sdna>=b->nstructs ||b->structs[block->sdna].type!=s->type)
    return BC_FAILURE;
  return BC_OK;
}
/* Strict phase16 IK target: unparented, static Object origin. No evaluated
   depsgraph, Object constraints, delta-locations or animated targets. */
static int c16_obj_location(BC_File *b,BC_U64 token,int out[3])
{
  unsigned int at,k,off,bits;BC_U64 ptr;const BC_Field *f;
  int world[3],t;
  if(c14_resolve_typed(b,token,"Object",&at)!=BC_OK)return BC_UNSUPPORTED;
  if(bc_read_struct_ptr(b,at,"Object","parent",&ptr)!=BC_OK ||!c14_zero(ptr))return BC_UNSUPPORTED;
  f=bc_get_field(b,"Object","adt");
  if(f &&(bc_read_struct_ptr(b,at,"Object","adt",&ptr)!=BC_OK ||!c14_zero(ptr)))return BC_UNSUPPORTED;
  f=bc_get_field(b,"Object","constraints");
  if(f &&(c14_list_first(b,at,"Object","constraints",&ptr)!=BC_OK ||!c14_zero(ptr)))return BC_UNSUPPORTED;
  f=bc_get_field(b,"Object","dloc");
  if(f){
    if(c14_at(b,at,"Object","dloc",12U,&off)!=BC_OK)return BC_UNSUPPORTED;
    for(k=0U;k<3U;k++){
      if(bc_read_word(b,off+4U*k,&bits)!=BC_OK ||bc_f32_q16(bits,&t)!=BC_OK ||t!=0)
        return BC_UNSUPPORTED;
    }
  }
  if(c14_at(b,at,"Object","loc",12U,&off)!=BC_OK)return BC_UNSUPPORTED;
  for(k=0U;k<3U;k++){
    if(bc_read_word(b,off+4U*k,&bits)!=BC_OK ||bc_f32_q16(bits,&world[k])!=BC_OK)
      return BC_UNSUPPORTED;
  }
  return bc_rig_mat_point(&c16_arm_inverse,world,out)==BC_RIG_OK?BC_OK:BC_UNSUPPORTED;
}
static int c16_ik(BC_File *b,unsigned int con_at,unsigned int owner)
{
  unsigned int kind,flags,root,maxroot,mode,off,at;
  unsigned int allow=1U|32U;
  int angle,weight,orient;
  BC_U64 data,tar,pole;
  char name[65],pname[65];
  if(bc_read_struct_ptr(b,con_at,"bConstraint","data",&data)!=BC_OK ||
     c14_resolve_typed(b,data,"bKinematicConstraint",&at)!=BC_OK ||
     c14_short(b,at,"bKinematicConstraint","rootbone",&root)!=BC_OK ||root!=2U ||
     c14_short(b,at,"bKinematicConstraint","max_rootbone",&maxroot)!=BC_OK ||maxroot!=0U ||
     c14_short(b,at,"bKinematicConstraint","type",&kind)!=BC_OK ||kind!=0U ||
     c14_short(b,at,"bKinematicConstraint","mode",&mode)!=BC_OK ||mode!=0U ||
     c14_short(b,at,"bKinematicConstraint","flag",&flags)!=BC_OK ||flags!=allow ||
     c14_qfield(b,at,"bKinematicConstraint","poleangle",&angle)!=BC_OK ||angle!=0 ||
     c14_qfield(b,at,"bKinematicConstraint","weight",&weight)!=BC_OK ||weight!=BC_RIG_ONE ||
     c14_qfield(b,at,"bKinematicConstraint","orientweight",&orient)!=BC_OK ||orient!=0 ||
     c14_string(b,at,"bKinematicConstraint","subtarget",name)!=BC_OK ||name[0] ||
     c14_string(b,at,"bKinematicConstraint","polesubtarget",pname)!=BC_OK ||pname[0] ||
     bc_read_struct_ptr(b,at,"bKinematicConstraint","tar",&tar)!=BC_OK ||c14_zero(tar) ||
     bc_read_struct_ptr(b,at,"bKinematicConstraint","poletar",&pole)!=BC_OK ||c14_zero(pole))
    return BC_UNSUPPORTED;
  /* No target-less, stretch, pole-angle, orientation, auto-IK or distance IK. */
  if(bc_get_field(b,"bKinematicConstraint","grabtarget")){
    if(c14_at(b,at,"bKinematicConstraint","grabtarget",12U,&off)!=BC_OK)return BC_UNSUPPORTED;
    for(root=0U;root<3U;root++){
      unsigned int bits;
      if(bc_read_word(b,off+4U*root,&bits)!=BC_OK ||bc_f32_q16(bits,&angle)!=BC_OK ||angle)
        return BC_UNSUPPORTED;
    }
  }
  if(c14_ops[owner].kind)return BC_UNSUPPORTED;
  if(c16_obj_location(b,tar,c14_ops[owner].goal)!=BC_OK ||
     c16_obj_location(b,pole,c14_ops[owner].pole)!=BC_OK)return BC_UNSUPPORTED;
  c14_ops[owner].kind=C14_IK;c14_ops[owner].target=-1;c16_has_ik=1;
  return BC_OK;
}
int bc_constraints_has_ik(void){return c16_has_ik;}
int bc_constraints_ik_goal(unsigned int owner,BC_IK2Goal *goal)
{
  unsigned int k;
  if(owner>=C14_BONES ||!goal)return -1;
  if(c14_ops[owner].kind!=C14_IK)return 0;
  for(k=0U;k<3U;k++){
    goal->target[k]=c14_ops[owner].goal[k];
    goal->pole[k]=c14_ops[owner].pole[k];
  }
  return 1;
}
static int c14_one(BC_File *b,unsigned int con_at,unsigned int owner,
                   unsigned int arm_object_at,const char *const *names,
                   unsigned int bones)
{
  unsigned int type,flag,own,tar,at,flags=0U,mix=0U,i,dat_at,tar_at;
  int influence,headtail;
  BC_U64 data,tarptr;
  char target_name[65];const char *dt;
  if(c14_short(b,con_at,"bConstraint","type",&type)!=BC_OK ||
     c14_short(b,con_at,"bConstraint","flag",&flag)!=BC_OK)return BC_FAILURE;
  if(flag&(C14_FLAG_OFF|C14_FLAG_DISABLED))return BC_OK;
  if(c14_qfield(b,con_at,"bConstraint","enforce",&influence)!=BC_OK)
    return BC_FAILURE;
  if(influence==0)return BC_OK;
  if(influence!=BC_RIG_ONE || (flag & ~(C14_FLAG_OFF|C14_FLAG_DISABLED|16U)))
    return BC_UNSUPPORTED;
  if(c14_qfield(b,con_at,"bConstraint","headtail",&headtail)!=BC_OK ||
     headtail!=0 ||c14_char(b,con_at,"bConstraint","ownspace",&own)!=BC_OK ||
     c14_char(b,con_at,"bConstraint","tarspace",&tar)!=BC_OK ||
     own!=2U ||tar!=2U)return BC_UNSUPPORTED;
  if(bc_get_field(b,"bConstraint","space_object")){
    BC_U64 sp;
    if(bc_read_struct_ptr(b,con_at,"bConstraint","space_object",&sp)!=BC_OK ||
       !c14_zero(sp))return BC_UNSUPPORTED;
  }
  if(type==C14_IK)return c16_ik(b,con_at,owner);
  if(type==C14_COPY_LOC)dt="bLocateLikeConstraint";
  else if(type==C14_COPY_ROT)dt="bRotateLikeConstraint";
  else if(type==C14_COPY_TRANS)dt="bTransLikeConstraint";
  else return BC_UNSUPPORTED;
  if(bc_read_struct_ptr(b,con_at,"bConstraint","data",&data)!=BC_OK ||
     c14_resolve_typed(b,data,dt,&dat_at)!=BC_OK ||
     bc_read_struct_ptr(b,dat_at,dt,"tar",&tarptr)!=BC_OK ||
     c14_resolve_typed(b,tarptr,"Object",&tar_at)!=BC_OK ||
     tar_at!=arm_object_at)return BC_UNSUPPORTED;
  if(c14_string(b,dat_at,dt,"subtarget",target_name)!=BC_OK)return BC_FAILURE;
  for(i=0U;i<bones;i++)if(strcmp(names[i],target_name)==0)break;
  /* Target must already have been evaluated; reject forward dependencies
     and cycles, rather than silently sampling stale pose matrices. */
  if(i>=owner)return BC_UNSUPPORTED;
  if(type==C14_COPY_LOC){
    if(bc_read_struct_word(b,dat_at,dt,"flag",&flags)!=BC_OK ||flags!=7U)
      return BC_UNSUPPORTED;
  }else if(type==C14_COPY_ROT){
    if(bc_read_struct_word(b,dat_at,dt,"flag",&flags)!=BC_OK ||flags!=7U)
      return BC_UNSUPPORTED;
    if(bc_get_field(b,dt,"mix_mode") &&
       (c14_char(b,dat_at,dt,"mix_mode",&mix)!=BC_OK ||mix!=0U))
      return BC_UNSUPPORTED;
    if(bc_get_field(b,dt,"euler_order") &&
       (c14_char(b,dat_at,dt,"euler_order",&mix)!=BC_OK ||mix!=0U))
      return BC_UNSUPPORTED;
  }else{
    if(bc_get_field(b,dt,"flag") &&
       (bc_read_struct_word(b,dat_at,dt,"flag",&flags)!=BC_OK ||flags!=0U))
      return BC_UNSUPPORTED;
    if(bc_get_field(b,dt,"mix_mode") &&
       (c14_char(b,dat_at,dt,"mix_mode",&mix)!=BC_OK ||mix!=0U))
      return BC_UNSUPPORTED;
  }
  /* One effective copy constraint per bone for this conservative phase. */
  if(c14_ops[owner].kind!=0U)return BC_UNSUPPORTED;
  c14_ops[owner].kind=type;c14_ops[owner].target=(int)i;
  (void)at;
  return BC_OK;
}
int bc_constraints_prepare(BC_File *b,unsigned int arm_object_at,
                           const char *const *names,unsigned int bones,
                           const BC_RigMat *arm_local)
{
  BC_U64 pose,chan,con,next;unsigned int p_at,ch_at,co_at,owner,steps,nc,avail;
  char name[65];int rc;
  if(!arm_local ||bones>C14_BONES ||!bc_get_field(b,"Object","pose") ||
     bc_rig_inverse_affine(arm_local,&c16_arm_inverse)!=BC_RIG_OK)return BC_UNSUPPORTED;
  c16_has_ik=0;
  for(owner=0U;owner<bones;owner++){c14_ops[owner].kind=0U;c14_ops[owner].target=-1;}
  if(bc_read_struct_ptr(b,arm_object_at,"Object","pose",&pose)!=BC_OK)
    return BC_FAILURE;
  if(c14_zero(pose))return BC_OK;
  if(c14_resolve_typed(b,pose,"bPose",&p_at)!=BC_OK ||
     c14_list_first(b,p_at,"bPose","chanbase",&chan)!=BC_OK)
    return BC_FAILURE;
  for(steps=0U;!c14_zero(chan);steps++){
    if(steps>=C14_BONES ||c14_resolve_typed(b,chan,"bPoseChannel",&ch_at)!=BC_OK ||
       c14_string(b,ch_at,"bPoseChannel","name",name)!=BC_OK)return BC_FAILURE;
    if(c14_list_first(b,ch_at,"bPoseChannel","constraints",&con)!=BC_OK)
      return BC_FAILURE;
    for(owner=0U;owner<bones;owner++)if(strcmp(names[owner],name)==0)break;
    for(nc=0U;!c14_zero(con);nc++){
      if(nc>=C14_LINKS ||c14_resolve_typed(b,con,"bConstraint",&co_at)!=BC_OK)
        return BC_FAILURE;
      if(owner==bones)return BC_UNSUPPORTED;
      rc=c14_one(b,co_at,owner,arm_object_at,names,bones);
      if(rc!=BC_OK)return rc;
      if(bc_read_struct_ptr(b,co_at,"bConstraint","next",&next)!=BC_OK)
        return BC_FAILURE;
      con=next;
    }
    if(bc_read_struct_ptr(b,ch_at,"bPoseChannel","next",&next)!=BC_OK)
      return BC_FAILURE;
    chan=next;
  }
  (void)avail;
  return BC_OK;
}
int bc_constraints_target(unsigned int bone)
{
  return bone<C14_BONES &&c14_ops[bone].kind!=0U?c14_ops[bone].target:-1;
}
/* The target's 3x3 matrix must be orthonormal for the narrow Copy Rotation
   implementation; mixed scaling/shear requires Blender's decomposition. */
static int c14_rigid(const BC_RigMat *m)
{
  unsigned int i,j,k;int sum,term;
  for(i=0U;i<3U;i++)for(j=i;j<3U;j++){
    sum=0;
    for(k=0U;k<3U;k++){
      if(bc_qmul_checked(m->m[k*4U+i],m->m[k*4U+j],&term)!=BC_RIG_OK ||
         bc_qadd_checked(sum,term,&sum)!=BC_RIG_OK)return 0;
    }
    if((i==j && (sum<BC_RIG_ONE-1024 ||sum>BC_RIG_ONE+1024)) ||
       (i!=j &&(sum< -1024 ||sum>1024)))return 0;
  }
  return 1;
}
int bc_constraints_apply(unsigned int owner,const BC_RigMat *target,BC_RigMat *pose)
{
  unsigned int r,c,kind;
  if(owner>=C14_BONES ||!pose)return BC_FAILURE;
  kind=c14_ops[owner].kind;
  if(!kind ||kind==C14_IK)return BC_OK;
  if(!target)return BC_FAILURE;
  if(kind==C14_COPY_TRANS){*pose=*target;return BC_OK;}
  if(kind==C14_COPY_LOC){
    pose->m[3]=target->m[3];pose->m[7]=target->m[7];pose->m[11]=target->m[11];
    return BC_OK;
  }
  if(kind==C14_COPY_ROT){
    if(!c14_rigid(target)||!c14_rigid(pose))return BC_UNSUPPORTED;
    for(r=0U;r<3U;r++)for(c=0U;c<3U;c++)pose->m[r*4U+c]=target->m[r*4U+c];
    return BC_OK;
  }
  return BC_UNSUPPORTED;
}
/* Fail closed in older/global playback commands which do not associate
   individual Object owners with their armatures. */
int bc_constraints_guard_global(BC_File *b)
{
  const BC_Struct *s=bc_get_struct(b,"bConstraint");
  unsigned int i,j,stride,at,flag;int influence;
  if(!s)return BC_OK;
  if(!s->valid)return BC_UNSUPPORTED;
  stride=b->type_len[s->type];if(!stride)return BC_UNSUPPORTED;
  for(i=0U;i<b->nblocks;i++){
    const BC_Block *blk=&b->blocks[i];
    if(blk->sdna>=b->nstructs ||b->structs[blk->sdna].type!=s->type)continue;
    if(blk->count>blk->size/stride)return BC_FAILURE;
    for(j=0U;j<blk->count;j++){
      at=blk->offset+j*stride;
      if(c14_short(b,at,"bConstraint","flag",&flag)!=BC_OK ||
         c14_qfield(b,at,"bConstraint","enforce",&influence)!=BC_OK)return BC_FAILURE;
      if(!(flag&(C14_FLAG_OFF|C14_FLAG_DISABLED)) && influence!=0)
        return BC_UNSUPPORTED;
    }
  }
  return BC_OK;
}
