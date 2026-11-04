/* Phase 11: serialized ID/Object -> AnimData -> bAction and NLA graph audit.
   A report is NOT NLA evaluation. All references are file offsets resolved via SDNA.
   Bounded and heapless in project code, including 64-bit file pointers as limbs. */
#include "bc.h"
#include "bc_rig_q16.h"
#include <string.h>
#include <limits.h>

#define BC_BIND_MAX_OBJS 8192U
#define BC_BIND_MAX_TRACKS 512U
#define BC_BIND_MAX_STRIPS 2048U

static int bind_zero(BC_U64 p){return p.lo==0U && p.hi==0U;}
static BC_U64 bind_add(BC_U64 p,unsigned int add)
{
  unsigned int old=p.lo;
  p.lo+=add;
  if(p.lo<old)p.hi++;
  return p;
}
static int bind_field_at(BC_File *b,unsigned int base,const char *type,
                         const char *field,unsigned int *out)
{
  const BC_Field *f=bc_get_field(b,type,field);
  if(!f || f->offset>UINT_MAX-base)return BC_UNSUPPORTED;
  *out=base+f->offset;
  return BC_OK;
}
static int bind_word(BC_File *b,unsigned int base,const char *type,
                     const char *field,unsigned int *out)
{
  return bc_read_struct_word(b,base,type,field,out);
}
static int bind_list_first(BC_File *b,unsigned int base,const char *type,
                           const char *list_field,BC_U64 *out)
{
  const BC_Field *f=bc_get_field(b,"ListBase","first");
  unsigned int at;
  if(!f ||bind_field_at(b,base,type,list_field,&at)!=BC_OK ||
     at>UINT_MAX-f->offset)return BC_UNSUPPORTED;
  return bc_read_pointer(b,at+f->offset,out);
}
static int bind_read_string(BC_File *b,unsigned int base,const char *type,
                            const char *field,char *dst,unsigned int cap)
{
  const BC_Field *f=bc_get_field(b,type,field);
  unsigned int i,at;
  if(!f ||f->offset>UINT_MAX-base ||cap<2U)return BC_UNSUPPORTED;
  at=base+f->offset;
  if(bc_at(b,at,cap-1U,(unsigned char*)dst)!=BC_OK)return BC_FAILURE;
  dst[cap-1U]=0;
  for(i=0U;i<cap-1U && dst[i];i++){
    if(dst[i]=='\r'||dst[i]=='\t'||dst[i]=='\n')dst[i]='_';
  }
  if(i==cap-1U)return BC_FAILURE;
  return BC_OK;
}
static int bind_obj_name(BC_File *b,unsigned int base,char *dst)
{
  unsigned int at;
  if(bind_field_at(b,base,"Object","id",&at)!=BC_OK ||
     bind_field_at(b,at,"ID","name",&at)!=BC_OK)return BC_UNSUPPORTED;
  if(bc_at(b,at,66U,(unsigned char*)dst)!=BC_OK)return BC_FAILURE;
  dst[65]=0;
  if(dst[0]=='O' &&dst[1]=='B')memmove(dst,dst+2U,strlen(dst+2U)+1U);
  return BC_OK;
}
static int bind_action_id(BC_File *b,BC_U64 pointer,unsigned int *id)
{
  const BC_Struct *s=bc_get_struct(b,"bAction");
  unsigned int i,j,idx=0U,stride;
  if(bind_zero(pointer))return BC_UNSUPPORTED;
  if(!s ||!s->valid)return BC_UNSUPPORTED;
  stride=b->type_len[s->type];
  if(!stride)return BC_UNSUPPORTED;
  for(i=0U;i<b->nblocks;i++){
    const BC_Block *blk=&b->blocks[i];
    if(blk->code[0]!='A'||blk->code[1]!='C'||
       blk->sdna>=b->nstructs||b->structs[blk->sdna].type!=s->type)continue;
    if(blk->count>blk->size/stride ||blk->count>BC_BIND_MAX_OBJS-idx)return BC_FAILURE;
    for(j=0U;j<blk->count;j++){
      BC_U64 p=bind_add(blk->old,j*stride);
      if(bc_same64(p,pointer)){*id=idx;return BC_OK;}
      idx++;
    }
  }
  return BC_UNSUPPORTED;
}
static int bind_count_strips(BC_File *b,unsigned int anim_at,
                              unsigned int *tracks,unsigned int *strips,FILE *out,
                              unsigned int object_id,const char *object_name)
{
  BC_U64 track,strip,action,next;
  unsigned int t=0U,s=0U,at,avail,i,a_id,slot,off,bits;
  char name[65];
  const BC_Struct *trackst=bc_get_struct(b,"NlaTrack");
  const BC_Struct *stripst=bc_get_struct(b,"NlaStrip");
  if(bind_list_first(b,anim_at,"AnimData","nla_tracks",&track)!=BC_OK)
    return BC_UNSUPPORTED;
  if(!bind_zero(track) && (!trackst ||!stripst ||!trackst->valid||!stripst->valid))
    return BC_UNSUPPORTED;
  while(!bind_zero(track)){
    if(t>=BC_BIND_MAX_TRACKS)return BC_FAILURE;
    if(bc_resolve(b,track,b->type_len[trackst->type],NULL,&at,&avail)!=BC_OK)
      return BC_FAILURE;
    if(bind_list_first(b,at,"NlaTrack","strips",&strip)!=BC_OK)return BC_FAILURE;
    for(i=0U;!bind_zero(strip);i++){
      int qstart,qend;
      if(i>=BC_BIND_MAX_STRIPS ||s>=BC_BIND_MAX_STRIPS)return BC_FAILURE;
      if(bc_resolve(b,strip,b->type_len[stripst->type],NULL,&off,&avail)!=BC_OK)
        return BC_FAILURE;
      if(bc_read_struct_ptr(b,off,"NlaStrip","act",&action)!=BC_OK ||
         bc_read_struct_ptr(b,off,"NlaStrip","next",&next)!=BC_OK ||
         bind_read_string(b,off,"NlaStrip","name",name,sizeof(name))!=BC_OK)
        return BC_FAILURE;
      a_id=UINT_MAX;
      if(!bind_zero(action) &&bind_action_id(b,action,&a_id)!=BC_OK)return BC_FAILURE;
      slot=0U;
      if(bc_get_field(b,"NlaStrip","action_slot_handle") &&
         bind_word(b,off,"NlaStrip","action_slot_handle",&slot)!=BC_OK)return BC_FAILURE;
      if(out){
        if(bind_field_at(b,off,"NlaStrip","start",&at)!=BC_OK ||
           bc_read_word(b,at,&bits)!=BC_OK ||bc_f32_q16(bits,&qstart)!=BC_OK ||
           bind_field_at(b,off,"NlaStrip","end",&at)!=BC_OK ||
           bc_read_word(b,at,&bits)!=BC_OK ||bc_f32_q16(bits,&qend)!=BC_OK)return BC_UNSUPPORTED;
        if(fprintf(out,"%u\t%s\t%u\t%u\t%s\t",object_id,object_name,t,i,name)<0)
          return BC_FAILURE;
        if(a_id==UINT_MAX){if(fputs("-1",out)==EOF)return BC_FAILURE;}
        else if(fprintf(out,"%u",a_id)<0)return BC_FAILURE;
        if(fprintf(out,"\t%u\t",slot)<0)return BC_FAILURE;
        bc_print_q16(out,qstart);fputc('\t',out);bc_print_q16(out,qend);
        if(fputc('\n',out)==EOF)return BC_FAILURE;
      }
      s++;
      strip=next;
    }
    if(bc_resolve(b,track,b->type_len[trackst->type],NULL,&at,&avail)!=BC_OK ||
       bc_read_struct_ptr(b,at,"NlaTrack","next",&next)!=BC_OK)return BC_FAILURE;
    track=next;t++;
  }
  *tracks=t;*strips=s;
  return BC_OK;
}
/* One row per Object, including objects without AnimData. No implicit assumptions
   about the active Action belonging to an arbitrary armature. */
int bc_export_animation_bindings(BC_File *b,FILE *out,int nla_only)
{
  const BC_Struct *obj=bc_get_struct(b,"Object");
  const BC_Struct *anim=bc_get_struct(b,"AnimData");
  unsigned int i,j,base,available,oid=0U,stride,an_at,aid,slot,tracks,strips;
  BC_U64 adt,action;
  char name[66];
  if(!obj ||!obj->valid ||!anim ||!anim->valid)return BC_UNSUPPORTED;
  stride=b->type_len[obj->type];if(!stride)return BC_UNSUPPORTED;
  if(fputs(nla_only?"# object_id\tobject\ttrack\tstrip\tname\taction_id\tslot\tstart\tend\n":
           "# object_id\tobject\taction_id\tslot\tnla_tracks\tnla_strips\n",out)==EOF)return BC_FAILURE;
  for(i=0U;i<b->nblocks;i++){
    const BC_Block *blk=&b->blocks[i];
    if(blk->code[0]!='O'||blk->code[1]!='B'||blk->sdna>=b->nstructs ||
       b->structs[blk->sdna].type!=obj->type)continue;
    if(blk->count>blk->size/stride ||blk->count>BC_BIND_MAX_OBJS-oid)return BC_FAILURE;
    for(j=0U;j<blk->count;j++){
      base=blk->offset+j*stride;
      if(bind_obj_name(b,base,name)!=BC_OK ||
         bc_read_struct_ptr(b,base,"Object","adt",&adt)!=BC_OK)return BC_FAILURE;
      aid=UINT_MAX;slot=0U;tracks=0U;strips=0U;
      if(!bind_zero(adt)){
        if(bc_resolve(b,adt,b->type_len[anim->type],NULL,&an_at,&available)!=BC_OK ||
           bc_read_struct_ptr(b,an_at,"AnimData","action",&action)!=BC_OK)return BC_FAILURE;
        if(!bind_zero(action)&&bind_action_id(b,action,&aid)!=BC_OK)return BC_FAILURE;
        if(bc_get_field(b,"AnimData","slot_handle") &&
           bind_word(b,an_at,"AnimData","slot_handle",&slot)!=BC_OK)return BC_FAILURE;
        if(bind_count_strips(b,an_at,&tracks,&strips,nla_only?out:NULL,oid,name)!=BC_OK)
          return BC_FAILURE;
      }
      if(!nla_only){
        if(fprintf(out,"%u\t%s\t",oid,name)<0)return BC_FAILURE;
        if(aid==UINT_MAX){if(fputs("-1",out)==EOF)return BC_FAILURE;}
        else if(fprintf(out,"%u",aid)<0)return BC_FAILURE;
        if(fprintf(out,"\t%u\t%u\t%u\n",slot,tracks,strips)<0)return BC_FAILURE;
      }
      oid++;
    }
  }
  return oid &&!ferror(out)?BC_OK:BC_UNSUPPORTED;
}
/* Phase 12: conservative NLA time mapping, evaluated at one scene frame.
   Supports disjoint, ordinary clip strips on one or several tracks, but only
   ONE contributing strip at a time. Does NOT blend tracks or Actions, process
   FCurve modifiers, drivers, constraints, time-warp, or extrapolation.
   Uses saved SDNA rather than hardcoded struct offsets. */
#define BC_NLA_ACTIVE     1U
#define BC_NLA_SELECTED   2U
#define BC_NLA_SYNC      512U
#define BC_NLA_MUTED    4096U
#define BC_NLA_TRACK_MUTED 4U
#define BC_NLA_TRACK_DISABLED 1024U
#define BC_NLA_MAX_REPEAT 16U

static int bind_qfield(BC_File *b,unsigned int base,const char *field,int *value)
{
  unsigned int at,raw;
  if(bind_field_at(b,base,"NlaStrip",field,&at)!=BC_OK ||
     bc_read_word(b,at,&raw)!=BC_OK)return BC_UNSUPPORTED;
  return bc_f32_q16(raw,value);
}
static int bind_short(BC_File *b,unsigned int base,const char *field,unsigned int *value)
{
  unsigned int at;
  unsigned char raw[2];
  if(bind_field_at(b,base,"NlaStrip",field,&at)!=BC_OK ||
     bc_at(b,at,2U,raw)!=BC_OK)return BC_UNSUPPORTED;
  *value=bc_u16(raw,b->endian_le);
  return BC_OK;
}
static int bind_nla_clip_time(BC_File *b,unsigned int sa,int scene_frame,
                             int *action_frame,BC_U64 *action,unsigned int *slot)
{
  unsigned int flag,type,blend,repeat_count,iter,target_at,target_avail;
  BC_U64 other,act;
  const BC_Field *optional;
  int start,end,astart,aend,scale,repeat,influence;
  int duration,span,cycle,total=0,delta,phase,offset,result,blendtime;
  if(bc_read_struct_ptr(b,sa,"NlaStrip","act",&act)!=BC_OK ||bind_zero(act))
    return BC_UNSUPPORTED;
  if(bind_list_first(b,sa,"NlaStrip","strips",&other)!=BC_OK ||!bind_zero(other) ||
     bind_list_first(b,sa,"NlaStrip","fcurves",&other)!=BC_OK ||!bind_zero(other) ||
     bind_list_first(b,sa,"NlaStrip","modifiers",&other)!=BC_OK ||!bind_zero(other) ||
     bind_short(b,sa,"type",&type)!=BC_OK ||type!=0U ||
     bind_short(b,sa,"blendmode",&blend)!=BC_OK ||blend!=0U ||
     bind_word(b,sa,"NlaStrip","flag",&flag)!=BC_OK ||
     (flag & ~(BC_NLA_ACTIVE|BC_NLA_SELECTED|BC_NLA_SYNC))!=0U ||
     bind_qfield(b,sa,"start",&start)!=BC_OK ||
     bind_qfield(b,sa,"end",&end)!=BC_OK ||
     bind_qfield(b,sa,"actstart",&astart)!=BC_OK ||
     bind_qfield(b,sa,"actend",&aend)!=BC_OK ||
     bind_qfield(b,sa,"scale",&scale)!=BC_OK ||
     bind_qfield(b,sa,"repeat",&repeat)!=BC_OK ||
     bind_qfield(b,sa,"influence",&influence)!=BC_OK)return BC_UNSUPPORTED;
  /* Strip blending ramps change influence even without FCurve controls. */
  optional=bc_get_field(b,"NlaStrip","blendin");
  if(optional && (bind_qfield(b,sa,"blendin",&blendtime)!=BC_OK ||blendtime!=0))
    return BC_UNSUPPORTED;
  optional=bc_get_field(b,"NlaStrip","blendout");
  if(optional && (bind_qfield(b,sa,"blendout",&blendtime)!=BC_OK ||blendtime!=0))
    return BC_UNSUPPORTED;
  /* For the first implementation, repeat count must be integral. A fractional
     repeat has an endpoint that requires additional Blender edge-case rules. */
  if(end<=start ||aend<=astart ||scale<=0 ||influence!=BC_RIG_ONE ||
     repeat<BC_RIG_ONE ||(repeat & 65535)!=0)return BC_UNSUPPORTED;
  repeat_count=(unsigned int)(repeat/BC_RIG_ONE);
  if(repeat_count==0U ||repeat_count>BC_NLA_MAX_REPEAT ||
     astart==INT_MIN ||start==INT_MIN ||
     bc_qadd_checked(aend,-astart,&span)!=BC_RIG_OK ||
     bc_qadd_checked(end,-start,&duration)!=BC_RIG_OK ||
     bc_qmul_checked(span,scale,&cycle)!=BC_RIG_OK ||cycle<=0)return BC_UNSUPPORTED;
  for(iter=0U;iter<repeat_count;iter++)
    if(bc_qadd_checked(total,cycle,&total)!=BC_RIG_OK)return BC_UNSUPPORTED;
  /* Blender serializes strip start/end, repeat and scale separately. Reject
     contradictory settings rather than guessing the editor's versioning rules. */
  if(total!=duration)return BC_UNSUPPORTED;
  if(scene_frame<start ||scene_frame>end ||
     bc_qadd_checked(scene_frame,-start,&delta)!=BC_RIG_OK)return BC_UNSUPPORTED;
  if(delta==duration){result=aend;} /* End of last cycle, not first of next. */
  else {
    phase=delta%cycle;
    if(bc_qdiv_checked(phase,scale,&offset)!=BC_RIG_OK ||
       bc_qadd_checked(astart,offset,&result)!=BC_RIG_OK ||
       result<astart ||result>aend)return BC_UNSUPPORTED;
  }
  if(bc_resolve(b,act,1U,NULL,&target_at,&target_avail)!=BC_OK)
    return BC_UNSUPPORTED;
  *slot=0U;
  if(bc_get_field(b,"NlaStrip","action_slot_handle") &&
     bind_word(b,sa,"NlaStrip","action_slot_handle",slot)!=BC_OK)return BC_FAILURE;
  *action_frame=result;*action=act;
  return BC_OK;
}

/* Select a single contributing strip over any number of strips and tracks.
   Muted strips/tracks and disjoint intervals are ignored; overlapping active
   clips are rejected. Crucially, multiple tracks != license to mix channels. */
static int bind_nla_timed(BC_File *b,unsigned int anim_at,int *frame,
                          BC_U64 *action,unsigned int *slot)
{
  BC_U64 track,strip,next,act,candidate;
  unsigned int ta,sa,available,t=0U,s=0U,flag,sflag,adt_flag=0U,found=0U,chosen_slot=0U;
  int start,end,scene=*frame,selected_frame=0,result;
  const BC_Struct *trackst=bc_get_struct(b,"NlaTrack");
  const BC_Struct *stripst=bc_get_struct(b,"NlaStrip");
  /* Solo, NLA-disabled and tweak modes require the actual Blender evaluator.
     The outliner expand/collapse bits are strictly UI-only and are allowed. */
  if(bc_get_field(b,"AnimData","flag") &&
     (bind_word(b,anim_at,"AnimData","flag",&adt_flag)!=BC_OK ||
      (adt_flag & (1U|2U|4U|8U|32U))!=0U))return BC_UNSUPPORTED;
  if(!trackst ||!stripst ||!trackst->valid ||!stripst->valid ||
     bind_list_first(b,anim_at,"AnimData","nla_tracks",&track)!=BC_OK)
    return BC_UNSUPPORTED;
  while(!bind_zero(track)){
    if(t++>=BC_BIND_MAX_TRACKS ||
       bc_resolve(b,track,b->type_len[trackst->type],NULL,&ta,&available)!=BC_OK ||
       bind_word(b,ta,"NlaTrack","flag",&flag)!=BC_OK ||
       bc_read_struct_ptr(b,ta,"NlaTrack","next",&next)!=BC_OK ||
       bind_list_first(b,ta,"NlaTrack","strips",&strip)!=BC_OK)
      return BC_UNSUPPORTED;
    /* UI active/selected/protected flags do not affect evaluated values.
       Solo/tweak and other unknown track flags require NLA's full evaluator. */
    if((flag & ~(1U|2U|4U|16U|BC_NLA_TRACK_DISABLED|65536U))!=0U)
      return BC_UNSUPPORTED;
    if((flag & (BC_NLA_TRACK_MUTED|BC_NLA_TRACK_DISABLED))!=0U){track=next;continue;}
    while(!bind_zero(strip)){
      if(s++>=BC_BIND_MAX_STRIPS ||
         bc_resolve(b,strip,b->type_len[stripst->type],NULL,&sa,&available)!=BC_OK ||
         bc_read_struct_ptr(b,sa,"NlaStrip","next",&candidate)!=BC_OK ||
         bind_word(b,sa,"NlaStrip","flag",&sflag)!=BC_OK ||
         bind_qfield(b,sa,"start",&start)!=BC_OK ||
         bind_qfield(b,sa,"end",&end)!=BC_OK ||end<=start)
        return BC_UNSUPPORTED;
      if(scene>=start &&scene<=end &&(sflag&BC_NLA_MUTED)==0U){
        if(found)return BC_UNSUPPORTED;
        result=bind_nla_clip_time(b,sa,scene,&selected_frame,&act,&chosen_slot);
        if(result!=BC_OK)return result;
        found=1U;
        *action=act;*slot=chosen_slot;
      }
      strip=candidate;
    }
    track=next;
  }
  if(found!=1U)return BC_UNSUPPORTED;
  *frame=selected_frame;
  return BC_OK;
}
/* Read-only audit for scene files with several Armature objects. This does
   not imply multi-rig geometry export; the Phase11 OBJ player is still
   bounded to one rig and one contributing Action at a time. */
int bc_export_rig_timeline(BC_File *b,int scene_frame,FILE *out)
{
  const BC_Struct *objects=bc_get_struct(b,"Object"),*anim=bc_get_struct(b,"AnimData");
  unsigned int i,j,stride,at,db,avail,an_at,tracks,strips,id=0U;
  unsigned int aid,slot,layers;
  BC_U64 data,adt,action;
  const BC_Block *blk;
  char name[66];
  const char *source;
  int frame;
  if(!objects ||!anim ||!objects->valid ||!anim->valid)return BC_UNSUPPORTED;
  stride=b->type_len[objects->type];if(!stride)return BC_UNSUPPORTED;
  if(fputs("# rig_id\tarmature_object\tsource\taction_id\tslot\taction_frame\n",out)==EOF)return BC_FAILURE;
  for(i=0U;i<b->nblocks;i++){
    const BC_Block *p=&b->blocks[i];
    if(p->code[0]!='O'||p->code[1]!='B'||p->sdna>=b->nstructs ||
       b->structs[p->sdna].type!=objects->type)continue;
    if(p->count>p->size/stride)return BC_FAILURE;
    for(j=0U;j<p->count;j++){
      at=p->offset+j*stride;
      if(bc_read_struct_ptr(b,at,"Object","data",&data)!=BC_OK)return BC_FAILURE;
      if(bind_zero(data))continue;
      if(bc_resolve(b,data,1U,&blk,&db,&avail)!=BC_OK)return BC_FAILURE;
      if(blk->code[0]!='A'||blk->code[1]!='R')continue;
      if(bind_obj_name(b,at,name)!=BC_OK ||
         bc_read_struct_ptr(b,at,"Object","adt",&adt)!=BC_OK)return BC_FAILURE;
      source="none";aid=UINT_MAX;slot=0U;frame=scene_frame;
      if(!bind_zero(adt)){
        if(bc_resolve(b,adt,b->type_len[anim->type],NULL,&an_at,&avail)!=BC_OK ||
           bc_read_struct_ptr(b,an_at,"AnimData","action",&action)!=BC_OK ||
           bind_count_strips(b,an_at,&tracks,&strips,NULL,id,name)!=BC_OK)return BC_FAILURE;
        if(tracks){
          if(!bind_zero(action))source="unsupported_mixed";
          else if(bind_nla_timed(b,an_at,&frame,&action,&slot)==BC_OK)source="nla";
          else source="unsupported_nla";
        }else if(!bind_zero(action)){
          source="direct";
          if(bc_get_field(b,"AnimData","slot_handle") &&
             bind_word(b,an_at,"AnimData","slot_handle",&slot)!=BC_OK)return BC_FAILURE;
        }
        if(strcmp(source,"direct")==0 ||strcmp(source,"nla")==0){
          if(bind_action_id(b,action,&aid)!=BC_OK ||
             bc_resolve(b,action,1U,NULL,&db,&avail)!=BC_OK)return BC_FAILURE;
          layers=0U;
          if(bc_get_field(b,"bAction","layer_array_num") &&
             bind_word(b,db,"bAction","layer_array_num",&layers)!=BC_OK)return BC_FAILURE;
          if(layers &&!slot)source="unsupported_slot";
        }
      }
      if(fprintf(out,"%u\t%s\t%s\t",id,name,source)<0)return BC_FAILURE;
      if(aid==UINT_MAX){if(fputs("-1",out)==EOF)return BC_FAILURE;}
      else if(fprintf(out,"%u",aid)<0)return BC_FAILURE;
      if(fprintf(out,"\t%u\t",slot)<0)return BC_FAILURE;
      if(strncmp(source,"unsupported",11)==0 ||strcmp(source,"none")==0){
        if(fputs("-",out)==EOF)return BC_FAILURE;
      }else bc_print_q16(out,frame);
      if(fputc('\n',out)==EOF)return BC_FAILURE;
      id++;
    }
  }
  return id &&!ferror(out)?BC_OK:BC_UNSUPPORTED;
}

/* Auto-select an active bAction for exactly one armature Object, and reject
   NLA tracks to avoid silently rendering the wrong timeline. */
int bc_find_rig_active_action(BC_File *b,int *frame,unsigned int *aid,unsigned int *slot,int *filter_slot)
{
  const BC_Struct *obj=bc_get_struct(b,"Object"),*anim=bc_get_struct(b,"AnimData");
  unsigned int i,j,stride,at,avail,db,an_at,tracks,strips,found=0U,layers;
  BC_U64 adt,data,action;
  const BC_Block *blk;
  if(!obj ||!anim ||!obj->valid ||!anim->valid)return BC_UNSUPPORTED;
  stride=b->type_len[obj->type];if(!stride)return BC_UNSUPPORTED;
  for(i=0U;i<b->nblocks;i++){
    const BC_Block *p=&b->blocks[i];
    if(p->code[0]!='O'||p->code[1]!='B'||p->sdna>=b->nstructs ||
       b->structs[p->sdna].type!=obj->type)continue;
    if(p->count>p->size/stride)return BC_FAILURE;
    for(j=0U;j<p->count;j++){
      at=p->offset+j*stride;
      if(bc_read_struct_ptr(b,at,"Object","data",&data)!=BC_OK ||bind_zero(data))continue;
      if(bc_resolve(b,data,1U,&blk,&db,&avail)!=BC_OK)return BC_FAILURE;
      if(blk->code[0]!='A'||blk->code[1]!='R')continue;
      if(bc_read_struct_ptr(b,at,"Object","adt",&adt)!=BC_OK ||bind_zero(adt))continue;
      if(bc_resolve(b,adt,b->type_len[anim->type],NULL,&an_at,&avail)!=BC_OK ||
         bc_read_struct_ptr(b,an_at,"AnimData","action",&action)!=BC_OK)return BC_FAILURE;
      if(bind_count_strips(b,an_at,&tracks,&strips,NULL,0U,"")!=BC_OK)return BC_UNSUPPORTED;
      if(bind_zero(action) &&tracks==0U)continue;
      if(found++)return BC_UNSUPPORTED;
      *slot=0U;*filter_slot=0;
      if(tracks){
        /* An active Action combined with NLA requires Blender's full mixing. */
        if(!bind_zero(action) ||bind_nla_timed(b,an_at,frame,&action,slot)!=BC_OK)
          return BC_UNSUPPORTED;
      }else{
        if(bc_get_field(b,"AnimData","slot_handle") &&
           bind_word(b,an_at,"AnimData","slot_handle",slot)!=BC_OK)return BC_FAILURE;
      }
      if(bind_action_id(b,action,aid)!=BC_OK)return BC_UNSUPPORTED;
      if(bc_resolve(b,action,1U,&blk,&db,&avail)!=BC_OK)return BC_FAILURE;
      layers=0U;
      if(bc_get_field(b,"bAction","layer_array_num") &&
         bind_word(b,db,"bAction","layer_array_num",&layers)!=BC_OK)return BC_FAILURE;
      if(layers){if(!bc_get_field(b,"AnimData","slot_handle") ||!*slot)return BC_UNSUPPORTED;
                 *filter_slot=1;}
    }
  }
  return found==1U?BC_OK:BC_UNSUPPORTED;
}

/* The per-armature selector deliberately doesn't use a scene-wide first Action.
   No accepted Action is silently borrowed from a different rig. */
int bc_find_armature_action(BC_File *b,unsigned int object_at,int *frame,
                            unsigned int *aid,unsigned int *slot,int *filter_slot)
{
  const BC_Struct *anim=bc_get_struct(b,"AnimData");
  unsigned int an_at,avail,tracks,strips,db,layers;
  BC_U64 adt,action;
  if(!anim ||!anim->valid ||
     bc_read_struct_ptr(b,object_at,"Object","adt",&adt)!=BC_OK ||bind_zero(adt))
    return BC_UNSUPPORTED;
  if(bc_resolve(b,adt,b->type_len[anim->type],NULL,&an_at,&avail)!=BC_OK ||
     bc_read_struct_ptr(b,an_at,"AnimData","action",&action)!=BC_OK ||
     bind_count_strips(b,an_at,&tracks,&strips,NULL,0U,"")!=BC_OK)return BC_UNSUPPORTED;
  *slot=0U;*filter_slot=0;
  if(tracks){
    if(!bind_zero(action) ||bind_nla_timed(b,an_at,frame,&action,slot)!=BC_OK)
      return BC_UNSUPPORTED;
  }else{
    if(bind_zero(action))return BC_UNSUPPORTED;
    if(bc_get_field(b,"AnimData","slot_handle") &&
       bind_word(b,an_at,"AnimData","slot_handle",slot)!=BC_OK)return BC_FAILURE;
  }
  if(bind_action_id(b,action,aid)!=BC_OK ||
     bc_resolve(b,action,1U,NULL,&db,&avail)!=BC_OK)return BC_UNSUPPORTED;
  layers=0U;
  if(bc_get_field(b,"bAction","layer_array_num") &&
     bind_word(b,db,"bAction","layer_array_num",&layers)!=BC_OK)return BC_FAILURE;
  if(layers){if(!*slot)return BC_UNSUPPORTED;*filter_slot=1;}
  return BC_OK;
}
