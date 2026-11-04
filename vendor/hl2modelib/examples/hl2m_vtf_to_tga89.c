/* Portable offline VTF -> RGBA TGA utility. C89, no heap or floating point. */
#include "hl2modelib.h"
#include <stdio.h>
#include <string.h>
#define INPUT_CAP (32u*1024u*1024u)
#define TEX_CAP (4096u*4096u*4u)
static hl2m_u8 file_data[INPUT_CAP];
static hl2m_u8 rgba_data[TEX_CAP];
int main(int argc, char **argv) {
    FILE *f;
    size_t n;
    unsigned int size;
    int repaired=0,r;
    unsigned int i,total;
    hl2m_vtf_info info;
    hl2m_u8 header[18];
    if(argc!=3) {fprintf(stderr,"usage: hl2m_vtf_to_tga89 input.vtf output.tga\n");return 1;}
    f=fopen(argv[1],"rb");
    if(!f) {perror(argv[1]);return 2;}
    n=fread(file_data,1,INPUT_CAP,f);
    if(!feof(f)){fprintf(stderr,"VTF exceeds buffer\n");fclose(f);return 2;}
    fclose(f);size=(unsigned int)n;
    r=hl2m_repair_missing_first_byte(file_data,&size,INPUT_CAP,HL2M_FORMAT_VTF,&repaired);
    if(r!=HL2M_OK){fprintf(stderr,"VTF signature error: %d\n",r);return 3;}
    r=hl2m_vtf_read_info(file_data,size,&info);
    if(r!=HL2M_OK){fprintf(stderr,"VTF info error: %d\n",r);return 4;}
    if(info.width>4096u || info.height>4096u){fprintf(stderr,"VTF dimensions exceed TGA example limit\n");return 5;}
    r=hl2m_vtf_decode_rgba(file_data,size,&info,rgba_data,TEX_CAP);
    if(r!=HL2M_OK){fprintf(stderr,"VTF decode error: %d\n",r);return 6;}
    f=fopen(argv[2],"wb");if(!f) {perror(argv[2]);return 7;}
    memset(header,0,sizeof(header));
    header[2]=2;header[12]=(hl2m_u8)(info.width&255u);header[13]=(hl2m_u8)(info.width>>8);
    header[14]=(hl2m_u8)(info.height&255u);header[15]=(hl2m_u8)(info.height>>8);
    header[16]=32;header[17]=0x28; /* Top-left origin, 8-bit alpha. */
    if(fwrite(header,1,18,f)!=18u){fclose(f);return 8;}
    total=info.width*info.height*4u;
    for(i=0;i<total;i+=4u) {
        /* TGA stores BGRA while API delivers RGBA. */
        unsigned char a=rgba_data[i];
        rgba_data[i]=rgba_data[i+2u];rgba_data[i+2u]=a;
    }
    if(fwrite(rgba_data,1,total,f)!=total){fclose(f);return 9;}
    fclose(f);
    printf("OK %ux%u format=%u mip=%u repaired=%d\n",info.width,info.height,info.image_format,info.mip_count,repaired);
    return 0;
}
