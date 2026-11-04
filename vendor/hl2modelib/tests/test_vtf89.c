/* Self-contained BC1/BC3 and fixed-point formatter regression, C89. */
#include "hl2modelib.h"
#include <stdio.h>
#include <string.h>
static hl2m_u8 data[128];
static hl2m_u8 rgba[64];
static void put32(hl2m_u8 *p,unsigned int v) {
    p[0]=(hl2m_u8)v;p[1]=(hl2m_u8)(v>>8);
    p[2]=(hl2m_u8)(v>>16);p[3]=(hl2m_u8)(v>>24);
}
static int init(unsigned int format) {
    hl2m_vtf_info info;
    unsigned int n= format == HL2M_VTF_DXT1 ? 8u : 16u;
    memset(data,0,sizeof(data));
    memcpy(data,"VTF\0",4u);
    put32(data+4,7u);put32(data+8,2u);put32(data+12,80u);
    data[16]=4;data[18]=4;data[24]=1;data[56]=1;
    put32(data+52,format);put32(data+57,0xFFFFFFFFu);
    data[63]=1;
    if(n==8u) {
        data[80]=0x00;data[81]=0xF8;data[82]=0xE0;data[83]=0x07;
    } else {
        data[80]=255;data[81]=0;
        data[88]=0x00;data[89]=0xF8;data[90]=0xE0;data[91]=0x07;
    }
    if(hl2m_vtf_read_info(data,80u+n,&info)!=HL2M_OK) return 1;
    if(info.width!=4u || info.height!=4u || info.data_offset!=80u) return 2;
    if(hl2m_vtf_decode_rgba(data,80u+n,&info,rgba,sizeof(rgba))!=HL2M_OK)return 3;
    if(rgba[0]!=255u || rgba[1]!=0u || rgba[2]!=0u || rgba[3]!=255u)return 4;
    return 0;
}
int main(void) {
    char f[48];
    int a=init(HL2M_VTF_DXT1),b=init(HL2M_VTF_DXT5);
    int c;
    hl2m_fixed_to_string((hl2m_fixed)(HL2M_FIXED_ONE+40000),f,sizeof(f),5);
    c=strcmp(f,"1.61035");
    printf("BC1=%d BC3=%d FIXED_POINT=%s\n",a,b,c?"FAIL":"PASS");
    return a||b||c ? 1 : 0;
}
