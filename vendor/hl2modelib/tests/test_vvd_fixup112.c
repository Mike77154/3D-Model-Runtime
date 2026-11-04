/* Source VVD fixup remaps both positions and tangent arrays.
   Synthetic two-fixup sample, strict C89 with IEEE32 bytes only. */
#include "hl2modelib.h"
#include <stdio.h>
#include <string.h>
static hl2m_u8 buffer[280];
static hl2m_vvd vvd;
static void w32(unsigned int p,unsigned int x){
    buffer[p]=(hl2m_u8)x;buffer[p+1u]=(hl2m_u8)(x>>8);
    buffer[p+2u]=(hl2m_u8)(x>>16);buffer[p+3u]=(hl2m_u8)(x>>24);
}
int main(void){
    int i,r;
    const unsigned int u[]={0x3f800000u,0x40000000u,0x40400000u};
    memcpy(buffer,"IDSV",4u);w32(4u,4u);w32(8u,1u);w32(12u,1u);
    w32(16u,3u);w32(48u,2u);w32(52u,64u);
    w32(56u,88u);w32(60u,232u);
    w32(64u,0u);w32(68u,2u);w32(72u,1u);
    w32(76u,0u);w32(80u,0u);w32(84u,2u);
    for(i=0;i<3;i++){
        unsigned int base=88u+(unsigned int)i*48u;
        w32(base,0x3f800000u);buffer[base+15u]=1u;
        w32(base+16u,u[i]);w32(base+28u,0x3f800000u);
        w32(232u+(unsigned int)i*16u,u[i]);
    }
    r=hl2m_decode_vvd(buffer,sizeof(buffer),0,&vvd);
    if(r!=HL2M_OK){printf("VVD fixup decode=%d\n",r);return 1;}
    if(vvd.vertex_count!=3||vvd.tangent_count!=3||
       vvd.vertices[0].position.x!=3*HL2M_FIXED_ONE||
       vvd.vertices[1].position.x!=1*HL2M_FIXED_ONE||
       vvd.vertices[2].position.x!=2*HL2M_FIXED_ONE||
       vvd.tangents[0].x!=3*HL2M_FIXED_ONE||
       vvd.tangents[1].x!=1*HL2M_FIXED_ONE||
       vvd.tangents[2].x!=2*HL2M_FIXED_ONE){
        puts("VVD fixup order incorrect");return 2;
    }
    puts("vvd112 PASS: remapped positions and tangents");return 0;
}
