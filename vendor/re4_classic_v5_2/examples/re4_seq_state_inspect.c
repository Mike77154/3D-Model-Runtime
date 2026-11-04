/* Optional local test CLI. Rules are AUTHOR-SUPPLIED hypotheses / mod data;
   never assumes a RE4-native opcode meaning. Strict ANSI C89. */
#include "re4c3d/re4c3d.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#define FILE_CAP (4UL*1024UL*1024UL)
#define MAX_RULES 32U
static re4_u8 fcv_bytes[FILE_CAP],seq_bytes[FILE_CAP],arena_bytes[65536U];
static re4_seq_state_rule rules[MAX_RULES];
static int readfile(const char *p,re4_u8 *dst,re4_span *out)
{
    FILE *fd;long len;
    fd=fopen(p,"rb");if(!fd)return 0;
    if(fseek(fd,0,SEEK_END)!=0 || (len=ftell(fd))<0L ||
       (unsigned long)len>FILE_CAP || fseek(fd,0,SEEK_SET)!=0){fclose(fd);return 0;}
    if(fread(dst,1U,(size_t)len,fd)!=(size_t)len){fclose(fd);return 0;}
    fclose(fd);out->data=dst;out->size=(re4_size)len;return 1;
}
static int read_decimal(const char *p,re4_u32 *out)
{
    char *end;unsigned long n;
    if(!p || !*p || *p=='-' || *p=='+')return 0;
    errno=0;n=strtoul(p,&end,10);
    if(errno==ERANGE || *end || n>0xFFFFFFFFUL)return 0;
    *out=(re4_u32)n;return 1;
}
/* Grammar: --rule=code:aux:action:target:value
   code and aux decimal (aux can be *), actions show/hide/pose/parent/unparent.
   Example: --rule=17:*:show:3:0 . This is a TEST rule, NOT a RE4 mapping. */
static int parse_rule(const char *source,re4_seq_state_rule *r)
{
    char buffer[128],*tok[5],*p;re4_u32 n,i;
    if(strlen(source)>127U)return 0;
    strcpy(buffer,source);
    tok[0]=buffer;
    for(i=0U;i<4U;++i){
        p=strchr(tok[i],':');if(!p)return 0;
        *p='\0';tok[i+1U]=p+1;
    }
    if(strchr(tok[4],':'))return 0;
    memset(r,0,sizeof(*r));
    if(!read_decimal(tok[0],&n) || n==0U || n>255U)return 0;
    r->event_code=(re4_u8)n;
    if(strcmp(tok[1],"*")==0)r->match_auxiliary=0U;
    else{
        if(!read_decimal(tok[1],&n) || n>255U)return 0;
        r->match_auxiliary=1U;r->auxiliary=(re4_u8)n;
    }
    if(strcmp(tok[2],"show")==0)r->action=RE4_SEQ_ACTION_SHOW;
    else if(strcmp(tok[2],"hide")==0)r->action=RE4_SEQ_ACTION_HIDE;
    else if(strcmp(tok[2],"pose")==0)r->action=RE4_SEQ_ACTION_SET_POSE;
    else if(strcmp(tok[2],"parent")==0)r->action=RE4_SEQ_ACTION_SET_PARENT;
    else if(strcmp(tok[2],"unparent")==0)r->action=RE4_SEQ_ACTION_CLEAR_PARENT;
    else return 0;
    if(!read_decimal(tok[3],&n) || n>255U)return 0;
    r->target=(re4_u8)n;
    if(!read_decimal(tok[4],&n) || n>255U)return 0;
    r->value=(re4_u8)n;
    return 1;
}
int main(int argc,char **argv)
{
    int i;re4_u32 target,n=0U;re4_bool strict=RE4_FALSE;
    re4_span fcv,seq;re4_arena arena;re4_seq_pair_report pair;
    re4_seq_timeline timeline;re4_seq_state state;re4_seq_state_stats stats;
    re4_err err;
    if(argc<4 || !read_decimal(argv[3],&target)){
        fputs("Usage: re4_seq_state_inspect CLIP.fcv EVENTS.seq TO_Q6 [--strict] [--rule=code:aux:action:target:value]...\n",stderr);
        return 2;
    }
    for(i=4;i<argc;++i){
        if(strcmp(argv[i],"--strict")==0)strict=RE4_TRUE;
        else if(strncmp(argv[i],"--rule=",7)==0 && n<MAX_RULES &&
                parse_rule(argv[i]+7,&rules[n]))++n;
        else{fprintf(stderr,"Invalid or excess rule: %s\n",argv[i]);return 2;}
    }
    if(!readfile(argv[1],fcv_bytes,&fcv) || !readfile(argv[2],seq_bytes,&seq)){
        fputs("Cannot read FCV / SEQ or file exceeds 4 MiB\n",stderr);return 2;
    }
    re4_arena_init(&arena,arena_bytes,sizeof(arena_bytes));
    err=re4_seq_pair_probe(fcv,seq,&arena,&pair);
    if(err!=RE4_OK){fprintf(stderr,"Malformed FCV/SEQ: %s\n",re4_err_name(err));return 2;}
    if(pair.timing==RE4_PAIR_SEQ_OVERRUN || pair.timing==RE4_PAIR_DISPATCH_TOO_LONG){
        fputs("Unsupported or out-of-duration SEQ pair\n",stderr);return 2;
    }
    err=re4_seq_timeline_read(seq,RE4_ENDIAN_LITTLE,&timeline);
    if(err!=RE4_OK)return 2;
    /* Bound one seek to 128 loops. Explicit CLI operands never prove canon. */
    err=re4_seq_state_seek(&timeline,pair.loop_period_q6,target,128U,
                           rules,n,strict,0U,0U,&state,&stats);
    if(err!=RE4_OK){fprintf(stderr,"State seek failed: %s\n",re4_err_name(err));return 1;}
    puts("timing_evidence_only,at_q6,rules,raw_nonzero,applied,unmapped,visible_hex,pose_id,clip_period_q6");
    printf("%s,%lu,%lu,%lu,%lu,%lu,%08lx,%u,%lu\n",
           pair.timing==RE4_PAIR_EXACT_END?"EXACT":"WITHIN",
           (unsigned long)target,(unsigned long)n,
           (unsigned long)stats.raw_nonzero,(unsigned long)stats.applied,
           (unsigned long)stats.unmapped,(unsigned long)state.visible_mask,
           (unsigned int)state.pose_id,(unsigned long)pair.loop_period_q6);
    return 0;
}
