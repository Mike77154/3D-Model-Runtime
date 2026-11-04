#include "c4d89_image.h"
#include <stdio.h>
#include <string.h>
static unsigned char bytes[262144];
static unsigned char pixels[1024ul*1024ul*4ul];
static int list(void *u,unsigned int id,char *dst,unsigned int cap)
{
    const char *p=id==0u?"models/female/tex/atlas.jpg":
        "models/male/tex/atlas.jpg";
    (void)u;
    if(strlen(p)+1ul>(unsigned long)cap)return 0;
    strcpy(dst,p);return 1;
}
static int read_resource(void *u,const char *name,
                         const unsigned char **data,unsigned long *n)
{
    FILE *f;size_t size;
    (void)u;
    if(strcmp(name,"models/female/tex/atlas.jpg")!=0)return 0;
    f=fopen("tests/fixtures/baseline_2.jpg","rb");if(!f)return 0;
    size=fread(bytes,1u,sizeof(bytes),f);fclose(f);
    *n=(unsigned long)size;*data=bytes;return size>0u;
}
int main(void)
{
    C4D89_ResourceIndex idx;C4D89_ImgInfo info;int rc;
    idx.get=list;idx.user=0;idx.count=2u;
    rc=c4d89_img_decode_resource("models/female/Female.c4d","atlas.jpg",
              &idx,read_resource,0,pixels,sizeof(pixels),&info);
    if(rc!=0||info.width!=39u||info.height!=32u||info.format!=4u)return 1;
    /* Transparent RGBA fixture; alpha must survive image dispatch. */
    {
        FILE *f=fopen("tests/fixtures/rgba.png","rb");size_t num;
        if(!f)return 2;
        num=fread(bytes,1u,sizeof(bytes),f);fclose(f);
        rc=c4d89_img_decode(bytes,(unsigned long)num,pixels,sizeof(pixels),&info);
        if(rc!=0||info.format!=C4D89_IMG_FORMAT_PNG||pixels[3]!=0u ||
           pixels[7]!=17u || pixels[39ul*4ul+3ul]!=11u)return 3;
    }
    puts("PASS: VFS resolver, JPEG RGBA8, PNG alpha");return 0;
}
