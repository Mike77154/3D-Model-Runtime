/* CC0-1.0 independent ISO C89 parser of RTCW wolfanim.script STRUCTURE.
   No runtime AI, condition evaluation, dynamic memory, floats, or 3rd party code.
   Source semantics checked against GPL project RTCW-SP src/game/bg_animation.c,
   no source code copied. Unknown command syntax fails closed. */
#include "rtcw_animscript.h"
#include "rtcw_clipbank.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#define TOKEN_LIMIT 32768U
#define DEPTH_LIMIT 4U
#define WORD_MAX 95U
typedef struct rtcw_word_s {char s[WORD_MAX+1U];unsigned long line;int type;} rtcw_word;
typedef struct rtcw_scope_s {
    char state[32],movement[32],event[48],old_state[32],new_state[32];
    char condition[RTCW_SCRIPT_CONDITION+1U];
} rtcw_scope;
static rtcw_word words[TOKEN_LIMIT];
static rtcw_script_command commands[RTCW_SCRIPT_MAX_COMMANDS];
static rtcw_scope scopes[DEPTH_LIMIT+1U];
static unsigned int nwords,ncommands,nunresolved,defines_seen;
static char err[200]="OK";
static int fail(const char *s){strncpy(err,s,sizeof(err)-1U);err[sizeof(err)-1U]=0;return 0;}
static int fold(int c){return c>='A'&&c<='Z'?c+32:c;}
static int eq(const char *a,const char *b){while(*a&&*b){if(fold((unsigned char)*a)!=fold((unsigned char)*b))return 0;a++;b++;}return !*a&&!*b;}
static int copy(char *dst,size_t cap,const char *s){size_t n=strlen(s);if(n>=cap)return 0;memcpy(dst,s,n+1U);return 1;}
static int addword(const char *s,unsigned long line,int type){
    if(nwords>=TOKEN_LIMIT)return fail("script has too many tokens (32768 limit)");
    if(!copy(words[nwords].s,sizeof(words[nwords].s),s))return fail("script token exceeds 95 chars");
    words[nwords].line=line;words[nwords].type=type;nwords++;return 1;
}
static int lex(const char *path){
    FILE *f=fopen(path,"rb");char buf[WORD_MAX+1U];unsigned int n=0U;
    unsigned long line=1UL;int c,next,in_block=0,in_quote=0;
    if(!f)return fail("cannot open wolfanim.script");
    while((c=fgetc(f))!=EOF){
        if(in_block){if(c=='*'){next=fgetc(f);if(next=='/')in_block=0;else if(next!=EOF)ungetc(next,f);}if(c=='\n')line++;continue;}
        if(!in_quote&&(c=='/'||c=='#')){
            if(c=='/'){
                next=fgetc(f);
                if(next=='*'){
                    if(n){buf[n]=0;if(!addword(buf,line,0))goto bad;n=0;}
                    in_block=1;continue;
                }
                if(next!='/'){
                    if(next!=EOF)ungetc(next,f);
                }else{
                    if(n){buf[n]=0;if(!addword(buf,line,0))goto bad;n=0;}
                    while((c=fgetc(f))!=EOF&&c!='\n'){}
                    if(c=='\n')line++;
                    continue;
                }
            }else{
                if(n){buf[n]=0;if(!addword(buf,line,0))goto bad;n=0;}
                while((c=fgetc(f))!=EOF&&c!='\n'){}
                if(c=='\n')line++;
                continue;
            }
        }
        if(c=='\n'){if(in_quote){fail("unterminated quoted token");goto bad;}if(n){buf[n]=0;if(!addword(buf,line,0))goto bad;n=0;}line++;continue;}
        if(!in_quote&&(c==' '||c=='\t'||c=='\r')){if(n){buf[n]=0;if(!addword(buf,line,0))goto bad;n=0;}continue;}
        if(!in_quote&&(c=='{'||c=='}')){if(n){buf[n]=0;if(!addword(buf,line,0))goto bad;n=0;}if(!addword(c=='{'?"{":"}",line,c=='{'?1:2))goto bad;continue;}
        if(c=='"'){if(n==0U&&!in_quote){in_quote=1;continue;}if(in_quote){in_quote=0;continue;}}
        if(c<32||c>126){fail("invalid control or non-ASCII byte in script");goto bad;}
        if(n>=WORD_MAX){fail("script token exceeds 95 chars");goto bad;}
        buf[n++]=(char)c;
    }
    if(ferror(f)){fail("script input read failed");goto bad;}
    if(in_quote||in_block){fail("unterminated comment or quoted string");goto bad;}
    if(n){buf[n]=0;if(!addword(buf,line,0))goto bad;}
    fclose(f);return 1;
 bad:fclose(f);return 0;
}
static int num(const char *s,unsigned long *out){unsigned long v=0UL,d;
    if(!s||!*s)return 0;
    while(*s){if(*s<'0'||*s>'9')return 0;d=(unsigned long)(*s-'0');if(v>(ULONG_MAX-d)/10UL)return 0;v=v*10UL+d;s++;}
    *out=v;return 1;
}
static int append_range(char *buf,size_t cap,unsigned int lo,unsigned int hi){
    unsigned int i;size_t p=0U,l;
    buf[0]=0;
    for(i=lo;i<hi;i++){
        l=strlen(words[i].s);
        if(p+l+(p?1U:0U)>=cap)return 0;
        if(p)buf[p++]=' ';
        memcpy(buf+p,words[i].s,l);p+=l;buf[p]=0;
    }
    return 1;
}
static int section_id(const char *s){
    if(eq(s,"defines"))return 1;
    if(eq(s,"animations"))return 2;
    if(eq(s,"canned_animations"))return 3;
    if(eq(s,"statechanges"))return 4;
    if(eq(s,"events"))return 5;
    return 0;
}
static const char *section_name(int id){
    static const char *arr[]={"","defines","animations","canned_animations","statechanges","events"};
    return id>=1&&id<=5?arr[id]:"unknown";
}
static int part(const char *s){return eq(s,"BOTH")||eq(s,"LEGS")||eq(s,"TORSO");}
static void resolve(const char *name,unsigned int *matches,unsigned int *playable){
    unsigned int i;*matches=0U;*playable=0U;
    for(i=0U;i<rtcw_clipbank_count();i++){
        const rtcw_clip *c=rtcw_clipbank_at(i);
        if(eq(name,c->name)){(*matches)++;if(c->valid)(*playable)++;}
    }
}
static int parse_command(unsigned int lo,unsigned int hi,int sec,const rtcw_scope *sc){
    rtcw_script_command *c;unsigned int pos,p=0U;unsigned long v;
    if(lo==hi)return 1;
    if(ncommands>=RTCW_SCRIPT_MAX_COMMANDS)return fail("command limit 2048 exceeded");
    c=&commands[ncommands];memset(c,0,sizeof(*c));
    copy(c->section,sizeof(c->section),section_name(sec));
    copy(c->state,sizeof(c->state),sc->state);copy(c->movement,sizeof(c->movement),sc->movement);
    copy(c->old_state,sizeof(c->old_state),sc->old_state);copy(c->new_state,sizeof(c->new_state),sc->new_state);
    copy(c->event,sizeof(c->event),sc->event);copy(c->condition,sizeof(c->condition),sc->condition);
    c->line=words[lo].line;pos=lo;
    while(pos<hi&&p<2U&&part(words[pos].s)){
        if(pos+1U>=hi||part(words[pos+1U].s))return fail("animation body part missing clip name");
        if(!copy(c->body[p],sizeof(c->body[p]),words[pos].s)||!copy(c->clip[p],sizeof(c->clip[p]),words[pos+1U].s))return fail("clip name too long");
        resolve(c->clip[p],&c->clip_matches[p],&c->playable[p]);
        if(c->clip_matches[p]!=1U||c->playable[p]!=1U)nunresolved++;
        pos+=2U;
        if(pos<hi&&eq(words[pos].s,"duration")){
            if(pos+1U>=hi||!num(words[pos+1U].s,&v)||v>3600000UL)return fail("invalid animation override duration");
            c->duration[p]=v;pos+=2U;
        }
        p++;if(eq(c->body[p-1U],"BOTH"))break;
    }
    c->parts=p;
    while(pos<hi){
        if(pos+1U>=hi)return fail("script command option missing value");
        if(eq(words[pos].s,"sound")){
            if(c->sound[0]||!copy(c->sound,sizeof(c->sound),words[pos+1U].s))return fail("duplicate/long sound reference");
        }else if(eq(words[pos].s,"showpart")){
            if(!num(words[pos+1U].s,&v)||v>7UL)return fail("invalid showpart index");
            c->showparts|=(1U<<(unsigned int)v);
        }else if(eq(words[pos].s,"hidepart")){
            if(!num(words[pos+1U].s,&v)||v>7UL)return fail("invalid hidepart index");
            c->hideparts|=(1U<<(unsigned int)v);
        }else{return fail("unsupported or malformed wolfanim.script command option");}
        pos+=2U;
    }
    if(!p&&!c->sound[0]&&!c->showparts&&!c->hideparts)return fail("unrecognized script command");
    c->sound_only=p==0U;ncommands++;return 1;
}
static int block_open(int sec,unsigned int depth,unsigned int lo,unsigned int hi,rtcw_scope *sc){
    unsigned int count=hi-lo;
    if(depth==0U){
        if((sec==2||sec==3)&&count==2U&&eq(words[lo].s,"state"))return copy(sc->state,sizeof(sc->state),words[lo+1U].s)?1:fail("state name too long");
        if(sec==4&&count==3U&&eq(words[lo].s,"statechange")){
            if(!copy(sc->old_state,sizeof(sc->old_state),words[lo+1U].s)||!copy(sc->new_state,sizeof(sc->new_state),words[lo+2U].s))return fail("statechange name too long");
            return 1;
        }
        if(sec==5&&count==1U)return copy(sc->event,sizeof(sc->event),words[lo].s)?1:fail("event name too long");
        return fail("invalid top-level block in animation script");
    }
    if(depth==1U&&(sec==2||sec==3)){
        if(count!=1U)return fail("invalid movement type header");
        return copy(sc->movement,sizeof(sc->movement),words[lo].s)?1:fail("movement name too long");
    }
    if(((sec==2||sec==3)&&depth==2U)||((sec==4||sec==5)&&depth==1U)){
        if(!count)return fail("empty condition header");
        if(!append_range(sc->condition,sizeof(sc->condition),lo,hi))return fail("condition text too long");
        return 1;
    }
    return fail("unexpected nested braces in animation script");
}
static int parse(void){
    unsigned int i=0U,start=0U,depth=0U,j;int sec=0,active;
    memset(scopes,0,sizeof(scopes));ncommands=nunresolved=defines_seen=0U;
    while(i<nwords){
        active=depth>0U&&((sec==2||sec==3)?depth==3U:(sec==4||sec==5)?depth==2U:0);
        if(words[i].type==1){
            if(active)return fail("command block cannot contain nested braces");
            if(depth>=DEPTH_LIMIT)return fail("too many nested script blocks");
            if(sec==0||sec==1)return fail("unexpected brace outside animation sections");
            scopes[depth+1U]=scopes[depth];
            if(!block_open(sec,depth,start,i,&scopes[depth+1U]))return 0;
            depth++;i++;start=i;continue;
        }
        if(words[i].type==2){
            if(active&&i>start){if(!parse_command(start,i,sec,&scopes[depth]))return 0;}
            else if(i>start)return fail("unexpected content before closing brace");
            if(depth==0U)return fail("unbalanced script closing brace");
            depth--;i++;start=i;continue;
        }
        /* A new source line delimits commands, but headers may put { on the next line. */
        if(i>start && words[i].line!=words[i-1U].line){
            if(active){if(!parse_command(start,i,sec,&scopes[depth]))return 0;start=i;}
            else if(depth==0U){
                if(i-start==1U&&section_id(words[start].s)){
                    sec=section_id(words[start].s);start=i;
                }else if(sec==1&&eq(words[start].s,"set")){defines_seen++;start=i;}
                else if(sec==1)return fail("unexpected defines directive");
                /* preserve STATE or condition header for a brace on following line */
            }
        }
        /* A section can immediately follow the last closed brace and be inline. */
        if(depth==0U&&i==start&&section_id(words[i].s)){
            if(i+1U==nwords||words[i+1U].line!=words[i].line){sec=section_id(words[i].s);i++;start=i;continue;}
        }
        i++;
    }
    if(depth!=0U)return fail("unclosed braces in wolfanim.script");
    /* trailing metadata lines after last section not treated as commands */
    if(ncommands==0U)return fail("no animation commands found");
    for(j=0U;j<ncommands;j++){
        if(commands[j].parts>2U)return fail("invalid internal part count");
    }
    strcpy(err,"OK");return 1;
}
int rtcw_animscript_read(const char *path){
    nwords=ncommands=nunresolved=0U;
    if(!path||!*path)return fail("missing script path");
    if(!lex(path))return 0;
    return parse();
}
const char *rtcw_animscript_error(void){return err;}
unsigned int rtcw_animscript_count(void){return ncommands;}
unsigned int rtcw_animscript_unresolved(void){return nunresolved;}
const rtcw_script_command *rtcw_animscript_at(unsigned int i){return i<ncommands?&commands[i]:NULL;}
int rtcw_animscript_report(const char *path){
    FILE *f;unsigned int i,p;
    if(!path||!*path)return fail("missing output path");
    f=fopen(path,"wb");if(!f)return fail("cannot open animation graph report");
    if(fputs("line\tsection\tstate\tmovement\tevent\told_state\tnew_state\tcondition\tpart\tclip\tclip_matches\tplayable_matches\tduration_override_ms\tsound\tshowpart_mask\thidepart_mask\tvalidation\n",f)==EOF){fclose(f);return fail("report header write failed");}
    for(i=0U;i<ncommands;i++){
        const rtcw_script_command *c=&commands[i];
        for(p=0U;p<(c->parts?c->parts:1U);p++){
            const char *valid=c->parts?(c->clip_matches[p]==1U&&c->playable[p]==1U?"resolved":"unresolved_or_ambiguous"):"sound_or_accessory_only";
            if(fprintf(f,"%lu\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%u\t%u\t%lu\t%s\t%u\t%u\t%s\n",
                 c->line,c->section,c->state,c->movement,c->event,c->old_state,c->new_state,
                 c->condition,c->body[p],c->clip[p],c->clip_matches[p],c->playable[p],c->duration[p],
                 c->sound,c->showparts,c->hideparts,valid)<0){fclose(f);return fail("report write failed");}
        }
    }
    if(fclose(f)!=0)return fail("report close failed");
    return 1;
}
