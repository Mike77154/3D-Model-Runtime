/* Render a native T-103 material atlas using VMT + VTF and C89 software shader.
   Four strips: original albedo, flat VertexLit, normal mapped, shadowed.
   This is a UV/material test swatch, not a full GPU model renderer. */
#include "hl2m_master89.h"
#include <stdio.h>
#include <string.h>
#define INPUT_MAX (3u*1024u*1024u)
#define RGBA_MAX (1024u*1024u*4u)
#define DIM 320
#define ONE HL2M_FIXED_ONE
static hl2m_u8 vmtbytes[32768];
static hl2m_u8 file_base[INPUT_MAX],file_normal[INPUT_MAX];
static hl2m_u8 rgba_base[RGBA_MAX],rgba_normal[RGBA_MAX];
static hl2m_u8 ppm[4u*DIM*DIM*3u];
static hl2m_vmt vmt;
static hl2m_material_fx89 fx;
static hl2m_fragment89 frag;
static hl2m_vtf_info baseinfo,ninfo;
static hl2m_texture_view89 tex_base,tex_normal;
static int load(const char *name,hl2m_u8 *out,unsigned int cap,unsigned int *size)
{
    FILE *f=fopen(name,"rb");size_t n;
    if(!f)return 1;
    n=fread(out,1u,cap,f);
    if(!feof(f)){fclose(f);return 2;}
    fclose(f);*size=(unsigned int)n;return 0;
}
int main(int argc,char **argv)
{
    unsigned int vsize,bsize,nsize;int repaired=0,r,x,y,k,panel;FILE *out;
    hl2m_u8 result[4],basecolor[4];hl2m_vec3 nor;hl2m_vec4 tangent;
    if(argc!=5){fputs("usage: preview head.vmt base.vtf normal.vtf preview.ppm\n",stderr);return 1;}
    if(load(argv[1],vmtbytes,sizeof(vmtbytes),&vsize))return 2;
    if(load(argv[2],file_base,sizeof(file_base),&bsize))return 3;
    if(load(argv[3],file_normal,sizeof(file_normal),&nsize))return 4;
    if(hl2m_repair_missing_first_byte(vmtbytes,&vsize,sizeof(vmtbytes),HL2M_FORMAT_VMT,&repaired))return 5;
    r=hl2m_master_material89(vmtbytes,vsize,&vmt,&fx);if(r)return 6;
    if(!fx.normal_map){fputs("VMT does not request a normal map\n",stderr);return 7;}
    repaired=0;
    if(hl2m_repair_missing_first_byte(file_base,&bsize,INPUT_MAX,HL2M_FORMAT_VTF,&repaired))return 8;
    if(hl2m_repair_missing_first_byte(file_normal,&nsize,INPUT_MAX,HL2M_FORMAT_VTF,&repaired))return 9;
    if(hl2m_master_texture89(file_base,bsize,rgba_base,RGBA_MAX,&baseinfo))return 10;
    if(hl2m_master_texture89(file_normal,nsize,rgba_normal,RGBA_MAX,&ninfo))return 11;
    tex_base.pixels=rgba_base;tex_base.width=baseinfo.width;tex_base.height=baseinfo.height;
    tex_base.repeat_u=1;tex_base.repeat_v=1;
    tex_normal.pixels=rgba_normal;tex_normal.width=ninfo.width;tex_normal.height=ninfo.height;
    tex_normal.repeat_u=1;tex_normal.repeat_v=1;
    nor.x=0;nor.y=0;nor.z=ONE;
    tangent.x=ONE;tangent.y=0;tangent.z=0;tangent.w=ONE;
    memset(&frag,0,sizeof(frag));
    frag.ambient=ONE/4;frag.light_strength=ONE*3/4;
    frag.light_rgb[0]=255;frag.light_rgb[1]=255;frag.light_rgb[2]=255;
    frag.view_dir.z=ONE;frag.half_dir.x=ONE/4;frag.half_dir.z=ONE*3/4;
    frag.light_dir.x=ONE/2;frag.light_dir.y=ONE/3;frag.light_dir.z=ONE*3/4;
    for(y=0;y<DIM;y++)for(x=0;x<DIM;x++){
        hl2m_fixed u=(hl2m_fixed)((unsigned int)x*ONE/DIM);
        hl2m_fixed v=(hl2m_fixed)((unsigned int)y*ONE/DIM);
        r=hl2m_master_fragment89(&frag,&fx,nor,tangent,&tex_base,&tex_normal,0,u,v);
        if(r)return 12;
        memcpy(basecolor,frag.base_rgba,4u);
        for(panel=0;panel<4;panel++){
            if(panel==0){memcpy(result,basecolor,4u);}
            else{
                frag.have_normal_tex=(panel>=2);
                frag.have_shadow=(panel==3);
                frag.shadow_factor=ONE/5;
                r=hl2m_master_shade89(&fx,&frag,result);
                if(r)return 13;
            }
            for(k=0;k<3;k++)ppm[((unsigned int)y*(4u*DIM)+panel*DIM+x)*3u+(unsigned int)k]=result[k];
        }
    }
    out=fopen(argv[4],"wb");if(!out)return 14;
    fprintf(out,"P6\n%d %d\n255\n",DIM*4,DIM);
    if(fwrite(ppm,1u,sizeof(ppm),out)!=sizeof(ppm)){fclose(out);return 15;}
    fclose(out);
    printf("T103_REAL_MATERIAL PASS shader=%s base=%ux%u bump=%ux%u normal=%d alpha_test=%d phong=%d rim=%d\n",
      fx.material.shader,baseinfo.width,baseinfo.height,ninfo.width,ninfo.height,fx.normal_map,
      (fx.material.flags&HL2M_VMT_FLAG_ALPHATEST)!=0,fx.phong,fx.rimlight);
    return 0;
}
