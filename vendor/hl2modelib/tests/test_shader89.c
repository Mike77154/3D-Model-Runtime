#include "hl2modelib.h"
#include <stdio.h>
#include <string.h>
static hl2m_vmt vmt;
static hl2m_material_fx89 fx;
static hl2m_fragment89 fragment;
int main(void)
{
    const char *source="\"VertexLitGeneric\" { \"$basetexture\" \"test\" \"$bumpmap\" \"normals\" \"$phong\" \"1\" \"$phongexponent\" \"3\" \"$rimlight\" \"1\" \"$rimlightboost\" \"0.5\" \"$alphatest\" \"1\" \"$alphatestreference\" \"0.5\" }";
    hl2m_u8 result[4],flat[4],bump[4];int r;
    r=hl2m_decode_vmt((const hl2m_u8*)source,(unsigned int)strlen(source),&vmt);if(r)return 1;
    r=hl2m_vmt_to_fx89(&vmt,&fx);if(r)return 2;
    if(!fx.normal_map || !fx.phong || !fx.rimlight || fx.alpha_test_ref!=32768)return 3;
    memset(&fragment,0,sizeof(fragment));
    fragment.normal.z=HL2M_FIXED_ONE;fragment.tangent.x=HL2M_FIXED_ONE;
    fragment.bitangent.y=HL2M_FIXED_ONE;
    fragment.light_dir.z=HL2M_FIXED_ONE;fragment.view_dir.z=HL2M_FIXED_ONE;
    fragment.half_dir.z=HL2M_FIXED_ONE;
    fragment.ambient=HL2M_FIXED_ONE/5;fragment.light_strength=HL2M_FIXED_ONE/2;
    fragment.base_rgba[0]=90;fragment.base_rgba[1]=100;fragment.base_rgba[2]=110;fragment.base_rgba[3]=255;
    fragment.light_rgb[0]=fragment.light_rgb[1]=fragment.light_rgb[2]=255;
    fragment.normal_tex[0]=128;fragment.normal_tex[1]=128;fragment.normal_tex[2]=255;
    fragment.have_normal_tex=0;
    r=hl2m_shade_fragment89(&fx,&fragment,flat);if(r)return 4;
    fragment.have_normal_tex=1;fragment.normal_tex[0]=255;fragment.normal_tex[2]=128;
    r=hl2m_shade_fragment89(&fx,&fragment,bump);if(r)return 5;
    if(flat[0]==bump[0])return 6;
    fragment.base_rgba[3]=30;
    r=hl2m_shade_fragment89(&fx,&fragment,result);if(r || result[3]!=0)return 7;
    printf("shader89 PASS phong=%d rim=%d alpha-test=%d lit=%d bumped=%d\n",fx.phong,fx.rimlight,result[3],flat[0],bump[0]);
    return 0;
}
