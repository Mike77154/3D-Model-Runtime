/* Resident Evil Workshop regression: a Source VVD vertex can reference bone
 * indices above 127. Preserve the 8-bit bit pattern as a nonnegative index.
 * Strict ANSI C89; no heap or floating-point arithmetic. */
#include "hl2modelib.h"
#include <stdio.h>
#include <string.h>

static unsigned char g_vvd[128];
static hl2m_vvd g_model;
static void put32(unsigned int off, unsigned int n)
{
    g_vvd[off]=(unsigned char)(n&255u);
    g_vvd[off+1u]=(unsigned char)((n>>8)&255u);
    g_vvd[off+2u]=(unsigned char)((n>>16)&255u);
    g_vvd[off+3u]=(unsigned char)((n>>24)&255u);
}
int main(void)
{
    int r;
    memset(g_vvd,0,sizeof(g_vvd));
    put32(0u,HL2M_VVD_MAGIC);
    put32(4u,4u);
    put32(12u,1u); /* one LOD */
    put32(16u,1u); /* one vertex */
    put32(56u,64u); /* VVD vertices */
    put32(60u,112u); /* tangents */
    put32(64u,0x3f800000u); /* IEEE 754 weight=1.0 */
    g_vvd[76]=200u; /* global bone index 200 */
    g_vvd[79]=1u;  /* one influence */
    r=hl2m_decode_vvd(g_vvd,sizeof(g_vvd),0,&g_model);
    if(r!=HL2M_OK) { printf("FAIL vvd code=%d\n",r);return 1; }
    if(g_model.vertex_count!=1 || g_model.vertices[0].bone[0]!=200) {
        printf("FAIL signed bone index=%d\n",g_model.vertices[0].bone[0]);return 2;
    }
    if(HL2M_MAX_VVD_VERTS<123495 || HL2M_MAX_ENGINE_VERTICES<512082 ||
       HL2M_MAX_VTX_VERTICES<65536 || HL2M_MAX_VTX_INDICES<262144) {
        puts("FAIL capacities insufficient for test corpus");return 3;
    }
    puts("PASS v117: unsigned VVD bone index 200, enlarged corpus limits");
    return 0;
}
