/* Original bounded Quake3/RTCW shader-stage subset. CC0-1.0, C89. */
#include "rtcw_stagefx.h"
#include <stdio.h>
#include <string.h>
#include <limits.h>
static char source[512UL*1024UL];
static char err[128]="OK";
static int bad(const char *p){strncpy(err,p,127U);err[127]=0;return -1;}
const char *rtcw_fx_error(void){return err;}
static int low(int c){return c>='A'&&c<='Z'?c+32:c;}
static int eq(const char *a,const char *b){
    while(*a&&*b){if(low((unsigned char)*a)!=low((unsigned char)*b))return 0;a++;b++;}
    return !*a&&!*b;
}
/* Lexical token with line number; comments and strings permitted; bounded. */
static int tok(const char **pp,char *out,unsigned int cap,unsigned long *line,unsigned long *where){
    const char *p=*pp;unsigned int k=0U;int quote=0;
    for(;;){
        while(*p==' '||*p=='\r'||*p=='\n'||*p=='\t'){if(*p=='\n')(*line)++;p++;}
        if(p[0]=='/'&&p[1]=='/'){p+=2;while(*p&&*p!='\n')p++;continue;}
        if(p[0]=='/'&&p[1]=='*'){
            p+=2;while(*p&&!(p[0]=='*'&&p[1]=='/')){if(*p=='\n')(*line)++;p++;}
            if(!*p)return -1;
            p+=2;continue;
        }
        break;
    }
    *where=*line;
    if(!*p){*pp=p;out[0]=0;return 0;}
    if(*p=='{'||*p=='}'||*p=='('||*p==')'){
        out[0]=*p;out[1]=0;*pp=p+1;return 1;
    }
    if(*p=='"'){p++;quote=1;}
    while(*p){
        if(quote){if(*p=='"'){p++;break;}}
        else if(*p=='{'||*p=='}'||*p=='('||*p==')'||*p==' '||*p=='\r'||*p=='\t'||*p=='\n')break;
        if(k+1U>=cap)return -1;
        out[k++]=*p++;
    }
    if(!k||(quote&&p[-1]!='"'))return -1;
    out[k]=0;*pp=p;return 1;
}
static int number(const char *p,long *n){
    int neg=0,dot=0;unsigned int dec=0U;long val=0L;
    if(*p=='-'||*p=='+'){neg=*p=='-';p++;}
    if(!*p)return 0;
    while(*p){
        if(*p=='.'&&!dot){dot=1;p++;continue;}
        if(*p<'0'||*p>'9')return 0;
        if(dot){if(dec>=3U)return 0;dec++;}
        if(val>100000L)return 0;
        val=val*10L+(long)(*p-'0');p++;
    }
    while(dec<3U){val*=10L;dec++;}
    if(val>16000000L)return 0; /* max 16000.000; bound later per operand */
    *n=neg?-val:val;return 1;
}
static int scalar(const char **p,unsigned long *line,long *out){
    char s[256];unsigned long where;
    if(tok(p,s,sizeof(s),line,&where)<=0||!number(s,out))return 0;
    return 1;
}
static int component(long milli,unsigned int *out){
    if(milli<0L||milli>1000L)return 0;
    *out=(unsigned int)((milli*255L+500L)/1000L);return 1;
}
static int pathok(const char *s){
    size_t i,n=strlen(s);
    if(!n||n>240U||s[0]=='/'||s[0]=='\\'||strchr(s,':'))return 0;
    for(i=0;i<n;i++)if((unsigned char)s[i]<32U||s[i]=='\\'||
        (s[i]=='.'&&i+1U<n&&s[i+1U]=='.'))return 0;
    return 1;
}
/* Bounded modular product, safe on 32-bit unsigned long. */
static unsigned long mulmod(unsigned long a,unsigned long b,unsigned long m){
    unsigned long z=0UL;
    if(!m)return 0UL;
    a%=m;
    while(b){if(b&1UL)z=(z+a)%m;b>>=1;if(b)a=(a+a)%m;}
    return z;
}
static int add_map(rtcw_fx_stage *s,const char *p){
    if(!pathok(p))return 0;
    strcpy(s->map,p);return 1;
}
static int parse_anim(const char **p,unsigned long *line,rtcw_fx_stage *s,unsigned long now){
    char word[256],frames[32][256];unsigned int count=0U;
    unsigned long where,animline,frames_line,mod,step,idx;
    long fps;
    if(tok(p,word,sizeof(word),line,&where)<=0||!number(word,&fps)||fps<=0L||fps>1000000L)return 0;
    /* AnimMap is line-scoped as in the existing v0.15 decoder. */
    animline=where;
    frames_line=animline;
    for(;;){
        const char *before=*p;unsigned long oldline=*line;
        int r=tok(p,word,sizeof(word),line,&where);
        if(r<0)return 0;
        if(r==0)break;
        if(where!=frames_line||eq(word,"{")||eq(word,"}")||
           eq(word,"rgbGen")||eq(word,"blendFunc")||eq(word,"tcMod")||eq(word,"alphaFunc")||eq(word,"alphaGen")){
            *p=before;*line=oldline;break;
        }
        if(count>=32U||!pathok(word))return 0;
        strcpy(frames[count++],word);
    }
    if(!count)return 0;
    mod=(unsigned long)count*1000000UL;
    step=mulmod(now,(unsigned long)fps,mod);
    idx=step/1000000UL;
    return add_map(s,frames[idx]);
}
static int parse_blend(const char **p,unsigned long *line,int *blend){
    char a[256],b[256];unsigned long w;
    if(tok(p,a,sizeof(a),line,&w)<=0)return 0;
    if(eq(a,"blend")){*blend=RTCW_BLEND_ALPHA;return 1;}
    if(eq(a,"add")){*blend=RTCW_BLEND_ADD;return 1;}
    if(eq(a,"filter")){*blend=RTCW_BLEND_FILTER;return 1;}
    if(tok(p,b,sizeof(b),line,&w)<=0)return 0;
    if(eq(a,"GL_SRC_ALPHA")&&eq(b,"GL_ONE_MINUS_SRC_ALPHA"))*blend=RTCW_BLEND_ALPHA;
    else if(eq(a,"GL_ONE")&&eq(b,"GL_ONE"))*blend=RTCW_BLEND_ADD;
    else if(eq(a,"GL_DST_COLOR")&&eq(b,"GL_ZERO"))*blend=RTCW_BLEND_FILTER;
    else *blend=-1;
    return 1;
}
static int parse_tcmod(const char **p,unsigned long *line,rtcw_fx_stage *s){
    char op[256];unsigned long w;long a,b;
    if(tok(p,op,sizeof(op),line,&w)<=0)return 0;
    if(eq(op,"scale")||eq(op,"scroll")){
        if(!scalar(p,line,&a)||!scalar(p,line,&b))return 0;
        if(a< -16000L||a>16000L||b< -16000L||b>16000L)return 0;
        if(eq(op,"scale")){
            /* Ordered composition: a scroll before a scale is scaled too. */
            s->scale_s=s->scale_s*a/1000L;
            s->scale_t=s->scale_t*b/1000L;
            s->scroll_s=s->scroll_s*a/1000L;
            s->scroll_t=s->scroll_t*b/1000L;
        }else{
            s->scroll_s+=a*RTCW_FX_Q/1000L;
            s->scroll_t+=b*RTCW_FX_Q/1000L;
        }
        /* Keep all subsequent Q12 math safe even with malicious tcMod chains. */
        if(s->scale_s< -65536L||s->scale_s>65536L||
           s->scale_t< -65536L||s->scale_t>65536L||
           s->scroll_s< -65536L||s->scroll_s>65536L||
           s->scroll_t< -65536L||s->scroll_t>65536L)return 0;
        return 1;
    }
    /* Keep common unsupported operations aligned, but never simulate them. */
    if(eq(op,"rotate")){
        if(!scalar(p,line,&a))return 0;
    }else if(eq(op,"turb")||eq(op,"transform")){
        unsigned int k,arity=eq(op,"turb")?4U:6U;
        for(k=0U;k<arity;k++)if(!scalar(p,line,&a))return 0;
    }else if(eq(op,"stretch")){
        unsigned int k;unsigned long w;
        if(tok(p,op,sizeof(op),line,&w)<=0)return 0;
        for(k=0U;k<4U;k++)if(!scalar(p,line,&a))return 0;
    }
    s->unhandled++;return 1;
}
static int gen_rgb(const char **p,unsigned long *line,rtcw_fx_stage *s){
    char op[256];unsigned long w;long a;unsigned int j;
    if(tok(p,op,sizeof(op),line,&w)<=0)return 0;
    if(eq(op,"identity"))return 1;
    if(!eq(op,"const")){s->unhandled++;return 1;}
    if(tok(p,op,sizeof(op),line,&w)<=0||strcmp(op,"("))return 0;
    for(j=0U;j<3U;j++){
        if(!scalar(p,line,&a)||!component(a,&s->rgb[j]))return 0;
    }
    return tok(p,op,sizeof(op),line,&w)>0&&strcmp(op,")")==0;
}
static int gen_alpha(const char **p,unsigned long *line,rtcw_fx_stage *s){
    char op[256];unsigned long w;long a;
    if(tok(p,op,sizeof(op),line,&w)<=0)return 0;
    if(eq(op,"identity"))return 1;
    if(!eq(op,"const")){s->unhandled++;return 1;}
    if(!scalar(p,line,&a)||!component(a,&s->alpha))return 0;
    return 1;
}
long rtcw_fx_uv(long uv,long scale,long scroll,unsigned long now){
    long sec=(long)((now/1000UL)%4096UL),ms=(long)(now%1000UL);
    long shift=(sec*(scroll%4096L)+(ms*scroll/1000L))%4096L;
    long out=(uv/4096L)*scale+((uv%4096L)*scale)/4096L;
    return out+shift;
}
int rtcw_fx_lookup(const char *file,const char *material,unsigned long now,rtcw_fx *out){
    FILE *f;size_t n;const char *p;char word[256],prev[256],a[256];
    unsigned long line=1UL,where;int r,depth=0,matching=0,found=0;unsigned int i;
    rtcw_fx_stage *s;
    if(!file||!material||!out||strlen(material)>240U)return bad("invalid shader lookup");
    memset(out,0,sizeof(*out));
    f=fopen(file,"rb");if(!f)return bad("cannot open shader");
    n=fread(source,1,sizeof(source)-1U,f);
    if(ferror(f)||!feof(f)){fclose(f);return bad("shader file too large");}
    if(fclose(f)!=0||memchr(source,0,n))return bad("invalid shader input");
    source[n]=0;p=source;prev[0]=0;
    while((r=tok(&p,word,sizeof(word),&line,&where))>0){
        if(eq(word,"{")){
            if(depth==0)matching=eq(prev,material);
            depth++;
            if(matching&&depth==2){
                if(out->stages>=RTCW_FX_MAX_STAGES)return bad("shader has too many stages");
                s=&out->stage[out->stages++];
                s->blend=RTCW_BLEND_AUTO;s->alpha_func=RTCW_ALPHA_NONE;
                s->scale_s=4096L;s->scale_t=4096L;s->alpha=255U;
                for(i=0U;i<3U;i++)s->rgb[i]=255U;
                found=1;
            }
        }else if(eq(word,"}")){
            if(depth==0)return bad("unexpected closing brace");
            depth--;
            if(depth==0&&matching)break;
        }else if(matching&&depth==2&&out->stages){
            s=&out->stage[out->stages-1U];
            if(eq(word,"map")||eq(word,"clampmap")){
                if(tok(&p,a,sizeof(a),&line,&where)<=0)return bad("map missing argument");
                if(a[0]=='$'||a[0]=='*')s->unhandled++;
                else if(!add_map(s,a))return bad("invalid stage image path");
            }else if(eq(word,"animmap")){
                if(!parse_anim(&p,&line,s,now))return bad("invalid animmap");
            }else if(eq(word,"blendfunc")){
                if(!parse_blend(&p,&line,&s->blend))return bad("invalid blendfunc");
                if(s->blend<0){s->blend=RTCW_BLEND_ALPHA;s->unhandled++;}
            }else if(eq(word,"alphafunc")){
                if(tok(&p,a,sizeof(a),&line,&where)<=0)return bad("invalid alphafunc");
                if(eq(a,"GT0"))s->alpha_func=RTCW_ALPHA_GT0;
                else if(eq(a,"GE128"))s->alpha_func=RTCW_ALPHA_GE128;
                else if(eq(a,"LT128"))s->alpha_func=RTCW_ALPHA_LT128;
                else s->unhandled++;
            }else if(eq(word,"tcmod")){
                if(!parse_tcmod(&p,&line,s))return bad("invalid tcMod");
            }else if(eq(word,"rgbgen")){
                if(!gen_rgb(&p,&line,s))return bad("invalid rgbGen");
            }else if(eq(word,"alphagen")){
                if(!gen_alpha(&p,&line,s))return bad("invalid alphaGen");
            }else if(!eq(word,"(")&&!eq(word,")"))s->unhandled++;
        }
        if(depth==0&&word[0]!='{'&&word[0]!='}')strcpy(prev,word);
    }
    if(r<0||depth!=0)return bad("invalid shader tokens or brace balance");
    if(found){for(i=0U;i<out->stages;i++)out->unhandled+=out->stage[i].unhandled;}
    strcpy(err,"OK");return found?1:0;
}
