#include "dts89_lighting40.h"

static dts89_fx16_16 l40_add(dts89_fx16_16 a,dts89_fx16_16 b)
{
 if(b>0 && a>0x7FFFFFFF-b)return 0x7FFFFFFF;
 if(b<0 && a<(-2147483647-1)-b)return (-2147483647-1);
 return a+b;
}
static dts89_fx16_16 l40_neg(dts89_fx16_16 a)
{ return a==(-2147483647-1)?0x7FFFFFFF:-a; }
static dts89_fx16_16 l40_sub(dts89_fx16_16 a,dts89_fx16_16 b)
{ return l40_add(a,l40_neg(b)); }
static dts89_fx16_16 l40_clamp(dts89_fx16_16 x)
{ return x<0?0:(x>DTS89_FX_ONE?DTS89_FX_ONE:x); }
static dts89_fx16_16 l40_dot(dts89_fx_vec3 a,dts89_fx_vec3 b)
{ return l40_add(l40_add(dts89_fx_mul(a.x,b.x),dts89_fx_mul(a.y,b.y)),dts89_fx_mul(a.z,b.z)); }
static dts89_fx_vec3 l40_cross(dts89_fx_vec3 a,dts89_fx_vec3 b)
{
 dts89_fx_vec3 r;
 r.x=l40_sub(dts89_fx_mul(a.y,b.z),dts89_fx_mul(a.z,b.y));
 r.y=l40_sub(dts89_fx_mul(a.z,b.x),dts89_fx_mul(a.x,b.z));
 r.z=l40_sub(dts89_fx_mul(a.x,b.y),dts89_fx_mul(a.y,b.x));
 return r;
}
static dts89_fx16_16 l40_sqrt(dts89_fx16_16 x)
{
 dts89_fx16_16 z,next;unsigned i;
 if(x<=0)return 0;
 z=x>DTS89_FX_ONE?x:DTS89_FX_ONE;
 for(i=0u;i<16u;++i){
  next=(z+dts89_fx_div(x,z))/2;
  if(next==z)break;
  z=next;
 }
 return z;
}
static dts89_fx_vec3 l40_normalize(dts89_fx_vec3 a)
{
 dts89_fx16_16 len=l40_sqrt(l40_dot(a,a));
 if(len<128){a.x=0;a.y=0;a.z=DTS89_FX_ONE;return a;}
 a.x=dts89_fx_div(a.x,len);
 a.y=dts89_fx_div(a.y,len);
 a.z=dts89_fx_div(a.z,len);
 return a;
}
static dts89_fx_vec3 l40_vecsub(dts89_fx_vec3 a,dts89_fx_vec3 b)
{ a.x=l40_sub(a.x,b.x);a.y=l40_sub(a.y,b.y);a.z=l40_sub(a.z,b.z);return a; }
static dts89_fx_vec3 l40_vecscale(dts89_fx_vec3 a,dts89_fx16_16 x)
{ a.x=dts89_fx_mul(a.x,x);a.y=dts89_fx_mul(a.y,x);a.z=dts89_fx_mul(a.z,x);return a; }
static dts89_fx_vec3 l40_vecadd(dts89_fx_vec3 a,dts89_fx_vec3 b)
{ a.x=l40_add(a.x,b.x);a.y=l40_add(a.y,b.y);a.z=l40_add(a.z,b.z);return a; }
static dts89_fx_vec3 l40_fallback_tangent(dts89_fx_vec3 n)
{
 dts89_fx_vec3 a;
 a.x=0;a.y=0;a.z=DTS89_FX_ONE;
 if(n.z>50000 || n.z< -50000){a.x=0;a.y=DTS89_FX_ONE;a.z=0;}
 return l40_normalize(l40_cross(a,n));
}
int dts89_lit40_triangle_basis(const dts89_fx_vec3 p[3],
 const dts89_fx_vec2 uv[3],const dts89_fx_vec3 normals[3],
 dts89_lit40_basis *out)
{
 dts89_fx_vec3 e1,e2,n,t,b,ns;
 dts89_fx16_16 du1,dv1,du2,dv2,det,sgn;
 if(!p||!uv||!normals||!out)return DTS89_ERR_BAD_ARG;
 ns=l40_vecadd(l40_vecadd(normals[0],normals[1]),normals[2]);
 n=l40_normalize(ns);
 e1=l40_vecsub(p[1],p[0]);e2=l40_vecsub(p[2],p[0]);
 du1=l40_sub(uv[1].x,uv[0].x);dv1=l40_sub(uv[1].y,uv[0].y);
 du2=l40_sub(uv[2].x,uv[0].x);dv2=l40_sub(uv[2].y,uv[0].y);
 det=l40_sub(dts89_fx_mul(du1,dv2),dts89_fx_mul(du2,dv1));
 if(det>-128 && det<128){t=l40_fallback_tangent(n);sgn=DTS89_FX_ONE;}
 else {
  t=l40_vecsub(l40_vecscale(e1,dv2),l40_vecscale(e2,dv1));
  /* Use magnitude of det only for normalization: preserves 32-bit range. */
  if(det<0)t=l40_vecscale(t,-DTS89_FX_ONE);
  t=l40_vecsub(t,l40_vecscale(n,l40_dot(t,n)));
  t=l40_normalize(t);
  sgn=det<0?-DTS89_FX_ONE:DTS89_FX_ONE;
 }
 b=l40_cross(n,t);
 if(sgn<0)b=l40_vecscale(b,-DTS89_FX_ONE);
 out->n=n;out->t=t;out->b=l40_normalize(b);
 return DTS89_OK;
}
static int l40_sample(const dts89_lit40_image *im,dts89_fx_vec2 uv,
 const dts89_shaderer_material *mat,dts89_shaderer_rgba8 *out)
{
 if(!im)return DTS89_ERR_BAD_ARG;
 return dts89_shaderer_sample_rgba8(im->rgba,im->width,im->height,im->stride,uv,mat,out);
}
static dts89_u8 l40_channel(int diffuse,dts89_fx16_16 lit,
 int spec,dts89_fx16_16 shine)
{
 dts89_s32 a,b,total;
 if(lit<0)lit=0;
 if(lit>131072)lit=131072;
 if(shine<0)shine=0;
 if(shine>131072)shine=131072;
 a=(dts89_s32)diffuse*lit/65536;
 b=(dts89_s32)spec*shine/65536;
 total=a+b;
 if(total>255)return 255u;
 if(total<0)return 0u;
 return (dts89_u8)total;
}
int dts89_lit40_shade(dts89_fx_vec2 uv,const dts89_lit40_basis *basis,
 const dts89_shaderer_material *mat,const dts89_lit40_maps *maps,
 const dts89_lit40_light *light,dts89_shaderer_rgba8 *out)
{
 dts89_shaderer_rgba8 diff,normal_tex,spec;
 dts89_fx_vec3 n,nt,h;
 dts89_fx16_16 lambert,specdot,specpow,lit,shine;
 dts89_u32 exponent,i;
 int rc;
 if(!basis||!mat||!maps||!light||!out)return DTS89_ERR_BAD_ARG;
 diff.r=diff.g=diff.b=diff.a=255u;
 spec.r=spec.g=spec.b=255u;spec.a=255u;
 if(maps->diffuse){rc=l40_sample(maps->diffuse,uv,mat,&diff);if(rc)return rc;}
 /* Stage-zero color multiplier; clamp components before 8-bit conversion. */
 diff.r=(dts89_u8)((dts89_u32)diff.r*(dts89_u32)l40_clamp(mat->stage[0].diffuse_color[0])/65536u);
 diff.g=(dts89_u8)((dts89_u32)diff.g*(dts89_u32)l40_clamp(mat->stage[0].diffuse_color[1])/65536u);
 diff.b=(dts89_u8)((dts89_u32)diff.b*(dts89_u32)l40_clamp(mat->stage[0].diffuse_color[2])/65536u);
 diff.a=(dts89_u8)((dts89_u32)diff.a*(dts89_u32)l40_clamp(mat->stage[0].diffuse_color[3])/65536u);
 if(mat->alpha_test && diff.a<mat->alpha_ref){out->r=out->g=out->b=0u;out->a=0u;return DTS89_OK;}
 n=l40_normalize(basis->n);
 if(maps->normal){
  rc=l40_sample(maps->normal,uv,mat,&normal_tex);if(rc)return rc;
  rc=dts89_shaderer_unpack_normal(normal_tex,maps->normal_packing,&nt);if(rc)return rc;
  n=l40_vecadd(l40_vecadd(l40_vecscale(basis->t,nt.x),l40_vecscale(basis->b,nt.y)),l40_vecscale(n,nt.z));
  n=l40_normalize(n);
 }
 if(maps->specular){rc=l40_sample(maps->specular,uv,mat,&spec);if(rc)return rc;}
 if(mat->self_illuminating){lit=DTS89_FX_ONE;shine=0;}
 else {
  lambert=l40_clamp(l40_dot(n,light->to_light));
  lit=l40_add(light->ambient,dts89_fx_mul(light->diffuse,lambert));
  h=l40_normalize(l40_vecadd(light->to_light,light->to_view));
  specdot=l40_clamp(l40_dot(n,h));
  specpow=mat->stage[0].specular_power;
  if(specpow<DTS89_FX_ONE)specpow=DTS89_FX_ONE;
  if(specpow>64*DTS89_FX_ONE)specpow=64*DTS89_FX_ONE;
  /* Approximate gloss exponent from map alpha; sampled specular RGB
     overrides script specular RGB, matching documented Torque behavior. */
  if(maps->specular)specpow=dts89_fx_mul(specpow,(dts89_fx16_16)(spec.a*257u));
  exponent=(dts89_u32)(specpow/65536);
  if(exponent<1u)exponent=1u;
  shine=specdot;
  for(i=1u;i<exponent;++i)shine=dts89_fx_mul(shine,specdot);
  shine=dts89_fx_mul(shine,light->specular);
 }
 out->r=l40_channel((int)diff.r,lit,(int)(maps->specular?spec.r:(dts89_u32)(mat->stage[0].specular_color[0]>>8)),shine);
 out->g=l40_channel((int)diff.g,lit,(int)(maps->specular?spec.g:(dts89_u32)(mat->stage[0].specular_color[1]>>8)),shine);
 out->b=l40_channel((int)diff.b,lit,(int)(maps->specular?spec.b:(dts89_u32)(mat->stage[0].specular_color[2]>>8)),shine);
 out->a=diff.a;
 return DTS89_OK;
}
static dts89_fx_vec3 l40_rotate(const dts89_fx_mat34 *m,dts89_fx_vec3 p)
{
 dts89_fx_vec3 a;
 a.x=l40_add(l40_add(dts89_fx_mul(m->a[0],p.x),dts89_fx_mul(m->a[1],p.y)),dts89_fx_mul(m->a[2],p.z));
 a.y=l40_add(l40_add(dts89_fx_mul(m->a[4],p.x),dts89_fx_mul(m->a[5],p.y)),dts89_fx_mul(m->a[6],p.z));
 a.z=l40_add(l40_add(dts89_fx_mul(m->a[8],p.x),dts89_fx_mul(m->a[9],p.y)),dts89_fx_mul(m->a[10],p.z));
 return l40_normalize(a);
}
int dts89_lit40_address_raw_uv(dts89_u32 bits,dts89_u32 mode,
 dts89_fx16_16 *out)
{
 dts89_u32 expbits,mant,frac,mask;dts89_s32 e,shift;
 int neg;
 if(!out || mode>DTS89_SHADERER_WRAP)return DTS89_ERR_BAD_ARG;
 expbits=(bits>>23)&255u;mant=bits&0x7FFFFFu;
 if(expbits==255u)return DTS89_ERR_FORMAT;
 neg=(bits&0x80000000u)!=0u;
 if(expbits==0u) {e=-126;}else{e=(dts89_s32)expbits-127;mant|=0x800000u;}
 if(e>=23)frac=0u;
 else if(e>=0){
  shift=23-e;
  mask=(1u<<(dts89_u32)shift)-1u;
  frac=mant&mask;
  if(e<=7)frac>>=(dts89_u32)(7-e);
  else frac<<=(dts89_u32)(e-7);
 }else {
  shift=7-e;
  frac=shift>=32?0u:(mant>>(dts89_u32)shift);
 }
 if(mode==DTS89_SHADERER_CLAMP){
  if(neg && (mant!=0u || expbits!=0u))*out=0;
  else if(e>=0)*out=DTS89_FX_ONE;
  else *out=(dts89_fx16_16)frac;
 }else{
  *out=(dts89_fx16_16)(neg && frac?65536u-frac:frac);
 }
 return DTS89_OK;
}
int dts89_lit40_pose_mesh(const dts89_shape *shape,
 const dts89_mesh_view *mesh,dts89_s32 object_node,
 const dts89_shaderer_material *material,
 const dts89_fx_mat34 *globals,dts89_u32 globals_count,
 dts89_fx_vec3 *positions,dts89_fx_vec3 *normals,
 dts89_fx_vec2 *uvs,dts89_fx16_16 *weight_sums,
 dts89_u32 vertex_capacity,dts89_fx_mat34 *palette,
 dts89_u32 palette_capacity)
{
 dts89_s32 i,n;int rc;
 dts89_fx_mat34 ident;
 if(!shape||!mesh||!material||!globals||!positions||!normals||!uvs||!weight_sums||!palette)return DTS89_ERR_BAD_ARG;
 n=mesh->num_verts;
 if(n<0||(dts89_u32)n>vertex_capacity)return DTS89_ERR_RANGE;
 if(mesh->mesh_type==DTS89_MESH_SKIN){
  rc=dts89_mesher37_palette(shape,mesh,globals,globals_count,palette,palette_capacity);
  if(rc)return rc;
  rc=dts89_mesher37_skin(mesh,palette,(dts89_u32)mesh->skin_node_index_count,positions,weight_sums,vertex_capacity);
  if(rc)return rc;
  rc=dts89_mesher37_skin_normals(mesh,palette,(dts89_u32)mesh->skin_node_index_count,normals,vertex_capacity);
  if(rc)return rc;
 } else {
  dts89_pose37_identity(&ident);
  if(object_node>=0){
   if((dts89_u32)object_node>=globals_count)return DTS89_ERR_RANGE;
   ident=globals[object_node];
  }
  for(i=0;i<n;++i){
   dts89_fx_vec3 p,q;
   rc=dts89_mesh_get_fx_vert(mesh,i,&p);if(rc)return rc;
   dts89_pose37_point(&ident,&p,&positions[i]);
   q.x=0;q.y=0;q.z=DTS89_FX_ONE;
   if(mesh->norms_bits){rc=dts89_fx_vec3_from_bits3(mesh->norms_bits+(dts89_u32)i*3u,&q);if(rc)return rc;}
   normals[i]=l40_rotate(&ident,q);
   weight_sums[i]=DTS89_FX_ONE;
  }
 }
 for(i=0;i<n;++i){
  rc=dts89_uv_texturer_get(mesh,0,i,DTS89_UV_TORQUE_TOP_LEFT,&uvs[i]);
  if(rc==DTS89_ERR_RANGE){
   if(!mesh->tverts_bits || i>=mesh->num_tverts)return rc;
   rc=dts89_lit40_address_raw_uv(mesh->tverts_bits[(dts89_u32)i*2u],
       material->sampler_s,&uvs[i].x);
   if(rc)return rc;
   rc=dts89_lit40_address_raw_uv(mesh->tverts_bits[(dts89_u32)i*2u+1u],
       material->sampler_t,&uvs[i].y);
  }
  if(rc)return rc;
 }
 return DTS89_OK;
}
