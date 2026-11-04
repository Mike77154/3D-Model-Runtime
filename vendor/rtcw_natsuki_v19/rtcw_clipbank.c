/* RTCW wolfanim.cfg and simple animation.cfg parser, CC0-1.0, C89. */
#include "rtcw_clipbank.h"
#include <stdio.h>
#include <string.h>
#include <limits.h>
static rtcw_clip bank[RTCW_CLIPS_MAX];
static unsigned int nbank;
static char error_text[160]="OK";
static unsigned long total_frames;
static unsigned int version_seen, version_number, saw_start, saw_end;
static int error(const char *s){strncpy(error_text,s,sizeof(error_text)-1U);error_text[sizeof(error_text)-1U]=0;return 0;}
static int fold(int c){return c>='A'&&c<='Z'?c+('a'-'A'):c;}
static int equal(const char *a,const char *b){while(*a&&*b){if(fold((unsigned char)*a)!=fold((unsigned char)*b))return 0;a++;b++;}return !*a&&!*b;}
static int digit(int c){return c>='0'&&c<='9';}
static int positive(const char *s,unsigned long *out){
    unsigned long n=0UL;
    if(!s||!*s)return 0;
    while(*s){unsigned long d;
        if(!digit((unsigned char)*s))return 0;
        d=(unsigned long)(*s-'0');if(n>(ULONG_MAX-d)/10UL)return 0;
        n=n*10UL+d;s++;
    }
    *out=n;return 1;
}
/* RTCW uses -1 for automatic movespeed calculation. */
static int signed_number(const char *s,long *out){
    unsigned long x,limit=(unsigned long)LONG_MAX;
    if(!s||!*s)return 0;
    if(*s=='-'){
        if(!positive(s+1,&x)||x>limit+1UL)return 0;
        if(x==limit+1UL)*out=LONG_MIN;
        else *out=-(long)x;
        return 1;
    }
    if(!positive(s,&x)||x>limit)return 0;
    *out=(long)x;return 1;
}
/* Parse positive decimal FPS into fixed milli-FPS, max 1000 FPS. */
static int fps_parse(const char *s,unsigned long *out){
    unsigned long whole=0UL,fract=0UL;unsigned int decimals=0U;
    if(!s||!digit((unsigned char)*s))return 0;
    while(digit((unsigned char)*s)){
        unsigned long d=(unsigned long)(*s++-'0');
        if(whole>1000UL)return 0;
        whole=whole*10UL+d;
    }
    if(*s=='.'){
        s++;if(!digit((unsigned char)*s))return 0;
        while(digit((unsigned char)*s)){
            if(decimals<3U)fract=fract*10UL+(unsigned long)(*s-'0');
            else if(*s!='0')return 0; /* exact fixed-point supported */
            decimals++;s++;
        }
    }
    if(*s||whole>1000UL)return 0;
    while(decimals<3U){fract*=10UL;decimals++;}
    *out=whole*1000UL+fract;
    return *out>0UL&&*out<=1000000UL;
}
/* Separate // and # comments, without reinterpreting source syntax. */
static unsigned int tokenize(char *s,char *cols[],unsigned int max){
    unsigned int n=0U;char *p=s;
    while(*p){
        while(*p==' '||*p=='\t'||*p=='\r'||*p=='\n')p++;
        if(!*p||*p=='#'||(p[0]=='/'&&p[1]=='/'))break;
        if(n>=max)return max+1U;
        cols[n++]=p;
        while(*p&&*p!=' '&&*p!='\t'&&*p!='\r'&&*p!='\n'){
            if(p[0]=='/'&&p[1]=='/')break;
            p++;
        }
        if(*p){
            if(p[0]=='/'&&p[1]=='/') { *p=0;break; }
            *p++=0;
        }
    }
    return n;
}
static int clip_add(char *cols[],unsigned int n,unsigned int named,unsigned long line){
    rtcw_clip *c;unsigned long vals[7];unsigned int i,offset;long movement=0L;
    if(nbank>=RTCW_CLIPS_MAX)return error("too many animation rows (max 512)");
    /* 4-column Quake style; 5+ RTCW wolfanim rows. Named v2 needs >=6. */
    offset=named?1U:0U;
    if(n<offset+((named||version_seen||saw_start)?5U:4U)||n>offset+7U)return error("unsupported animation row columns");
    for(i=0U;i<7U;i++)vals[i]=0UL;
    for(i=0U;i<n-offset;i++){
        if(i==3U){if(!fps_parse(cols[offset+i],&vals[i]))return error("invalid fixed-point fps");}
        else if(i==4U){if(!signed_number(cols[offset+i],&movement))return error("invalid signed move speed");}
        else if(!positive(cols[offset+i],&vals[i]))return error("invalid unsigned animation field");
    }
    if(vals[1]==0UL||vals[2]>vals[1]||vals[0]>ULONG_MAX-vals[1])
        return error("invalid frame range or loop count");
    c=&bank[nbank];memset(c,0,sizeof(*c));
    if(named){
        size_t j,l=strlen(cols[0]);
        if(l>RTCW_CLIP_NAME)return error("clip name too long");
        for(j=0U;j<l;j++){
            unsigned char ch=(unsigned char)cols[0][j];
            if(ch<'!'||ch>'~'||ch=='/'||ch=='\\')return error("unsafe clip name");
            c->name[j]=(char)fold(ch);
        }
        c->name[l]=0;
    }else{
        if(sprintf(c->name,"row_%03u",nbank)>= (int)sizeof(c->name))return error("clip name failed");
    }
    c->line=line;c->first=vals[0];c->count=vals[1];c->loop=vals[2];
    c->fps_milli=vals[3];c->move_speed=movement;c->blend=vals[5];c->priority=vals[6];
    c->valid= vals[0]+vals[1]<=total_frames;
    nbank++;return 1;
}
int rtcw_clipbank_read(const char *cfg,unsigned long frames){
    FILE *f;char buf[1024];char *cols[10];unsigned int n;unsigned long line=0UL;
    int in_anims=0,kind=0,ch;
    nbank=0U;total_frames=frames;version_seen=version_number=saw_start=saw_end=0U;
    if(!cfg||!cfg[0]||frames==0UL)return error("missing CFG or model frames");
    f=fopen(cfg,"rb");if(!f)return error("cannot open animation CFG");
    while(fgets(buf,sizeof(buf),f)){
        size_t len=strlen(buf);line++;
        if(len&&buf[len-1]!='\n'&&!feof(f)){
            while((ch=fgetc(f))!='\n'&&ch!=EOF){}
            fclose(f);return error("animation CFG line too long");
        }
        n=tokenize(buf,cols,9U);
        if(n>9U){fclose(f);return error("too many tokens on CFG line");}
        if(n==0U)continue;
        if(equal(cols[0],"version")&&n==2U){
            unsigned long v;
            if(!positive(cols[1],&v)||v>100UL){fclose(f);return error("invalid wolfanim version");}
            version_seen=1U;version_number=(unsigned int)v;continue;
        }
        if(equal(cols[0],"startanims")){
            if(saw_start){fclose(f);return error("duplicate STARTANIMS");}
            saw_start=1U;in_anims=1;kind=version_seen&&version_number>1U?2:1;continue;
        }
        if(equal(cols[0],"endanims")){
            if(!in_anims){fclose(f);return error("ENDANIMS without STARTANIMS");}
            saw_end=1U;in_anims=0;break;
        }
        if(equal(cols[0],"headframes")||equal(cols[0],"startheadanims"))break;
        if(!in_anims){
            if(saw_start)continue;
            if(digit((unsigned char)cols[0][0])){
                if(version_seen&&version_number>1U){fclose(f);return error("wolfanim v2 needs STARTANIMS");}
                if(!clip_add(cols,n,0U,line)){fclose(f);return 0;}
            }
            /* version, footsteps, skeletal, headoffset, sex etc are metadata. */
        }else{
            if(!clip_add(cols,n,kind==2?1U:0U,line)){fclose(f);return 0;}
        }
    }
    if(ferror(f)){fclose(f);return error("error reading animation CFG");}
    fclose(f);
    if(version_seen&&version_number>1U&&(!saw_start||!saw_end))return error("wolfanim v2 STARTANIMS/ENDANIMS required");
    if(nbank==0U)return error("no recognized animation rows");
    strcpy(error_text,"OK");return 1;
}
unsigned int rtcw_clipbank_count(void){return nbank;}
const rtcw_clip *rtcw_clipbank_at(unsigned int idx){return idx<nbank?&bank[idx]:NULL;}
const char *rtcw_clipbank_error(void){return error_text;}
int rtcw_clipbank_report(const char *output){
    FILE *f;unsigned int i;
    if(!output)return error("missing report name");
    f=fopen(output,"wb");if(!f)return error("cannot create clip catalog");
    if(fputs("id\tname\tsource_line\tfirst_frame\tframe_count\tloop_frames\tfps_milli\tmove_speed\tblend\tpriority\tmodel_frames\tstatus\n",f)==EOF){fclose(f);return error("write error");}
    for(i=0U;i<nbank;i++){
        const rtcw_clip *c=&bank[i];
        if(fprintf(f,"%u\t%s\t%lu\t%lu\t%lu\t%lu\t%lu\t%ld\t%lu\t%lu\t%lu\t%s\n",
             i,c->name,c->line,c->first,c->count,c->loop,c->fps_milli,
             c->move_speed,c->blend,c->priority,total_frames,c->valid?"playable":"outside_model_frame_range")<0){fclose(f);return error("write error");}
    }
    if(fclose(f)!=0)return error("close report failed");
    return 1;
}
/* Exact integer division of frame_offset*1e6/fps_milli without 32-bit overflow. */
int rtcw_clip_elapsed_ms(unsigned long f,unsigned long fps,unsigned long *out){
    unsigned long q,r,a,b,c;
    if(!fps||!out)return 0;
    q=f/fps;r=f%fps;
    if(q>ULONG_MAX/1000000UL)return 0;
    a=q*1000000UL;
    b=r*1000UL; /* fps <= 1,000,000 ensures b <= 1e9 */
    c=(b/fps)*1000UL+((b%fps)*1000UL)/fps;
    if(a>ULONG_MAX-c)return 0;
    *out=a+c;return 1;
}
