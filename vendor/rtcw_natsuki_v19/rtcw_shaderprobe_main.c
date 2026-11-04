/* Inspect first-stage material flags, C89, CC0-1.0. */
#include "rtcw_shaderstyle.h"
#include <stdio.h>
int main(int argc,char **argv){
    rtcw_shaderstyle s;int r;
    if(argc!=3){fprintf(stderr,"usage: rtcw_shaderprobe shader_file material_name\n");return 2;}
    r=rtcw_shaderstyle_lookup(argv[1],argv[2],&s);
    if(r<0){fprintf(stderr,"shader parser: %s\n",rtcw_shaderstyle_error());return 1;}
    printf("found\tblend\talpha_func\tstages\tunhandled\n");
    printf("%d\t%d\t%d\t%u\t%u\n",r,s.blend,s.alpha_func,s.stage_count,s.unhandled);
    return 0;
}
