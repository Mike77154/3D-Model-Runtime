#include "c4d89_image.h"
#include <string.h>
/* One shared file buffer. Deliberately serial, no malloc/free. */
static unsigned char image_file_buffer[C4D89_IMG_FILE_CAP];
int c4d89_img_decode(const unsigned char *src,unsigned long len,
                     unsigned char *rgba,unsigned long cap,C4D89_ImgInfo *info)
{
    if(!src||!rgba||!info||len<2ul)return C4D89_IMG_INVALID;
    memset(info,0,sizeof(*info));
    if(len>=8ul && memcmp(src,"\211PNG\r\n\032\n",8u)==0)
        return c4d89_img_png(src,len,rgba,cap,info);
    if(src[0]==0xffu && src[1]==0xd8u)
        return c4d89_img_jpeg(src,len,rgba,cap,info);
    if(len>=2ul && src[0]=='B' && src[1]=='M')
        return c4d89_img_bmp(src,len,rgba,cap,info);
    /* TGA has no mandatory magic. Validation is inside the TGA module. */
    return c4d89_img_tga(src,len,rgba,cap,info);
}
int c4d89_img_decode_file(const char *filename,unsigned char *rgba,
                          unsigned long cap,C4D89_ImgInfo *info)
{
    FILE *f;unsigned long n=0ul;size_t x;
    if(!filename||!rgba||!info)return C4D89_IMG_INVALID;
    f=fopen(filename,"rb");if(!f)return C4D89_IMG_INVALID;
    while(n<C4D89_IMG_FILE_CAP) {
        x=fread(image_file_buffer+n,1u,(size_t)(C4D89_IMG_FILE_CAP-n),f);
        n+=(unsigned long)x;
        if(x==0u)break;
    }
    if(n==C4D89_IMG_FILE_CAP && fgetc(f)!=EOF) { fclose(f);return C4D89_IMG_LIMIT; }
    if(ferror(f)){fclose(f);return C4D89_IMG_INVALID;}
    fclose(f);
    return c4d89_img_decode(image_file_buffer,n,rgba,cap,info);
}
int c4d89_img_decode_embedded(const char *filename,unsigned long offset,
                             unsigned long size,unsigned char *rgba,
                             unsigned long cap,C4D89_ImgInfo *info)
{
    FILE *f;
    if(!filename||!rgba||!info)return C4D89_IMG_INVALID;
    if(size>C4D89_IMG_FILE_CAP || offset>2147483647ul)return C4D89_IMG_LIMIT;
    f=fopen(filename,"rb");if(!f)return C4D89_IMG_INVALID;
    if(fseek(f,(long)offset,SEEK_SET)!=0||
       fread(image_file_buffer,1u,(size_t)size,f)!=(size_t)size) {
       fclose(f);return C4D89_IMG_TRUNCATED;
    }
    fclose(f);
    return c4d89_img_decode(image_file_buffer,size,rgba,cap,info);
}
