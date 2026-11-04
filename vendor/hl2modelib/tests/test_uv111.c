#include "hl2m_master89.h"
#include <stdio.h>
#include <string.h>
#define FAIL(msg) do {puts("UV111 FAIL " msg);return 1;} while(0)
int main(void)
{
    /* Top row red/green, bottom blue/white. */
    static const hl2m_u8 pix[16]={255,0,0,255, 0,255,0,255,
                                   0,0,255,255, 255,255,255,255};
    hl2m_texture_view89 t;
    hl2m_u8 rgb[4];
    memset(&t,0,sizeof(t));t.pixels=pix;t.width=2u;t.height=2u;
    t.repeat_u=1;t.repeat_v=1;
    if(hl2m_master_uv_sample_linear89(&t,32768,32768,rgb)!=HL2M_OK)FAIL("center");
    if(rgb[0]!=128 || rgb[1]!=128 || rgb[2]!=128 || rgb[3]!=255)FAIL("bilinear mean");
    if(hl2m_texture_sample_linear89(&t,16384,16384,rgb)!=HL2M_OK)FAIL("texel center");
    if(rgb[0]!=255 || rgb[1]!=0 || rgb[2]!=0)FAIL("texel 0");
    if(hl2m_texture_sample_linear89(&t,49152,16384,rgb)!=HL2M_OK)FAIL("texel1");
    if(rgb[0]!=0 || rgb[1]!=255 || rgb[2]!=0)FAIL("texel 1");
    if(hl2m_texture_sample_linear89(&t,16384,49152,rgb)!=HL2M_OK)FAIL("bottom left");
    if(rgb[2]!=255 || rgb[0]!=0)FAIL("V must not be inverted");
    if(hl2m_texture_sample_linear89(&t,-16384,16384,rgb)!=HL2M_OK)FAIL("repeat negative");
    if(rgb[1]!=255)FAIL("negative repeat");
    t.repeat_u=0;t.repeat_v=0;
    if(hl2m_texture_sample_linear89(&t,-65536,-65536,rgb)!=HL2M_OK)FAIL("clamp negative");
    if(rgb[0]!=255 || rgb[1]!=0)FAIL("clamp red");
    if(hl2m_texture_sample_linear89(&t,65536,65536,rgb)!=HL2M_OK)FAIL("clamp positive");
    if(rgb[0]!=255 || rgb[1]!=255 || rgb[2]!=255)FAIL("clamp white");
    {
        hl2m_engine_vertex a,b,c;
        hl2m_material_fx89 fx;
        hl2m_fragment89 frag;
        memset(&a,0,sizeof(a));memset(&b,0,sizeof(b));
        memset(&c,0,sizeof(c));memset(&fx,0,sizeof(fx));
        memset(&frag,0,sizeof(frag));
        a.normal.z=b.normal.z=c.normal.z=65536;
        a.tangent.x=b.tangent.x=c.tangent.x=65536;
        a.tangent.w=b.tangent.w=c.tangent.w=65536;
        b.texcoord.x=65536;
        c.texcoord.y=65536;
        t.repeat_u=1;t.repeat_v=1;
        if(hl2m_master_smooth_fragment_linear89(&frag,&fx,&a,&b,&c,
            0,32768,32768,&t,0,0)!=HL2M_OK)FAIL("fragment bridge");
        if(frag.base_rgba[0]!=128 || frag.base_rgba[1]!=128 ||
            frag.base_rgba[2]!=128 || frag.base_rgba[3]!=255)
            FAIL("fragment interpolation");
    }
    puts("UV111 PASS: texel centers, bilinear, top-left V, wrap/clamp");
    return 0;
}
