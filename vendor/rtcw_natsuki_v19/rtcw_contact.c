/* CC0 v0.12: P6 thumbnails, bounded, integer-only, no heap / dependencies.
   Fixed 4x3 contact sheets, nearest-neighbor at quarter resolution. */
#include "rtcw_contact.h"
#include <stdio.h>
#include <string.h>
#define SRC_WIDTH 800U
#define SRC_HEIGHT 800U
#define TILE_WIDTH 200U
#define TILE_HEIGHT 200U
#define COLS 4U
#define ROWS 3U
#define SHEET_WIDTH (COLS*TILE_WIDTH)
#define SHEET_HEIGHT (ROWS*TILE_HEIGHT)
#define PER_SHEET (COLS*ROWS)
static unsigned char sheet[SHEET_WIDTH*SHEET_HEIGHT*3U];
static unsigned char scan[SRC_WIDTH*3U];
static char problem[160]="OK";
const char *rtcw_contact_error(void){return problem;}
static int fail(const char *s){strncpy(problem,s,sizeof(problem)-1U);problem[sizeof(problem)-1U]=0;return 0;}
static int file_path(char *out,const char *folder,const char *name){size_t n=strlen(folder),k=strlen(name);
    if(n<1U||n+k+2U>=768U)return fail("contact sheet path too long");
    strcpy(out,folder);
    if(out[n-1U]!='/'&&out[n-1U]!='\\')strcat(out,"/");
    strcat(out,name);return 1;
}
static int add_thumbnail(const char *folder,unsigned int number,unsigned int slot){
    char filepath[768],leaf[40],h1[32],h2[32],h3[32];
    FILE *fp;unsigned int w,h,y,x,ty,tx;unsigned long offset;
    sprintf(leaf,"pose_%03u.ppm",number);
    if(!file_path(filepath,folder,leaf))return 0;
    fp=fopen(filepath,"rb");if(!fp)return fail("missing snapshot PPM for contact sheet");
    if(!fgets(h1,sizeof(h1),fp)||strcmp(h1,"P6\n")!=0||
       !fgets(h2,sizeof(h2),fp)||!fgets(h3,sizeof(h3),fp)){
        fclose(fp);return fail("unexpected PPM header");
    }
    if(sscanf(h2,"%u %u",&w,&h)!=2||w!=SRC_WIDTH||h!=SRC_HEIGHT||strcmp(h3,"255\n")!=0){
        fclose(fp);return fail("contact sheet accepts only generated 800x800 P6");
    }
    offset= (unsigned long)ftell(fp);
    /* Pixel data is untrusted; verify file covers its full expected size. */
    if(fseek(fp,0L,SEEK_END)!=0||ftell(fp)<0L||
       (unsigned long)ftell(fp)<offset+(unsigned long)SRC_WIDTH*SRC_HEIGHT*3UL){
        fclose(fp);return fail("truncated PPM image");
    }
    if(fseek(fp,(long)offset,SEEK_SET)!=0){fclose(fp);return fail("PPM seek failed");}
    for(y=0U;y<SRC_HEIGHT;y++){
        if(fread(scan,1,sizeof(scan),fp)!=sizeof(scan)){fclose(fp);return fail("PPM pixel read failed");}
        if(y%4U!=0U)continue;
        ty=(slot/COLS)*TILE_HEIGHT+y/4U;
        for(x=0U;x<SRC_WIDTH;x+=4U){
            unsigned long d=((unsigned long)ty*SHEET_WIDTH+(slot%COLS)*TILE_WIDTH+x/4U)*3UL;
            unsigned int s=x*3U;tx=(unsigned int)d;
            sheet[tx]=scan[s];sheet[tx+1U]=scan[s+1U];sheet[tx+2U]=scan[s+2U];
        }
    }
    if(fclose(fp)!=0)return fail("PPM close failed");
    return 1;
}
int rtcw_contact_generate(const char *folder,unsigned int shots){
    unsigned int page,pages,n,slot;char filepath[768],leaf[40];FILE *out;
    if(!folder||shots==0U||shots>96U)return fail("contact snapshot count invalid");
    pages=(shots+PER_SHEET-1U)/PER_SHEET;
    for(page=0U;page<pages;page++){
        unsigned long i;for(i=0UL;i<(unsigned long)sizeof(sheet);i+=3UL){
            sheet[i]=27U;sheet[i+1UL]=30U;sheet[i+2UL]=41U;
        }
        for(slot=0U;slot<PER_SHEET;slot++){
            n=page*PER_SHEET+slot;
            if(n>=shots)break;
            if(!add_thumbnail(folder,n,slot))return 0;
        }
        sprintf(leaf,"contact_%03u.ppm",page);
        if(!file_path(filepath,folder,leaf))return 0;
        out=fopen(filepath,"wb");if(!out)return fail("cannot create contact sheet");
        if(fprintf(out,"P6\n%u %u\n255\n",SHEET_WIDTH,SHEET_HEIGHT)<0||
           fwrite(sheet,1,sizeof(sheet),out)!=sizeof(sheet)){
            fclose(out);return fail("contact sheet write failed");
        }
        if(fclose(out)!=0)return fail("contact sheet close failed");
    }
    return 1;
}
