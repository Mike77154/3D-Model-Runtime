#include "dts89_lighting40.h"
#include <stdio.h>
#include <string.h>
#define ONE DTS89_FX_ONE
static dts89_fx_vec3 vec(dts89_s32 x,dts89_s32 y,dts89_s32 z)
{ dts89_fx_vec3 a;a.x=x;a.y=y;a.z=z;return a; }
static dts89_fx_vec2 uv(dts89_s32 x,dts89_s32 y)
{ dts89_fx_vec2 a;a.x=x;a.y=y;return a; }
#define EXPECT(x) do {if(!(x)){printf("failure line %d\n",__LINE__);return 1;}}while(0)
int main(void)
{
 dts89_fx_vec3 p[3],n[3];dts89_fx_vec2 tc[3];
 dts89_lit40_basis b;dts89_lit40_light l;dts89_lit40_maps maps;
 dts89_shaderer_material m;dts89_shaderer_rgba8 a,dark,normal_lit;
 dts89_lit40_image diffuse,normap,specmap;
 dts89_u8 dp[4]={160,80,40,255};
 dts89_u8 np[4]={128,128,255,255};
 dts89_u8 sp[4]={255,255,255,255};
 int rc;unsigned int i;
 p[0]=vec(0,0,0);p[1]=vec(ONE,0,0);p[2]=vec(0,ONE,0);
 tc[0]=uv(0,0);tc[1]=uv(ONE,0);tc[2]=uv(0,ONE);
 for(i=0;i<3u;++i)n[i]=vec(0,0,ONE);
 EXPECT(dts89_lit40_triangle_basis(p,tc,n,&b)==DTS89_OK);
 EXPECT(b.t.x>65000 && b.t.y<100 && b.b.y>65000);
 memset(&m,0,sizeof(m));m.sampler_s=m.sampler_t=DTS89_SHADERER_CLAMP;
 m.stage[0].specular_power=8*ONE;
 m.stage[0].diffuse_color[0]=m.stage[0].diffuse_color[1]=m.stage[0].diffuse_color[2]=m.stage[0].diffuse_color[3]=ONE;
 m.stage[0].specular_color[0]=m.stage[0].specular_color[1]=m.stage[0].specular_color[2]=ONE;
 diffuse.rgba=dp;diffuse.width=diffuse.height=1u;diffuse.stride=4u;
 normap.rgba=np;normap.width=normap.height=1u;normap.stride=4u;
 specmap.rgba=sp;specmap.width=specmap.height=1u;specmap.stride=4u;
 maps.diffuse=&diffuse;maps.normal=0;maps.specular=0;maps.normal_packing=DTS89_SHADERER_NORMAL_RGB;
 l.to_light=vec(0,0,ONE);l.to_view=vec(0,0,ONE);
 l.ambient=ONE/5;l.diffuse=ONE*3/4;l.specular=0;
 rc=dts89_lit40_shade(tc[0],&b,&m,&maps,&l,&a);
 EXPECT(rc==DTS89_OK);EXPECT(a.r>140 && a.g>70 && a.b>35);
 l.to_light=vec(ONE,0,0);
 rc=dts89_lit40_shade(tc[0],&b,&m,&maps,&l,&dark);
 EXPECT(rc==DTS89_OK);EXPECT(dark.r<a.r/2 && dark.g<a.g/2);
 maps.normal=&normap;l.to_light=vec(0,0,ONE);
 rc=dts89_lit40_shade(tc[0],&b,&m,&maps,&l,&normal_lit);
 EXPECT(rc==DTS89_OK);EXPECT(normal_lit.r>140);
 np[0]=255;np[1]=128;np[2]=128;
 l.to_light=vec(ONE,0,0);
 rc=dts89_lit40_shade(tc[0],&b,&m,&maps,&l,&normal_lit);
 EXPECT(rc==DTS89_OK);EXPECT(normal_lit.r>dark.r*2);
 maps.normal=0;maps.specular=&specmap;l.to_light=vec(0,0,ONE);
 l.specular=ONE/2;
 rc=dts89_lit40_shade(tc[0],&b,&m,&maps,&l,&normal_lit);
 EXPECT(rc==DTS89_OK);EXPECT(normal_lit.r>a.r);
 m.alpha_test=1u;m.alpha_ref=100u;dp[3]=20u;
 rc=dts89_lit40_shade(tc[0],&b,&m,&maps,&l,&normal_lit);
 EXPECT(rc==DTS89_OK);EXPECT(normal_lit.a==0u);
 tc[1]=tc[0];tc[2]=tc[0];
 EXPECT(dts89_lit40_triangle_basis(p,tc,n,&b)==DTS89_OK);
 EXPECT(b.t.x!=0 || b.t.y!=0 || b.t.z!=0);
 {
  dts89_fx16_16 t;
  EXPECT(dts89_lit40_address_raw_uv(0x3F800000u,DTS89_SHADERER_WRAP,&t)==DTS89_OK);
  EXPECT(t==0);
  EXPECT(dts89_lit40_address_raw_uv(0x3FC00000u,DTS89_SHADERER_WRAP,&t)==DTS89_OK);
  EXPECT(t==ONE/2);
  EXPECT(dts89_lit40_address_raw_uv(0xBFA00000u,DTS89_SHADERER_WRAP,&t)==DTS89_OK);
  EXPECT(t==ONE*3/4);
  EXPECT(dts89_lit40_address_raw_uv(0x7F800000u,DTS89_SHADERER_WRAP,&t)==DTS89_ERR_FORMAT);
 }
 puts("lighting40 selftest ok (tangent, light, normal, specular, alpha, degenerate UV)");
 return 0;
}
