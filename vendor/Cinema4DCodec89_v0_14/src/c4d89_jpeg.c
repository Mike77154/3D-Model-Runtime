#include "c4d89_internal.h"
#include <limits.h>
/* Walk JPEG marker grammar; never assume the next FF D9 by naive scan is valid.
   Strict bounds, bounded output; no decoder and no external dependencies. */
static int c4d89_u16(FILE *f)
{
    int a=fgetc(f),b=fgetc(f);
    return (a==EOF||b==EOF)?-1:((a<<8)|b);
}
static int c4d89_sof(int m)
{
    return (m>=0xc0 && m<=0xcf && m!=0xc4 && m!=0xc8 && m!=0xcc);
}
int c4d89_jpeg_at(FILE *f,long start,long total,C4D89_Image *image)
{
    int x,y,marker=-1,sof=0,sos=0,in_entropy=0;
    unsigned int width=0u,height=0u,comp=0u;
    if(start<8L || total-start<6L || fseek(f,start,SEEK_SET)!=0) return 0;
    x=fgetc(f); y=fgetc(f);
    if(x!=0xff||y!=0xd8) return 0;
    while(ftell(f)>=0L && ftell(f)<total && ftell(f)-start<=67108864L) {
        long seg_end; int l,m;
        if(in_entropy) {
            x=fgetc(f);
            if(x==EOF) return 0;
            if(x!=0xff) continue;
            do {m=fgetc(f);} while(m==0xff);
            if(m==EOF) return 0;
            if(m==0x00||(m>=0xd0&&m<=0xd7)) continue;
            in_entropy=0;
            marker=m;
        }
        if(marker>=0) {m=marker;marker=-1;}
        else {
            x=fgetc(f);
            if(x!=0xff) return 0;
            do {m=fgetc(f);} while(m==0xff);
            if(m==EOF || m==0x00) return 0;
        }
        if(m==0xd9) {
            long end=ftell(f);
            if(!sof||!sos||end<=start||end-start>67108864L) return 0;
            image->offset=(unsigned long)start;
            image->length=(unsigned long)(end-start);
            image->width=width; image->height=height; image->components=comp;
            return 1;
        }
        if(m==0xd8||(m>=0xd0&&m<=0xd7)||m==0x01) return 0;
        l=c4d89_u16(f);
        if(l<2) return 0;
        seg_end=ftell(f)+(long)(l-2);
        if(seg_end>total || seg_end<0L || seg_end-start>67108864L) return 0;
        if(c4d89_sof(m)) {
            int precision=fgetc(f),h=c4d89_u16(f),w=c4d89_u16(f),n=fgetc(f);
            if(precision<1||h<1||w<1||n<1||n>4) return 0;
            if((long)w>32768L || (long)h>32768L) return 0;
            height=(unsigned int)h; width=(unsigned int)w;
            comp=(unsigned int)n; sof=1;
        }
        if(m==0xda) {if(!sof) return 0;sos=1;in_entropy=1;}
        if(fseek(f,seg_end,SEEK_SET)!=0) return 0;
    }
    return 0;
}
int c4d89_scan_images(FILE *f,long filesize,C4D89_Report *rep)
{
    long pos; int c;
    if(fseek(f,8L,SEEK_SET)!=0) return C4D89_ERR_IO;
    while((pos=ftell(f))>=0L && pos<filesize-2L && (c=fgetc(f))!=EOF) {
        int next;
        C4D89_Image im;
        if(c!=0xff) continue;
        next=fgetc(f);
        if(next==0xd8 && c4d89_jpeg_at(f,pos,filesize,&im)) {
            if(rep->image_count<C4D89_MAX_IMAGES)
                rep->images[rep->image_count++]=im;
            else rep->images_over_limit++;
            if(im.offset+im.length<=(unsigned long)filesize)
                fseek(f,(long)(im.offset+im.length),SEEK_SET);
            continue;
        }
        /* Failed candidate: resume one byte later. */
        if(fseek(f,pos+1L,SEEK_SET)!=0) return C4D89_ERR_IO;
    }
    return ferror(f) ? C4D89_ERR_IO : C4D89_OK;
}
