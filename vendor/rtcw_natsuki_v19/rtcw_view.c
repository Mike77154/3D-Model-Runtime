/* CLI wireframe and OBJ exporter: RTCW .md3 .mdc .mds */
#include "rtcw_codec.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
int main(int argc,char **argv){
    char *end;
    unsigned long frame=0;
    if(argc<5||argc>7){
        fprintf(stderr,"Usage: rtcw_view model.mds output.obj output.ppm report.txt [frame] [skin]\n");
        return 2;
    }
    if(argc>=6){
        errno=0;frame=strtoul(argv[5],&end,10);
        if(errno||end==argv[5]||*end!='\0'){fprintf(stderr,"invalid frame\n");return 2;}
    }
    if(!rtcw_open(argv[1])||!rtcw_decode(frame)){
        fprintf(stderr,"RTCW ERROR: %s\n",rtcw_error());return 1;
    }
    if(argc==7 && !rtcw_apply_skin(argv[6])){
        fprintf(stderr,"skin ERROR: cannot load/parse skin\n");return 1;
    }
    if(!rtcw_save_obj(argv[2])||!rtcw_save_ppm(argv[3])||!rtcw_report(argv[4])){
        fprintf(stderr,"export ERROR: %s\n",rtcw_error());return 1;
    }
    printf("OK: %s frame %lu/%lu: %lu vertices, %lu triangles, %u surfaces, %lu bones\n",
        rtcw_current.format,frame,rtcw_current.frames,rtcw_current.vertex_count,
        rtcw_current.triangle_count,rtcw_current.surface_count,rtcw_current.bone_count);
    return 0;
}
