#include "hl2modelib.h"
#include <string.h>
#include <stdio.h>
static hl2m_u8 file[128];
static hl2m_u8 rgba[16];
static hl2m_vtf_info info;
static void put32(unsigned int p,unsigned int v){file[p]=(hl2m_u8)v;file[p+1]=(hl2m_u8)(v>>8);file[p+2]=(hl2m_u8)(v>>16);file[p+3]=(hl2m_u8)(v>>24);}
static int test_fmt(unsigned int fmt, hl2m_u8 c0,hl2m_u8 c1,hl2m_u8 c2,hl2m_u8 c3)
{
    int r;
    memset(file,0,sizeof(file));
    memcpy(file,"VTF\0",4u);put32(4u,7u);put32(8u,2u);put32(12u,80u);
    file[16]=1u;file[18]=1u;file[24]=1u;file[56]=1u;file[63]=1u;
    put32(52u,fmt);put32(57u,0xffffffffu);
    file[80]=c0;file[81]=c1;file[82]=c2;file[83]=c3;
    r=hl2m_vtf_read_info(file,84u,&info);
    if(r!=HL2M_OK){printf("fmt%u info=%d\n",fmt,r);return 1;}
    r=hl2m_vtf_decode_rgba(file,84u,&info,rgba,sizeof(rgba));
    if(r!=HL2M_OK){printf("fmt%u decode=%d\n",fmt,r);return 2;}
    if(rgba[0]!=30u || rgba[1]!=20u || rgba[2]!=10u || rgba[3]!=40u){
        printf("fmt%u got %u %u %u %u\n",fmt,rgba[0],rgba[1],rgba[2],rgba[3]);return 3;}
    return 0;
}
int main(void)
{
    int r=0;
    r|=test_fmt(HL2M_VTF_BGRA8888,10u,20u,30u,40u);
    r|=test_fmt(HL2M_VTF_ABGR8888,40u,10u,20u,30u);
    /* A8 not stored for BGR888: verify opaque alpha independently. */
    if(r)return r;
    memset(file,0,sizeof(file));memcpy(file,"VTF\0",4u);
    put32(4u,7u);put32(8u,2u);put32(12u,80u);
    file[16]=1u;file[18]=1u;file[24]=1u;file[56]=1u;file[63]=1u;
    put32(52u,HL2M_VTF_BGR888);put32(57u,0xffffffffu);
    file[80]=10u;file[81]=20u;file[82]=30u;
    if(hl2m_vtf_read_info(file,83u,&info)!=HL2M_OK ||
       hl2m_vtf_decode_rgba(file,83u,&info,rgba,sizeof(rgba))!=HL2M_OK ||
       rgba[0]!=30u || rgba[1]!=20u || rgba[2]!=10u || rgba[3]!=255u) return 4;
    puts("vtf112 PASS: BGRA8888, ABGR8888, BGR888");return 0;
}
