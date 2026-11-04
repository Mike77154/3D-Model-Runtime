/* Source material bridge regression, strictly C89, no IO needed. */
#include "hl2m_master89.h"
#include <stdio.h>
#include <string.h>
static hl2m_vmt vmt;
static hl2m_material_fx89 fx;
static hl2m_fragment89 frag;
static hl2m_texture_view89 base,bump;
static hl2m_u8 basepx[16]={200,120,80,255,150,20,10,20,200,120,80,255,200,120,80,255};
static hl2m_u8 bumppx[16]={128,128,255,255,220,128,220,255,128,128,255,255,128,128,255,255};
#define ONE HL2M_FIXED_ONE
#define ENSURE(x,reason) do{if(!(x)){printf("FAIL: %s\n",reason);return 1;}}while(0)
int main(void)
{
    const char *phase1="\"VertexLitGeneric\" { \"$basetexture\" \"phase1/tyrant_head\" //\"$bumpmap\" \"ignored\"\n\"$nocull\" \"1\" }";
    const char *withbump="\"VertexLitGeneric\" { \"$basetexture\" \"tyrant01/head\" \"$bumpmap\" \"tyrant01/head_n\" \"$alphatest\" \"1\" \"$nocull\" \"1\" }";
    hl2m_vec3 n,light;hl2m_vec4 tan;char resolved[128];hl2m_u8 shaded[4],flat[4],shadow[4];int r;
    r=hl2m_master_material89((const hl2m_u8*)phase1,(unsigned int)strlen(phase1),&vmt,&fx);
    ENSURE(r==HL2M_OK,"decode phase1 VMT");
    ENSURE(!fx.normal_map && (fx.material.flags&HL2M_VMT_FLAG_NOCULL),"commented bump must remain disabled");
    ENSURE(!fx.phong && !fx.rimlight,"do not invent advanced effects not in VMT");
    r=hl2m_master_material89((const hl2m_u8*)withbump,(unsigned int)strlen(withbump),&vmt,&fx);
    ENSURE(r==HL2M_OK,"decode tyrant01 VMT");
    ENSURE(fx.normal_map && (fx.material.flags&HL2M_VMT_FLAG_ALPHATEST),"tyrant01 normal and alpha");
    base.pixels=basepx;base.width=2;base.height=2;base.repeat_u=1;base.repeat_v=1;
    bump.pixels=bumppx;bump.width=2;bump.height=2;bump.repeat_u=1;bump.repeat_v=1;
    memset(&frag,0,sizeof(frag));
    n.x=0;n.y=0;n.z=ONE;
    tan.x=ONE;tan.y=0;tan.z=0;tan.w=-ONE;
    ENSURE(hl2m_master_fragment89(&frag,&fx,n,tan,&base,&bump,0,0,0)==HL2M_OK,"first sample");
    ENSURE(frag.normal.z>ONE-600 && frag.bitangent.y < 0,"normal tangent handedness");
    ENSURE(frag.base_rgba[0]==200 && frag.have_normal_tex,"VTF RGBA bridge");
    light.x=ONE/2;light.y=0;light.z=ONE/2;
    frag.light_dir=light;frag.light_rgb[0]=255;frag.light_rgb[1]=255;frag.light_rgb[2]=255;
    frag.ambient=ONE/8;frag.light_strength=ONE*3/4;frag.base_rgba[3]=255;
    ENSURE(hl2m_master_shade89(&fx,&frag,flat)==HL2M_OK,"flat shading");
    ENSURE(hl2m_master_fragment89(&frag,&fx,n,tan,&base,&bump,0,ONE/2,0)==HL2M_OK,"second sample");
    frag.base_rgba[3]=255;
    ENSURE(hl2m_master_shade89(&fx,&frag,shaded)==HL2M_OK,"bumped shading");
    ENSURE(shaded[0]!=flat[0],"normal map should change lighting");
    frag.have_shadow=1;frag.shadow_factor=0;
    ENSURE(hl2m_master_shade89(&fx,&frag,shadow)==HL2M_OK,"shadow shading");
    ENSURE(shadow[0]<shaded[0],"shadow should reduce direct light");
    ENSURE(hl2m_master_fragment89(&frag,&fx,n,tan,&base,&bump,0,ONE/2,0)==HL2M_OK,"alpha UV sample");
    ENSURE(hl2m_master_shade89(&fx,&frag,shadow)==HL2M_OK,"alpha-test shading");
    ENSURE(shadow[3]==0,"alpha test threshold");
    ENSURE(hl2m_material_vtf_path89("models\\PLAYER\\CUSTOM/ETY1_HEAD_N",resolved,sizeof(resolved))==HL2M_OK,"material path");
    ENSURE(!strcmp(resolved,"models/player/custom/ety1_head_n.vtf"),"canonical Source path");
    ENSURE(HL2M_MASTER_ABI89==0x0109,"master abi bump");
    printf("MATERIAL_PIPELINE109 PASS phase1_comment=1 tyrant01_normal=1 alpha=1 tangent=1 bump=%u/%u shadow=%u\n",(unsigned)flat[0],(unsigned)shaded[0],(unsigned)shadow[0]);
    return 0;
}
