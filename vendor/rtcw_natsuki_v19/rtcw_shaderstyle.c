/* Source-independent shader diagnostic/parser. CC0-1.0, C89, fixed buffers. */
#include "rtcw_shaderstyle.h"
#include <stdio.h>
#include <string.h>
static char contents[512UL*1024UL];
static char style_error[120]="OK";
static int fail(const char *s){strncpy(style_error,s,119);style_error[119]=0;return -1;}
const char *rtcw_shaderstyle_error(void){return style_error;}
static int low(int c){return c>='A'&&c<='Z'?c+32:c;}
static int equals(const char *a,const char *b){
    while(*a&&*b){if(low((unsigned char)*a)!=low((unsigned char)*b))return 0;a++;b++;}
    return !*a&&!*b;
}
static int next(const char **pp,char *out,unsigned int cap){
    const char *p=*pp;unsigned int n=0;int quote=0;
    for(;;){
        while(*p==' '||*p=='\t'||*p=='\r'||*p=='\n')p++;
        if(p[0]=='/'&&p[1]=='/'){p+=2;while(*p&&*p!='\n')p++;continue;}
        if(p[0]=='/'&&p[1]=='*'){
            p+=2;while(*p&&!(p[0]=='*'&&p[1]=='/'))p++;
            if(!*p)return -1;
            p+=2;continue;
        }
        break;
    }
    if(!*p){out[0]=0;*pp=p;return 0;}
    if(*p=='{'||*p=='}'){out[0]=*p++;out[1]=0;*pp=p;return 1;}
    if(*p=='"'){quote=1;p++;}
    while(*p){
        if(quote){if(*p=='"'){p++;break;}}
        else if(*p==' '||*p=='\n'||*p=='\t'||*p=='\r'||*p=='{'||*p=='}')break;
        if(n+1U>=cap)return -1;
        out[n++]=*p++;
    }
    if(quote&&p[-1]!='"')return -1;
    if(!n)return -1;
    out[n]=0;*pp=p;return 1;
}
int rtcw_shaderstyle_lookup(const char *filename,const char *material,
                            rtcw_shaderstyle *out){
    FILE *f;size_t n;const char *p;char word[256],previous[256],a[256],b[256];
    int st,depth=0,matching=0,seen=0;
    if(!out||!material||!filename)return fail("invalid shader lookup arguments");
    out->blend=RTCW_BLEND_AUTO;out->alpha_func=RTCW_ALPHA_NONE;
    out->stage_count=0;out->unhandled=0;
    if(strlen(material)>240U)return fail("material path too long");
    f=fopen(filename,"rb");if(!f)return fail("cannot open shader file");
    n=fread(contents,1,sizeof(contents)-1UL,f);
    if(ferror(f)||!feof(f)){fclose(f);return fail("shader too large or read error");}
    if(fclose(f)!=0)return fail("shader close failure");
    if(memchr(contents,0,n)!=NULL)return fail("binary shader data rejected");
    contents[n]=0;p=contents;previous[0]=0;
    while((st=next(&p,word,sizeof(word)))>0){
        if(equals(word,"{")){
            if(depth==0)matching=equals(previous,material);
            depth++;
            if(matching&&depth==2){out->stage_count++;seen=1;}
        }else if(equals(word,"}")){
            if(depth==0)return fail("unbalanced shader braces");
            depth--;
            if(depth==0&&matching)break;
        }else if(matching&&depth==2&&out->stage_count==1U){
            if(equals(word,"alphafunc")){
                if(next(&p,a,sizeof(a))<=0)return fail("missing alphaFunc argument");
                if(equals(a,"GT0"))out->alpha_func=RTCW_ALPHA_GT0;
                else if(equals(a,"GE128"))out->alpha_func=RTCW_ALPHA_GE128;
                else if(equals(a,"LT128"))out->alpha_func=RTCW_ALPHA_LT128;
                else out->unhandled++;
            }else if(equals(word,"blendfunc")){
                if(next(&p,a,sizeof(a))<=0)return fail("missing blendFunc argument");
                if(equals(a,"blend"))out->blend=RTCW_BLEND_ALPHA;
                else if(equals(a,"add"))out->blend=RTCW_BLEND_ADD;
                else if(equals(a,"filter"))out->blend=RTCW_BLEND_FILTER;
                else{
                    if(next(&p,b,sizeof(b))<=0)return fail("missing blendFunc destination factor");
                    if(equals(a,"GL_SRC_ALPHA")&&equals(b,"GL_ONE_MINUS_SRC_ALPHA"))out->blend=RTCW_BLEND_ALPHA;
                    else if(equals(a,"GL_ONE")&&equals(b,"GL_ONE"))out->blend=RTCW_BLEND_ADD;
                    else if(equals(a,"GL_DST_COLOR")&&equals(b,"GL_ZERO"))out->blend=RTCW_BLEND_FILTER;
                    else out->unhandled++;
                }
            }
        }
        if(depth==0&&word[0]!='{'&&word[0]!='}'){
            strcpy(previous,word);
        }
    }
    if(st<0||depth!=0)return fail("invalid shader token or unbalanced braces");
    if(out->stage_count>1U)out->unhandled+=out->stage_count-1U;
    strcpy(style_error,"OK");return seen?1:0;
}
