/* Two-piece tag scene exporter, strict C89. */
#include "rtcw_codec.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
static int parse_frame(const char *str,unsigned long *v){
    char *end;
    errno=0;
    *v=strtoul(str,&end,10);
    return !errno && end!=str && *end=='\0' && str[0]!='-';
}
int main(int argc,char **argv){
    unsigned long pf,cf;
    const char *ps,*cs;
    if(argc!=11){
        fprintf(stderr,"Usage: rtcw_attach parent.mds parent_frame parent.skin|- tag child.md3 child_frame child.skin|- out.obj out.ppm report.txt\n");
        return 2;
    }
    if(!parse_frame(argv[2],&pf)||!parse_frame(argv[6],&cf)){
        fprintf(stderr,"invalid frame number\n");return 2;
    }
    ps=argv[3][0]=='-' && argv[3][1]=='\0'?NULL:argv[3];
    cs=argv[7][0]=='-' && argv[7][1]=='\0'?NULL:argv[7];
    if(!rtcw_attach_pair(argv[1],pf,ps,argv[4],argv[5],cf,cs)){
        fprintf(stderr,"RTCW attach failed: %s\n",rtcw_attach_error());return 1;
    }
    if(!rtcw_save_obj(argv[8])||!rtcw_save_ppm(argv[9])||!rtcw_report(argv[10])){
        fprintf(stderr,"RTCW output failed: %s\n",rtcw_error());return 1;
    }
    printf("OK tag-scene: %lu vertices, %lu triangles, %u surfaces\n",
           rtcw_current.vertex_count,rtcw_current.triangle_count,
           rtcw_current.surface_count);
    return 0;
}
