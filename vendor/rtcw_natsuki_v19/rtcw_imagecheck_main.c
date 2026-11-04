/* Compact source-only texture validator and PPM exporter. CC0-1.0, ISO C89. */
#include "rtcw_texture.h"
#include <stdio.h>
int main(int argc,char **argv){
    FILE *out;unsigned long i,count;
    if(argc!=3){fprintf(stderr,"usage: rtcw_imagecheck image.tga|image.jpg output.ppm\n");return 2;}
    if(!rtcw_texture_load(argv[1])){
        fprintf(stderr,"texture: %s\n",rtcw_texture_error());return 1;
    }
    out=fopen(argv[2],"wb");if(!out){fprintf(stderr,"cannot write PPM\n");return 1;}
    if(fprintf(out,"P6\n%u %u\n255\n",rtcw_texture_current.width,rtcw_texture_current.height)<0){
        fclose(out);return 1;
    }
    count=(unsigned long)rtcw_texture_current.width*(unsigned long)rtcw_texture_current.height;
    for(i=0UL;i<count;i++){
        if(fwrite(&rtcw_texture_current.pixel[i*4UL],1,3,out)!=3U){fclose(out);return 1;}
    }
    if(fclose(out)!=0)return 1;
    printf("OK image=%ux%u\n",rtcw_texture_current.width,rtcw_texture_current.height);
    return 0;
}
