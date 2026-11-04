#include "c4d89_image.h"
#include <stdio.h>
/* Diagnostic CLI. Core decoders can instead feed an engine-owned RGBA texture. */
static unsigned char image_pixels[4096ul*4096ul*4ul];
int main(int argc,char **argv)
{
    C4D89_ImgInfo info;FILE *f;unsigned long y,x,p;int rc;
    if(argc!=3){fprintf(stderr,"Usage: c4d89_imgtool image.(png|jpg|bmp|tga) out.ppm\n");return 2;}
    rc=c4d89_img_decode_file(argv[1],image_pixels,sizeof(image_pixels),&info);
    if(rc){fprintf(stderr,"Decode error %d\n",rc);return 1;}
    f=fopen(argv[2],"wb");if(!f)return 2;
    fprintf(f,"P6\n%u %u\n255\n",info.width,info.height);
    for(y=0ul;y<info.height;y++)for(x=0ul;x<info.width;x++){
        p=(y*(unsigned long)info.width+x)*4ul;
        if(fwrite(image_pixels+p,1u,3u,f)!=3u){fclose(f);return 3;}
    }
    fclose(f);printf("decoded %u x %u format=%u\n",info.width,info.height,info.format);
    return 0;
}
