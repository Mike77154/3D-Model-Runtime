/* Header-only VTF corpus probe. Does not decode full pixels. */
#include "hl2modelib.h"
#include <stdio.h>
#define CAP (24u*1024u*1024u)
static unsigned char image[CAP];
int main(int argc, char **argv)
{
    FILE *f;
    unsigned int n;
    int r;
    hl2m_vtf_info info;
    if(argc!=2)return 1;
    f=fopen(argv[1],"rb");if(!f)return 2;
    n=(unsigned int)fread(image,1u,CAP,f);
    if(!feof(f)){fclose(f);return 3;}
    fclose(f);
    r=hl2m_vtf_read_info(image,n,&info);
    if(r==HL2M_OK)printf("OK %u %u fmt=%u mip=%u\n",info.width,info.height,info.image_format,info.mip_count);
    else printf("FAIL %d\n",r);
    return r==HL2M_OK ? 0 : 4;
}
