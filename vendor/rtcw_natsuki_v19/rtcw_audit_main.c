#include "rtcw_audit.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static int cap_arg(const char *s,unsigned int *v){
    unsigned long n=0UL;
    if(!s||!*s)return 0;
    while(*s){unsigned int d;
        if(*s<'0'||*s>'9')return 0;
        d=(unsigned int)(*s-'0');
        if(n>(256UL-d)/10UL)return 0;
        n=n*10UL+d;s++;
    }
    if(n<1UL||n>256UL)return 0;
    *v=(unsigned int)n;return 1;
}
int main(int argc,char **argv){
    unsigned int cap=24U;int ok=0;
    if(argc>=2&&strcmp(argv[1],"local")==0&&(argc==6||argc==7)){
        if(argc==7&&!cap_arg(argv[6],&cap))return 2;
        ok=rtcw_audit_local(argv[2],argv[3],argv[4],argv[5],cap);

    }else if(argc>=2&&strcmp(argv[1],"batch-vfs")==0&&(argc==4||argc==5||argc==6)){
        unsigned int max_models=128U;
        if(argc>=5&&!cap_arg(argv[4],&max_models))return 2;
        if(argc==6&&!cap_arg(argv[5],&cap))return 2;
        ok=rtcw_audit_batch_vfs(argv[2],argv[3],max_models,cap);
    }else if(argc>=2&&strcmp(argv[1],"vfs")==0&&(argc==5||argc==6)){
        if(argc==6&&!cap_arg(argv[5],&cap))return 2;
        ok=rtcw_audit_vfs(argv[2],argv[3],argv[4],cap);
    }else{
        fprintf(stderr,"Usage:\n rtcw_audit local MODEL CFG|- SKIN|- NEW_OUTDIR [1..256]\n rtcw_audit vfs MOUNTS.TXT VIRTUAL_MODEL NEW_OUTDIR [1..256]\n rtcw_audit batch-vfs MOUNTS.TXT NEW_OUTDIR [max_models:1..256] [frame_cap:1..256]\n");
        return 2;
    }
    if(!ok){fprintf(stderr,"RTCW audit: %s\n",rtcw_audit_error());return 1;}
    return 0;
}
