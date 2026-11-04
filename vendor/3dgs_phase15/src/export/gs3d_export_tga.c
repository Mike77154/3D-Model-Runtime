#include "../../include/gs3d.h"
#include <stdio.h>
#include <string.h>
int gs3d_save_tga(const char *path,unsigned long w,unsigned long h){
    FILE *f;unsigned char hd[18],px[4];unsigned long i,n=w*h;
    memset(hd,0,sizeof(hd));hd[2]=2;hd[12]=(unsigned char)w;
    hd[13]=(unsigned char)(w>>8);hd[14]=(unsigned char)h;
    hd[15]=(unsigned char)(h>>8);hd[16]=32;hd[17]=0x28; /* top-left and alpha */
    f=fopen(path,"wb");if(!f)return GS3D_ERR_IO;
    if(fwrite(hd,1,18,f)!=18){fclose(f);return GS3D_ERR_IO;}
    for(i=0;i<n;i++){
        px[0]=gs3d_rgba[i*4UL+2UL];px[1]=gs3d_rgba[i*4UL+1UL];
        px[2]=gs3d_rgba[i*4UL];px[3]=gs3d_rgba[i*4UL+3UL];
        if(fwrite(px,1,4,f)!=4){fclose(f);return GS3D_ERR_IO;}
    }
    fclose(f);return 0;
}
