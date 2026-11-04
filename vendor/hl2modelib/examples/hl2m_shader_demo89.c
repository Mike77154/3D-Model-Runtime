/* Shader fragment reference/demo: generated gradient panels, strict C89.
   Not a render of a 3D character or GPU shader parity test. */
#include "hl2modelib.h"
#include <stdio.h>
#include <string.h>
#define W 600
#define H 240
static hl2m_fragment89 frag;
static hl2m_material_fx89 fx;
static unsigned char pixels[W*H*3];
int main(int argc,char **argv)
{
    FILE *out; int x,y,i,p,r,localx;hl2m_u8 rgba[4];
    if(argc!=2){fputs("usage: hl2m_shader_demo89 output.ppm\n",stderr);return 1;}
    memset(&frag,0,sizeof(frag));memset(&fx,0,sizeof(fx));
    fx.material.flags=HL2M_VMT_FLAG_VERTEXLIT;
    fx.phong_exponent=4*HL2M_FIXED_ONE;
    fx.phong_boost=HL2M_FIXED_ONE/3;
    fx.rim_exponent=2*HL2M_FIXED_ONE;
    fx.rim_boost=HL2M_FIXED_ONE/4;
    fx.alpha=HL2M_FIXED_ONE;
    frag.normal.z=HL2M_FIXED_ONE;
    frag.tangent.x=HL2M_FIXED_ONE;
    frag.bitangent.y=HL2M_FIXED_ONE;
    frag.light_dir.x=HL2M_FIXED_ONE/3;
    frag.light_dir.y=HL2M_FIXED_ONE/4;
    frag.light_dir.z=HL2M_FIXED_ONE*3/4;
    frag.view_dir.z=HL2M_FIXED_ONE;
    frag.half_dir.x=HL2M_FIXED_ONE/6;
    frag.half_dir.z=HL2M_FIXED_ONE*7/8;
    frag.ambient=HL2M_FIXED_ONE/8;
    frag.light_strength=HL2M_FIXED_ONE*3/4;
    frag.base_rgba[0]=109;frag.base_rgba[1]=120;frag.base_rgba[2]=130;frag.base_rgba[3]=255;
    frag.light_rgb[0]=255;frag.light_rgb[1]=248;frag.light_rgb[2]=232;
    frag.have_normal_tex=1;
    for(y=0;y<H;++y)for(x=0;x<W;++x){
        p=x/200;localx=x%200;
        fx.normal_map=p>0;
        fx.phong=p==2;
        fx.rimlight=p==2;
        frag.normal_tex[0]=(hl2m_u8)(80+(localx*175)/199);
        frag.normal_tex[1]=(hl2m_u8)(80+(y*175)/(H-1));
        frag.normal_tex[2]=255;
        if((localx<3)||(y<3)||(localx>196)||(y>236)){
            rgba[0]=rgba[1]=rgba[2]=32;
        }else{
            r=hl2m_shade_fragment89(&fx,&frag,rgba);if(r)return 2;
        }
        i=(y*W+x)*3;
        pixels[i]=rgba[0];pixels[i+1]=rgba[1];pixels[i+2]=rgba[2];
    }
    out=fopen(argv[1],"wb");if(!out)return 3;
    fprintf(out,"P6\n%d %d\n255\n",W,H);
    if(fwrite(pixels,1u,sizeof(pixels),out)!=sizeof(pixels)){fclose(out);return 4;}
    fclose(out);puts("shader demo: three panels generated with hl2m_shade_fragment89");return 0;
}
