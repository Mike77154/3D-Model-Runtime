#include "dts89_shaderer.h"
#include <stdio.h>
#include <string.h>
static int failures=0;
static void chk(int condition,const char *label)
{
 if(!condition){fprintf(stderr,"FAIL: %s\n",label);failures++;}
}
static dts89_string str(char *x)
{
 dts89_string s;s.ptr=x;s.len=(dts89_u32)strlen(x);return s;
}
static void test_uv(void)
{
 dts89_shaderer_material m;
 dts89_fx_vec2 in,out;
 dts89_u8 pix[16]={255,0,0,255,0,255,0,255,0,0,255,255,255,255,0,255};
 dts89_shaderer_rgba8 c;
 memset(&m,0,sizeof(m));m.sampler_s=DTS89_SHADERER_WRAP;m.sampler_t=DTS89_SHADERER_CLAMP;
 in.x=-16384;in.y=131072;
 chk(dts89_shaderer_address_uv(in,&m,&out)==DTS89_OK,"address rc");
 chk(out.x==49152 && out.y==65536,"wrap negative U clamp V");
 in.x=65536;in.y=0;
 chk(dts89_shaderer_sample_rgba8(pix,2,2,8,in,&m,&c)==DTS89_OK,"sample rc");
 chk(c.r==255 && c.g==0,"sample wrap u1 => u0");
 m.sampler_s=DTS89_SHADERER_CLAMP;
 chk(dts89_shaderer_sample_rgba8(pix,2,2,8,in,&m,&c)==DTS89_OK,"clamp sample rc");
 chk(c.g==255 && c.r==0,"sample clamp u1 => right");
 in.x=(-2147483647-1);in.y=2147483647;
 m.sampler_s=DTS89_SHADERER_WRAP;m.sampler_t=DTS89_SHADERER_WRAP;
 chk(dts89_shaderer_address_uv(in,&m,&out)==DTS89_OK,"extreme uv no overflow");
 chk(out.x==0 && out.y==65535,"extreme modulo remainder");
}
static void test_blend(void)
{
 dts89_shaderer_rgba8 a,b,r;
 a.r=200;a.g=100;a.b=0;a.a=128;
 b.r=0;b.g=20;b.b=200;b.a=255;
 chk(!dts89_shaderer_blend_rgba8(a,b,DTS89_SHADERER_BLEND_ALPHA,&r),"alpha blend");
 chk(r.r==100 && r.g>=59 && r.g<=61 && r.b>=99 && r.b<=101,"alpha blend values");
 chk(!dts89_shaderer_blend_rgba8(a,b,DTS89_SHADERER_BLEND_ADD,&r),"add blend");
 chk(r.r==200 && r.g==120 && r.b==200,"add values");
 chk(!dts89_shaderer_blend_rgba8(a,b,DTS89_SHADERER_BLEND_SUB,&r),"sub blend");
 chk(r.r==0 && r.g==0 && r.b==200,"sub values");
 chk(dts89_shaderer_blend_rgba8(a,b,DTS89_SHADERER_BLEND_TONE,&r)==DTS89_ERR_UNSUPPORTED,"tone unsupported explicit");
}
static void test_packed_normals(void)
{
 dts89_u8 image[8u*8u*4u];dts89_u32 i,mode;
 dts89_shaderer_rgba8 c;dts89_fx_vec3 v;
 for(i=0u;i<64u;++i){image[i*4u]=255u;image[i*4u+1u]=128u;
  image[i*4u+2u]=0u;image[i*4u+3u]=127u;}
 chk(!dts89_shaderer_detect_normal_packing(image,8u,8u,32u,256u,&mode),"normal pack detect rc");
 chk(mode==DTS89_SHADERER_NORMAL_DXT5_AG,"DXT5nm detection");
 c.r=255u;c.g=128u;c.b=0u;c.a=128u;
 chk(!dts89_shaderer_unpack_normal(c,mode,&v),"DXT5nm unpack rc");
 chk(v.x==0 && v.y==0 && v.z==65536,"DXT5nm unpack flat");
 c.r=128u;c.g=128u;c.b=255u;c.a=255u;
 chk(!dts89_shaderer_unpack_normal(c,DTS89_SHADERER_NORMAL_RGB,&v),"RGB normal decode");
 chk(v.x==0 && v.y==0 && v.z==65024,"RGB normal z");
}
static void test_material(void)
{
 dts89_shape shape;dts89_material mat;dts89_shaderer_material m;
 dts89_ts_file file;dts89_ts_object obj;dts89_ts_assignment a[6];
 char n[]="skin",cl[]="Material",on[]="MySkin";
 char k0[]="mapTo",k1[]="diffuseMap",k2[]="normalMap",k3[]="specularMap";
 char k4[]="translucent",k5[]="translucentBlendOp";
 char v0[]="skin",v1[]="tex.dds",v2[]="normal.dds",v3[]="spec.dds";
 char v4[]="true",v5[]="AddAlpha";
 dts89_s32 i;
 memset(&shape,0,sizeof(shape));memset(&mat,0,sizeof(mat));
 mat.name=str(n);mat.flags=DTS89_MATF_S_WRAP;
 mat.reflectance_map=0xFFFFFFFFu;mat.bump_map=0xFFFFFFFFu;mat.detail_map=0xFFFFFFFFu;
 mat.detail_scale_bits=DTS89_F32_BITS_ONE;mat.reflection_amount_bits=0u;
 shape.num_materials=1;shape.materials=&mat;
 chk(dts89_shaderer_from_dts(&shape,0,&m)==DTS89_OK,"material base");
 chk(m.sampler_s==1u && m.sampler_t==0u,"dts wrap axis independent");
 memset(a,0,sizeof(a));
 a[0].key=str(k0);a[0].value=str(v0);
 a[1].key=str(k1);a[1].value=str(v1);
 a[2].key=str(k2);a[2].value=str(v2);
 a[3].key=str(k3);a[3].value=str(v3);
 a[4].key=str(k4);a[4].value=str(v4);
 a[5].key=str(k5);a[5].value=str(v5);
 for(i=0;i<6;i++)a[i].array_index=-1;
 obj.class_name=str(cl);obj.object_name=str(on);obj.num_assignments=6;obj.assignments=a;
 file.num_objects=1;file.objects=&obj;
 chk(dts89_shaderer_apply_script(&m,&file)==DTS89_OK,"script bind mapTo");
 chk(m.script_matched && m.translucent && m.blend==DTS89_SHADERER_BLEND_ADD_ALPHA,"blend from script");
 chk((m.stage[0].maps&(DTS89_SHADERER_MAP_NORMAL|DTS89_SHADERER_MAP_SPECULAR))==
 (DTS89_SHADERER_MAP_NORMAL|DTS89_SHADERER_MAP_SPECULAR),"normal specular stages");
}
int main(void)
{
 test_uv();test_blend();test_material();test_packed_normals();
 if(failures){fprintf(stderr,"shaderer test failures=%d\n",failures);return 1;}
 puts("shaderer39 selftest ok");return 0;
}
