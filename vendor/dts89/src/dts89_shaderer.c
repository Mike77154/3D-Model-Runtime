#include "dts89_shaderer.h"
#include <string.h>

static int ascii_lower(int c) { return c>='A' && c<='Z' ? c+('a'-'A') : c; }
static int span_eq(dts89_string s,const char *lit)
{
 dts89_u32 i;
 if(!s.ptr || !lit) return 0;
 for(i=0;i<s.len;++i) {
  if(!lit[i] || ascii_lower((unsigned char)s.ptr[i])!=ascii_lower((unsigned char)lit[i]))return 0;
 }
 return lit[i]==0;
}
static int span_match(dts89_string a,dts89_string b)
{
 dts89_u32 i;
 if(!a.ptr || !b.ptr || a.len!=b.len)return 0;
 for(i=0;i<a.len;++i)if(ascii_lower((unsigned char)a.ptr[i])!=ascii_lower((unsigned char)b.ptr[i]))return 0;
 return 1;
}
static int span_yes(dts89_string s)
{ return span_eq(s,"true") || span_eq(s,"1") || span_eq(s,"yes"); }
static dts89_fx16_16 clamp_fx(dts89_fx16_16 x)
{ return x<0?0:(x>DTS89_FX_ONE?DTS89_FX_ONE:x); }
/* Bounded decimal parser; no binary real arithmetic or 64-bit arithmetic. */
static int decimal_fixed(const char *p,dts89_u32 n,dts89_fx16_16 *out)
{
 dts89_u32 i,whole,frac,scale,digit;
 dts89_s32 sign;
 int any;
 if(!p || !out || !n)return DTS89_ERR_BAD_ARG;
 i=0;while(i<n && (p[i]==' ' || p[i]=='\t'))i++;
 sign=1;if(i<n && (p[i]=='+' || p[i]=='-')){if(p[i]=='-')sign=-1;i++;}
 whole=0;any=0;
 while(i<n && p[i]>='0' && p[i]<='9') {
  any=1;digit=(dts89_u32)(p[i++]-'0');
  if(whole>32767u/10u || (whole==32767u/10u && digit>32767u%10u))return DTS89_ERR_RANGE;
  whole=whole*10u+digit;
 }
 frac=0;scale=1;
 if(i<n && p[i]=='.'){
  i++;
  while(i<n && p[i]>='0' && p[i]<='9') {
   any=1;digit=(dts89_u32)(p[i++]-'0');
   if(scale<=100000u){frac=frac*10u+digit;scale*=10u;}
  }
 }
 if(!any)return DTS89_ERR_FORMAT;
 while(i<n && (p[i]==' ' || p[i]=='\t'))i++;
 if(i<n)return DTS89_ERR_FORMAT;
 /* frac <= 999999; shift by 16 would overflow; split by scale. */
 {
  dts89_u32 f=frac,sc=scale;
  while(sc>10000u){f/=10u;sc/=10u;}
  digit=(f*65536u+sc/2u)/sc;
 }
 if(whole*65536u+digit>2147483647u)return DTS89_ERR_RANGE;
 *out=(dts89_fx16_16)(whole*65536u+digit);
 if(sign<0)*out=-*out;
 return DTS89_OK;
}
static int parse_vector(dts89_string val,dts89_fx16_16 *out,dts89_u32 count)
{
 dts89_u32 i,j,k;
 int rc;
 if(!val.ptr || !out)return DTS89_ERR_BAD_ARG;
 i=0;
 for(k=0;k<count;++k){
  while(i<val.len && (val.ptr[i]==' ' || val.ptr[i]=='\t' || val.ptr[i]==','))i++;
  j=i;
  while(i<val.len && val.ptr[i]!=' ' && val.ptr[i]!='\t' && val.ptr[i]!=',')i++;
  if(j==i)return DTS89_ERR_FORMAT;
  rc=decimal_fixed(val.ptr+j,i-j,out+k);
  if(rc)return rc;
 }
 return DTS89_OK;
}
static const dts89_ts_assignment *get_key(const dts89_ts_object *o,const char *key,int stage)
{
 dts89_s32 i;
 for(i=o->num_assignments-1;i>=0;--i){
  if(span_eq(o->assignments[i].key,key) &&
     (o->assignments[i].array_index==stage ||
      (stage==0 && o->assignments[i].array_index==-1)))return &o->assignments[i];
 }
 return 0;
}
static int get_bool(const dts89_ts_object *o,const char *key,dts89_u32 *out)
{
 const dts89_ts_assignment *a=get_key(o,key,0);
 if(!a)return 0;
 *out=(dts89_u32)span_yes(a->value);
 return 1;
}
static void stage_map(const dts89_ts_object *o,const char *key,dts89_s32 n,
 dts89_string *dst,dts89_u32 bit,dts89_shaderer_stage *stage)
{
 const dts89_ts_assignment *a=get_key(o,key,(int)n);
 if(a && a->value.len){*dst=a->value;stage->maps |= bit;}
}
static int blend_name(dts89_string s,dts89_u32 *out)
{
 if(span_eq(s,"none")){*out=DTS89_SHADERER_BLEND_OPAQUE;return 1;}
 if(span_eq(s,"mul")){*out=DTS89_SHADERER_BLEND_MUL;return 1;}
 if(span_eq(s,"premul")){*out=DTS89_SHADERER_BLEND_PREMUL;return 1;}
 if(span_eq(s,"add")){*out=DTS89_SHADERER_BLEND_ADD;return 1;}
 if(span_eq(s,"addalpha")){*out=DTS89_SHADERER_BLEND_ADD_ALPHA;return 1;}
 if(span_eq(s,"sub")){*out=DTS89_SHADERER_BLEND_SUB;return 1;}
 if(span_eq(s,"lerpalpha")){*out=DTS89_SHADERER_BLEND_LERP_ALPHA;return 1;}
 if(span_eq(s,"tonemap")){*out=DTS89_SHADERER_BLEND_TONE;return 1;}
 return 0;
}
int dts89_shaderer_from_dts(const dts89_shape *shape,dts89_s32 idx,dts89_shaderer_material *out)
{
 const dts89_material *m;
 dts89_u32 i;
 if(!shape || !out || idx<0 || idx>=shape->num_materials || !shape->materials)return DTS89_ERR_BAD_ARG;
 m=&shape->materials[idx];memset(out,0,sizeof(*out));
 out->name=m->name;out->map_to=m->name;out->raw_flags=m->flags;
 out->sampler_s=(m->flags & DTS89_MATF_S_WRAP)?DTS89_SHADERER_WRAP:DTS89_SHADERER_CLAMP;
 out->sampler_t=(m->flags & DTS89_MATF_T_WRAP)?DTS89_SHADERER_WRAP:DTS89_SHADERER_CLAMP;
 out->mipmap=(m->flags & DTS89_MATF_NO_MIPMAP)?DTS89_SHADERER_MIP_NONE:
     ((m->flags & DTS89_MATF_MIPMAP_ZERO_BORDER)?DTS89_SHADERER_MIP_ZERO_BORDER:DTS89_SHADERER_MIPMAP);
 out->translucent=(m->flags & DTS89_MATF_TRANSLUCENT)?1u:0u;
 out->alpha_ref=1u;
 out->self_illuminating=(m->flags & DTS89_MATF_SELF_ILLUMINATING)?1u:0u;
 out->never_envmap=(m->flags & DTS89_MATF_NEVER_ENVMAP)?1u:0u;
 out->blend=(m->flags & DTS89_MATF_SUBTRACTIVE)?DTS89_SHADERER_BLEND_SUB:
      ((m->flags & DTS89_MATF_ADDITIVE)?DTS89_SHADERER_BLEND_ADD:
      (out->translucent?DTS89_SHADERER_BLEND_ALPHA:DTS89_SHADERER_BLEND_OPAQUE));
 out->legacy_reflectance=m->reflectance_map;out->legacy_bump=m->bump_map;
 out->legacy_detail=m->detail_map;
 out->legacy_detail_scale=dts89_f32bits_to_fx(m->detail_scale_bits);
 out->legacy_reflection=dts89_f32bits_to_fx(m->reflection_amount_bits);
 if(out->legacy_reflectance!=0xFFFFFFFFu && !out->never_envmap)
  out->unsupported_features|=DTS89_SHADERER_CAP_ENV_MAP;
 if(out->legacy_bump!=0xFFFFFFFFu)out->unsupported_features|=DTS89_SHADERER_CAP_NORMAL_RENDER;
 if(out->legacy_detail!=0xFFFFFFFFu)out->unsupported_features|=DTS89_SHADERER_CAP_DETAIL_RENDER;
 for(i=0;i<DTS89_SHADERER_STAGES;i++){
  out->stage[i].diffuse_color[0]=DTS89_FX_ONE;
  out->stage[i].diffuse_color[1]=DTS89_FX_ONE;
  out->stage[i].diffuse_color[2]=DTS89_FX_ONE;
  out->stage[i].diffuse_color[3]=DTS89_FX_ONE;
  out->stage[i].detail_scale=2*DTS89_FX_ONE;
  out->stage[i].specular_power=DTS89_FX_ONE;
 }
 return DTS89_OK;
}
static void stage_fill(dts89_shaderer_stage *s,const dts89_ts_object *o,dts89_s32 i,
 dts89_shaderer_material *mat)
{
 const dts89_ts_assignment *a;
 stage_map(o,"diffuseMap",i,&s->diffuse,DTS89_SHADERER_MAP_DIFFUSE,s);
 stage_map(o,"normalMap",i,&s->normal,DTS89_SHADERER_MAP_NORMAL,s);
 stage_map(o,"specularMap",i,&s->specular,DTS89_SHADERER_MAP_SPECULAR,s);
 stage_map(o,"detailMap",i,&s->detail,DTS89_SHADERER_MAP_DETAIL,s);
 stage_map(o,"detailNormalMap",i,&s->detail_normal,DTS89_SHADERER_MAP_DETAIL_NORMAL,s);
 stage_map(o,"overlayMap",i,&s->overlay,DTS89_SHADERER_MAP_OVERLAY,s);
 stage_map(o,"glowMap",i,&s->glow,DTS89_SHADERER_MAP_GLOW,s);
 stage_map(o,"aoMap",i,&s->ao,DTS89_SHADERER_MAP_AO,s);
 stage_map(o,"roughMap",i,&s->rough,DTS89_SHADERER_MAP_ROUGH,s);
 stage_map(o,"metalMap",i,&s->metal,DTS89_SHADERER_MAP_METAL,s);
 stage_map(o,"ormConfigMap",i,&s->orm,DTS89_SHADERER_MAP_ORM,s);
 a=get_key(o,"diffuseColor",i);
 if(a && parse_vector(a->value,s->diffuse_color,4u))mat->unknown_properties++;
 a=get_key(o,"specular",i);
 if(a && parse_vector(a->value,s->specular_color,4u))mat->unknown_properties++;
 a=get_key(o,"specularPower",i);
 if(a && decimal_fixed(a->value.ptr,a->value.len,&s->specular_power))mat->unknown_properties++;
 a=get_key(o,"detailScale",i);
 if(a && decimal_fixed(a->value.ptr,a->value.len,&s->detail_scale))mat->unknown_properties++;
 a=get_key(o,"glow",i);if(a)s->glow_enabled=(dts89_u32)span_yes(a->value);
 a=get_key(o,"pixelSpecular",i);if(a)s->pixel_specular=(dts89_u32)span_yes(a->value);
 a=get_key(o,"animFlags",i);
 if(a){s->anim_flags=1u;mat->unsupported_features|=DTS89_SHADERER_CAP_ANIM_UV;}
 if(s->maps & (DTS89_SHADERER_MAP_NORMAL | DTS89_SHADERER_MAP_DETAIL_NORMAL))
  mat->unsupported_features|=DTS89_SHADERER_CAP_NORMAL_RENDER;
 if((s->maps & DTS89_SHADERER_MAP_SPECULAR) || s->pixel_specular)
  mat->unsupported_features|=DTS89_SHADERER_CAP_SPECULAR_RENDER;
 if(s->maps & DTS89_SHADERER_MAP_DETAIL)
  mat->unsupported_features|=DTS89_SHADERER_CAP_DETAIL_RENDER;
 if(i && s->maps)mat->unsupported_features|=DTS89_SHADERER_CAP_MULTI_STAGE;
}
static int key_known(dts89_string key)
{
 static const char *names[]={"mapTo","diffuseMap","normalMap","specularMap",
 "detailMap","detailNormalMap","overlayMap","glowMap","aoMap","roughMap",
 "metalMap","ormConfigMap","diffuseColor","specular","specularPower","detailScale",
 "glow","pixelSpecular","animFlags","translucent","translucentBlendOp",
 "alphaTest","alphaRef","doubleSided","emissive","castShadows","footstepSoundId",
 "showFootprints","dynamicCubemap","cubemap","parallaxScale","scrollDir",
 "scrollSpeed","rotSpeed","waveFreq","waveAmp","waveType","sequenceFramePerSec",
 "sequenceSegmentSize","diffuseMapSRGB","alphaTestValue","materialTag0",
 "materialTag1","materialTag2","materialTag3","materialTag4","materialTag5"};
 dts89_u32 i;
 for(i=0;i<sizeof(names)/sizeof(names[0]);++i)if(span_eq(key,names[i]))return 1;
 return 0;
}
static int apply_object(dts89_shaderer_material *mat,const dts89_ts_object *o)
{
 dts89_s32 i;
 dts89_u32 u;
 const dts89_ts_assignment *a;
 for(i=0;i<(dts89_s32)DTS89_SHADERER_STAGES;++i)stage_fill(&mat->stage[i],o,i,mat);
 a=get_key(o,"mapTo",0);if(a)mat->map_to=a->value;
 if(get_bool(o,"translucent",&u))mat->translucent=u;
 if(get_bool(o,"alphaTest",&u))mat->alpha_test=u;
 if(get_bool(o,"doubleSided",&u))mat->double_sided=u;
 if(get_bool(o,"emissive",&u))mat->self_illuminating=u;
 if(get_bool(o,"dynamicCubemap",&u) && u)mat->unsupported_features|=DTS89_SHADERER_CAP_ENV_MAP;
 a=get_key(o,"cubemap",0);if(a && a->value.len)mat->unsupported_features|=DTS89_SHADERER_CAP_ENV_MAP;
 a=get_key(o,"alphaRef",0);
 if(a){dts89_fx16_16 t;if(!decimal_fixed(a->value.ptr,a->value.len,&t)){
  if(t<0)t=0;
  if(t>255*65536)t=255*65536;
  mat->alpha_ref=(dts89_u32)(t/65536);
 } else mat->unknown_properties++;}
 a=get_key(o,"translucentBlendOp",0);
 if(a){if(!blend_name(a->value,&mat->blend))mat->unknown_properties++;}
 else if(mat->translucent)mat->blend=DTS89_SHADERER_BLEND_PREMUL;
 /* A blend operation without enabled translucency is not applied by Torque. */
 if(!mat->translucent && !mat->alpha_test)mat->blend=DTS89_SHADERER_BLEND_OPAQUE;
 if(mat->alpha_test)mat->unsupported_features|=DTS89_SHADERER_CAP_ALPHA_TEST;
 if(mat->blend==DTS89_SHADERER_BLEND_TONE)mat->unsupported_features|=DTS89_SHADERER_CAP_TONE_MAP;
 for(i=0;i<o->num_assignments;++i){
  dts89_string key=o->assignments[i].key;
  dts89_s32 si=o->assignments[i].array_index;
  if(!key_known(key))mat->unknown_properties++;
  if(si>=(dts89_s32)DTS89_SHADERER_STAGES)
   mat->unsupported_features|=DTS89_SHADERER_CAP_TOO_MANY_STAGES;
  if(span_eq(key,"parallaxScale"))mat->unsupported_features|=DTS89_SHADERER_CAP_PARALLAX;
  if(span_eq(key,"diffuseMapSRGB"))mat->unsupported_features|=DTS89_SHADERER_CAP_COLORSPACE;
  if(span_eq(key,"scrollDir") || span_eq(key,"scrollSpeed") ||
     span_eq(key,"rotSpeed") || span_eq(key,"waveFreq") ||
     span_eq(key,"waveAmp") || span_eq(key,"waveType") ||
     span_eq(key,"sequenceFramePerSec") || span_eq(key,"sequenceSegmentSize"))
   mat->unsupported_features|=DTS89_SHADERER_CAP_ANIM_UV;
 }
 if(mat->unknown_properties)mat->unsupported_features|=DTS89_SHADERER_CAP_SCRIPT_UNKNOWN;
 mat->script_matched=1u;
 return DTS89_OK;
}
int dts89_shaderer_apply_script(dts89_shaderer_material *mat,const dts89_ts_file *file)
{
 dts89_s32 i,j;
 const dts89_ts_assignment *a;
 const dts89_ts_object *o;
 if(!mat || !file)return DTS89_ERR_BAD_ARG;
 for(i=0;i<file->num_objects;++i){
  o=&file->objects[i];if(!span_eq(o->class_name,"Material"))continue;
  a=get_key(o,"mapTo",0);
  if(a && span_match(a->value,mat->name))return apply_object(mat,o);
 }
 for(j=0;j<file->num_objects;++j){
  o=&file->objects[j];
  if(span_eq(o->class_name,"Material") && span_match(o->object_name,mat->name))
   return apply_object(mat,o);
 }
 return DTS89_ERR_UNSUPPORTED;
}
int dts89_shaderer_build(const dts89_shape *shape,dts89_s32 idx,
 const dts89_ts_file *script,dts89_shaderer_material *out)
{
 int rc=dts89_shaderer_from_dts(shape,idx,out);
 if(rc)return rc;
 if(script){rc=dts89_shaderer_apply_script(out,script);if(rc && rc!=DTS89_ERR_UNSUPPORTED)return rc;}
 return DTS89_OK;
}
static dts89_fx16_16 address_one(dts89_fx16_16 v,dts89_u32 mode)
{
 dts89_s32 r;
 if(mode==DTS89_SHADERER_CLAMP)return clamp_fx(v);
 r=v%DTS89_FX_ONE;if(r<0)r+=DTS89_FX_ONE;
 return r;
}
int dts89_shaderer_address_uv(dts89_fx_vec2 uv,const dts89_shaderer_material *mat,
 dts89_fx_vec2 *out)
{
 if(!mat || !out)return DTS89_ERR_BAD_ARG;
 if(mat->sampler_s>DTS89_SHADERER_WRAP || mat->sampler_t>DTS89_SHADERER_WRAP)return DTS89_ERR_RANGE;
 out->x=address_one(uv.x,mat->sampler_s);
 out->y=address_one(uv.y,mat->sampler_t);
 return DTS89_OK;
}
int dts89_shaderer_sample_rgba8(const dts89_u8 *pixels,dts89_u32 width,
 dts89_u32 height,dts89_u32 stride,dts89_fx_vec2 uv,
 const dts89_shaderer_material *mat,dts89_shaderer_rgba8 *out)
{
 dts89_fx_vec2 addr;
 dts89_u32 x,y,off;
 if(!pixels || !mat || !out || !width || !height)return DTS89_ERR_BAD_ARG;
 if(width>65535u || height>65535u || width>0xFFFFFFFFu/4u)return DTS89_ERR_RANGE;
 if(stride<width*4u || height>0xFFFFFFFFu/stride)return DTS89_ERR_RANGE;
 if(dts89_shaderer_address_uv(uv,mat,&addr))return DTS89_ERR_RANGE;
 /* Exact 1 clamps to last texel, repeated 1 addresses texel 0. */
 x=(((dts89_u32)addr.x>>8)*width)>>8;
 y=(((dts89_u32)addr.y>>8)*height)>>8;
 if(x>=width)x=width-1u;
 if(y>=height)y=height-1u;
 off=y*stride+x*4u;
 out->r=pixels[off];out->g=pixels[off+1u];
 out->b=pixels[off+2u];out->a=pixels[off+3u];
 return DTS89_OK;
}
static dts89_u8 sat8(int x) { return (dts89_u8)(x<0?0:(x>255?255:x)); }
static int scale8(int a,int alpha){return (a*alpha+127)/255;}
int dts89_shaderer_blend_rgba8(dts89_shaderer_rgba8 src,
 dts89_shaderer_rgba8 dst,dts89_u32 mode,dts89_shaderer_rgba8 *out)
{
 int r,g,b,a;
 if(!out)return DTS89_ERR_BAD_ARG;
 a=(int)src.a;
 switch(mode){
 case DTS89_SHADERER_BLEND_OPAQUE:
  *out=src;return DTS89_OK;
 case DTS89_SHADERER_BLEND_ALPHA:
 case DTS89_SHADERER_BLEND_LERP_ALPHA:
  r=scale8(src.r,a)+scale8(dst.r,255-a);
  g=scale8(src.g,a)+scale8(dst.g,255-a);
  b=scale8(src.b,a)+scale8(dst.b,255-a);break;
 case DTS89_SHADERER_BLEND_ADD:
  r=src.r+dst.r;g=src.g+dst.g;b=src.b+dst.b;break;
 case DTS89_SHADERER_BLEND_ADD_ALPHA:
  r=scale8(src.r,a)+dst.r;g=scale8(src.g,a)+dst.g;b=scale8(src.b,a)+dst.b;break;
 case DTS89_SHADERER_BLEND_SUB:
  r=(int)dst.r-src.r;g=(int)dst.g-src.g;b=(int)dst.b-src.b;break;
 case DTS89_SHADERER_BLEND_MUL:
  r=scale8(src.r,dst.r);g=scale8(src.g,dst.g);b=scale8(src.b,dst.b);break;
 case DTS89_SHADERER_BLEND_PREMUL:
  r=src.r+scale8(dst.r,255-a);g=src.g+scale8(dst.g,255-a);
  b=src.b+scale8(dst.b,255-a);break;
 default:return DTS89_ERR_UNSUPPORTED;
 }
 out->r=sat8(r);out->g=sat8(g);out->b=sat8(b);
 out->a=sat8((int)src.a+scale8(dst.a,255-a));
 return DTS89_OK;
}

/* Bounded integer square root: 0..16384 -> 0..128. */
static dts89_u32 shaderer_sqrt_u14(dts89_u32 number)
{
 dts89_u32 lo=0u,hi=128u,m;
 if(number>=16384u)return 128u;
 while(lo<hi){
  m=(lo+hi+1u)/2u;
  if(m*m<=number)lo=m;
  else hi=m-1u;
 }
 return lo;
}
int dts89_shaderer_unpack_normal(dts89_shaderer_rgba8 pixel,dts89_u32 mode,
 dts89_fx_vec3 *normal)
{
 dts89_s32 x,y;
 dts89_u32 remains;
 if(!normal)return DTS89_ERR_BAD_ARG;
 if(mode==DTS89_SHADERER_NORMAL_RGB){
  x=(dts89_s32)pixel.r-128;
  y=(dts89_s32)pixel.g-128;
  normal->z=((dts89_s32)pixel.b-128)*512;
 }else if(mode==DTS89_SHADERER_NORMAL_DXT5_AG){
  x=(dts89_s32)pixel.a-128;
  y=(dts89_s32)pixel.g-128;
  remains=(dts89_u32)(x*x+y*y);
  normal->z=(dts89_fx16_16)(shaderer_sqrt_u14(
    remains>=16384u?0u:16384u-remains)*512u);
 }else return DTS89_ERR_BAD_ARG;
 normal->x=x*512;
 normal->y=y*512;
 return DTS89_OK;
}
int dts89_shaderer_detect_normal_packing(const dts89_u8 *rgba,dts89_u32 width,
 dts89_u32 height,dts89_u32 stride,dts89_u32 capacity,dts89_u32 *out_mode)
{
 dts89_u32 x,y,n=0u,likely=0u,alpha_low=0u;
 if(!rgba || !out_mode || !width || !height)return DTS89_ERR_BAD_ARG;
 if(width>0xFFFFFFFFu/4u || stride<width*4u ||
    height>0xFFFFFFFFu/stride || capacity<height*stride)return DTS89_ERR_RANGE;
 for(y=0u;y<8u;++y)for(x=0u;x<8u;++x){
  dts89_u32 col=x*(width-1u)/7u,row=y*(height-1u)/7u;
  const dts89_u8 *p=rgba+row*stride+col*4u;
  n++;
  if(p[0]>=245u && p[2]<=10u)likely++;
  if(p[3]<245u)alpha_low++;
 }
 *out_mode=(likely>=n*7u/8u && alpha_low>=n/4u)?
     DTS89_SHADERER_NORMAL_DXT5_AG:DTS89_SHADERER_NORMAL_RGB;
 return DTS89_OK;
}
