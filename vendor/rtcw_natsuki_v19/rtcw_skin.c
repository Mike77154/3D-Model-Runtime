/* Original independent RTCW skin material map parser, CC0-1.0. */
#include "rtcw_codec.h"
#include <stdio.h>
#include <string.h>
static int lower_ascii(int c) { if(c>='A' && c<='Z')return c+('a'-'A');return c; }
static int equal_ascii(const char *a,const char *b) {
    while(*a && *b) { if(lower_ascii((unsigned char)*a)!=lower_ascii((unsigned char)*b))return 0; a++;b++; }
    return *a==0 && *b==0;
}
static char *trim(char *s) {
    char *e;
    while(*s==' ' || *s=='\t' || *s=='\r' || *s=='\n')s++;
    e=s+strlen(s);while(e>s && (e[-1]==' ' || e[-1]=='\t' || e[-1]=='\r' || e[-1]=='\n'))*--e=0;
    return s;
}
int rtcw_apply_skin(const char *filename) {
    static char row[1024];
    FILE *f;unsigned int i;char *comma,*key,*val;int chars,matched=0;
    if(rtcw_current.surface_count==0)return 0;
    f=fopen(filename,"rb");if(!f)return 0;
    while(fgets(row,sizeof(row),f)!=NULL) {
        size_t n=strlen(row);
        if(n && row[n-1]!='\n' && !feof(f)) {
            /* Discard overlong line, rejecting ambiguous material names. */
            while((chars=fgetc(f))!='\n' && chars!=EOF){}
            fclose(f);return 0;
        }
        key=trim(row);if(!*key || *key=='#' || (key[0]=='/' && key[1]=='/'))continue;
        comma=strchr(key,',');if(!comma)continue;
        *comma++=0;key=trim(key);val=trim(comma);
        if(!*key || !*val || strlen(val)>64)continue;
        for(i=0;i<rtcw_current.surface_count;i++) {
            if(equal_ascii(key,rtcw_current.surfaces[i].name)) {
                strcpy(rtcw_current.surfaces[i].shader,val);matched++;break;
            }
        }
    }
    if(ferror(f)){fclose(f);return 0;}
    fclose(f);return matched+1; /* 1: readable, 2+: mapped surfaces */
}
