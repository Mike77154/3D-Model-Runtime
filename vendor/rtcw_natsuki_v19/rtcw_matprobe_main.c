/* Query the first supported shader texture at a specified animation time. CC0. */
#include "rtcw_material.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
int main(int argc,char **argv){
    char texture[256],*end;unsigned long ms;int rc;
    if(argc!=4){fprintf(stderr,"Usage: rtcw_matprobe file.shader material time_ms\n");return 2;}
    errno=0;ms=strtoul(argv[3],&end,10);
    if(errno||end==argv[3]||*end||argv[3][0]=='-')return 2;
    rc=rtcw_material_reference_time(argv[1],argv[2],ms,texture,sizeof(texture));
    if(rc<0){fprintf(stderr,"invalid/unreadable shader or unsupported animMap\n");return 1;}
    if(rc==0){fprintf(stderr,"material not found or missing supported stage\n");return 1;}
    printf("%s\n",texture);return 0;
}
