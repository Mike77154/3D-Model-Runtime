/* Full-pixel VTF decode smoke test. Source C89 no malloc or floating. */
#include "hl2modelib.h"
#include <stdio.h>
#define IN_CAP (32u*1024u*1024u)
#define PIX_CAP (4096u*4096u*4u)
static hl2m_u8 data[IN_CAP];static hl2m_u8 rgba[PIX_CAP];
int main(int argc,char **argv)
{
    FILE *f;size_t len;unsigned int n;int repaired=0,r;hl2m_vtf_info info;
    if(argc!=2){fputs("vtf_audit input.vtf\n",stderr);return 1;}
    f=fopen(argv[1],"rb");if(!f)return 2;
    len=fread(data,1u,IN_CAP,f);
    if(!feof(f)){fclose(f);puts("ERR_SIZE");return 3;}
    fclose(f);n=(unsigned int)len;
    r=hl2m_repair_missing_first_byte(data,&n,IN_CAP,HL2M_FORMAT_VTF,&repaired);
    if(r){printf("ERR_HEADER %d\n",r);return 4;}
    r=hl2m_vtf_read_info(data,n,&info);
    if(r){printf("ERR_INFO %d\n",r);return 5;}
    if(info.width>4096u || info.height>4096u || info.width==0u || info.height==0u){puts("ERR_DIM_LIMIT");return 6;}
    r=hl2m_vtf_decode_rgba(data,n,&info,rgba,PIX_CAP);
    if(r){printf("ERR_DECODE %d format=%u w=%u h=%u\n",r,info.image_format,info.width,info.height);return 7;}
    printf("OK|%u|%u|%u|%u|%d\n",info.width,info.height,info.image_format,info.mip_count,repaired);
    return 0;
}
