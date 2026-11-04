#include "hl2m_master89.h"
#include <stdio.h>
#include <string.h>
static hl2m_engine_vertex a,b,c;
int main(void)
{
    hl2m_material_fx89 fx;hl2m_fragment89 f;int r;
    memset(&a,0,sizeof(a));memset(&b,0,sizeof(b));memset(&c,0,sizeof(c));
    memset(&fx,0,sizeof(fx));memset(&f,0,sizeof(f));
    a.normal.z=65536;b.normal.z=65536;c.normal.y=65536;
    a.tangent.x=b.tangent.x=c.tangent.x=65536;
    a.tangent.w=b.tangent.w=c.tangent.w=65536;
    a.texcoord.x=0;b.texcoord.x=65536;c.texcoord.x=0;
    r=hl2m_master_smooth_fragment89(&f,&fx,&a,&b,&c,32768,16384,16384,0,0,0);
    if(r || f.normal.z<49000 || f.normal.y<14000 || f.normal.y>26000){
        printf("FAIL interpolation r=%d nz=%d ny=%d\n",r,(int)f.normal.z,(int)f.normal.y);return 1;
    }
    if(f.tangent.x<65000 || f.bitangent.y<0){fputs("FAIL tangent basis\n",stdout);return 2;}
    r=hl2m_master_smooth_fragment89(&f,&fx,&a,&b,&c,32768,32768,16384,0,0,0);
    if(r!=HL2M_ERR_RANGE){fputs("FAIL barycentric weights\n",stdout);return 3;}
    c.material=2;
    r=hl2m_master_smooth_fragment89(&f,&fx,&a,&b,&c,32768,16384,16384,0,0,0);
    if(r!=HL2M_ERR_RANGE){fputs("FAIL material hard edge\n",stdout);return 4;}
    c.material=0;c.tangent.w=-65536;
    r=hl2m_master_smooth_fragment89(&f,&fx,&a,&b,&c,32768,16384,16384,0,0,0);
    if(r!=HL2M_ERR_RANGE){fputs("FAIL tangent handedness seam\n",stdout);return 5;}
    puts("SMOOTH110 PASS barycentric N/T/UV + hard-material and handedness seam guards");
    return 0;
}
