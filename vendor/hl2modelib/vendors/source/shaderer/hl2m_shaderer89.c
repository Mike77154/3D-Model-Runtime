/* Source software shaderer; fixed-point per-fragment lighting.
   Tangent basis / texture sampling supplied by renderer. Not Valve GPU parity. */
#include "hl2modelib.h"
#define F89_ONE HL2M_FIXED_ONE
static hl2m_fixed f89_mul(hl2m_fixed a,hl2m_fixed b)
{
    unsigned int x,y,r;int neg=0;
    if(a<0){x=(unsigned int)(-(a+1))+1u;neg=!neg;}else x=(unsigned int)a;
    if(b<0){y=(unsigned int)(-(b+1))+1u;neg=!neg;}else y=(unsigned int)b;
    r=(x>>16)*(y>>16)*65536u+(x>>16)*(y&65535u)+(y>>16)*(x&65535u);
    r+=((x&65535u)*(y&65535u))>>16;
    return neg?-(hl2m_fixed)r:(hl2m_fixed)r;
}
static hl2m_fixed f89_clamp(hl2m_fixed a)
{if(a<0)return 0;if(a>F89_ONE)return F89_ONE;return a;}
static int f89_u8(int a)
{if(a<0)return 0;if(a>255)return 255;return a;}
static hl2m_fixed f89_dot(hl2m_vec3 a,hl2m_vec3 b)
{ return f89_mul(a.x,b.x)+f89_mul(a.y,b.y)+f89_mul(a.z,b.z); }

/* Q16 square root via unsigned 32-bit integer only; about 1/256 precision. */
static unsigned int f89_isqrt(unsigned int x)
{
    unsigned int bit=1u<<30,root=0;
    while(bit>x)bit>>=2;
    while(bit){
        if(x>=root+bit){x-=root+bit;root=(root>>1)+bit;}
        else root>>=1;
        bit>>=2;
    }
    return root;
}
static hl2m_fixed f89_div(hl2m_fixed x,hl2m_fixed y)
{
    int d;hl2m_fixed whole,rem;
    if(y<256 && y>-256)return 0;
    whole=(x/y)*F89_ONE;rem=x%y;d=y/256;
    return whole+((rem*256)/d);
}
static hl2m_vec3 f89_normalize(hl2m_vec3 n,hl2m_vec3 fallback)
{
    hl2m_fixed len2=f89_mul(n.x,n.x)+f89_mul(n.y,n.y)+f89_mul(n.z,n.z);
    hl2m_fixed len;
    if(len2<1)return fallback;
    len=(hl2m_fixed)(f89_isqrt((unsigned int)len2)*256u);
    if(len<256)return fallback;
    n.x=f89_div(n.x,len);n.y=f89_div(n.y,len);n.z=f89_div(n.z,len);
    return n;
}
/* Source bump: RGB normal texture or DXT5nm (X stored alpha, Y green).
   Bump values are tangent-space; geometry tangent handedness comes from VVD. */
static hl2m_vec3 f89_normal(const hl2m_fragment89 *frag,const hl2m_material_fx89 *fx)
{
    hl2m_vec3 n=frag->normal,ns;hl2m_fixed x,y,z,zz;
    if(fx->normal_map && frag->have_normal_tex){
        int a=fx->normal_dxt5nm?3:0;
        x=((int)frag->normal_tex[a]-128)*512;
        y=((int)frag->normal_tex[1]-128)*512;
        if(fx->normal_dxt5nm){
            zz=F89_ONE-f89_mul(x,x)-f89_mul(y,y);
            if(zz<0)zz=0;
            z=(hl2m_fixed)(f89_isqrt((unsigned int)zz)*256u);
        }else z=((int)frag->normal_tex[2]-128)*512;
        ns.x=f89_mul(frag->tangent.x,x)+f89_mul(frag->bitangent.x,y)+f89_mul(frag->normal.x,z);
        ns.y=f89_mul(frag->tangent.y,x)+f89_mul(frag->bitangent.y,y)+f89_mul(frag->normal.y,z);
        ns.z=f89_mul(frag->tangent.z,x)+f89_mul(frag->bitangent.z,y)+f89_mul(frag->normal.z,z);
        n=f89_normalize(ns,frag->normal);
    }
    return n;
}
static hl2m_fixed f89_pow(hl2m_fixed val,int exp)
{
    hl2m_fixed v=f89_clamp(val),r=F89_ONE;int i;
    if(exp>128)exp=128;
    if(exp<1)exp=1;
    for(i=0;i<exp;i++)r=f89_mul(r,v);
    return r;
}
int hl2m_shade_fragment89(const hl2m_material_fx89 *fx,const hl2m_fragment89 *frag,hl2m_u8 out_rgba[4])
{
    hl2m_vec3 n;hl2m_fixed diffuse,spec=0,rim=0;int ch;
    if(!fx || !frag || !out_rgba)return HL2M_ERR_NULL;
    n=f89_normal(frag,fx);
    diffuse=f89_dot(n,frag->light_dir);
    if(fx->half_lambert){
        diffuse=f89_clamp((diffuse+F89_ONE)/2);
        diffuse=f89_mul(diffuse,diffuse);
    }else diffuse=f89_clamp(diffuse);
    if(fx->phong){
        spec=f89_pow(f89_clamp(f89_dot(n,frag->half_dir)),fx->phong_exponent/F89_ONE);
        spec=f89_mul(spec,fx->phong_boost);
    }
    if(frag->have_shadow){
        diffuse=f89_mul(diffuse,f89_clamp(frag->shadow_factor));
        spec=f89_mul(spec,f89_clamp(frag->shadow_factor));
    }
    if(fx->rimlight){
        rim=f89_pow(F89_ONE-f89_clamp(f89_dot(n,frag->view_dir)),fx->rim_exponent/F89_ONE);
        rim=f89_mul(rim,fx->rim_boost);
    }
    for(ch=0;ch<3;ch++){
        int base=frag->base_rgba[ch],light,shaded;
        hl2m_fixed intensity;
        if(fx->detail_map && frag->have_detail_tex){
            int detail=frag->detail_rgba[ch],blended;
            if(fx->detail_mode==1)blended=f89_u8(base+detail-128);
            else blended=f89_u8(base*detail/128);
            base=(base*(F89_ONE-fx->detail_blend)+blended*fx->detail_blend)/F89_ONE;
        }
        light=(int)frag->light_rgb[ch];
        intensity=(fx->material.flags&HL2M_VMT_FLAG_UNLIT)? F89_ONE : (frag->ambient+f89_mul(diffuse,frag->light_strength));
        if(fx->material.flags&HL2M_VMT_FLAG_SELFILLUM)intensity=F89_ONE;
        intensity=f89_clamp(intensity);
        shaded=base*intensity/F89_ONE;
        if(fx->material.flags&HL2M_VMT_FLAG_VERTEXLIT)shaded=shaded*light/255;
        if(fx->phong)shaded+=light*f89_clamp(spec)/F89_ONE;
        if(fx->rimlight)shaded+=light*f89_clamp(rim)/F89_ONE;
        if(fx->environment && frag->have_env_tex)shaded+=frag->env_rgba[ch]*fx->env_strength/F89_ONE;
        out_rgba[ch]=(hl2m_u8)f89_u8(shaded);
    }
    out_rgba[3]=(hl2m_u8)((int)frag->base_rgba[3]*fx->alpha/F89_ONE);
    if((fx->material.flags&HL2M_VMT_FLAG_ALPHATEST) && (int)out_rgba[3]*F89_ONE/255<fx->alpha_test_ref)out_rgba[3]=0;
    return HL2M_OK;
}
