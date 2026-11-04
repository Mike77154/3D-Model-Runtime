/* Independent original TGA decoder, CC0-1.0; ANSI C89 no heap/floats. */
#include "rtcw_texture.h"
#include <stdio.h>
#include <string.h>
static char tex_error[128]="not loaded";
rtcw_texture rtcw_texture_current;
const char *rtcw_texture_error(void) {return tex_error;}
static int terr(const char *s) {strncpy(tex_error,s,sizeof(tex_error)-1);tex_error[sizeof(tex_error)-1]=0;return 0;}
static unsigned int le16(const unsigned char *p){return (unsigned int)p[0]+((unsigned int)p[1]<<8);}
static int read_pixel(FILE *f,unsigned int bpp,unsigned char rgba[4]){
    unsigned char bgr[4];unsigned int n=bpp/8U;
    if(fread(bgr,1,n,f)!=n)return 0;
    if(n==1){rgba[0]=rgba[1]=rgba[2]=bgr[0];rgba[3]=255;}
    else {rgba[0]=bgr[2];rgba[1]=bgr[1];rgba[2]=bgr[0];rgba[3]=n==4?bgr[3]:255;}
    return 1;
}
int rtcw_texture_tga(const char *filename){
    FILE *f;unsigned char header[18],rgba[4]={0,0,0,255};
    unsigned int w,h,kind,bpp,descriptor,bytespp;
    unsigned long pixels,p=0,run,i,n,offset,dest;
    int rle,repeat;
    rtcw_texture_current.width=0;rtcw_texture_current.height=0;
    f=fopen(filename,"rb");if(!f)return terr("cannot open TGA");
    if(fread(header,1,18,f)!=18){fclose(f);return terr("short TGA header");}
    kind=header[2];bpp=header[16];descriptor=header[17];
    if(header[1]!=0 || le16(header+5)!=0 || (kind!=2&&kind!=10&&kind!=3&&kind!=11)){
        fclose(f);return terr("unsupported TGA type/color map");
    }
    if((kind==2||kind==10)?(bpp!=24&&bpp!=32):(bpp!=8)){
        fclose(f);return terr("unsupported TGA pixel depth");
    }
    if((descriptor&0xC0)!=0){fclose(f);return terr("TGA interleave unsupported");}
    w=le16(header+12);h=le16(header+14);pixels=(unsigned long)w*(unsigned long)h;
    if(w==0||h==0||w>RTCW_TEXTURE_MAX_DIM||h>RTCW_TEXTURE_MAX_DIM||pixels>RTCW_TEXTURE_MAX_PIXELS){
        fclose(f);return terr("TGA image capacity exceeded");
    }
    bytespp=bpp/8U;rle=(kind==10||kind==11);
    if(header[0]!=0 && fseek(f,(long)header[0],SEEK_CUR)!=0){fclose(f);return terr("TGA ID truncated");}
    while(p<pixels){
        run=1;repeat=0;
        if(rle){int packet=fgetc(f);if(packet==EOF){fclose(f);return terr("TGA RLE truncated");}
            run=(unsigned long)((packet&127)+1);repeat=(packet&128)!=0;
            if(run>pixels-p){fclose(f);return terr("TGA RLE overrun");}
        }
        for(i=0;i<run;i++){
            if(i==0||!repeat){if(!read_pixel(f,bpp,rgba)){fclose(f);return terr("TGA pixels truncated");}}
            offset=p+i;
            /* Bit 5: top-origin; bit 4: right-origin. Normalize to top-left. */
            n=offset/(unsigned long)w;
            dest=(unsigned long)((descriptor&0x20)?n:((unsigned long)h-1UL-n))*(unsigned long)w;
            n=offset%(unsigned long)w;
            dest+=(descriptor&0x10)?((unsigned long)w-1UL-n):n;
            dest*=4UL;
            rtcw_texture_current.pixel[dest]=rgba[0];
            rtcw_texture_current.pixel[dest+1]=rgba[1];
            rtcw_texture_current.pixel[dest+2]=rgba[2];
            rtcw_texture_current.pixel[dest+3]=rgba[3];
        }
        p+=run;
    }
    /* Unused byte count is intentional; bpp byte depth was validated above. */
    (void)bytespp;
    if(ferror(f)){fclose(f);return terr("TGA input error");}
    fclose(f);rtcw_texture_current.width=w;rtcw_texture_current.height=h;
    strcpy(tex_error,"OK");return 1;
}

static int ascii_eq(char a,char b){
    if(a>='A'&&a<='Z')a=(char)(a+32);
    if(b>='A'&&b<='Z')b=(char)(b+32);
    return a==b;
}
static int ends_with(const char *s,const char *ext){
    size_t n=strlen(s),m=strlen(ext),i;
    if(n<m)return 0;
    for(i=0;i<m;i++)if(!ascii_eq(s[n-m+i],ext[i]))return 0;
    return 1;
}
int rtcw_texture_load(const char *name){
    if(!name)return terr("missing texture filename");
    if(ends_with(name,".tga"))return rtcw_texture_tga(name);
    if(ends_with(name,".jpg")||ends_with(name,".jpeg")){
        if(rtcw_texture_jpeg(name)){strcpy(tex_error,"OK");return 1;}
        return terr(rtcw_jpeg_error());
    }
    return terr("unsupported texture extension");
}
