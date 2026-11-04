/* Independent small RTCW material resolver, CC0-1.0. Does not evaluate shader pipelines. */
#include "rtcw_material.h"
#include <stdio.h>
#include <string.h>
static char material_error[160]="OK";
static char source[512UL*1024UL];
const char *rtcw_material_error(void){return material_error;}
static int fail(const char *s){strncpy(material_error,s,159);material_error[159]=0;return 0;}
static int lower(int c){return (c>='A'&&c<='Z')?c+32:c;}
static int eq(const char *a,const char *b){while(*a&&*b){if(lower((unsigned char)*a++)!=lower((unsigned char)*b++))return 0;}return !*a&&!*b;}
static int sane(const char *name){
    size_t i,n=strlen(name);
    if(n==0||n>240||name[0]=='/'||name[0]=='\\'||strchr(name,':'))return 0;
    for(i=0;i<n;i++){
        if((name[i]=='.'&&i+1<n&&name[i+1]=='.')||
           (unsigned char)name[i]<32U||name[i]=='\\')return 0;
    }
    return 1;
}
static int makepath(const char *root,const char *name,char *out,unsigned int cap){
    size_t a,b;char cand[512],base[512];FILE *f;unsigned int i;int ext=0;
    static const char *trials[]={".tga",".jpg",".jpeg"};
    if(!sane(name)||!root||!root[0])return fail("unsafe or missing material path");
    a=strlen(root);b=strlen(name);
    if(a+b+9U>=sizeof(base))return fail("path too long");
    strcpy(base,root);if(a&&base[a-1U]!='/'&&base[a-1U]!='\\')strcat(base,"/");strcat(base,name);
    for(i=(unsigned int)b;i>0U;i--){if(name[i-1U]=='/')break;if(name[i-1U]=='.'){ext=1;break;}}
    for(i=0;i<(ext?1U:3U);i++){
        strcpy(cand,base);if(!ext)strcat(cand,trials[i]);
        if(strlen(cand)+1U>cap)return fail("output path capacity exceeded");
        f=fopen(cand,"rb");if(f){fclose(f);strcpy(out,cand);strcpy(material_error,"OK");return 1;}
    }
    return fail("texture not found (or format unsupported)");
}
/* Time-aware first-stage shader resolver. Conservative subset of Quake3 shader grammar:
   map/clampmap or animMap on a single line. No VM or shader execution. */
static char cached_shader_file[768];
static int cached_shader_valid=0;
void rtcw_material_cache_clear(void){cached_shader_file[0]=0;cached_shader_valid=0;}
static int read_shader(const char *filename){
    FILE *f;size_t n;
    if(cached_shader_valid && strcmp(filename,cached_shader_file)==0)return 1;
    cached_shader_valid=0;
    if(strlen(filename)>=sizeof(cached_shader_file))return 0;
    f=fopen(filename,"rb");if(!f)return 0;
    n=fread(source,1,sizeof(source)-1U,f);
    if(ferror(f)||!feof(f)){fclose(f);return 0;}
    if(fclose(f)!=0)return 0;
    if(memchr(source,0,n)!=NULL)return 0;
    source[n]=0;strcpy(cached_shader_file,filename);cached_shader_valid=1;return 1;
}
/* Consume a single token and record its source line. Comment-aware. */
static int token_time(const char **pp,char *out,unsigned int cap,
                      unsigned long *line,unsigned long *tok_line){
    const char *p=*pp;unsigned int i=0U;int quoted=0;
    for(;;){
        while(*p==' '||*p=='\t'||*p=='\r'||*p=='\n'){if(*p=='\n')(*line)++;p++;}
        if(p[0]=='/'&&p[1]=='/'){p+=2;while(*p&&*p!='\n')p++;continue;}
        if(p[0]=='/'&&p[1]=='*'){
            p+=2;while(*p&&!(p[0]=='*'&&p[1]=='/')){if(*p=='\n')(*line)++;p++;}
            if(!*p)return -1;
            p+=2;continue;
        }
        break;
    }
    if(!*p){*pp=p;out[0]=0;return 0;}
    *tok_line=*line;
    if(*p=='{'||*p=='}'){out[0]=*p++;out[1]=0;*pp=p;return 1;}
    if(*p=='"'){quoted=1;p++;}
    while(*p){
        if(quoted){if(*p=='"'){p++;break;}}
        else if(*p=='{'||*p=='}'||*p==' '||*p=='\t'||*p=='\r'||*p=='\n')break;
        if(*p=='\n')(*line)++;
        if(i+1U>=cap)return -1;
        out[i++]=*p++;
    }
    out[i]=0;*pp=p;return i>0U?1:-1;
}
/* Frames per second in Q1000; bounded to 1000 fps, 3 decimal places. */
static int frequency(const char *s,unsigned long *out){
    unsigned long whole=0UL,fract=0UL;unsigned int digits=0U,seen_dot=0U;
    if(!*s)return 0;
    while(*s){
        if(*s=='.'&&!seen_dot){seen_dot=1U;s++;continue;}
        if(*s<'0'||*s>'9')return 0;
        if(!seen_dot){whole=whole*10UL+(unsigned long)(*s-'0');if(whole>1000UL)return 0;}
        else{if(digits>=3U)return 0;fract=fract*10UL+(unsigned long)(*s-'0');digits++;}
        s++;
    }
    while(digits<3U){fract*=10UL;digits++;}
    *out=whole*1000UL+fract;
    return *out>0UL&&*out<=1000000UL;
}
static unsigned long addmod(unsigned long a,unsigned long b,unsigned long m){
    return a>=m-b?a-(m-b):a+b;
}
static unsigned long mulmod(unsigned long a,unsigned long b,unsigned long m){
    unsigned long r=0UL;
    a%=m;
    while(b){if(b&1UL)r=addmod(r,a,m);b>>=1;if(b)a=addmod(a,a,m);}
    return r;
}
int rtcw_material_reference_time(const char *shader_file,const char *material,
                                 unsigned long time_ms,char *out,unsigned int cap){
    const char *p,*prevp;char word[256],prev[256],chosen[256],freq[256];
    static char frames[32][256];
    int status,depth=0,match=0,found=0;unsigned int count;
    unsigned long line=1UL,tokline=1UL,oldline,frq,idx,animline;
    if(!shader_file||!material||!sane(material)||!out||cap==0U)return -1;
    out[0]=0;
    if(!read_shader(shader_file))return -1;
    p=source;prev[0]=0;chosen[0]=0;
    while((status=token_time(&p,word,sizeof(word),&line,&tokline))>0){
        if(eq(word,"{")){
            if(depth==0)match=eq(prev,material);
            depth++;
        }else if(eq(word,"}")){
            if(depth<=0)return -1;
            depth--;
            if(depth==0&&match)break;
        }else if(match&&depth==2&&!chosen[0]){
            if(eq(word,"map")||eq(word,"clampmap")){
                status=token_time(&p,word,sizeof(word),&line,&tokline);
                if(status<=0||eq(word,"{" )||eq(word,"}"))return -1;
                if(word[0]!='$'&&word[0]!='*'){strcpy(chosen,word);found=1;}
            }else if(eq(word,"animmap")){
                status=token_time(&p,freq,sizeof(freq),&line,&tokline);
                if(status<=0||!frequency(freq,&frq))return -1;
                animline=tokline;count=0U;
                for(;;){
                    prevp=p;oldline=line;
                    status=token_time(&p,word,sizeof(word),&line,&tokline);
                    if(status<0)return -1;
                    if(status==0)break;
                    if(tokline!=animline||eq(word,"{")||eq(word,"}")||
                       eq(word,"rgbgen")||eq(word,"alphagen")||eq(word,"blendfunc")||
                       eq(word,"tcmod")||eq(word,"alphafunc")){
                        p=prevp;line=oldline;break;
                    }
                    if(count>=32U)return -1;
                    if(!sane(word))return -1;
                    strcpy(frames[count++],word);
                }
                if(count==0U)return -1;
                idx=mulmod(time_ms,frq,(unsigned long)count*1000000UL)/1000000UL;
                strcpy(chosen,frames[idx]);found=1;
            }
        }
        if(depth==0 && word[0]!='}' && word[0]!='{')strcpy(prev,word);
    }
    if(status<0||depth!=0)return -1;
    if(found){if(!sane(chosen)||strlen(chosen)+1U>cap)return -1;strcpy(out,chosen);return 1;}
    return 0;
}
int rtcw_material_reference(const char *shader_file,const char *material,char *out,unsigned int cap){
    return rtcw_material_reference_time(shader_file,material,0UL,out,cap);
}
int rtcw_material_path_time(const char *root,const char *shader_file,const char *material,
                            unsigned long time_ms,char *out,unsigned int cap){
    char chosen[256];int rc=0;
    if(!out||!cap)return fail("invalid output capacity");
    out[0]=0;
    if(!material||!sane(material))return fail("invalid material");
    if(shader_file&&strcmp(shader_file,"-")!=0){
        rc=rtcw_material_reference_time(shader_file,material,time_ms,chosen,sizeof(chosen));
        if(rc<0)return fail("invalid or unreadable shader script");
    }
    return makepath(root,rc==1?chosen:material,out,cap);
}

int rtcw_material_path(const char *root,const char *shader_file,const char *material,
                       char *out,unsigned int cap){
    return rtcw_material_path_time(root,shader_file,material,0UL,out,cap);
}
