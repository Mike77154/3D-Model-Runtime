/* Renderer-neutral Source VMT shading features, C89 fixed 16.16.
   This supplies software fragment shading/parameters; not a Valve GPU shader. */
#include "hl2modelib.h"
#include <string.h>

#define F89_ONE HL2M_FIXED_ONE
static hl2m_fixed f89_clamp(hl2m_fixed a)
{if(a<0)return 0;if(a>F89_ONE)return F89_ONE;return a;}
static hl2m_fixed f89_parse(const char *s,hl2m_fixed fallback)
{
    int sign=1,nd=0;unsigned int whole=0,frac=0,place=10u;
    if(!s || !*s)return fallback;
    while(*s==' ' || *s=='\t')s++;
    if(*s=='-'){sign=-1;s++;}else if(*s=='+')s++;
    while(*s>='0' && *s<='9'){
        nd=1;if(whole<32000u)whole=whole*10u+(unsigned int)(*s-'0');s++;
    }
    if(*s=='.'){
        s++;while(*s>='0' && *s<='9'){
            nd=1;if(place<=100000u){frac+=((unsigned int)(*s-'0')*65536u)/place;place*=10u;}s++;
        }
    }
    if(!nd)return fallback;
    if(whole>=32767u)return sign<0?-2147483647:2147483647;
    return sign*(hl2m_fixed)(whole*65536u+frac);
}
static void f89_get(const hl2m_vmt *vmt,const char *key,char *out,unsigned int size)
{out[0]=0;(void)hl2m_vmt_find_param(vmt,key,out,size);}
static hl2m_fixed f89_value(const hl2m_vmt *vmt,const char *key,hl2m_fixed def)
{
    char value[HL2M_VALUE_MAX];f89_get(vmt,key,value,sizeof(value));return f89_parse(value,def);
}
static int f89_bool(const hl2m_vmt *vmt,const char *key)
{return f89_value(vmt,key,0)>0;}
int hl2m_vmt_to_fx89(const hl2m_vmt *vmt,hl2m_material_fx89 *fx)
{
    int r;
    if(!vmt || !fx)return HL2M_ERR_NULL;
    memset(fx,0,sizeof(*fx));
    r=hl2m_vmt_to_material_desc(vmt,&fx->material);if(r)return r;
    fx->phong=f89_bool(vmt,"$phong");
    fx->half_lambert=f89_bool(vmt,"$halflambert");
    fx->rimlight=f89_bool(vmt,"$rimlight");
    fx->normal_map=fx->material.bumpmap[0]!=0;
    fx->detail_map=fx->material.detail[0]!=0;
    fx->environment=fx->material.envmap[0]!=0;
    fx->normal_dxt5nm=0; /* Caller selects DXT5nm/swizzled normal sampling when appropriate. */
    fx->phong_boost=f89_value(vmt,"$phongboost",F89_ONE);
    fx->phong_exponent=f89_value(vmt,"$phongexponent",16*F89_ONE);
    fx->rim_boost=f89_value(vmt,"$rimlightboost",F89_ONE);
    fx->rim_exponent=f89_value(vmt,"$rimlightexponent",4*F89_ONE);
    fx->detail_scale=f89_value(vmt,"$detailscale",4*F89_ONE);
    fx->detail_blend=f89_clamp(f89_value(vmt,"$detailblendfactor",F89_ONE));
    fx->detail_mode=(int)(f89_value(vmt,"$detailblendmode",0)/F89_ONE);
    fx->alpha_test_ref=f89_clamp(f89_value(vmt,"$alphatestreference",F89_ONE/2));
    fx->alpha=f89_clamp(f89_value(vmt,"$alpha",F89_ONE));
    fx->env_contrast=f89_value(vmt,"$envmapcontrast",F89_ONE);
    fx->env_saturation=f89_value(vmt,"$envmapsaturation",F89_ONE);
    fx->env_strength=f89_clamp(f89_value(vmt,"$envmapstrength",F89_ONE/4));
    f89_get(vmt,"$phongexponenttexture",fx->phong_exponent_texture,HL2M_PATH_MAX);
    f89_get(vmt,"$selfillumtint",fx->selfillum_tint,sizeof(fx->selfillum_tint));
    return HL2M_OK;
}
