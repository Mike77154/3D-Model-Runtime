/* Source material samples for renderer-neutral fixed point presenter.
   C89, 32-bit arithmetic, no malloc, no float, no IO.
   Texture RGBA buffers are provided and owned by caller. */
#include "hl2modelib.h"
#include <string.h>

static hl2m_fixed fx_mul89(hl2m_fixed a,hl2m_fixed b)
{
    unsigned int x,y,r;int neg=0;
    if(a<0){x=(unsigned int)(-(a+1))+1u;neg=!neg;}else x=(unsigned int)a;
    if(b<0){y=(unsigned int)(-(b+1))+1u;neg=!neg;}else y=(unsigned int)b;
    r=(x>>16)*(y>>16)*65536u+(x>>16)*(y&65535u)+(y>>16)*(x&65535u);
    r+=((x&65535u)*(y&65535u))>>16;
    return neg?-(hl2m_fixed)r:(hl2m_fixed)r;
}
static hl2m_vec3 fx_cross89(hl2m_vec3 a,hl2m_vec3 b)
{
    hl2m_vec3 c;
    c.x=fx_mul89(a.y,b.z)-fx_mul89(a.z,b.y);
    c.y=fx_mul89(a.z,b.x)-fx_mul89(a.x,b.z);
    c.z=fx_mul89(a.x,b.y)-fx_mul89(a.y,b.x);
    return c;
}
static unsigned int fx_sqrt_u32(unsigned int x)
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
static hl2m_fixed fx_div89(hl2m_fixed a,hl2m_fixed b)
{
    hl2m_fixed q,r;int d;
    if(b<256 && b>-256)return 0;
    q=(a/b)*HL2M_FIXED_ONE;
    r=a%b;d=b/256;
    if(!d)return q;
    return q+(r*256)/d;
}
static hl2m_vec3 fx_normalize89(hl2m_vec3 a,hl2m_vec3 fallback)
{
    hl2m_fixed len2=fx_mul89(a.x,a.x)+fx_mul89(a.y,a.y)+fx_mul89(a.z,a.z);
    hl2m_fixed length;
    if(len2<=0)return fallback;
    length=(hl2m_fixed)(fx_sqrt_u32((unsigned int)len2)*256u);
    if(length<256)return fallback;
    a.x=fx_div89(a.x,length);
    a.y=fx_div89(a.y,length);
    a.z=fx_div89(a.z,length);
    return a;
}
/* VVD encodes tangent handedness in tangent.w: B = sign * cross(N,T). */
int hl2m_fragment_tangent_basis89(hl2m_fragment89 *frag,hl2m_vec3 normal,hl2m_vec4 tangent)
{
    hl2m_vec3 t,b,n,flat,axis,by;hl2m_fixed parallel;
    if(!frag)return HL2M_ERR_NULL;
    flat.x=0;flat.y=0;flat.z=HL2M_FIXED_ONE;
    axis.x=HL2M_FIXED_ONE;axis.y=0;axis.z=0;
    by.x=0;by.y=HL2M_FIXED_ONE;by.z=0;
    n=fx_normalize89(normal,flat);
    t.x=tangent.x;t.y=tangent.y;t.z=tangent.z;
    parallel=fx_mul89(n.x,t.x)+fx_mul89(n.y,t.y)+fx_mul89(n.z,t.z);
    t.x-=fx_mul89(n.x,parallel);t.y-=fx_mul89(n.y,parallel);t.z-=fx_mul89(n.z,parallel);
    t=fx_normalize89(t,axis);
    b=fx_cross89(n,t);
    if(tangent.w<0){b.x=-b.x;b.y=-b.y;b.z=-b.z;}
    frag->normal=n;frag->tangent=t;frag->bitangent=fx_normalize89(b,by);
    return HL2M_OK;
}
/* RGBA8 image sample; UV in signed Q16.16; repeat or clamp at boundary.
   Deliberately nearest neighbour; GPU bilinear/trilinear selection remains presenter-owned. */
int hl2m_texture_sample89(const hl2m_texture_view89 *t,hl2m_fixed u,hl2m_fixed v,hl2m_u8 rgba[4])
{
    unsigned int tx,ty,off;int iu,iv;
    if(!t || !rgba || !t->pixels)return HL2M_ERR_NULL;
    if(!t->width || !t->height || t->width>8192u || t->height>8192u ||
       t->width>0xFFFFFFFFu/t->height/4u)return HL2M_ERR_RANGE;
    iu=u%HL2M_FIXED_ONE;iv=v%HL2M_FIXED_ONE;
    if(t->repeat_u){if(iu<0)iu+=HL2M_FIXED_ONE;}
    else {if(u<=0)iu=0;else if(u>=HL2M_FIXED_ONE)iu=HL2M_FIXED_ONE-1;}
    if(t->repeat_v){if(iv<0)iv+=HL2M_FIXED_ONE;}
    else {if(v<=0)iv=0;else if(v>=HL2M_FIXED_ONE)iv=HL2M_FIXED_ONE-1;}
    tx=(unsigned int)iu*t->width/65536u;
    ty=(unsigned int)iv*t->height/65536u;
    if(tx>=t->width)tx=t->width-1u;
    if(ty>=t->height)ty=t->height-1u;
    off=(ty*t->width+tx)*4u;
    memcpy(rgba,t->pixels+off,4u);
    return HL2M_OK;
}
/* One call connects decoded VTF RGBA and Source VMT feature flags to fragment.
   Missing maps do not activate a feature: shader will use vertex normal/base rgba. */
int hl2m_fragment_sample_maps89(hl2m_fragment89 *frag,const hl2m_material_fx89 *fx,
    const hl2m_texture_view89 *base,const hl2m_texture_view89 *bump,
    const hl2m_texture_view89 *detail,hl2m_fixed u,hl2m_fixed v)
{
    int r;hl2m_fixed du,dv;
    if(!frag || !fx)return HL2M_ERR_NULL;
    frag->have_normal_tex=0;frag->have_detail_tex=0;
    if(base){r=hl2m_texture_sample89(base,u,v,frag->base_rgba);if(r)return r;}
    if(fx->normal_map && bump){r=hl2m_texture_sample89(bump,u,v,frag->normal_tex);if(r)return r;frag->have_normal_tex=1;}
    if(fx->detail_map && detail){
        du=fx_mul89(u,fx->detail_scale);dv=fx_mul89(v,fx->detail_scale);
        r=hl2m_texture_sample89(detail,du,dv,frag->detail_rgba);if(r)return r;
        frag->have_detail_tex=1;
    }
    return HL2M_OK;
}
/* Canonical Source material path relative to materials/ root, lower case,
   forward slashes. Strict fixed buffer; no file system or loading. */
int hl2m_material_vtf_path89(const char *name,char *out,unsigned int capacity)
{
    unsigned int i=0u,j=0u;const char *p=name;
    if(!name || !out || !capacity)return HL2M_ERR_NULL;
    if(strlen(p)>=10u && (p[0]=='m'||p[0]=='M') && (p[1]=='a'||p[1]=='A') &&
       (p[2]=='t'||p[2]=='T') && (p[3]=='e'||p[3]=='E') &&
       (p[4]=='r'||p[4]=='R') && (p[5]=='i'||p[5]=='I') &&
       (p[6]=='a'||p[6]=='A') && (p[7]=='l'||p[7]=='L') &&
       (p[8]=='s'||p[8]=='S') && (p[9]=='/'||p[9]=='\\'))p+=10;
    while(p[i]){
        char c=p[i++];
        if(j+5u>=capacity)return HL2M_ERR_LIMIT;
        if(c=='\\')c='/';
        if(c>='A' && c<='Z')c=(char)(c-'A'+'a');
        out[j++]=c;
    }
    if(j>4u && out[j-4u]=='.' && out[j-3u]=='v' && out[j-2u]=='t' && out[j-1u]=='f'){
        out[j]=0;return HL2M_OK;
    }
    if(j==0u)return HL2M_ERR_PARSE;
    if(j+5u>capacity)return HL2M_ERR_LIMIT;
    out[j++]='.';out[j++]='v';out[j++]='t';out[j++]='f';out[j]=0;
    return HL2M_OK;
}
