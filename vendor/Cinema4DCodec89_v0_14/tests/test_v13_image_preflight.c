#include "c4d89_image.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* Run with a fixture directory argument; no heap anywhere. */
static unsigned char src[C4D89_IMG_FILE_CAP];
static unsigned char rgba[1024ul*1024ul*4ul];
static int test_file(const char *path)
{
    FILE *f;unsigned long n;long len;unsigned long required=0ul;
    C4D89_ImgInfo inf,actual;int rc;
    f=fopen(path,"rb");if(!f){printf("cannot open %s\n",path);return 0;}
    if(fseek(f,0L,SEEK_END)!=0||(len=ftell(f))<0L ||
       (unsigned long)len>sizeof(src) || fseek(f,0L,SEEK_SET)!=0){fclose(f);return 0;}
    n=(unsigned long)len;
    if(fread(src,1u,(size_t)n,f)!=(size_t)n){fclose(f);return 0;}
    fclose(f);
    rc=c4d89_img_probe_info(src,n,&inf,&required);
    if(rc!=C4D89_IMG_OK || required!=(unsigned long)inf.width*inf.height*4ul ||
       required>sizeof(rgba)){
        printf("probe failure: %s (%d)\n",path,rc);return 0;
    }
    rc=c4d89_img_decode(src,n,rgba,sizeof(rgba),&actual);
    if(rc!=C4D89_IMG_OK||inf.width!=actual.width||inf.height!=actual.height||
       inf.format!=actual.format){printf("decode mismatch: %s %d\n",path,rc);return 0;}
    if(required>0ul){
        /* One byte too small must fail rather than overwrite the caller. */
        rc=c4d89_img_decode(src,n,rgba,required-1ul,&actual);
        if(rc!=C4D89_IMG_LIMIT){printf("capacity was ignored: %s %d\n",path,rc);return 0;}
    }
    return 1;
}
int main(int argc,char **argv)
{
    static const char *names[]={
      "rgb.png","rgba.png","pal.png","gray.png","tiny.png",
      "rgb.tga","rgb_rle.tga","pal.tga",
      "rgb.bmp","pal.bmp","gray.jpg","baseline_0.jpg","baseline_1.jpg","baseline_2.jpg"};
    unsigned int i,count=0u;char path[1024];C4D89_ImgInfo inf;unsigned long need;
    unsigned char invalid_tga[22];
    if(argc<2)return 2;
    for(i=0u;i<sizeof(names)/sizeof(names[0]);i++){
        if(strlen(argv[1])+strlen(names[i])+2u>=sizeof(path))return 2;
        strcpy(path,argv[1]);strcat(path,"/");strcat(path,names[i]);
        if(!test_file(path))return 1;
        count++;
    }
    memset(invalid_tga,0,sizeof(invalid_tga));
    invalid_tga[2]=10u;invalid_tga[12]=2u;invalid_tga[14]=2u;
    invalid_tga[16]=24u;invalid_tga[17]=0x20u;
    invalid_tga[18]=0x84u; /* RLE of five pixels into 2x2 image */
    if(c4d89_img_probe_info(invalid_tga,sizeof(invalid_tga),&inf,&need)!=C4D89_IMG_OK ||
       need!=16ul)return 1;
    if(c4d89_img_decode(invalid_tga,sizeof(invalid_tga),rgba,sizeof(rgba),&inf)!=
       C4D89_IMG_CORRUPT){puts("oversized TGA packet not rejected");return 1;}
    printf("v0.13 image probes passed (%u real format fixtures + TGA RLE overflow rejection)\n",count);
    return 0;
}
