/* CC0-1.0: fixed-capacity color contact sheets 4x3, 160x160 tiles.
   C89, no malloc, no dependencies, no floating point. */
#include "rtcw_contact_color.h"
#include <stdio.h>
#include <string.h>
#define DIM 640U
#define THUMB 160U
#define COLS 4U
#define COUNT 12U
#define SW (COLS*THUMB)
#define SH (3U*THUMB)
static unsigned char sheet[SW*SH*3UL];
static unsigned char scan[DIM*3U];
static char problem[160]="OK";
static int fail(const char *msg){strncpy(problem,msg,sizeof(problem)-1U);problem[sizeof(problem)-1U]=0;return 0;}
const char *rtcw_contact_color_error(void){return problem;}
static int join(char *dst,const char *folder,const char *leaf){size_t n=strlen(folder);
    if(!n||n+strlen(leaf)+2U>=768U)return fail("contact path too long");
    strcpy(dst,folder);if(dst[n-1U]!='/'&&dst[n-1U]!='\\')strcat(dst,"/");
    strcat(dst,leaf);return 1;
}
static int tile(const char *folder,unsigned int n,unsigned int slot){
    char leaf[64],full[768],header[32];FILE *f;
    unsigned int x,y,r,c;long offset,length;
    sprintf(leaf,"pose_%03u_color.ppm",n);
    if(!join(full,folder,leaf))return 0;
    f=fopen(full,"rb");if(!f)return fail("missing color screenshot");
    if(!fgets(header,sizeof(header),f)||strcmp(header,"P6\n")!=0||
       !fgets(header,sizeof(header),f)||strcmp(header,"640 640\n")!=0||
       !fgets(header,sizeof(header),f)||strcmp(header,"255\n")!=0){
        fclose(f);return fail("unexpected color PPM header");
    }
    offset=ftell(f);
    if(offset<0L||fseek(f,0L,SEEK_END)!=0){fclose(f);return fail("color PPM seek failed");}
    length=ftell(f);
    if(length<0L||length-offset!=(long)(DIM*DIM*3UL)||fseek(f,offset,SEEK_SET)!=0){
        fclose(f);return fail("color PPM size invalid");
    }
    r=slot/COLS;c=slot%COLS;
    for(y=0U;y<DIM;y++){
        if(fread(scan,1,sizeof(scan),f)!=sizeof(scan)){fclose(f);return fail("color PPM truncated");}
        if(y%4U)continue;
        for(x=0U;x<DIM;x+=4U){
            unsigned long a=((unsigned long)(r*THUMB+y/4U)*SW+c*THUMB+x/4U)*3UL;
            sheet[a]=scan[x*3U];sheet[a+1UL]=scan[x*3U+1U];sheet[a+2UL]=scan[x*3U+2U];
        }
    }
    if(fclose(f)!=0)return fail("color PPM close error");
    return 1;
}
int rtcw_contact_color_generate(const char *folder,unsigned int count){
    unsigned int pages,page,slot,index;unsigned long i;char leaf[64],full[768];FILE *f;
    if(!folder||!count||count>96U)return fail("invalid color contact count");
    pages=(count+COUNT-1U)/COUNT;
    for(page=0U;page<pages;page++){
        for(i=0UL;i<sizeof(sheet);i+=3UL){sheet[i]=27U;sheet[i+1UL]=30U;sheet[i+2UL]=41U;}
        for(slot=0U;slot<COUNT;slot++){
            index=page*COUNT+slot;if(index>=count)break;
            if(!tile(folder,index,slot))return 0;
        }
        sprintf(leaf,"contact_color_%03u.ppm",page);
        if(!join(full,folder,leaf))return 0;
        f=fopen(full,"wb");if(!f)return fail("cannot create color sheet");
        if(fprintf(f,"P6\n%u %u\n255\n",SW,SH)<0||
           fwrite(sheet,1,sizeof(sheet),f)!=sizeof(sheet)){
            fclose(f);return fail("color sheet write failed");
        }
        if(fclose(f)!=0)return fail("color sheet close failed");
    }
    return 1;
}
