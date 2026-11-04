/* HOD2 Phase12 native bridge. POL/PRS mesh, H2MC catalog and verified H2TB
 * tables (separate 1999/2000 pack, without original EXE). No heap or floats.
 * Phase12 native CLI supports skinned GLB export; in-runtime POL-to-MOT body
 * stitching is still not implemented. Tables do not contain model resources. */
#include "../modelrt_internal.h"
#include "../../vendor/hod2_phase12/native_c89/hod2_native.h"
#include "../../vendor/hod2_phase12/inherited/c89/hod2_extra_parts.h"
#include "../../include/modelrt_hod2_phase12.h"
#include <stdio.h>
#include <string.h>
#include <limits.h>
#define H2_MAX_DECOMP (32u*1024u*1024u)
#define H2_MAX_MODELS 4096u
#define H2_MAX_TRIANGLES 1000000u
#define H2_MAX_MATERIALS 4096u

typedef struct h2_state_s {
 const modelrt_u8 *bytes;
 size_t len;
 modelrt_u32 model_count;
 modelrt_u32 triangles;
 modelrt_u32 materials;
 modelrt_u32 kind; /* 1=POL, 2=H2MC, 3=H2TB verified tables */
 modelrt_u32 motion, type, frames, ticks, bones;
 Hod2PE tables;
 modelrt_u32 selected_type;
 int selected;
} h2_state;
static modelrt_u32 h2_u32(const modelrt_u8 *p)
{ return (modelrt_u32)p[0]|((modelrt_u32)p[1]<<8)|((modelrt_u32)p[2]<<16)|((modelrt_u32)p[3]<<24); }
static int h2_contains_pol_dir(const char *path)
{
 const char *p;
 for(p=path;*p;++p){
  if((*p=='/'||*p=='\\') && p[1] && p[2] && p[3] && p[4] && (p[1]=='p'||p[1]=='P') && (p[2]=='o'||p[2]=='O') &&
     (p[3]=='l'||p[3]=='L') && (p[4]=='/'||p[4]=='\\'))return 1;
 }
 return 0;
}
static int h2_probe(const char *path,const modelrt_u8 *head,size_t n)
{
 if(modelrt_ext_eq(path,".h2tb"))
  return n>=16u && memcmp(head,"H2TB",4u)==0 && h2_u32(head+4u)==1u ? MODELRT_PROBE_YES:MODELRT_PROBE_NO;
 if(modelrt_ext_eq(path,".h2mc"))
  return n>=40u && memcmp(head,"H2MC",4u)==0 && h2_u32(head+4u)==1u ? MODELRT_PROBE_YES:MODELRT_PROBE_NO;
 if(modelrt_ext_eq(path,".pol")||modelrt_ext_eq(path,".h2pol"))return MODELRT_PROBE_YES;
 if(modelrt_ext_eq(path,".bin")&&h2_contains_pol_dir(path))return MODELRT_PROBE_MAYBE;
 return MODELRT_PROBE_NO;
}
/* IEEE754 bits -> signed Q16, without float/double. Reject NaN/Inf and overflow. */
static int h2_fq16(modelrt_u32 bits,modelrt_q16 *out)
{
 modelrt_u32 exp,mant,mag,val;
 int shift,neg;
 neg=(bits&0x80000000u)!=0u;
 mag=bits&0x7fffffffu;exp=(mag>>23)&255u;mant=mag&0x7fffffu;
 if(exp==255u)return 0;
 if(exp==0u){*out=0;return 1;}
 mant|=0x800000u;shift=(int)exp-134;
 if(shift>=0){
  if(shift>7||mant>(neg?0x80000000u:0x7fffffffu)>>shift)return 0;
  val=mant<<shift;
 }else{
  if(shift<=-32){*out=0;return 1;}
  val=(mant+(1u<<(-shift-1)))>>(-shift);
 }
 if(val>(neg?0x80000000u:0x7fffffffu))return 0;
 *out=val==0x80000000u?(-2147483647-1):(neg?-(modelrt_i32)val:(modelrt_i32)val);
 return 1;
}
static int h2_vertex(const modelrt_u8 *v,modelrt_vertex_view *out)
{
 modelrt_q16 q[8];modelrt_u32 i;
 for(i=0u;i<8u;i++)if(!h2_fq16(h2_u32(v+i*4u),&q[i]))return 0;
 modelrt_zero(out,sizeof(*out));
 out->position.x=q[0];out->position.y=q[1];out->position.z=q[2];
 out->normal.x=q[3];out->normal.y=q[4];out->normal.z=q[5];
 out->uv0.x=q[6];out->uv0.y=q[7];
 out->flags=MODELRT_VTX_NORMAL|MODELRT_VTX_UV0;
 out->rgba[0]=out->rgba[1]=out->rgba[2]=out->rgba[3]=255u;
 return 1;
}
static int h2_pol_open(modelrt_handle *h,h2_state *s)
{
 const modelrt_u8 *d=s->bytes;size_t n=s->len;
 modelrt_u32 i,count=0u;unsigned long root,end;
 Hod2PolyStat st;
 if(!hod2_pol_model(d,n,0u,&root,&end,(unsigned int*)&count) || count==0u ||count>H2_MAX_MODELS)return MODELRT_ERR_FORMAT;
 for(i=0u;i<count;++i){
  if(!hod2_pol_model(d,n,i,&root,&end,0) || !hod2_pol_scan(d,n,root,end,&st,0,0))return MODELRT_ERR_FORMAT;
  /* Alternate POL layout is recognized by Phase11 but not drawable by the native bridge. */
  if(st.alt_strips || (h2_u32(d+root+4u)&16u))return MODELRT_ERR_UNSUPPORTED;
  if(st.nonfinite_uv||st.triangles>H2_MAX_TRIANGLES-s->triangles ||
      st.materials>H2_MAX_MATERIALS-s->materials)return MODELRT_ERR_FORMAT;
  s->triangles+=(modelrt_u32)st.triangles;
  s->materials+=(modelrt_u32)st.materials;
 }
 s->kind=1u;s->model_count=count;
 h->info.capabilities=MODELRT_CAP_GEOMETRY|MODELRT_CAP_NORMALS|
                       MODELRT_CAP_UV0|MODELRT_CAP_MATERIALS;
 h->info.mesh_count=count;h->info.material_count=s->materials;
 return MODELRT_OK;
}
static int h2_open(modelrt_runtime *rt,modelrt_handle *h,const char *path)
{
 modelrt_u8 *data,*decoded;size_t n,produced;
 modelrt_u32 expected,frames,bones,ticks,stride,block;
 h2_state *s;int rc;
 (void)rt;
 rc=modelrt_read_file_arena(h,path,&data,&n);
 if(rc!=MODELRT_OK)return rc;
 s=(h2_state*)modelrt_arena_alloc(h,sizeof(*s),8u);
 if(!s)return MODELRT_ERR_NO_SPACE;
 modelrt_zero(s,sizeof(*s));s->bytes=data;s->len=n;h->native_state=s;
 if(modelrt_ext_eq(path,".h2tb")){
  modelrt_u32 profile;
  if(n<48u||memcmp(data,"H2TB",4u)!=0||h2_u32(data+4u)!=1u)return MODELRT_ERR_FORMAT;
  profile=h2_u32(data+8u);
  if(profile>1u||!hod2_pe_init(&s->tables,data,n,(int)profile))return MODELRT_ERR_FORMAT;
  s->kind=3u; /* multi-character table archive: select type explicitly */
  h->info.capabilities=0u;h->info.bone_count=0u;
  return MODELRT_OK;
 }
 if(modelrt_ext_eq(path,".h2mc")){
  if(n<40u||memcmp(data,"H2MC",4u)!=0||h2_u32(data+4u)!=1u)return MODELRT_ERR_FORMAT;
  bones=h2_u32(data+20u);ticks=h2_u32(data+24u);frames=h2_u32(data+28u);
  stride=h2_u32(data+32u);block=h2_u32(data+36u);
  if((h2_u32(data+8u)>1u)||h2_u32(data+12u)>=86u||bones==0u||bones>128u||
     ticks<3u||ticks>100000u||frames<2u||frames>2048u||
     frames!=(ticks-1u)/2u+2u||stride!=(12u+6u*bones+3u)/4u*4u||
     block!=4u+frames*stride||n!=40u+(size_t)block)return MODELRT_ERR_FORMAT;
  s->kind=2u;s->motion=h2_u32(data+16u);s->type=h2_u32(data+12u);
  s->frames=frames;s->ticks=ticks;s->bones=bones;
  h->info.capabilities=MODELRT_CAP_ANIMATION;
  h->info.animation_count=1u;
  return MODELRT_OK;
 }
 /* Try original raw POL first. The binary can contain a PRS header, in
  * which case decompress into the caller's fixed workspace, never heap. */
 rc=h2_pol_open(h,s);
 if(rc==MODELRT_OK)return MODELRT_OK;
 if(rc==MODELRT_ERR_UNSUPPORTED)return rc;
 if(n<9u)return MODELRT_ERR_FORMAT;
 expected=h2_u32(data);
 if(expected==0u||expected>H2_MAX_DECOMP)return MODELRT_ERR_FORMAT;
 decoded=(modelrt_u8*)modelrt_arena_alloc(h,(size_t)expected,8u);
 if(!decoded)return MODELRT_ERR_NO_SPACE;
 if(!hod2_prs_decode(data+4u,n-4u,decoded,(size_t)expected,&produced)||produced!=(size_t)expected)return MODELRT_ERR_FORMAT;
 s->bytes=decoded;s->len=produced;s->triangles=s->materials=0u;
 return h2_pol_open(h,s);
}
static void h2_close(modelrt_handle *h){(void)h;}
/* Same list/strip winding and reference-offset logic as Phase11 hod2_glb.c. */
static int h2_emit_pol(const h2_state *s,modelrt_u32 model,modelrt_u32 material_base,
                       const modelrt_visit_sink *sink,void *user)
{
 unsigned long root,end,pos,matend=0UL,ctrl,flags,count,i,ref,need,w;
 const modelrt_u8 *d=s->bytes,*v,*prev1,*prev2,*v0,*v1;
 long delta;
 modelrt_u32 mat=material_base;int list;
 modelrt_triangle_view tr;
 if(!hod2_pol_model(d,s->len,model,&root,&end,0))return MODELRT_ERR_FORMAT;
 flags=hod2_u32(d+root+4UL);if(flags&16UL)return MODELRT_ERR_UNSUPPORTED;
 pos=root+24UL;mat=material_base;
 for(;;){
  if(pos+4UL>end)return MODELRT_ERR_FORMAT;
  ctrl=hod2_u32(d+pos);if(!ctrl)break;
  if(ctrl&0x80000000UL){
   if(matend&&pos!=matend)return MODELRT_ERR_FORMAT;
   if(pos+80UL>end)return MODELRT_ERR_FORMAT;
   matend=pos+80UL+(hod2_u32(d+pos+76UL)&~3UL);
   if(matend>end)return MODELRT_ERR_FORMAT;
   {modelrt_material_view m;
    modelrt_zero(&m,sizeof(m));m.index=mat++;
    sprintf(m.name,"HOD2_POL_material_%u",(unsigned)m.index);
    m.texture_index=-1;
    m.rgba[0]=m.rgba[1]=m.rgba[2]=m.rgba[3]=255u;
    if(sink->on_material && sink->on_material(user,&m))return MODELRT_ERR_CALLBACK;
   }
   pos+=80UL;continue;
  }
  if(mat==material_base||pos+8UL>end)return MODELRT_ERR_FORMAT;
  count=hod2_u32(d+pos+4UL);list=(ctrl&8UL)!=0UL;
  if(list){if(count>(end-pos)/96UL)return MODELRT_ERR_FORMAT;count*=3UL;}
  if(count>HOD2_MAX_VERTICES)return MODELRT_ERR_FORMAT;
  pos+=8UL;prev1=prev2=v0=v1=0;
  for(i=0UL;i<count;i++){
   if(pos+4UL>end)return MODELRT_ERR_FORMAT;
   w=hod2_u32(d+pos);ref=pos;need=32UL;
   if(ctrl&32UL && !(w&1UL)){
    if(pos+8UL>end)return MODELRT_ERR_FORMAT;
    delta=hod2_s32(d+pos+4UL);
    if(delta<0L){if((unsigned long)(-delta)>pos+8UL)return MODELRT_ERR_FORMAT;ref=pos+8UL-(unsigned long)(-delta);}
    else{if((unsigned long)delta>end-pos-8UL)return MODELRT_ERR_FORMAT;ref=pos+8UL+(unsigned long)delta;}
    need=8UL;
   }
   if(ref<root||ref+32UL>end||pos+need>end)return MODELRT_ERR_FORMAT;
   v=d+ref;
   if(list){
    if(i%3UL==0UL)v0=v;
    else if(i%3UL==1UL)v1=v;
    else{
     modelrt_zero(&tr,sizeof(tr));tr.mesh_index=model;tr.material_index=(modelrt_i32)mat-1;
     tr.flags=MODELRT_TRI_DOUBLE_SIDED;
     if(!h2_vertex(v0,&tr.v[0])||!h2_vertex(v1,&tr.v[1])||!h2_vertex(v,&tr.v[2]))return MODELRT_ERR_FORMAT;
     if(sink->on_triangle&&sink->on_triangle(user,&tr))return MODELRT_ERR_CALLBACK;
    }
   }else{
    if(i>=2UL){
     modelrt_zero(&tr,sizeof(tr));tr.mesh_index=model;tr.material_index=(modelrt_i32)mat-1;
     tr.flags=MODELRT_TRI_DOUBLE_SIDED;
     if(!h2_vertex((i&1UL)?prev1:prev2,&tr.v[0])||
        !h2_vertex((i&1UL)?prev2:prev1,&tr.v[1])||
        !h2_vertex(v,&tr.v[2]))return MODELRT_ERR_FORMAT;
     if(sink->on_triangle&&sink->on_triangle(user,&tr))return MODELRT_ERR_CALLBACK;
    }
    prev2=prev1;prev1=v;
   }
   pos+=need;
  }
 }
 if(matend&&matend!=pos)return MODELRT_ERR_FORMAT;
 return MODELRT_OK;
}
static int h2_visit(modelrt_handle *h,int animation_index,modelrt_q16 time,
                    const modelrt_visit_sink *sink,void *user)
{
 h2_state *s=(h2_state*)h->native_state;
 modelrt_u32 i,base=0u;Hod2PolyStat st;unsigned long root,end;int rc;
 (void)time;
 if(!s||!sink)return MODELRT_ERR_BAD_ARG;
 if(animation_index!=MODELRT_ANIM_BIND_POSE)return MODELRT_ERR_UNSUPPORTED;
 if(s->kind==3u){
  Hod2Node nodes[HOD2_MAX_NODES]; unsigned int count=0U,j;modelrt_bone_view b;
  if(s->selected && !hod2_skeleton(&s->tables,(unsigned int)s->selected_type,
                                      nodes,HOD2_MAX_NODES,&count))return MODELRT_ERR_FORMAT;
  if(sink->on_model_begin&&sink->on_model_begin(user,&h->info))return MODELRT_ERR_CALLBACK;
  if(s->selected){
   for(j=0U;j<count;j++){
    modelrt_zero(&b,sizeof(b));b.index=nodes[j].id;
    b.parent=nodes[j].parent==0xffffU?-1:(modelrt_i32)nodes[j].parent;
    sprintf(b.name,"HOD2_bone_%u",(unsigned)b.index);
    if(!h2_fq16((modelrt_u32)nodes[j].pos_bits[0],&b.position.x)||
       !h2_fq16((modelrt_u32)nodes[j].pos_bits[1],&b.position.y)||
       !h2_fq16((modelrt_u32)nodes[j].pos_bits[2],&b.position.z))return MODELRT_ERR_FORMAT;
    if(sink->on_bone&&sink->on_bone(user,&b))return MODELRT_ERR_CALLBACK;
   }
  }
  if(sink->on_model_end&&sink->on_model_end(user,&h->info))return MODELRT_ERR_CALLBACK;
  return MODELRT_OK;
 }
 /* H2MC is metadata only: bind pose can be enumerated as an empty model.
  * Non-bind animation playback remains unsupported. */
 if(sink->on_model_begin&&sink->on_model_begin(user,&h->info))return MODELRT_ERR_CALLBACK;
 for(i=0u;i<s->model_count;i++){
  modelrt_mesh_info m;
  if(!hod2_pol_model(s->bytes,s->len,i,&root,&end,0)||
     !hod2_pol_scan(s->bytes,s->len,root,end,&st,0,0))return MODELRT_ERR_FORMAT;
  modelrt_zero(&m,sizeof(m));m.index=i;
  sprintf(m.name,"HOD2_POL_%u",(unsigned)i);
  m.vertex_count=(modelrt_u32)(st.triangles*3UL);
  m.triangle_count=(modelrt_u32)st.triangles;
  m.material_count=(modelrt_u32)st.materials;
  if(sink->on_mesh_begin&&sink->on_mesh_begin(user,&m))return MODELRT_ERR_CALLBACK;
  rc=h2_emit_pol(s,i,base,sink,user);
  if(rc!=MODELRT_OK)return rc;
  if(sink->on_mesh_end&&sink->on_mesh_end(user,&m))return MODELRT_ERR_CALLBACK;
  base+=(modelrt_u32)st.materials;
 }
 if(sink->on_model_end&&sink->on_model_end(user,&h->info))return MODELRT_ERR_CALLBACK;
 return MODELRT_OK;
}
static int h2_anim_count(const modelrt_handle *h)
{h2_state *s=(h2_state*)h->native_state;return s&&s->kind==2u?1:0;}
static int h2_anim_info(modelrt_handle *h,int ix,modelrt_animation_info *out)
{
 h2_state *s=(h2_state*)h->native_state;
 if(!s||!out||s->kind!=2u||ix!=0)return MODELRT_ERR_BAD_ARG;
 modelrt_zero(out,sizeof(*out));out->index=0;
 sprintf(out->name,"HOD2_type%u_MOT%u",(unsigned)s->type,(unsigned)s->motion);
 out->flags=MODELRT_ANIM_UNBOUND|MODELRT_ANIM_DENSE_FRAMES|MODELRT_ANIM_TICK_RATE_KNOWN;
 out->frame_count=s->frames;out->source_key_count=s->frames;
 out->first_tick=0;out->last_tick=(modelrt_i32)(s->frames-1u)*2;
 out->ticks_per_second=60u;
 out->end_time=(modelrt_q16)(((s->frames-1u)*65536u)/30u);
 return MODELRT_OK;
}
static int h2_tex_count(const modelrt_handle *h){(void)h;return 0;}
const modelrt_provider modelrt_provider_hod2_89={
 MODELRT_CODEC_HOD2,"HOD2 Phase12 POL/PRS + H2MC + H2TB (native C89)",
 h2_probe,h2_open,h2_close,h2_visit,h2_anim_count,h2_anim_info,
 h2_tex_count,0,0,0,0,0,0
};

/* Optional Phase12 table browsing API, intentionally explicit about a selected
 * character; a standalone .h2tb is a multicharacter data-bank, not a mesh. */
static h2_state *h2_tables_state(const modelrt_handle *h)
{
 h2_state *s;
 if(!h||!h->is_open||h->info.codec_id!=MODELRT_CODEC_HOD2)return 0;
 s=(h2_state*)h->native_state;
 return s&&s->kind==3u?s:0;
}
int modelrt_hod2_table_profile(const modelrt_handle *h,modelrt_u32 *out)
{
 h2_state *s=h2_tables_state(h);
 if(!s||!out)return MODELRT_ERR_BAD_ARG;
 *out=(modelrt_u32)s->tables.profile;return MODELRT_OK;
}
int modelrt_hod2_select_character(modelrt_handle *h,modelrt_u32 type)
{
 h2_state *s=h2_tables_state(h);Hod2Node nodes[HOD2_MAX_NODES];unsigned int count=0U;
 if(!s||type>=86u)return MODELRT_ERR_BAD_ARG;
 if(!hod2_skeleton(&s->tables,(unsigned int)type,nodes,HOD2_MAX_NODES,&count))return MODELRT_ERR_FORMAT;
 s->selected=1;s->selected_type=type;h->info.bone_count=(modelrt_u32)count;
 h->info.capabilities|=MODELRT_CAP_SKELETON;
 return MODELRT_OK;
}
int modelrt_hod2_extra_part_count(const modelrt_handle *h,modelrt_u32 type,modelrt_u32 *out)
{
 h2_state *s=h2_tables_state(h);Hod2ExtraExe ctx;unsigned int count=0U;
 if(!s||!out||type>=86u)return MODELRT_ERR_BAD_ARG;
 hod2ex_init(&ctx,s->bytes,s->len,s->tables.profile);
 if(hod2ex_count(&ctx,(unsigned int)type,&count)!=1)return MODELRT_ERR_FORMAT;
 *out=(modelrt_u32)count;return MODELRT_OK;
}
int modelrt_hod2_extra_part_get(const modelrt_handle *h,modelrt_u32 type,
                               modelrt_u32 part_index,modelrt_hod2_part_info *out)
{
 h2_state *s=h2_tables_state(h);Hod2ExtraExe ctx;Hod2ExtraPart part;int rc;unsigned int i;
 if(!s||!out||type>=86u||part_index>=HOD2EX_MAX_PARTS)return MODELRT_ERR_BAD_ARG;
 hod2ex_init(&ctx,s->bytes,s->len,s->tables.profile);
 rc=hod2ex_read(&ctx,(unsigned int)type,(unsigned int)part_index,&part);
 if(rc<0)return MODELRT_ERR_FORMAT;
 if(rc==0)return MODELRT_ERR_UNSUPPORTED; /* null part descriptor */
 modelrt_zero(out,sizeof(*out));out->slot=(modelrt_u32)part.slot;
 out->draw_bone=(modelrt_i32)part.draw_bone;
 out->row_count=(modelrt_u32)part.row_count;
 out->draw_mode=(modelrt_u32)part.draw_mode;
 for(i=0U;i<4U;i++){
  out->group_bone[i]=(modelrt_i32)part.group_bone[i];
  out->source_vertices[i]=(modelrt_u32)part.source_vertex_count[i];
 }
 return MODELRT_OK;
}
