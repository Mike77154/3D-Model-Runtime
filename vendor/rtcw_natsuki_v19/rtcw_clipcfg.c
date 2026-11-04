/* CC0 / C89 strict, fixed line buffer, positive integer-only .cfg range candidate.
   Numeric four-column rows are indexed. No guessing labels or fps conversion. */
#include "rtcw_clipcfg.h"
#include <stdio.h>
#include <string.h>
#include <limits.h>
static int digit(int c){return c>='0'&&c<='9';}
static int parse4(const char *s,unsigned long fields[4]){
    unsigned int i;unsigned long val;
    for(i=0;i<4U;i++){
        while(*s==' '||*s=='\t')s++;
        if(!digit((unsigned char)*s))return 0;
        val=0;
        do{
            unsigned int n=(unsigned int)(*s-'0');
            if(val>(ULONG_MAX-n)/10UL)return 0;
            val=val*10UL+(unsigned long)n;s++;
        }while(digit((unsigned char)*s));
        fields[i]=val;
        if(i!=3U && *s!=' ' && *s!='\t')return 0;
    }
    while(*s==' '||*s=='\t'||*s=='\r'||*s=='\n')s++;
    if(*s && !(s[0]=='/'&&s[1]=='/') && *s!='#')return 0;
    if(fields[1]==0UL||fields[3]==0UL||fields[3]>1000UL)return 0;
    if(fields[0]>ULONG_MAX-fields[1])return 0;
    return 1;
}
int rtcw_clipcfg_append(const char *input,const char *source,FILE *table,
                        unsigned long *rows){
    char line[1024];FILE *in;unsigned long r=0,line_no=0,vals[4];int ch;
    if(!input||!source||!table||!rows)return 0;
    in=fopen(input,"rb");if(!in)return 0;
    while(fgets(line,sizeof(line),in)){
        size_t n=strlen(line);line_no++;
        if(n && line[n-1]!='\n' && !feof(in)){
            while((ch=fgetc(in))!='\n'&&ch!=EOF){}
            fclose(in);return 0;
        }
        if(parse4(line,vals)){
            if(fprintf(table,"%s\t%lu\t%lu\t%lu\t%lu\t%lu\tframe_table_candidate\n",
                source,line_no,vals[0],vals[1],vals[2],vals[3])<0){fclose(in);return 0;}
            r++;
        }
    }
    if(ferror(in)){fclose(in);return 0;}
    fclose(in);*rows+=r;return 1;
}
