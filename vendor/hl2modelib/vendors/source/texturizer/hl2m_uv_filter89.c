/* Source-style texel-center bilinear sampling of decoded RGBA VTF.
   Strict ISO C89, signed Q16.16, exclusively 32-bit intermediates.
   UVs are Source/VVD top-left convention: DO NOT invert V here.
   Exporting OBJ is a separate coordinate-system conversion. */
#include "hl2modelib.h"
#include <string.h>

static int address89(int i,unsigned int dimension,int repeat)
{
    int d=(int)dimension;
    if(repeat){ i%=d;if(i<0)i+=d;return i; }
    if(i<0)return 0;
    if(i>=d)return d-1;
    return i;
}
static int floor_fixed89(int signed_value)
{
    int q=signed_value/65536;
    if(signed_value<0 && signed_value%65536)q--;
    return q;
}
static int normalized89(hl2m_fixed x,int repeat)
{
    if(repeat){x%=65536;if(x<0)x+=65536;return x;}
    if(x<0)return 0;
    if(x>65536)return 65536;
    return x;
}
static unsigned int read_channel89(const hl2m_texture_view89 *t,int x,int y,int ch)
{
    unsigned int col=(unsigned int)address89(x,t->width,t->repeat_u);
    unsigned int row=(unsigned int)address89(y,t->height,t->repeat_v);
    return (unsigned int)t->pixels[(row*t->width+col)*4u+(unsigned int)ch];
}
int hl2m_texture_sample_linear89(const hl2m_texture_view89 *t,
    hl2m_fixed u,hl2m_fixed v,hl2m_u8 rgba[4])
{
    int fx,fy,x,y,rx,ry,ch;
    unsigned int v00,v10,v01,v11,h0,h1,result;
    if(!t || !rgba || !t->pixels)return HL2M_ERR_NULL;
    if(t->width==0u || t->height==0u ||
       t->width>8192u || t->height>8192u ||
       t->width>0xFFFFFFFFu/t->height/4u)return HL2M_ERR_RANGE;
    /* D3D texel-center convention: u*width - half a texel.
       With normalized u/v in [0,65536], width <= 8192 cannot overflow. */
    fx=normalized89(u,t->repeat_u)*(int)t->width-32768;
    fy=normalized89(v,t->repeat_v)*(int)t->height-32768;
    x=floor_fixed89(fx);y=floor_fixed89(fy);
    rx=fx-x*65536;ry=fy-y*65536;
    for(ch=0;ch<4;ch++){
        v00=read_channel89(t,x,y,ch);
        v10=read_channel89(t,x+1,y,ch);
        v01=read_channel89(t,x,y+1,ch);
        v11=read_channel89(t,x+1,y+1,ch);
        h0=(v00*(unsigned int)(65536-rx)+v10*(unsigned int)rx+32768u)>>16;
        h1=(v01*(unsigned int)(65536-rx)+v11*(unsigned int)rx+32768u)>>16;
        result=(h0*(unsigned int)(65536-ry)+h1*(unsigned int)ry+32768u)>>16;
        rgba[ch]=(hl2m_u8)(result>255u?255u:result);
    }
    return HL2M_OK;
}
/* Source detail scale, consistent with existing Q16.16 sampler. */
static hl2m_fixed mul89(hl2m_fixed a,hl2m_fixed b)
{
    unsigned int x,y,r;int neg=0;
    if(a<0){x=(unsigned int)(-(a+1))+1u;neg=!neg;}else x=(unsigned int)a;
    if(b<0){y=(unsigned int)(-(b+1))+1u;neg=!neg;}else y=(unsigned int)b;
    r=(x>>16)*(y>>16)*65536u+(x>>16)*(y&65535u)+(y>>16)*(x&65535u);
    r+=((x&65535u)*(y&65535u))>>16;
    return neg?-(hl2m_fixed)r:(hl2m_fixed)r;
}
int hl2m_fragment_sample_maps_linear89(hl2m_fragment89 *frag,
    const hl2m_material_fx89 *fx,const hl2m_texture_view89 *base,
    const hl2m_texture_view89 *bump,const hl2m_texture_view89 *detail,
    hl2m_fixed u,hl2m_fixed v)
{
    int r;
    if(!frag || !fx)return HL2M_ERR_NULL;
    frag->have_normal_tex=0;frag->have_detail_tex=0;
    if(base){r=hl2m_texture_sample_linear89(base,u,v,frag->base_rgba);if(r)return r;}
    if(fx->normal_map && bump){
        r=hl2m_texture_sample_linear89(bump,u,v,frag->normal_tex);
        if(r)return r;
        frag->have_normal_tex=1;
    }
    if(fx->detail_map && detail){
        r=hl2m_texture_sample_linear89(detail,mul89(u,fx->detail_scale),
                                       mul89(v,fx->detail_scale),frag->detail_rgba);
        if(r)return r;
        frag->have_detail_tex=1;
    }
    return HL2M_OK;
}
